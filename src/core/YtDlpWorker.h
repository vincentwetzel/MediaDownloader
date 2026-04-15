#ifndef YTDLPWORKER_H
#define YTDLPWORKER_H

#include <QObject>
#include <QProcess>
#include <QVariantMap>
#include <QTimer> // Include QTimer
#include <QStringList> // Include QStringList

class ConfigManager;

class YtDlpWorker : public QObject {
    Q_OBJECT

public:
    explicit YtDlpWorker(const QString &id, const QStringList &args, ConfigManager *configManager, QObject *parent = nullptr);
    QString getId() const { return m_id; }
    void start();
    void killProcess();

signals:
    void progressUpdated(const QString &id, const QVariantMap &progressData);
    void finished(const QString &id, bool success, const QString &message, const QString &finalFilename, const QString &originalDownloadedFilename, const QVariantMap &videoMetadata);
    void outputReceived(const QString &id, const QString &output);
    void ytDlpErrorDetected(const QString &id, const QString &errorType, const QString &userMessage, const QString &rawError);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void readInfoJsonWithRetry(); // New slot for retry mechanism

private:
    void parseStandardOutput(const QByteArray &output);
    void parseStandardError(const QByteArray &output);
    void handleOutputLine(const QString &line);
    bool parseYtDlpProgressLine(const QString &line);
    bool parseAria2ProgressLine(const QString &line);
    double parseSizeStringToBytes(const QString &sizeString);
    QString formatBytes(double bytes);
    QString normalizeConsoleLine(const QString &line) const;

    QString m_id;
    QStringList m_args;
    ConfigManager *m_configManager;
    QProcess *m_process;
    QString m_finalFilename;
    QString m_originalDownloadedFilename;
    QString m_videoTitle; // Added to store the video title
    QString m_thumbnailPath;
    bool m_finishEmitted;
    QByteArray m_outputBuffer;
    QByteArray m_errorBuffer; // New member for stderr buffering
    QStringList m_allOutputLines; // Stores all output lines for post-processing

    QString m_infoJsonPath; // Path to info.json file that needs to be read (Corrected from m_pendingInfoJsonPath)
    int m_infoJsonRetryCount;      // Retry counter for reading info.json
    QVariantMap m_fullMetadata;    // Stores the full metadata parsed from info.json
    bool m_errorEmitted;           // Tracks if a specific error has been emitted
    QStringList m_errorLines;      // Stores ERROR: lines from stderr
};

#endif // YTDLPWORKER_H
