#include "UrlValidator.h"
#include <QDebug>

UrlValidator::UrlValidator(ConfigManager *configManager, QObject *parent)
    : QObject(parent), m_configManager(configManager) {

    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &UrlValidator::onProcessFinished);
}

void UrlValidator::validate(const QString &url) {
    QStringList args;
    args << "--simulate";
    args << url;

    // Add cookies if configured
    QString cookiesBrowser = m_configManager->get("General", "cookies_from_browser", "None").toString();
    if (cookiesBrowser != "None") {
        args << "--cookies-from-browser" << cookiesBrowser;
    }

    qDebug() << "UrlValidator executing command: yt-dlp" << args;
    m_process->start("yt-dlp", args);
}

void UrlValidator::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    bool isValid = (exitStatus == QProcess::NormalExit && exitCode == 0);
    QString error;
    if (!isValid) {
        QString stderrOutput = m_process->readAllStandardError();
        error = "yt-dlp encountered an error.";
        QStringList lines = stderrOutput.split('\n');
        for (const QString &line : lines) {
            if (line.startsWith("ERROR:")) {
                error = line.trimmed();
                break;
            }
        }
    }
    emit validationFinished(isValid, error);
}
