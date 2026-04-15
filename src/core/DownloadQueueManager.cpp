#include "DownloadQueueManager.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QTimer>
#include <QDir>

DownloadQueueManager::DownloadQueueManager(ConfigManager *configManager, ArchiveManager *archiveManager, DownloadQueueState *queueState, QObject *parent)
    : QObject(parent), m_configManager(configManager), m_archiveManager(archiveManager), m_queueState(queueState) {
    connect(m_queueState, &DownloadQueueState::resumeDownloadsRequested, this, &DownloadQueueManager::processResumeDownloadsSelection);
    QTimer::singleShot(0, this, [this]() { m_queueState->load(); });
}

DownloadQueueManager::~DownloadQueueManager() {
    // Clear backup on exit for paused items
    if (!m_pausedItems.isEmpty()) {
        m_queueState->clear();
    }
}

DownloadQueueManager::DuplicateStatus DownloadQueueManager::getDuplicateStatus(const QString &url, const QMap<QString, DownloadItem> &activeItems) const {
    // Check in the pending queue
    for (const DownloadItem &item : m_downloadQueue) {
        if (item.url == url) {
            return DuplicateInQueue;
        }
    }
    
    // Check in active downloads (provided by DownloadManager)
    for (const DownloadItem &item : activeItems) {
        if (item.url == url) {
            return DuplicateActive;
        }
    }
    
    // Check in paused items
    for (const DownloadItem &item : m_pausedItems) {
        if (item.url == url) {
            return DuplicatePaused;
        }
    }
    
    // Check in archive (completed downloads)
    if (m_archiveManager && m_archiveManager->isInArchive(url)) {
        return DuplicateCompleted;
    }
    
    return NotDuplicate;
}

bool DownloadQueueManager::isUrlInQueue(const QString &url, const QMap<QString, DownloadItem> &activeItems) const {
    return getDuplicateStatus(url, activeItems) != NotDuplicate;
}

void DownloadQueueManager::enqueueDownload(const DownloadItem &item, bool isNew) {
    m_downloadQueue.enqueue(item);
    
    QVariantMap uiData;
    uiData["id"] = item.id;
    uiData["url"] = item.url;
    uiData["status"] = "Queued";
    uiData["progress"] = 0;
    uiData["options"] = item.options;
    
    if (isNew) {
        emit downloadAddedToQueue(uiData);
    } else {
        // If it's not a new item (e.g., updated after playlist expansion), update existing UI
        emit playlistExpansionPlaceholderUpdated(item.id, uiData);
    }

    emitQueueCountsChanged();
    QMetaObject::invokeMethod(this, [this]() { saveQueueState(QMap<QString, DownloadItem>()); }, Qt::QueuedConnection);
    emit requestStartNextDownload();
}

bool DownloadQueueManager::cancelQueuedOrPausedDownload(const QString &id) {
    // Check if this is a pending expansion
    if (m_pendingExpansions.contains(id)) {
        m_pendingExpansions.remove(id);
        emit downloadCancelled(id);
        return true;
    }

    for (int i = 0; i < m_downloadQueue.size(); ++i) {
        if (m_downloadQueue.at(i).id == id) {
            m_downloadQueue.removeAt(i);
            qDebug() << "Cancelled queued download:" << id;
            emit downloadCancelled(id);
            emitQueueCountsChanged();
            QMetaObject::invokeMethod(this, [this]() { saveQueueState(QMap<QString, DownloadItem>()); }, Qt::QueuedConnection);
            emit requestStartNextDownload();
            return true;
        }
    }

    if (m_pausedItems.contains(id)) {
        DownloadItem item = m_pausedItems.take(id);
        if (!item.tempFilePath.isEmpty()) {
            QFile::remove(item.tempFilePath);
            QFileInfo fi(item.tempFilePath);
            QString infoFilePath = fi.absoluteDir().filePath(fi.completeBaseName() + ".info.json");
            QFile::remove(infoFilePath);
            qDebug() << "Cleaned up temporary files for cancelled paused download:" << id;
        }
        emit downloadCancelled(id);
        emitQueueCountsChanged();
        QMetaObject::invokeMethod(this, [this]() { saveQueueState(QMap<QString, DownloadItem>()); }, Qt::QueuedConnection);
        emit requestStartNextDownload();
        return true;
    }
    return false;
}

