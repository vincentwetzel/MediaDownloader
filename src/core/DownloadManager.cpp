#include "DownloadManager.h"
#include "GalleryDlArgsBuilder.h"
#include "YtDlpArgsBuilder.h"
#include "DownloadQueueManager.h" // Include the new queue manager
#include "DownloadQueueState.h"
#include "ArchiveManager.h"
#include "SortingManager.h"
#include "GalleryDlWorker.h"
#include "YtDlpWorker.h"
#include "PlaylistExpander.h"
#include "DownloadFinalizer.h"
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
#include <QJsonArray>
#include <QThread>
#include <QCoreApplication>

namespace {
bool shouldNormalizeSectionContainer(const DownloadItem &item)
{
    if (item.options.value("download_sections").toString().isEmpty()) {
        return false;
    }

    const QString suffix = QFileInfo(item.tempFilePath).suffix().toLower();
    return suffix == "mp4" || suffix == "m4v" || suffix == "mov" || suffix == "m4a";
}

bool isMetadataSidecarPath(const QString &path)
{
    return path.endsWith(".info.json", Qt::CaseInsensitive);
}

void appendCleanupCandidate(QVariantMap &options, const QString &path)
{
    const QString normalizedPath = QDir::fromNativeSeparators(path.trimmed());
    if (normalizedPath.isEmpty()) {
        return;
    }

    QStringList cleanupCandidates = options.value("cleanup_candidates").toStringList();
    if (!cleanupCandidates.contains(normalizedPath, Qt::CaseInsensitive)) {
        cleanupCandidates.append(normalizedPath);
        options["cleanup_candidates"] = cleanupCandidates;
    }
}
}

DownloadManager::DownloadManager(ConfigManager *configManager, QObject *parent) : QObject(parent),
    m_configManager(configManager), m_archiveManager(nullptr), m_sleepMode(NoSleep),
    m_queuedDownloadsCount(0), m_activeDownloadsCount(0), m_completedDownloadsCount(0), m_errorDownloadsCount(0),
    m_isShuttingDown(false)
{

    m_queueState = new DownloadQueueState(this);
    m_sortingManager = new SortingManager(m_configManager, this);
    m_archiveManager = new ArchiveManager(m_configManager, this);

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

    m_finalizer = new DownloadFinalizer(m_configManager, m_sortingManager, m_archiveManager, this);
    connect(m_finalizer, &DownloadFinalizer::progressUpdated, this, [this](const QString &id, const QVariantMap &data) {
        emit downloadProgress(id, data);
    });
    connect(m_finalizer, &DownloadFinalizer::finalPathReady, this, &DownloadManager::downloadFinalPathReady);
    connect(m_finalizer, &DownloadFinalizer::finalizationComplete, this, &DownloadManager::onFinalizationComplete);

    m_queueManager = new DownloadQueueManager(m_configManager, m_archiveManager, m_queueState, this); // m_queueState is passed to queueManager
    connect(m_queueManager, &DownloadQueueManager::downloadAddedToQueue, this, &DownloadManager::downloadAddedToQueue);
    connect(m_queueManager, &DownloadQueueManager::downloadCancelled, this, &DownloadManager::downloadCancelled);
    connect(m_queueManager, &DownloadQueueManager::downloadPaused, this, &DownloadManager::downloadPaused);
    connect(m_queueManager, &DownloadQueueManager::downloadResumed, this, &DownloadManager::downloadResumed);
    connect(m_queueManager, &DownloadQueueManager::duplicateDownloadDetected, this, &DownloadManager::duplicateDownloadDetected);
    connect(m_queueManager, &DownloadQueueManager::requestStartNextDownload, this, &DownloadManager::onRequestStartNextDownload);
    connect(m_queueManager, &DownloadQueueManager::queueCountsChanged, this, &DownloadManager::onQueueCountsChanged);
    connect(m_queueManager, &DownloadQueueManager::playlistExpansionPlaceholderRemoved, this, &DownloadManager::onPlaylistExpansionPlaceholderRemoved);
    connect(m_queueManager, &DownloadQueueManager::playlistExpansionPlaceholderUpdated, this, &DownloadManager::onPlaylistExpansionPlaceholderUpdated);
    QTimer::singleShot(0, this, [this]() { m_queueState->load(); }); // Load queue state after connections are established

    emitDownloadStats();
}

