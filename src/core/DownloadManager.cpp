#include "DownloadManager.h"
#include "GalleryDlArgsBuilder.h"
#include "YtDlpArgsBuilder.h"
#include "ArchiveManager.h"
#include "SortingManager.h"
#include "GalleryDlWorker.h"
#include "YtDlpWorker.h"
#include "PlaylistExpander.h"
#include "MetadataEmbedder.h"
#include "core/ProcessUtils.h"
#include <QUuid>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QThread>
#include <QCoreApplication>
#include <QMessageBox>
#include <QPushButton>

namespace {
    // Helper function for moving gallery-dl directories across different hard drives
    bool copyDirectoryRecursively(const QString &sourceDir, const QString &destDir) {
        QDir source(sourceDir);
        if (!source.exists()) return false;
        QDir dest(destDir);
        if (!dest.exists()) dest.mkpath(".");
        bool success = true;
        QFileInfoList entries = source.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
        for (const QFileInfo &entry : entries) {
            QString srcPath = entry.absoluteFilePath();
            QString dstPath = dest.absoluteFilePath(entry.fileName());
            if (entry.isDir()) success &= copyDirectoryRecursively(srcPath, dstPath);
            else { if (QFile::exists(dstPath)) QFile::remove(dstPath); success &= QFile::copy(srcPath, dstPath); }
        }
        return success;
    }
}

DownloadManager::DownloadManager(ConfigManager *configManager, QObject *parent)
    : QObject(parent), m_configManager(configManager), m_sleepMode(NoSleep),
      m_queuedDownloadsCount(0), m_activeDownloadsCount(0), m_completedDownloadsCount(0) {

    m_sortingManager = new SortingManager(m_configManager, this);

    QString maxThreadsStr = m_configManager->get("General", "max_threads", "4").toString();
    if (maxThreadsStr == "1 (short sleep)") {
        m_maxConcurrentDownloads = 1;
        m_sleepMode = ShortSleep;
    } else if (maxThreadsStr == "1 (long sleep)") {
        m_maxConcurrentDownloads = 1;
        m_sleepMode = LongSleep;
    } else {
        m_maxConcurrentDownloads = maxThreadsStr.toInt();
        m_sleepMode = NoSleep;
    }

    m_sleepTimer = new QTimer(this);
    m_sleepTimer->setSingleShot(true);
    connect(m_sleepTimer, &QTimer::timeout, this, &DownloadManager::onSleepTimerTimeout);

    QTimer::singleShot(0, this, &DownloadManager::loadQueueState);

    emitDownloadStats();
}

DownloadManager::~DownloadManager() {
    // Prevent starting new downloads during destruction
    m_downloadQueue.clear();

    // Safely cancel all active downloads to ensure child processes are killed
    QStringList activeIds = m_activeWorkers.keys();
    for (const QString &id : activeIds) {
        cancelDownload(id);
    }
    QStringList embedderIds = m_activeEmbedders.keys();
    for (const QString &id : embedderIds) {
        cancelDownload(id);
    }
    QStringList pausedIds = m_pausedItems.keys();
    for (const QString &id : pausedIds) {
        cancelDownload(id);
    }
}

void DownloadManager::enqueueDownload(const QString &url, const QVariantMap &options) {
    QString downloadType = options.value("type", "video").toString();

    // Intercept for runtime format selection before doing anything else
    bool needsRuntimeSelection = false;
    if (downloadType == "video") {
        if (m_configManager->get("Video", "video_quality").toString() == "Select at Runtime") {
            needsRuntimeSelection = true;
        }
    } else if (downloadType == "audio") {
        if (m_configManager->get("Audio", "audio_quality").toString() == "Select at Runtime") {
            needsRuntimeSelection = true;
        }
    }

    if (needsRuntimeSelection && !options.value("runtime_format_selected", false).toBool()) {
        fetchFormatsForSelection(url, options);
        return;
    }

    if (downloadType == "gallery") {
        DownloadItem item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.url = url;
        item.options = options;
        item.playlistIndex = -1;

        m_downloadQueue.enqueue(item);
        m_queuedDownloadsCount++;

        QVariantMap uiData;
        uiData["id"] = item.id;
        uiData["url"] = item.url;
        uiData["status"] = "Queued";
        uiData["progress"] = 0;
        uiData["options"] = options;
        emit downloadAddedToQueue(uiData);

        emitDownloadStats();
        saveQueueState();
        startNextDownload();
    } else {
        PlaylistExpander *expander = new PlaylistExpander(url, m_configManager, this);
        expander->setProperty("options", options);

        connect(expander, &PlaylistExpander::expansionFinished, this, &DownloadManager::onPlaylistExpanded);
        connect(expander, &PlaylistExpander::playlistDetected, this, &DownloadManager::onPlaylistDetected);

        QString playlistLogic = options.value("playlist_logic", "Ask").toString();
        expander->startExpansion(playlistLogic);
        emit playlistExpansionStarted(url);
    }
}