bool DownloadQueueManager::pauseQueuedDownload(const QString &id, DownloadItem &pausedItem) {
    for (int i = 0; i < m_downloadQueue.size(); ++i) {
        if (m_downloadQueue.at(i).id == id) {
            pausedItem = m_downloadQueue.takeAt(i);
            m_pausedItems[id] = pausedItem;
            qDebug() << "Paused queued download:" << id;
            emit downloadPaused(id);
            emitQueueCountsChanged();
            QMetaObject::invokeMethod(this, [this]() { saveQueueState(QMap<QString, DownloadItem>()); }, Qt::QueuedConnection);
            emit requestStartNextDownload();
            return true;
        }
    }
    return false;
}

bool DownloadQueueManager::unpauseDownload(const QString &id) {
    if (m_pausedItems.contains(id)) {
        DownloadItem item = m_pausedItems.take(id);
        m_downloadQueue.prepend(item); // Insert at front to resume immediately
        qDebug() << "Unpaused download:" << id;
        emit downloadResumed(id);
        emitQueueCountsChanged();
        QMetaObject::invokeMethod(this, [this]() { saveQueueState(QMap<QString, DownloadItem>()); }, Qt::QueuedConnection);
        emit requestStartNextDownload();
        return true;
    }
    return false;
}

void DownloadQueueManager::moveDownloadUp(const QString &id) {
    for (int i = 1; i < m_downloadQueue.size(); ++i) { // Can't move 0 up
        if (m_downloadQueue.at(i).id == id) {
            m_downloadQueue.swapItemsAt(i, i - 1);
            QMetaObject::invokeMethod(this, [this]() { saveQueueState(QMap<QString, DownloadItem>()); }, Qt::QueuedConnection);
            break;
        }
    }
}

void DownloadQueueManager::moveDownloadDown(const QString &id) {
    for (int i = 0; i < m_downloadQueue.size() - 1; ++i) { // Can't move last down
        if (m_downloadQueue.at(i).id == id) {
            m_downloadQueue.swapItemsAt(i, i + 1);
            QMetaObject::invokeMethod(this, [this]() { saveQueueState(QMap<QString, DownloadItem>()); }, Qt::QueuedConnection);
            break;
        }
    }
}

void DownloadQueueManager::retryDownload(const QVariantMap &itemData) {
    DownloadItem item;
    item.id = itemData["id"].toString();
    item.url = itemData["url"].toString();
    item.options = itemData["options"].toMap();

    enqueueDownload(item); // Use enqueueDownload to add to queue and emit UI signals
}

void DownloadQueueManager::resumeDownload(const QVariantMap &itemData) {
    retryDownload(itemData);
}

void DownloadQueueManager::saveQueueState(const QMap<QString, DownloadItem> &activeItems) {
    m_queueState->save(activeItems.values(), m_pausedItems, m_downloadQueue);
}

void DownloadQueueManager::processResumeDownloadsSelection(const QJsonArray &arr) {
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
            emit downloadAddedToQueue(uiData);
        }
    }
    emitQueueCountsChanged();
    emit requestStartNextDownload();
}

DownloadItem DownloadQueueManager::takeNextQueuedDownload() {
    if (!m_downloadQueue.isEmpty()) {
        DownloadItem item = m_downloadQueue.dequeue();
        emitQueueCountsChanged();
        return item;
    }
    return DownloadItem(); // Return an invalid item if queue is empty
}

bool DownloadQueueManager::hasQueuedDownloads() const {
    return !m_downloadQueue.isEmpty();
}

void DownloadQueueManager::emitQueueCountsChanged() {
    emit queueCountsChanged(m_downloadQueue.size(), m_pausedItems.size());
}