DownloadManager::~DownloadManager() {
    shutdown();
}

void DownloadManager::shutdown() {
    if (m_isShuttingDown) {
        return;
    }
    m_isShuttingDown = true;

    qInfo() << "[DownloadManager] Shutdown requested. Terminating active downloads and helper processes.";

    if (m_queueManager) {
        m_queueManager->saveQueueState(m_activeItems);
    }

    const QList<QProcess*> descendantProcesses = findChildren<QProcess*>();
    for (QProcess *process : descendantProcesses) {
        process->disconnect(); // Prevent reading buffers from dying process
        ProcessUtils::terminateProcessTree(process);
    }

    const QStringList activeIds = m_activeWorkers.keys();
    for (const QString &id : activeIds) {
        QObject *worker = m_activeWorkers.take(id);
        if (!worker) {
            continue;
        }
        worker->disconnect(this);
        delete worker;
    }
    m_activeWorkers.clear();

    const QStringList embedderIds = m_activeEmbedders.keys();
    for (const QString &id : embedderIds) {
        QObject *embedder = m_activeEmbedders.take(id);
        if (!embedder) {
            continue;
        }
        embedder->disconnect(this);
        delete embedder;
    }
    m_activeEmbedders.clear();

    m_workerSpeeds.clear();
}

void DownloadManager::onQueueCountsChanged(int queued, int paused) {
    m_queuedDownloadsCount = queued;
    // m_pausedDownloadsCount is not directly stored in DownloadManager, but can be derived if needed.
    emitDownloadStats();
}

void DownloadManager::onRequestStartNextDownload() {
    startNextDownload();
}

void DownloadManager::enqueueDownload(const QString &url, const QVariantMap &options) {
    // Check if URL is already in any state (prevents duplicate enqueuing)
    bool overrideArchive = options.value("override_archive", false).toBool();
    DownloadQueueManager::DuplicateStatus status = m_queueManager->getDuplicateStatus(url, m_activeItems);
    
    if (status != DownloadQueueManager::NotDuplicate) {
        // If it's only in completed and override is enabled, allow it
        if (status == DownloadQueueManager::DuplicateCompleted && overrideArchive) {
            qDebug() << "DownloadManager: Allowing re-download of completed URL (override enabled):" << url;
        } else {
            QString reason;
            switch (status) {
                case DownloadQueueManager::DuplicateInQueue:
                    reason = "This URL is already waiting in the download queue.";
                    break;
                case DownloadQueueManager::DuplicateActive:
                    reason = "This URL is currently being downloaded.";
                    break;
                case DownloadQueueManager::DuplicatePaused:
                    reason = "This download is paused.";
                    break;
                case DownloadQueueManager::DuplicateCompleted:
                    reason = "This URL has already been downloaded (use 'Override duplicate check' to re-download).";
                    break;
                default:
                    reason = "This URL is already in the system.";
                    break;
            }
            qDebug() << "DownloadManager: Skipping duplicate URL:" << url << "- Reason:" << reason;
            emit duplicateDownloadDetected(url, reason);
            return;
        }
    }

    // Intercept for download sections before anything else
    bool useSections = m_configManager->get("DownloadOptions", "download_sections_enabled", false).toBool();
    QString downloadTypeCheck = options.value("type", "video").toString();
    // The "download_sections_set" flag prevents an infinite loop.
    if (useSections && !options.contains("download_sections_set") && (downloadTypeCheck == "video" || downloadTypeCheck == "audio")) {
        qDebug() << "Download sections enabled, fetching metadata for" << url;
        fetchInfoForSections(url, options);
        return;
    }

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
        item.playlistIndex = -1; // Not part of a playlist initially

        m_queueManager->enqueueDownload(item);
    } else {
        // IMMEDIATE UI FEEDBACK: Create the download item in UI before playlist expansion
        DownloadItem item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.url = url;
        item.options = options;
        item.playlistIndex = -1;

        QVariantMap uiData;
        uiData["id"] = item.id;
        uiData["url"] = url;
        uiData["status"] = "Checking for playlist...";
        uiData["options"] = options;
        emit downloadAddedToQueue(uiData);

        PlaylistExpander *expander = new PlaylistExpander(url, m_configManager, this);
        expander->setProperty("options", options);
        expander->setProperty("queueId", item.id); // Store queue ID for later

        // Add to queue manager and pending expansions
        m_queueManager->enqueueDownload(item, false); // Enqueue as placeholder, not a "new" item for UI
        m_queueManager->m_pendingExpansions[item.id] = url;
        connect(expander, &PlaylistExpander::expansionFinished, this, &DownloadManager::onPlaylistExpanded);
        connect(expander, &PlaylistExpander::playlistDetected, this, &DownloadManager::onPlaylistDetected);

        QString playlistLogic = options.value("playlist_logic", "Ask").toString();
        expander->startExpansion(playlistLogic);
        emit playlistExpansionStarted(url);
    }
}