void DownloadManager::fetchFormatsForSelection(const QString &url, const QVariantMap &options) {
    QProcess *process = new QProcess(this);
    QString ytDlpPath = ProcessUtils::findBinary("yt-dlp", m_configManager).path;
    
    QStringList args;
    args << "--dump-json" << "--no-playlist" << url;
    
    QString cookiesBrowser = m_configManager->get("General", "cookies_from_browser", "None").toString();
    if (cookiesBrowser != "None") {
        args << "--cookies-from-browser" << cookiesBrowser.toLower();
    }
    
    connect(process, &QProcess::finished, this, [this, process, url, options](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            QByteArray output = process->readAllStandardOutput();
            QJsonDocument doc = QJsonDocument::fromJson(output);
            if (doc.isObject()) {
                emit formatSelectionRequested(url, options, doc.object().toVariantMap());
            } else {
                const QString message = "yt-dlp returned invalid format metadata.";
                qWarning() << "DownloadManager: Invalid JSON returned from yt-dlp -J";
                emit formatSelectionFailed(url, message);
            }
        } else {
            const QString errorText = QString::fromUtf8(process->readAllStandardError()).trimmed();
            qWarning() << "DownloadManager: yt-dlp -J failed:" << errorText;
            emit formatSelectionFailed(url, errorText.isEmpty() ? "Failed to retrieve available formats." : errorText);
        }
        process->deleteLater();
    });
    
    process->start(ytDlpPath, args);
}

void DownloadManager::resumeDownloadWithFormat(const QString &url, const QVariantMap &options, const QString &formatId) {
    QVariantMap newOptions = options;
    newOptions["runtime_format_selected"] = true;
    if (options.value("type", "video").toString() == "audio") {
        newOptions["runtime_audio_format"] = formatId;
    } else {
        newOptions["runtime_video_format"] = formatId;
    }
    enqueueDownload(url, newOptions);
}

void DownloadManager::cancelDownload(const QString &id) {
    bool cancelled = false;
    for (int i = 0; i < m_downloadQueue.size(); ++i) {
        if (m_downloadQueue.at(i).id == id) {
            m_downloadQueue.removeAt(i);
            m_queuedDownloadsCount--;
            qDebug() << "Cancelled queued download:" << id;
            emit downloadCancelled(id);
            cancelled = true;
            break;
        }
    }

    if (!cancelled && m_pausedItems.contains(id)) {
        DownloadItem item = m_pausedItems.take(id);
        if (!item.tempFilePath.isEmpty()) {
            QFile::remove(item.tempFilePath);
            QFileInfo fi(item.tempFilePath);
            QString infoFilePath = fi.absoluteDir().filePath(fi.completeBaseName() + ".info.json");
            QFile::remove(infoFilePath);
            qDebug() << "Cleaned up temporary files for cancelled paused download:" << id;
        }
        emit downloadCancelled(id);
        cancelled = true;
    }

    if (!cancelled && m_activeWorkers.contains(id)) {
        QObject *worker = m_activeWorkers.take(id);
        DownloadItem item = m_activeItems.value(id);

        if (!item.tempFilePath.isEmpty()) {
            QFile::remove(item.tempFilePath);
            QFileInfo fi(item.tempFilePath);
            QString infoFilePath = fi.absoluteDir().filePath(fi.completeBaseName() + ".info.json");
            QFile::remove(infoFilePath);
            qDebug() << "Cleaned up temporary files for cancelled download:" << id;
        }

        m_activeItems.remove(id);
        m_workerSpeeds.remove(id);
        updateTotalSpeed();

        YtDlpWorker *ytDlpWorker = qobject_cast<YtDlpWorker*>(worker);
        if (ytDlpWorker) {
            ytDlpWorker->killProcess();
        } else {
            GalleryDlWorker *galleryDlWorker = qobject_cast<GalleryDlWorker*>(worker);
            if (galleryDlWorker) {
                galleryDlWorker->killProcess();
            }
        }

        worker->deleteLater();
        m_activeDownloadsCount--;
        emit downloadCancelled(id);
        cancelled = true;
    } else if (!cancelled && m_activeEmbedders.contains(id)) {
        // Cancel a download that is currently in the post-processing metadata phase
        QObject *embedder = m_activeEmbedders.take(id);
        DownloadItem item = m_activeItems.take(id);
        
        if (!item.tempFilePath.isEmpty()) {
            QFile::remove(item.tempFilePath);
            QFileInfo fi(item.tempFilePath);
            QString infoFilePath = fi.absoluteDir().filePath(fi.completeBaseName() + ".info.json");
            QFile::remove(infoFilePath);
            qDebug() << "Cleaned up temporary files for cancelled metadata embed:" << id;
        }
        
        // Deleting the embedder will kill any active QProcess internally
        embedder->deleteLater();
        
        m_activeDownloadsCount--;
        emit downloadCancelled(id);
        cancelled = true;
    }

    if (cancelled) {
        emitDownloadStats();
        saveQueueState();
        startNextDownload();
    }
}

