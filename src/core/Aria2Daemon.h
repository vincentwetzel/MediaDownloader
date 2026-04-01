#pragma once

#include <QObject>
#include <QProcess>

class ConfigManager;

class Aria2Daemon : public QObject
{
    Q_OBJECT
public:
    explicit Aria2Daemon(ConfigManager *configManager, QObject *parent = nullptr);
    ~Aria2Daemon();

    bool start();
    void stop();
    bool isRunning() const;

signals:
    void error(const QString &errorString);

private slots:
    void onDaemonError(QProcess::ProcessError processError);
    void onDaemonFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    ConfigManager *m_configManager;
    QProcess *m_process;
};