void DownloadManager::fetchInfoForSections(const QString &url, const QVariantMap &options)
{
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
                QVariantMap infoJson = doc.object().toVariantMap();
                QMetaObject::invokeMethod(this, [this, url, options, infoJson]() {
                    emit downloadSectionsRequested(url, options, infoJson);
                }, Qt::QueuedConnection);
            } else {
                qWarning() << "Failed to parse JSON for sections, enqueuing without them.";
                QVariantMap newOptions = options;
                newOptions["download_sections_set"] = true; // Prevent re-triggering
                enqueueDownload(url, newOptions);
            }
        } else {
            qWarning() << "Failed to fetch info for sections, enqueuing without them. Error:" << process->readAllStandardError();
            QVariantMap newOptions = options;
            newOptions["download_sections_set"] = true; // Prevent re-triggering
            enqueueDownload(url, newOptions);
        }
        process->deleteLater();
    });

    process->start(ytDlpPath, args);
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
                QVariantMap metadata = doc.object().toVariantMap();
                QVariantMap newOptions = options;
                if (metadata.value("is_live", false).toBool()) {
                    newOptions["is_live"] = true;
                }
                QMetaObject::invokeMethod(this, [this, url, newOptions, metadata]() {
                    emit formatSelectionRequested(url, newOptions, metadata);
                }, Qt::QueuedConnection);
            } else {
                const QString message = "yt-dlp returned invalid format metadata.";
                qWarning() << "DownloadManager: Invalid JSON returned from yt-dlp -J";
                QMetaObject::invokeMethod(this, [this, url, message]() {
                    emit formatSelectionFailed(url, message);
                }, Qt::QueuedConnection);
            }
        } else {
            const QString errorText = QString::fromUtf8(process->readAllStandardError()).trimmed();
            qWarning() << "DownloadManager: yt-dlp -J failed:" << errorText;
            QString message = errorText.isEmpty() ? "Failed to retrieve available formats." : errorText;
            QMetaObject::invokeMethod(this, [this, url, message]() {
                emit formatSelectionFailed(url, message);
            }, Qt::QueuedConnection);
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
    
    // Delegate to queue manager first for queued/paused items
    if (m_queueManager->cancelQueuedOrPausedDownload(id)) {
        cancelled = true;
    }

    if (m_queueManager->m_pendingExpansions.contains(id)) {
        m_queueManager->m_pendingExpansions.remove(id);
        
        // Find and terminate the background PlaylistExpander process
        const QList<PlaylistExpander*> expanders = findChildren<PlaylistExpander*>();
        for (PlaylistExpander *expander : expanders) {
            if (expander->property("queueId").toString() == id) {
                expander->disconnect(this);
                const QList<QProcess*> processes = expander->findChildren<QProcess*>();
                for (QProcess *p : processes) {
                    if (p->state() != QProcess::NotRunning) {
                        p->disconnect(); // Prevent reading buffers from dying process
                        ProcessUtils::terminateProcessTree(p);
                        p->kill();
                    }
                }
                expander->deleteLater();
                break;
            }
        }
        
        if (!cancelled) {
            emit downloadCancelled(id); 
            cancelled = true;
        }
    }

    // Always check active workers to ensure no ghost processes remain
    if (m_activeWorkers.contains(id)) {
        QObject *worker = m_activeWorkers.take(id);
        DownloadItem item = m_activeItems.take(id);

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

        worker->disconnect(this);
        
        // Ensure all child processes belonging to this worker are forcefully killed
        const QList<QProcess*> processes = worker->findChildren<QProcess*>();
        for (QProcess *p : processes) {
            if (p->state() != QProcess::NotRunning) {
                p->disconnect();
                ProcessUtils::terminateProcessTree(p);
                p->kill();
            }
        }

        worker->deleteLater();
        m_activeDownloadsCount--;
        
        item.options["is_stopped"] = true;
        m_queueManager->m_pausedItems[id] = item;
        
        if (!cancelled) {
            emit downloadCancelled(id);
            cancelled = true;
        }
    } 
    
    if (m_activeEmbedders.contains(id)) {
        // Cancel a download that is currently in the post-processing metadata phase
        QObject *embedder = m_activeEmbedders.take(id);
        DownloadItem item = m_activeItems.take(id);
        
        embedder->disconnect(this);
        
        const QList<QProcess*> processes = embedder->findChildren<QProcess*>();
        for (QProcess *p : processes) {
            if (p->state() != QProcess::NotRunning) {
                p->disconnect();
                ProcessUtils::terminateProcessTree(p);
                p->kill();
            }
        }

        // Deleting the embedder will kill any active QProcess internally
        embedder->deleteLater();
        
        item.options["is_stopped"] = true;
        m_queueManager->m_pausedItems[id] = item;
        
        m_activeDownloadsCount--;
        if (!cancelled) {
            emit downloadCancelled(id);
            cancelled = true;
        }
    }

    if (cancelled) {
        emitDownloadStats();
        m_queueManager->saveQueueState(m_activeItems);
        startNextDownload();
    }
}