void DownloadManager::retryDownload(const QVariantMap &itemData) {
    DownloadItem item;
    item.id = itemData["id"].toString();
    item.url = itemData["url"].toString();
    item.options = itemData["options"].toMap();

    m_downloadQueue.enqueue(item);
    m_queuedDownloadsCount++;
    emit downloadAddedToQueue(itemData);
    emitDownloadStats();
    saveQueueState();
    startNextDownload();
}

void DownloadManager::resumeDownload(const QVariantMap &itemData) {
    retryDownload(itemData);
}

void DownloadManager::pauseDownload(const QString &id) {
    bool paused = false;
    
    for (int i = 0; i < m_downloadQueue.size(); ++i) {
        if (m_downloadQueue.at(i).id == id) {
            m_pausedItems[id] = m_downloadQueue.takeAt(i);
            m_queuedDownloadsCount--;
            qDebug() << "Paused queued download:" << id;
            emit downloadPaused(id);
            paused = true;
            break;
        }
    }

    if (!paused && m_activeWorkers.contains(id)) {
        QObject *worker = m_activeWorkers.take(id);
        DownloadItem item = m_activeItems.take(id);
        m_pausedItems[id] = item;
        
        m_workerSpeeds.remove(id);
        updateTotalSpeed();

        YtDlpWorker *ytDlpWorker = qobject_cast<YtDlpWorker*>(worker);
        if (ytDlpWorker) {
            ytDlpWorker->killProcess();
        } else {
            GalleryDlWorker *galleryDlWorker = qobject_cast<GalleryDlWorker*>(worker);
            if (galleryDlWorker) {
                galleryDlWorker->killProcess();
            }
        }
        
        worker->deleteLater();
        m_activeDownloadsCount--;
        qDebug() << "Paused active download:" << id;
        emit downloadPaused(id);
        paused = true;
    } else if (!paused && m_activeEmbedders.contains(id)) {
        qWarning() << "Cannot pause a download that is currently embedding metadata:" << id;
        emit downloadResumed(id); // Revert UI
        paused = true;
    }

    if (paused) {
        emitDownloadStats();
        saveQueueState();
        startNextDownload();
    }
}

void DownloadManager::unpauseDownload(const QString &id) {
    if (m_pausedItems.contains(id)) {
        DownloadItem item = m_pausedItems.take(id);
        m_downloadQueue.prepend(item); // Insert at front to resume immediately
        m_queuedDownloadsCount++;
        qDebug() << "Unpaused download:" << id;
        emit downloadResumed(id);
        emitDownloadStats();
        saveQueueState();
        startNextDownload();
    }
}

void DownloadManager::moveDownloadUp(const QString &id) {
    for (int i = 1; i < m_downloadQueue.size(); ++i) { // Can't move 0 up
        if (m_downloadQueue.at(i).id == id) {
            m_downloadQueue.swapItemsAt(i, i - 1);
            saveQueueState();
            break;
        }
    }
}

void DownloadManager::moveDownloadDown(const QString &id) {
    for (int i = 0; i < m_downloadQueue.size() - 1; ++i) { // Can't move last down
        if (m_downloadQueue.at(i).id == id) {
            m_downloadQueue.swapItemsAt(i, i + 1);
            saveQueueState();
            break;
        }
    }
}

