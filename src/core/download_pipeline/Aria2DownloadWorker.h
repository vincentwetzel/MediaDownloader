#pragma once

#include <QObject>
#include <QTimer>
#include <QStringList>
#include "core/ConfigManager.h"
#include <QVariantMap>
#include "YtDlpDownloadInfoExtractor.h"
#include "Aria2RpcClient.h"
#include "FfmpegMuxer.h"

class Aria2DownloadWorker : public QObject {
    Q_OBJECT
public:
    enum class State { Idle, Extracting, Downloading, PostProcessing, Finished, Error };
    
    // Takes a pointer to the globally running aria2 RPC client
    explicit Aria2DownloadWorker(Aria2RpcClient* globalDaemon, QObject* parent = nullptr);
    ~Aria2DownloadWorker();

    // Begin the state machine, passing config objects to build correct args
    void start(const QString& ytDlpPath, const QString& ffmpegPath, const QString& url, const QString& saveDir, ConfigManager* configManager, const QVariantMap& options);
    
    // Abort the download
    void cancel();

signals:
    void statusTextChanged(const QString& status);
    void progressUpdated(int percentage, const QString& speed);
    void finished(const QString& finalFilePath);
    void error(const QString& errorMsg);

private slots:
    void onExtractionSuccess(const QString& title, const QString& thumbnailUrl, const QList<DownloadTarget>& targets, const QString& finalFilename, const QMap<QString, QString>& httpHeaders, const QVariantMap& metadata);
    void onExtractionFailed(const QString& errorMsg);
    void onDownloadProgress(const QString& gid, qint64 completedLength, qint64 totalLength, qint64 downloadSpeed, const QString& status);
    void onMergeSuccess(const QString& outputFile);
    void onMergeFailed(const QString& errorMsg);
    void pollAria2Status();

    void cleanupPartialFiles();

private:
    State m_state = State::Idle;
    
    Aria2RpcClient* m_daemon;
    YtDlpDownloadInfoExtractor* m_extractor;
    FfmpegMuxer* m_ffmpeg;
    QTimer* m_pollTimer;

    QString m_ffmpegPath;
    QString m_saveDir;
    QString m_finalFileName;
    QString m_title;
    QString m_thumbnailUrl;
    QString m_thumbnailPath;
    bool m_isCancelled = false;
    
    QStringList m_activeGids;
    QStringList m_allGids;
    QMap<QString, qint64> m_completedLengths;
    QMap<QString, qint64> m_totalLengths;
    QMap<QString, qint64> m_downloadSpeeds;
    QStringList m_downloadedParts;
    QList<SubtitleFile> m_downloadedSubtitles;
};