void DownloadManager::retryDownload(const QVariantMap &itemData) {
    m_queueManager->retryDownload(itemData);
}

void DownloadManager::restartDownloadWithOptions(const QVariantMap &itemData) {
    QString id = itemData.value("id").toString();

    if (!m_activeItems.contains(id)) {
        // Fallback for non-active items, just treat as a normal retry
        qWarning() << "restartDownloadWithOptions called for non-active ID:" << id << ". Falling back to retry.";
        retryDownload(itemData);
        return;
    }

    qDebug() << "Restarting active download with new options:" << id;

    // 1. Get the active worker and kill it.
    if (m_activeWorkers.contains(id)) {
        QObject *worker = m_activeWorkers.take(id);
        // Disconnect signals to prevent onWorkerFinished from being called with an error
        worker->disconnect(this);
        
        YtDlpWorker *ytDlpWorker = qobject_cast<YtDlpWorker*>(worker);
        if (ytDlpWorker) {
            ytDlpWorker->killProcess();
        }
        
        const QList<QProcess*> processes = worker->findChildren<QProcess*>();
        for (QProcess *p : processes) {
            if (p->state() != QProcess::NotRunning) {
                p->disconnect();
                ProcessUtils::terminateProcessTree(p);
                p->kill();
            }
        }

        worker->deleteLater();
    }

    // 2. The item is still in m_activeItems. We will reuse it.
    DownloadItem &item = m_activeItems[id];
    item.options = itemData.value("options").toMap(); // Update options

    // 3. Tell the UI to reset its state for the existing item.
    QVariantMap resetData;
    resetData["id"] = id;
    resetData["status"] = "Waiting for video...";
    resetData["progress"] = -1; // Indeterminate progress
    emit downloadProgress(id, resetData);

    // 4. Create and start a new worker with the same ID and new options.
    YtDlpArgsBuilder argsBuilder;
    QStringList args = argsBuilder.build(m_configManager, item.url, item.options);
    YtDlpWorker *newWorker = new YtDlpWorker(item.id, args, m_configManager, this);
    m_activeWorkers[item.id] = newWorker;

    connect(newWorker, &YtDlpWorker::progressUpdated, this, &DownloadManager::onWorkerProgress);
    connect(newWorker, &YtDlpWorker::finished, this, &DownloadManager::onWorkerFinished);
    connect(newWorker, &YtDlpWorker::outputReceived, this, &DownloadManager::onWorkerOutputReceived);
    connect(newWorker, &YtDlpWorker::ytDlpErrorDetected, this, &DownloadManager::onYtDlpErrorDetected);

    newWorker->start();
}