void DownloadManager::onPlaylistDetected(const QString &url, int itemCount, const QVariantMap &options, const QList<QVariantMap> &expandedItems) {
    PlaylistExpander *expander = qobject_cast<PlaylistExpander*>(sender());
    QVariantMap storedOptions = options;
    if (expander) {
        storedOptions = expander->property("options").toMap();
        expander->deleteLater();
    }

    QMessageBox msgBox;
    msgBox.setWindowTitle("Playlist Detected");
    msgBox.setText(QString("A playlist with %1 items was detected for URL:\n%2").arg(itemCount).arg(url));
    msgBox.setInformativeText("What would you like to do?");

    QPushButton *downloadAllButton = msgBox.addButton("Download All", QMessageBox::AcceptRole);
    QPushButton *downloadSingleButton = nullptr;

    bool isSingleItemInPlaylist = false;
    if (!expandedItems.isEmpty()) {
        if (expandedItems.first().value("playlist_index", -1).toInt() != -1) {
            isSingleItemInPlaylist = true;
        }
    }

    if (isSingleItemInPlaylist) {
        downloadSingleButton = msgBox.addButton("Download Single Item", QMessageBox::ActionRole);
    }

    msgBox.addButton("Cancel", QMessageBox::RejectRole);
    msgBox.exec();

    QList<QVariantMap> finalItems;

    if (msgBox.clickedButton() == downloadAllButton) {
        finalItems = expandedItems;
    } else if (downloadSingleButton && msgBox.clickedButton() == downloadSingleButton) {
        if (!expandedItems.isEmpty()) {
            finalItems.append(expandedItems.first());
        }
    } else {
        emit downloadFinished(QUuid::createUuid().toString(QUuid::WithoutBraces), false, "Playlist download cancelled by user.");
        return;
    }

    emit playlistExpansionFinished(url, finalItems.count());

    for (const QVariantMap &itemData : finalItems) {
        DownloadItem item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.url = itemData["url"].toString();
        item.options = storedOptions;
        item.playlistIndex = itemData.value("playlist_index", -1).toInt();

        m_downloadQueue.enqueue(item);
        m_queuedDownloadsCount++;

        QVariantMap uiData;
        uiData["id"] = item.id;
        uiData["url"] = item.url;
        uiData["status"] = "Queued";
        uiData["progress"] = 0;
        uiData["options"] = storedOptions;
        emit downloadAddedToQueue(uiData);
    }

    emitDownloadStats();
    saveQueueState();
    startNextDownload();
}

void DownloadManager::onPlaylistExpanded(const QString &originalUrl, const QList<QVariantMap> &expandedItems, const QString &error) {
    PlaylistExpander *expander = qobject_cast<PlaylistExpander*>(sender());
    QVariantMap options;
    if (expander) {
        options = expander->property("options").toMap();
        expander->deleteLater();
    }

    if (!error.isEmpty()) {
        qDebug() << "Playlist expansion failed:" << error;
        return;
    }

    emit playlistExpansionFinished(originalUrl, expandedItems.count());

    for (const QVariantMap &itemData : expandedItems) {
        DownloadItem item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.url = itemData["url"].toString();
        item.options = options; // Pass the options from the UI
        item.playlistIndex = itemData.value("playlist_index", -1).toInt();

        m_downloadQueue.enqueue(item);
        m_queuedDownloadsCount++;

        QVariantMap uiData;
        uiData["id"] = item.id;
        uiData["url"] = item.url;
        uiData["status"] = "Queued";
        uiData["progress"] = 0;
        uiData["options"] = options;
        emit downloadAddedToQueue(uiData);
    }

    emitDownloadStats();
    saveQueueState();
    startNextDownload();
}

void DownloadManager::saveQueueState() {
    QJsonArray queueArray;
    for (auto it = m_activeItems.begin(); it != m_activeItems.end(); ++it) {
        QJsonObject obj;
        obj["id"] = it.value().id;
        obj["url"] = it.value().url;
        obj["options"] = QJsonObject::fromVariantMap(it.value().options);
        obj["status"] = "queued"; // Active items revert to queued on app start
        obj["playlistIndex"] = it.value().playlistIndex;
        queueArray.append(obj);
    }
    for (auto it = m_pausedItems.begin(); it != m_pausedItems.end(); ++it) {
        QJsonObject obj;
        obj["id"] = it.value().id;
        obj["url"] = it.value().url;
        obj["options"] = QJsonObject::fromVariantMap(it.value().options);
        obj["status"] = "paused";
        obj["playlistIndex"] = it.value().playlistIndex;
        queueArray.append(obj);
    }
    for (const auto& item : m_downloadQueue) {
        QJsonObject obj;
        obj["id"] = item.id;
        obj["url"] = item.url;
        obj["options"] = QJsonObject::fromVariantMap(item.options);
        obj["status"] = "queued";
        obj["playlistIndex"] = item.playlistIndex;
        queueArray.append(obj);
    }

    QString backupPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).filePath("downloads_backup.json");
    if (queueArray.isEmpty()) {
        QFile::remove(backupPath);
    } else {
        QFile file(backupPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(queueArray).toJson());
            file.close();
        }
    }
}

