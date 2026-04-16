#ifndef METADATAEMBEDDER_H
#define METADATAEMBEDDER_H

#include <QObject>
#include <QProcess>

class ConfigManager;

class MetadataEmbedder : public QObject {
    Q_OBJECT

public:
    explicit MetadataEmbedder(ConfigManager *configManager, QObject *parent = nullptr);
    void processFile(const QString &filePath, int trackNumber, bool normalizeContainerTimestamps);

signals:
    void finished(bool success, const QString &error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    enum class Stage {
        Idle,
        ProbingDuration,
        RewritingFile
    };

    void startDurationProbe();
    void startRewrite();

    QProcess *m_process;
    ConfigManager *m_configManager;
    QString m_tempFilePath;
    QString m_originalFilePath;
    int m_pendingTrackNumber;
    bool m_normalizeContainerTimestamps;
    double m_targetDurationSeconds;
    Stage m_stage;
};

#endif // METADATAEMBEDDER_H