void DownloadManager::resumeDownload(const QVariantMap &itemData) {
    retryDownload(itemData);
}

void DownloadManager::pauseDownload(const QString &id) {
    bool paused = false;
    
    DownloadItem pausedItem; // To capture the item if it's from the queue

    if (!paused && m_activeWorkers.contains(id)) {
        QObject *worker = m_activeWorkers.take(id);
        m_queueManager->m_pausedItems[id] = m_activeItems.take(id); // Add to queue manager's paused items
        
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
        
        worker->disconnect(this);
        
        const QList<QProcess*> processes = worker->findChildren<QProcess*>();
        for (QProcess *p : processes) {
            if (p->state() != QProcess::NotRunning) {
                p->disconnect();
                ProcessUtils::terminateProcessTree(p);
                p->kill();
            }
        }

        worker->deleteLater();
        m_activeDownloadsCount--;
        qDebug() << "Paused active download:" << id;
        emit downloadPaused(id);
        paused = true; // Corrected: paused = true
    } else if (!paused && m_activeEmbedders.contains(id)) {
        qWarning() << "Cannot pause a download that is currently embedding metadata:" << id;
        emit downloadResumed(id); // Revert UI
        paused = true;
    }

    if (paused) {
        emitDownloadStats();
        m_queueManager->saveQueueState(m_activeItems);
        startNextDownload();
    }
}

void DownloadManager::unpauseDownload(const QString &id) {
    m_queueManager->unpauseDownload(id);
}

void DownloadManager::moveDownloadUp(const QString &id) {
    m_queueManager->moveDownloadUp(id);
}

void DownloadManager::moveDownloadDown(const QString &id) {
    m_queueManager->moveDownloadDown(id);
}

void DownloadManager::onPlaylistDetected(const QString &url, int itemCount, const QVariantMap &options, const QList<QVariantMap> &expandedItems) {
    PlaylistExpander *expander = qobject_cast<PlaylistExpander*>(sender());
    QVariantMap storedOptions = options;
    if (expander) {
        storedOptions = expander->property("options").toMap();
        expander->deleteLater();
    }

    // Delegate UI presentation to the View layer
    QMetaObject::invokeMethod(this, [this, url, itemCount, storedOptions, expandedItems]() {
        emit playlistActionRequested(url, itemCount, storedOptions, expandedItems);
    }, Qt::QueuedConnection);
}

void DownloadManager::processPlaylistSelection(const QString &url, const QString &action, const QVariantMap &options, const QList<QVariantMap> &expandedItems) {
    QList<QVariantMap> finalItems;

    if (action == "Download All") {
        finalItems = expandedItems;
    } else if (action == "Download Single Item" && !expandedItems.isEmpty()) {
        finalItems.append(expandedItems.first());
    } else {
        emit downloadFinished(QUuid::createUuid().toString(QUuid::WithoutBraces), false, "Playlist download cancelled by user.");
        return;
    }

    emit playlistExpansionFinished(url, finalItems.count());

    for (const QVariantMap &itemData : finalItems) {
        DownloadItem item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.url = itemData["url"].toString();
        QVariantMap itemOptions = options; // Use the 'options' parameter from the function
        if (itemData.contains("is_live")) { // Use 'options' from parameter
            itemOptions["is_live"] = itemData.value("is_live").toBool();
        }
        item.options = itemOptions;
        item.playlistIndex = itemData.value("playlist_index", -1).toInt();
        m_queueManager->enqueueDownload(item);
    }
}

