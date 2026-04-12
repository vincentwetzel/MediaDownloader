#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QQueue>
#include <QMap>
#include <QTimer>
#include "ConfigManager.h"

// Forward declarations
class SortingManager;
class ArchiveManager;
class PlaylistExpander;

struct DownloadItem {
    QString id;
    QString url;
    QVariantMap options;
    QString tempFilePath;
    QString originalDownloadedFilePath;
    QVariantMap metadata;
    int playlistIndex = -1;
};

class DownloadManager : public QObject {
    Q_OBJECT

public:
    explicit DownloadManager(ConfigManager *configManager, QObject *parent = nullptr);
    ~DownloadManager();

public slots:
    void enqueueDownload(const QString &url, const QVariantMap &options);
    void cancelDownload(const QString &id);
    void retryDownload(const QVariantMap &itemData);
    void resumeDownload(const QVariantMap &itemData);
    void pauseDownload(const QString &id);
    void unpauseDownload(const QString &id);
    void moveDownloadUp(const QString &id);
    void moveDownloadDown(const QString &id);
    void onWorkerOutputReceived(const QString &id, const QString &output);
    void resumeDownloadWithFormat(const QString &url, const QVariantMap &options, const QString &formatId);

signals:
    void downloadAddedToQueue(const QVariantMap &itemData);
    void downloadStarted(const QString &id);
    void downloadPaused(const QString &id);
    void downloadResumed(const QString &id);
    void downloadProgress(const QString &id, const QVariantMap &progressData);
    void downloadFinished(const QString &id, bool success, const QString &message);
    void downloadCancelled(const QString &id);
    void downloadFinalPathReady(const QString &id, const QString &path);
    void playlistExpansionStarted(const QString &url);
    void playlistExpansionFinished(const QString &url, int count);
    void queueFinished();
    void totalSpeedUpdated(double speed);
    void videoQualityWarning(const QString &url, const QString &message);
    void downloadStatsUpdated(int queued, int active, int completed);
    void formatSelectionRequested(const QString &url, const QVariantMap &options, const QVariantMap &infoDict);
    void formatSelectionFailed(const QString &url, const QString &message);

private slots:
    void onPlaylistDetected(const QString &url, int itemCount, const QVariantMap &options, const QList<QVariantMap> &expandedItems);
    void onPlaylistExpanded(const QString &originalUrl, const QList<QVariantMap> &expandedItems, const QString &error);
    void startNextDownload();
    void onSleepTimerTimeout();
    void onWorkerProgress(const QString &id, const QVariantMap &progressData);
    void onWorkerFinished(const QString &id, bool success, const QString &message, const QString &finalFilename, const QString &originalDownloadedFilename, const QVariantMap &metadata);
    void onGalleryDlWorkerFinished(const QString &id, bool success, const QString &message, const QString &finalFilename, const QVariantMap &metadata);
    void onMetadataEmbedded(const QString &id, bool success, const QString &error);

private:
    Q_INVOKABLE void saveQueueState();
    Q_INVOKABLE void loadQueueState();
    void proceedWithDownload();
    void finalizeDownload(const QString &id, DownloadItem &item, const QString &filePath);
    void checkQueueFinished();
    void updateTotalSpeed();
    void emitDownloadStats();
    void fetchFormatsForSelection(const QString &url, const QVariantMap &options);

    ConfigManager *m_configManager;
    SortingManager *m_sortingManager;
    ArchiveManager *m_archiveManager;
    QQueue<DownloadItem> m_downloadQueue;
    QMap<QString, DownloadItem> m_pausedItems;
    QMap<QString, QObject*> m_activeWorkers;
    QMap<QString, DownloadItem> m_activeItems;
    QMap<QString, QObject*> m_activeEmbedders;
    QMap<QString, double> m_workerSpeeds;
    QMap<QString, QString> m_pendingExpansions; // Maps queue ID to URL during playlist expansion

    int m_maxConcurrentDownloads;
    enum SleepMode { NoSleep, ShortSleep, LongSleep };
    SleepMode m_sleepMode;
    QTimer *m_sleepTimer;

    int m_queuedDownloadsCount;
    int m_activeDownloadsCount;
    int m_completedDownloadsCount;
};

#endif // DOWNLOADMANAGER_H
