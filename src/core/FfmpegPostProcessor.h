#pragma once

#include <QObject>
#include <QByteArray>
#include <QProcess>
#include <QVariantMap>

class ConfigManager;

class FfmpegPostProcessor : public QObject
{
    Q_OBJECT
public:
    explicit FfmpegPostProcessor(ConfigManager *configManager, QObject *parent = nullptr);
    void embedTrackNumber(const QString &filePath, int trackNumber, int totalTracks);

signals:
    void finished();
    void error(const QString &errorString);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError processError);

private:
    void appendProcessOutput(const QByteArray &data);

    ConfigManager *m_configManager;
    QProcess *m_process;
    QString m_originalFile;
    QString m_tempFile;
    QString m_processOutputTail;
};