void DownloadManager::onPlaylistExpanded(const QString &originalUrl, const QList<QVariantMap> &expandedItems, const QString &error) {
    PlaylistExpander *expander = qobject_cast<PlaylistExpander*>(sender());
    QVariantMap options;
    QString queueId;
    if (expander) {
        options = expander->property("options").toMap();
        queueId = expander->property("queueId").toString();
        expander->deleteLater();
    }

    if (!error.isEmpty()) {
        qDebug() << "Playlist expansion failed:" << error;
        // Update the UI item to show error
        if (!queueId.isEmpty()) {
            emit downloadProgress(queueId, {{"status", "Failed to check playlist"}});
            emit downloadFinished(queueId, false, "Playlist expansion failed: " + error);
            m_queueManager->m_pendingExpansions.remove(queueId);
            m_queueManager->cancelQueuedOrPausedDownload(queueId); // Remove placeholder from queue
        }
        return;
    }

    emit playlistExpansionFinished(originalUrl, expandedItems.count());

    // If this was a single video (no expansion needed), update the existing UI item
    if (expandedItems.size() == 1 && !queueId.isEmpty()) {
        QVariantMap itemData = expandedItems.first();

        // Find the placeholder item in the queue and update it in-place.
        // This avoids re-enqueueing and causing a duplicate download.
        bool found = false;
        for (DownloadItem &item : m_queueManager->m_downloadQueue) { // Assumes m_downloadQueue is accessible
            if (item.id == queueId) {
                item.url = itemData.value("url").toString();
                item.playlistIndex = itemData.value("playlist_index", -1).toInt();
                item.options = options;
                item.options["original_playlist_url"] = originalUrl;
                if (itemData.contains("is_live")) {
                    item.options["is_live"] = itemData.value("is_live").toBool();
                }
                found = true;
                break;
            }
        }

        if (found) {
            m_queueManager->m_pendingExpansions.remove(queueId); // Assumes m_pendingExpansions is accessible
            emit downloadProgress(queueId, {{"status", "Queued"}, {"progress", 0}});
            // Manually save the queue state since we modified an item in-place
            QMetaObject::invokeMethod(m_queueManager, [this]() { m_queueManager->saveQueueState(m_activeItems); }, Qt::QueuedConnection);
        }
    } else if (expandedItems.size() > 1) {
        // This is an actual playlist - remove the placeholder from queue
        if (!queueId.isEmpty()) {
            m_queueManager->cancelQueuedOrPausedDownload(queueId); // Remove placeholder from queue
            m_queueManager->m_pendingExpansions.remove(queueId);
        }
        
        for (const QVariantMap &itemData : expandedItems) {
            DownloadItem item;
            item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            item.url = itemData["url"].toString();
            item.options = options; // Use the options from the original enqueue
            item.options["original_playlist_url"] = originalUrl;
            if (itemData.contains("is_live")) item.options["is_live"] = itemData.value("is_live").toBool();
            item.playlistIndex = itemData.value("playlist_index", -1).toInt();
            m_queueManager->enqueueDownload(item);
        }
    }
    // No need to call emitDownloadStats() or startNextDownload() here,
    // as enqueueDownload() already triggers these via signals.
}

void DownloadManager::onPlaylistExpansionPlaceholderRemoved(const QString &id) {
    // Handle UI update if necessary, e.g., remove the placeholder item from the UI
    // This signal is emitted by DownloadQueueManager when a placeholder is removed
}

void DownloadManager::onPlaylistExpansionPlaceholderUpdated(const QString &id, const QVariantMap &itemData) {
    // Handle UI update if necessary, e.g., update the placeholder item's status to "Queued"
    emit downloadProgress(id, itemData);
}

