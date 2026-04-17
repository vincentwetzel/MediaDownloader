#include "AuthenticationPage.h"
#include "core/ConfigManager.h"
#include "utils/BrowserUtils.h"
#include "core/ProcessUtils.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QTimer>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>

AuthenticationPage::AuthenticationPage(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *authGroup = new QGroupBox("Authentication Access", this);
    authGroup->setToolTip("Settings for accessing content that requires login, using browser cookies.");
    QFormLayout *authLayout = new QFormLayout(authGroup);

    QStringList installedBrowsers = BrowserUtils::getInstalledBrowsers();
    QStringList orderedBrowsers;

    int firefoxIndex = -1, chromeIndex = -1;
    for (int i = 0; i < installedBrowsers.size(); ++i) {
        if (installedBrowsers.at(i).compare("Firefox", Qt::CaseInsensitive) == 0) firefoxIndex = i;
        if (installedBrowsers.at(i).compare("Chrome", Qt::CaseInsensitive) == 0) chromeIndex = i;
    }
    if (firefoxIndex != -1) orderedBrowsers.append(installedBrowsers.takeAt(firefoxIndex));
    if (chromeIndex != -1) orderedBrowsers.append(installedBrowsers.takeAt(chromeIndex));

    std::sort(installedBrowsers.begin(), installedBrowsers.end(), [](const QString &s1, const QString &s2){
        return s1.compare(s2, Qt::CaseInsensitive) < 0;
    });
    orderedBrowsers.append(installedBrowsers);
    orderedBrowsers.append("None");

    m_cookiesBrowserCombo = new QComboBox(this);
    m_cookiesBrowserCombo->setToolTip("Choose a web browser to get your login cookies from.");
    m_cookiesBrowserCombo->addItems(orderedBrowsers);
    authLayout->addRow("Cookies from browser:", m_cookiesBrowserCombo);

    layout->addWidget(authGroup);
    layout->addStretch();

    m_cookieCheckProcess = new QProcess(this);
    connect(m_cookieCheckProcess, &QProcess::started, this, &AuthenticationPage::onCookieCheckProcessStarted);
    connect(m_cookieCheckProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &AuthenticationPage::onCookieCheckProcessFinished);
    connect(m_cookieCheckProcess, &QProcess::errorOccurred, this, &AuthenticationPage::onCookieCheckProcessErrorOccurred);
    connect(m_cookieCheckProcess, &QProcess::readyReadStandardOutput, this, &AuthenticationPage::onCookieCheckProcessReadyReadStandardOutput);
    connect(m_cookieCheckProcess, &QProcess::readyReadStandardError, this, &AuthenticationPage::onCookieCheckProcessReadyReadStandardError);

    m_cookieCheckTimeoutTimer = new QTimer(this);
    m_cookieCheckTimeoutTimer->setSingleShot(true);
    connect(m_cookieCheckTimeoutTimer, &QTimer::timeout, this, &AuthenticationPage::onCookieCheckTimeout);

    connect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AuthenticationPage::onCookiesBrowserChanged);
    connect(m_configManager, &ConfigManager::settingChanged, this, &AuthenticationPage::handleConfigSettingChanged);
}

AuthenticationPage::~AuthenticationPage() {
    if (m_cookieCheckProcess->state() != QProcess::NotRunning) {
        ProcessUtils::terminateProcessTree(m_cookieCheckProcess);
    }
}

void AuthenticationPage::loadSettings() {
    disconnect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AuthenticationPage::onCookiesBrowserChanged);
    m_lastSavedBrowser = m_configManager->get("General", "cookies_from_browser", "None").toString();
    if (m_lastSavedBrowser == "None") {
        m_lastSavedBrowser = m_configManager->get("General", "gallery_cookies_from_browser", "None").toString();
    }
    m_cookiesBrowserCombo->setCurrentText(m_lastSavedBrowser);
    connect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AuthenticationPage::onCookiesBrowserChanged);
}

