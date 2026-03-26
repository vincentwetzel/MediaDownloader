#pragma once
#include <QWidget>
#include <QVariant>
#include <QProcess>

class ConfigManager;
class QComboBox;
class QTimer;

class AuthenticationPage : public QWidget {
    Q_OBJECT
public:
    explicit AuthenticationPage(ConfigManager *configManager, QWidget *parent = nullptr);
    ~AuthenticationPage() override;
public slots:
    void loadSettings();
private slots:
    void onCookiesBrowserChanged(const QString &text);
    void onCookieCheckProcessStarted();
    void onCookieCheckProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onCookieCheckProcessErrorOccurred(QProcess::ProcessError error);
    void onCookieCheckProcessReadyReadStandardOutput();
    void onCookieCheckProcessReadyReadStandardError();
    void onCookieCheckTimeout();
    void handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value);
private:
    ConfigManager *m_configManager;
    QComboBox *m_cookiesBrowserCombo;
    QString m_lastSavedBrowser;
    QProcess *m_cookieCheckProcess;
    QTimer *m_cookieCheckTimeoutTimer;
};