void DownloadManager::proceedWithDownload() {
    if (!m_queueManager->hasQueuedDownloads()) {
        checkQueueFinished();
        return;
    }

    DownloadItem item = m_queueManager->takeNextQueuedDownload();
    m_activeDownloadsCount++;

    QString downloadType = item.options.value("type", "video").toString();

    if (downloadType == "gallery") {
        item.options["id"] = item.id;
        item.options["playlist_index"] = item.playlistIndex;
        GalleryDlArgsBuilder argsBuilder(m_configManager);
        QStringList args = argsBuilder.build(item.url, item.options);

        GalleryDlWorker *worker = new GalleryDlWorker(item.id, args, m_configManager, this);
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

        YtDlpWorker *worker = new YtDlpWorker(item.id, args, m_configManager, this);
        m_activeWorkers[item.id] = worker;
        m_activeItems[item.id] = item;

        connect(worker, &YtDlpWorker::progressUpdated, this, &DownloadManager::onWorkerProgress);
        connect(worker, &YtDlpWorker::finished, this, &DownloadManager::onWorkerFinished);
        connect(worker, &YtDlpWorker::outputReceived, this, &DownloadManager::onWorkerOutputReceived);
        connect(worker, &YtDlpWorker::ytDlpErrorDetected, this, &DownloadManager::onYtDlpErrorDetected);

        emit downloadStarted(item.id);
        worker->start();
    }
    emitDownloadStats();
}

void DownloadManager::startNextDownload() {
    if (m_activeWorkers.count() >= m_maxConcurrentDownloads || !m_queueManager->hasQueuedDownloads()) {
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
    if (m_activeItems.contains(id)) {
        DownloadItem &item = m_activeItems[id];
        const QString currentFile = progressData.value("current_file").toString().trimmed();
        if (!currentFile.isEmpty()) {
            const QString normalizedCurrentFile = QDir::fromNativeSeparators(currentFile);
            item.tempFilePath = normalizedCurrentFile;
            appendCleanupCandidate(item.options, normalizedCurrentFile);
            if (!isMetadataSidecarPath(normalizedCurrentFile)) {
                item.originalDownloadedFilePath = normalizedCurrentFile;
            }
        }

        const QString thumbnailPath = progressData.value("thumbnail_path").toString().trimmed();
        if (!thumbnailPath.isEmpty()) {
            appendCleanupCandidate(item.options, thumbnailPath);
        }
    }

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
        DownloadItem item = m_activeItems.take(id);
        item.options["is_failed"] = true;
        m_queueManager->m_pausedItems[id] = item;
        
        m_errorDownloadsCount++;
        emit downloadFinished(id, false, message); // This will trigger emitDownloadStats()
        m_queueManager->saveQueueState(m_activeItems);
        emitDownloadStats();
        startNextDownload();
        return;
    }

    if (metadata.contains("height") && metadata["height"].toInt() < 480) {
        QString url = item.url;
        QMetaObject::invokeMethod(this, [this, url]() {
            emit videoQualityWarning(url, "Downloaded video quality is below 480p.");
        }, Qt::QueuedConnection);
    }

    QString normalizedFinal = QDir::fromNativeSeparators(finalFilename);
    QString normalizedOriginal = QDir::fromNativeSeparators(originalDownloadedFilename);

    item.tempFilePath = normalizedFinal.isEmpty() ? normalizedOriginal : normalizedFinal;
    item.originalDownloadedFilePath = normalizedOriginal;
    item.metadata = metadata;

    // Inject playlist_index into metadata for sorting manager
    if (item.playlistIndex != -1) {
        item.metadata["playlist_index"] = item.playlistIndex;
        qDebug() << "Injected playlist_index" << item.playlistIndex << "into metadata for sorting.";
    }

    const bool needsTrackEmbedding = (item.options.value("type").toString() == "audio" && item.playlistIndex > 0);
    const bool needsSectionNormalization = shouldNormalizeSectionContainer(item);

    if (needsTrackEmbedding || needsSectionNormalization) {
        QVariantMap progressData;
        progressData["status"] = needsSectionNormalization
            ? "Normalizing clip container metadata..."
            : "Embedding metadata...";
        emit downloadProgress(id, progressData);

        MetadataEmbedder *embedder = new MetadataEmbedder(m_configManager, this);
        m_activeEmbedders[id] = embedder;
        connect(embedder, &MetadataEmbedder::finished, this, [this, id](bool s, const QString &e){
            onMetadataEmbedded(id, s, e);
        });
        embedder->processFile(item.tempFilePath, needsTrackEmbedding ? item.playlistIndex : 0, needsSectionNormalization);
    } else {
        m_finalizer->finalize(id, item);
    }
    emitDownloadStats(); // Update stats after worker finishes, before finalizer starts
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
        DownloadItem item = m_activeItems.take(id);
        item.options["is_failed"] = true;
        m_queueManager->m_pausedItems[id] = item;
        
        m_errorDownloadsCount++;
        emit downloadFinished(id, false, message); // This will trigger emitDownloadStats()
        m_queueManager->saveQueueState(m_activeItems);
        emitDownloadStats();
        startNextDownload();
        return;
    }

    item.tempFilePath = finalFilename;
    item.originalDownloadedFilePath = "";
    item.metadata = metadata;

    m_finalizer->finalize(id, item);
    emitDownloadStats();
}