void DownloadManager::loadQueueState() {
    QString backupPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).filePath("downloads_backup.json");
    QFile file(backupPath);
    if (!file.exists()) return;

    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray() && !doc.array().isEmpty()) {
            QJsonArray arr = doc.array();
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(qobject_cast<QWidget*>(parent()), "Resume Downloads",
                QString("You have %1 incomplete download(s) from a previous session.\nWould you like to resume them?").arg(arr.size()),
                QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                for (const QJsonValue& val : arr) {
                    QJsonObject obj = val.toObject();
                    DownloadItem item;
                    item.id = obj["id"].toString();
                    item.url = obj["url"].toString();
                    item.options = obj["options"].toObject().toVariantMap();
                    item.playlistIndex = obj["playlistIndex"].toInt(-1);
                    
                    QString status = obj["status"].toString("queued");

                    QVariantMap uiData;
                    uiData["id"] = item.id;
                    uiData["url"] = item.url;
                    uiData["status"] = (status == "paused") ? "Paused" : "Queued";
                    uiData["progress"] = 0;
                    uiData["options"] = item.options;

                    if (status == "paused") {
                        m_pausedItems[item.id] = item;
                        emit downloadAddedToQueue(uiData);
                        emit downloadPaused(item.id);
                    } else {
                        m_downloadQueue.enqueue(item);
                        m_queuedDownloadsCount++;
                        emit downloadAddedToQueue(uiData);
                    }
                }
                emitDownloadStats();
                startNextDownload();
            } else {
                QFile::remove(backupPath);
            }
        }
    }
}

void DownloadManager::proceedWithDownload() {
    if (m_downloadQueue.isEmpty()) {
        checkQueueFinished();
        return;
    }

    DownloadItem item = m_downloadQueue.dequeue();
    m_queuedDownloadsCount--;
    m_activeDownloadsCount++;

    QString downloadType = item.options.value("type", "video").toString();

    if (downloadType == "gallery") {
        item.options["id"] = item.id;
        item.options["playlist_index"] = item.playlistIndex;
        GalleryDlArgsBuilder argsBuilder(m_configManager);
        QStringList args = argsBuilder.build(item.url, item.options);

        GalleryDlWorker *worker = new GalleryDlWorker(item.id, args);
        m_activeWorkers[item.id] = worker;
        m_activeItems[item.id] = item;

        connect(worker, &GalleryDlWorker::progressUpdated, this, &DownloadManager::onWorkerProgress);
        connect(worker, &GalleryDlWorker::finished, this, &DownloadManager::onGalleryDlWorkerFinished);
        connect(worker, &GalleryDlWorker::outputReceived, this, &DownloadManager::onWorkerOutputReceived);

        emit downloadStarted(item.id);
        worker->start();
    } else {
        item.options["id"] = item.id;
        item.options["playlist_index"] = item.playlistIndex;
        YtDlpArgsBuilder argsBuilder;
        QStringList args = argsBuilder.build(m_configManager, item.url, item.options);

        YtDlpWorker *worker = new YtDlpWorker(item.id, args);
        m_activeWorkers[item.id] = worker;
        m_activeItems[item.id] = item;

        connect(worker, &YtDlpWorker::progressUpdated, this, &DownloadManager::onWorkerProgress);
        connect(worker, &YtDlpWorker::finished, this, &DownloadManager::onWorkerFinished);
        connect(worker, &YtDlpWorker::outputReceived, this, &DownloadManager::onWorkerOutputReceived);

        emit downloadStarted(item.id);
        worker->start();
    }
    emitDownloadStats();
}

void DownloadManager::startNextDownload() {
    if (m_activeWorkers.count() >= m_maxConcurrentDownloads || m_downloadQueue.isEmpty()) {
        checkQueueFinished();
        return;
    }

    if (m_sleepMode != NoSleep && m_maxConcurrentDownloads == 1) {
        if (!m_sleepTimer->isActive()) {
            int sleepDuration = (m_sleepMode == ShortSleep) ? 5000 : 30000;
            qDebug() << "Starting sleep timer for" << sleepDuration << "ms.";
            m_sleepTimer->start(sleepDuration);
            return;
        } else {
            return;
        }
    }

    proceedWithDownload();
}