void AuthenticationPage::onCookiesBrowserChanged(const QString &text) {
    if (text == m_lastSavedBrowser) return;
    if (text.compare("None", Qt::CaseInsensitive) == 0) {
        m_lastSavedBrowser = text;
        m_configManager->set("General", "cookies_from_browser", text);
        m_configManager->set("General", "gallery_cookies_from_browser", text);
        return;
    }
    if (m_cookieCheckProcess->state() != QProcess::NotRunning) m_cookieCheckProcess->terminate();
    setCursor(Qt::WaitCursor);
    m_cookiesBrowserCombo->setEnabled(false);

    QStringList args;
    args << "--cookies-from-browser" << text.toLower() << "--simulate" << "--verbose" << "https://www.youtube.com/watch?v=7x52ID-2H0E";

    QString denoPath;
    const QString appDir = QCoreApplication::applicationDirPath();
    if (QFile::exists(QDir(appDir).filePath("deno.exe"))) denoPath = QDir(appDir).filePath("deno.exe");
    else if (QFile::exists(QDir(appDir).filePath("bin/deno.exe"))) denoPath = QDir(appDir).filePath("bin/deno.exe");

    if (!denoPath.isEmpty()) args << "--js-runtimes" << QString("deno:%1").arg(denoPath);
    m_cookieCheckProcess->start("yt-dlp", args);
}

void AuthenticationPage::onCookieCheckProcessStarted() { m_cookieCheckTimeoutTimer->start(30000); }
void AuthenticationPage::onCookieCheckProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    m_cookieCheckTimeoutTimer->stop();
    unsetCursor();
    m_cookiesBrowserCombo->setEnabled(true);
    QString selectedBrowser = m_cookiesBrowserCombo->currentText();

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        QString stderrOutput = m_cookieCheckProcess->readAllStandardError();
        QString errorMessage = (stderrOutput.contains("Unable to get cookie info", Qt::CaseInsensitive) || stderrOutput.contains("database is locked", Qt::CaseInsensitive))
            ? QString("Failed to access cookies for %1. The browser may be running, which can lock the cookie database. Please close %1 and try again.").arg(selectedBrowser)
            : QString("An unexpected error occurred while checking cookies for %1.\n\nDetails:\n%2").arg(selectedBrowser, stderrOutput);
        QMessageBox::warning(this, "Cookie Access Failed", errorMessage);
        loadSettings(); // Reset to last saved implicitly
    } else {
        m_lastSavedBrowser = selectedBrowser;
        m_configManager->set("General", "cookies_from_browser", selectedBrowser);
        m_configManager->set("General", "gallery_cookies_from_browser", selectedBrowser);
    }
}
void AuthenticationPage::onCookieCheckProcessErrorOccurred(QProcess::ProcessError error) {
    m_cookieCheckTimeoutTimer->stop();
    if (m_cookieCheckProcess->state() == QProcess::NotRunning) return;
    unsetCursor(); m_cookiesBrowserCombo->setEnabled(true);
    QMessageBox::critical(this, "Process Error", error == QProcess::FailedToStart ? "Failed to start yt-dlp. Please ensure 'yt-dlp.exe' is in the application directory or 'bin/' folder." : QString("An unknown error occurred: %1").arg(m_cookieCheckProcess->errorString()));
    loadSettings();
}
void AuthenticationPage::onCookieCheckProcessReadyReadStandardOutput() { qDebug().noquote() << "yt-dlp stdout:" << m_cookieCheckProcess->readAllStandardOutput(); }
void AuthenticationPage::onCookieCheckProcessReadyReadStandardError() { qWarning().noquote() << "yt-dlp stderr:" << m_cookieCheckProcess->readAllStandardError(); }
void AuthenticationPage::onCookieCheckTimeout() { ProcessUtils::terminateProcessTree(m_cookieCheckProcess); unsetCursor(); m_cookiesBrowserCombo->setEnabled(true); QMessageBox::warning(this, "Timed Out", "The cookie check took too long to respond."); loadSettings(); }
void AuthenticationPage::handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value) {
    if (section == "General" && (key == "cookies_from_browser" || key == "gallery_cookies_from_browser")) {
        m_lastSavedBrowser = value.toString();
        if (m_cookiesBrowserCombo->currentText() != m_lastSavedBrowser) {
            disconnect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AuthenticationPage::onCookiesBrowserChanged);
            m_cookiesBrowserCombo->setCurrentText(m_lastSavedBrowser);
            connect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AuthenticationPage::onCookiesBrowserChanged);
        }
    }
}