void DownloadManager::onYtDlpErrorDetected(const QString &id, const QString &errorType, const QString &userMessage, const QString &rawError) {
    if (!m_activeItems.contains(id)) {
        qWarning() << "onYtDlpErrorDetected called for inactive/unknown ID:" << id;
        return;
    }

    // Pass the item data to the UI to allow for actions like retrying or opening the URL.
    QVariantMap itemData;
    const DownloadItem& item = m_activeItems.value(id);
    itemData["id"] = item.id;
    itemData["url"] = item.url;
    itemData["options"] = item.options;
    itemData["playlistIndex"] = item.playlistIndex;

    // Forward to UI for popup display
    QMetaObject::invokeMethod(this, [this, id, errorType, userMessage, rawError, itemData]() {
        emit ytDlpErrorPopupRequested(id, errorType, userMessage, rawError, itemData);
    }, Qt::QueuedConnection);
}

void DownloadManager::onMetadataEmbedded(const QString &id, bool success, const QString &error) {
    if (!m_activeEmbedders.contains(id)) return;

    MetadataEmbedder *embedder = qobject_cast<MetadataEmbedder*>(m_activeEmbedders.take(id));
    embedder->deleteLater();

    DownloadItem &item = m_activeItems[id];

    if (success) {
        m_finalizer->finalize(id, item);
    } else {
        DownloadItem item = m_activeItems.take(id);
        item.options["is_failed"] = true;
        m_queueManager->m_pausedItems[id] = item;
        
        m_errorDownloadsCount++;
        emit downloadFinished(id, false, "Metadata embedding failed: " + error); // This will trigger emitDownloadStats()
        m_queueManager->saveQueueState(m_activeItems);
        emitDownloadStats();
        startNextDownload();
    }
}

void DownloadManager::onFinalizationComplete(const QString &id, bool success, const QString &message) {
    if (success) {
        m_completedDownloadsCount++;
    } else {
        m_errorDownloadsCount++;
    }
    
    emit downloadFinished(id, success, message);
    m_activeItems.remove(id);

    m_queueManager->saveQueueState(m_activeItems);
    emitDownloadStats();
    startNextDownload();
}

/*
FIXME: The implementation for onItemCleared was causing build errors because it is not declared in DownloadManager.h.
To fix this, declare it as a public slot in DownloadManager.h:
    public slots:
        void onItemCleared(const QString &id, bool wasSuccessful, bool wasFinished);
Then, uncomment the function body below and the corresponding connect() call in MainWindow.cpp.
*/

void DownloadManager::checkQueueFinished() {
    if (m_activeWorkers.isEmpty() && !m_queueManager->hasQueuedDownloads() && m_activeItems.isEmpty()) {
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
    emit downloadStatsUpdated(m_queuedDownloadsCount, m_activeDownloadsCount, m_completedDownloadsCount, m_errorDownloadsCount);
}

void DownloadManager::onWorkerOutputReceived(const QString &id, const QString &output)
{
    qDebug().noquote() << output;
}