void DownloadManager::onSleepTimerTimeout() {
    qDebug() << "Sleep timer timed out. Attempting to start next download.";
    startNextDownload();
}

void DownloadManager::onWorkerProgress(const QString &id, const QVariantMap &progressData) {
    m_workerSpeeds[id] = progressData.value("speed_bytes", 0.0).toDouble();
    updateTotalSpeed();
    emit downloadProgress(id, progressData);
}

void DownloadManager::onWorkerFinished(const QString &id, bool success, const QString &message, const QString &finalFilename, const QString &originalDownloadedFilename, const QVariantMap &metadata) {
    if (!m_activeWorkers.contains(id)) return;

    QObject *workerObj = m_activeWorkers.take(id);
    DownloadItem &item = m_activeItems[id];
    m_workerSpeeds.remove(id);
    updateTotalSpeed();
    workerObj->deleteLater();

    m_activeDownloadsCount--;

    if (!success) {
        m_activeItems.remove(id);
        emit downloadFinished(id, false, message);
        saveQueueState();
        emitDownloadStats();
        startNextDownload();
        return;
    }

    if (metadata.contains("height") && metadata["height"].toInt() < 480) {
        emit videoQualityWarning(item.url, "Downloaded video quality is below 480p.");
    }

    QString normalizedFinal = QDir::fromNativeSeparators(finalFilename);
    QString normalizedOriginal = QDir::fromNativeSeparators(originalDownloadedFilename);

    item.tempFilePath = normalizedFinal.isEmpty() ? normalizedOriginal : normalizedFinal;
    item.originalDownloadedFilePath = normalizedOriginal;
    item.metadata = metadata;

    if (item.options.value("type").toString() == "audio" && item.playlistIndex > 0) {
        emit downloadProgress(id, {{"status", "Embedding metadata..."}});
        MetadataEmbedder *embedder = new MetadataEmbedder(this);
        m_activeEmbedders[id] = embedder;
        connect(embedder, &MetadataEmbedder::finished, this, [this, id](bool s, const QString &e){
            onMetadataEmbedded(id, s, e);
        });
        embedder->embedTrackNumber(item.tempFilePath, item.playlistIndex);
    } else {
        finalizeDownload(id, item, item.tempFilePath);
    }
    emitDownloadStats();
}

void DownloadManager::onGalleryDlWorkerFinished(const QString &id, bool success, const QString &message, const QString &finalFilename, const QVariantMap &metadata) {
    if (!m_activeWorkers.contains(id)) return;

    QObject *workerObj = m_activeWorkers.take(id);
    DownloadItem &item = m_activeItems[id];
    m_workerSpeeds.remove(id);
    updateTotalSpeed();
    workerObj->deleteLater();

    m_activeDownloadsCount--;

    if (!success) {
        m_activeItems.remove(id);
        emit downloadFinished(id, false, message);
        saveQueueState();
        emitDownloadStats();
        startNextDownload();
        return;
    }

    item.tempFilePath = finalFilename;
    item.originalDownloadedFilePath = "";
    item.metadata = metadata;

    finalizeDownload(id, item, item.tempFilePath);
    emitDownloadStats();
}

void DownloadManager::onMetadataEmbedded(const QString &id, bool success, const QString &error) {
    if (!m_activeEmbedders.contains(id)) return;

    MetadataEmbedder *embedder = qobject_cast<MetadataEmbedder*>(m_activeEmbedders.take(id));
    embedder->deleteLater();

    DownloadItem &item = m_activeItems[id];

    if (success) {
        finalizeDownload(id, item, item.tempFilePath);
    } else {
        m_activeItems.remove(id);
        emit downloadFinished(id, false, "Metadata embedding failed: " + error);
        saveQueueState();
        emitDownloadStats();
        startNextDownload();
    }
}

