#pragma once

#include <QObject>
#include <QProcess>
#include <QVariantMap>

class ConfigManager;

class YtDlpJsonExtractor : public QObject
{
    Q_OBJECT
public:
    explicit YtDlpJsonExtractor(ConfigManager *configManager, QObject *parent = nullptr);
    void getInfo(const QString &url);

signals:
    void infoReady(const QVariantMap &info);
    void error(const QString &errorString);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError processError);

private:
    ConfigManager *m_configManager;
    QProcess *m_process;
};