void DownloadManager::finalizeDownload(const QString &id, DownloadItem &item, const QString &filePath) {
    Q_UNUSED(filePath);
    qDebug() << "Starting finalizeDownload for id:" << id;

    if (item.options.value("type").toString() != "gallery" && item.metadata.isEmpty()) {
        QString tempDirPath = m_configManager->get("Paths", "temporary_downloads_directory").toString();
        QDir tempDir(tempDirPath);
        QStringList jsonFiles = tempDir.entryList(QStringList() << "*.info.json", QDir::Files, QDir::Time);
        if (jsonFiles.isEmpty()) {
            qWarning() << "No info.json found in temp dir";
            emit downloadFinished(id, false, "Downloaded file not found.");
            m_activeItems.remove(id);
            saveQueueState();
            emitDownloadStats();
            startNextDownload();
            return;
        }
        QString jsonPath = tempDir.filePath(jsonFiles.first());
        QFile jsonFile(jsonPath);
        if (!jsonFile.open(QIODevice::ReadOnly)) {
            qWarning() << "Could not open info.json:" << jsonPath;
            emit downloadFinished(id, false, "Downloaded file not found.");
            m_activeItems.remove(id);
            saveQueueState();
            emitDownloadStats();
            startNextDownload();
            return;
        }
        QByteArray jsonData = jsonFile.readAll();
        jsonFile.close();
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (!doc.isObject()) {
            qWarning() << "Invalid info.json";
            emit downloadFinished(id, false, "Downloaded file not found.");
            m_activeItems.remove(id);
            saveQueueState();
            emitDownloadStats();
            startNextDownload();
            return;
        }
        item.metadata = doc.object().toVariantMap();
    }

    QString finalName = QFileInfo(item.tempFilePath).fileName();
    if (finalName.isEmpty()) {
        qWarning() << "Could not resolve final media filename from yt-dlp after_move path output.";
        emit downloadFinished(id, false, "Download completed, but could not resolve output filename from yt-dlp.");
        m_activeItems.remove(id);
        saveQueueState();
        emitDownloadStats();
        startNextDownload();
        return;
    }

    qDebug() << "Resolved finalName:" << finalName;
    item.tempFilePath = QDir(m_configManager->get("Paths", "temporary_downloads_directory").toString()).filePath(finalName);

    QFileInfo fileInfo(item.tempFilePath);
    qint64 lastSize = -1;
    int stableCount = 0;
    int maxRetries = 20;

    emit downloadProgress(id, {{"status", "Verifying file..."}});

    if (fileInfo.isFile()) {
        for (int i = 0; i < maxRetries; ++i) {
            fileInfo.refresh();
            qint64 currentSize = fileInfo.size();
            if (currentSize == lastSize && currentSize > 0) {
                stableCount++;
            } else {
                stableCount = 0;
            }
            lastSize = currentSize;
            if (stableCount >= 3) break;
            QThread::msleep(100);
            QCoreApplication::processEvents();
        }
    }

    QString finalDir = m_sortingManager->getSortedDirectory(item.metadata, item.options);
    qDebug() << "finalDir from sorting:" << finalDir;
    QDir().mkpath(finalDir);
    finalDir = QDir(finalDir).absolutePath();
    qDebug() << "absolute finalDir:" << finalDir;

    if (item.options.value("type").toString() == "audio" && item.playlistIndex > 0) {
        QString paddedIndex = QString("%1").arg(item.playlistIndex, 2, 10, QChar('0'));
        finalName = QString("%1 - %2").arg(paddedIndex, finalName);
    }

    QString destPath = QDir(finalDir).filePath(finalName);

    qDebug() << "source:" << item.tempFilePath;
    qDebug() << "dest:" << destPath;

    if (item.options.value("type").toString() == "audio" && item.playlistIndex > 0) {
        QString thumbTempPath = QDir(m_configManager->get("Paths", "temporary_downloads_directory").toString()).filePath(id + "_folder.jpg");
        if (QFile::exists(thumbTempPath)) {
            QString thumbDestPath = QDir(finalDir).filePath("folder.jpg");
            if (!QFile::exists(thumbDestPath)) {
                QFile::copy(thumbTempPath, thumbDestPath);
                qDebug() << "Copied folder.jpg for audio playlist to:" << thumbDestPath;
            }
            QFile::remove(thumbTempPath);
        }
    }

    if (item.options.value("type").toString() == "gallery") {
        QDir tempDir(item.tempFilePath);
        QFileInfoList entries = tempDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
        if (entries.size() == 1) {
            // Move the single sub-directory to the final destination
            QFileInfo entry = entries.first();
            QString newDestPath = QDir(finalDir).filePath(entry.fileName());
            if (QDir().rename(entry.absoluteFilePath(), newDestPath)) {
                ArchiveManager archive(m_configManager);
                archive.addToArchive(item.url);
                emit downloadFinalPathReady(id, newDestPath);
                emit downloadFinished(id, true, "Gallery download completed and moved.");
                m_completedDownloadsCount++;
            } else {
                qWarning() << "Direct rename failed for gallery, attempting recursive copy+remove from" << entry.absoluteFilePath() << "to" << newDestPath;
                if (copyDirectoryRecursively(entry.absoluteFilePath(), newDestPath)) {
                    QDir(entry.absoluteFilePath()).removeRecursively();
                    ArchiveManager archive(m_configManager);
                    archive.addToArchive(item.url);
                    emit downloadFinalPathReady(id, newDestPath);
                    emit downloadFinished(id, true, "Gallery download completed and moved.");
                    m_completedDownloadsCount++;
                } else {
                    emit downloadFinished(id, false, "Gallery download completed, but failed to move directory across drives.");
                }
            }
        } else {
            // Fallback for unexpected directory structure
            if (QDir().rename(item.tempFilePath, destPath)) {
                ArchiveManager archive(m_configManager);
                archive.addToArchive(item.url);
                emit downloadFinalPathReady(id, destPath);
                emit downloadFinished(id, true, "Gallery download completed and moved.");
                m_completedDownloadsCount++;
            } else {
                qWarning() << "Direct rename failed for gallery fallback, attempting recursive copy+remove from" << item.tempFilePath << "to" << destPath;
                if (copyDirectoryRecursively(item.tempFilePath, destPath)) {
                    QDir(item.tempFilePath).removeRecursively();
                    ArchiveManager archive(m_configManager);
                    archive.addToArchive(item.url);
                    emit downloadFinalPathReady(id, destPath);
                    emit downloadFinished(id, true, "Gallery download completed and moved.");
                    m_completedDownloadsCount++;
                } else {
                    emit downloadFinished(id, false, "Gallery download completed, but failed to move file/directory.");
                }
            }
        }
    } else {
        if (QFile::exists(destPath) && !QFile::remove(destPath)) {
            qWarning() << "Failed to remove existing destination before move:" << destPath;
            emit downloadFinished(id, false, "Download completed, but failed to replace existing file.");
            m_activeItems.remove(id);
            saveQueueState();
            emitDownloadStats();
            startNextDownload();
            return;
        }

        bool moved = QFile::rename(item.tempFilePath, destPath);
        if (!moved) {
            qWarning() << "Direct rename failed, attempting copy+remove from" << item.tempFilePath << "to" << destPath;
            moved = QFile::copy(item.tempFilePath, destPath);
            if (moved) {
                if (!QFile::remove(item.tempFilePath)) {
                    qWarning() << "Copied output but failed to remove source temp file:" << item.tempFilePath;
                }
            }
        }

        if (moved) {
            qDebug() << "Move succeeded";
            ArchiveManager archive(m_configManager);
            archive.addToArchive(item.url);
            emit downloadFinalPathReady(id, destPath);
            emit downloadFinished(id, true, "Download completed and moved.");
            m_completedDownloadsCount++;

            if (!item.originalDownloadedFilePath.isEmpty() && item.originalDownloadedFilePath != item.tempFilePath) {
                QFile::remove(item.originalDownloadedFilePath);
                qDebug() << "Removed original downloaded file after transcoding:" << item.originalDownloadedFilePath;
            }
        } else {
            qWarning() << "Move failed. Source:" << item.tempFilePath << "Destination:" << destPath;
            emit downloadFinished(id, false, "Download completed, but failed to move file.");
        }
    }

    if (item.options.value("type").toString() != "gallery") {
        QFileInfo fi(item.tempFilePath);
        QString infoFilePath = fi.absoluteDir().filePath(fi.completeBaseName() + ".info.json");
        QFile::remove(infoFilePath);
    }

    m_activeItems.remove(id);
    saveQueueState();
    emitDownloadStats();
    startNextDownload();
    qDebug() << "FinalizeDownload completed";
}

void DownloadManager::checkQueueFinished() {
    if (m_activeWorkers.isEmpty() && m_downloadQueue.isEmpty() && m_activeItems.isEmpty()) {
        emit queueFinished();
    }
}

void DownloadManager::updateTotalSpeed() {
    double totalSpeed = 0.0;
    for (double speed : m_workerSpeeds.values()) {
        totalSpeed += speed;
    }
    emit totalSpeedUpdated(totalSpeed);
}

void DownloadManager::emitDownloadStats() {
    emit downloadStatsUpdated(m_queuedDownloadsCount, m_activeDownloadsCount, m_completedDownloadsCount);
}

void DownloadManager::onWorkerOutputReceived(const QString &id, const QString &output)
{
    qDebug().noquote() << output;
}
