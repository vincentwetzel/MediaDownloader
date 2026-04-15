#include "AppUpdater.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QProcess>
#include <QStandardPaths>
#include <QFile>
#include <QCoreApplication>
#include <QDir>

AppUpdater::AppUpdater(const QString &repoUrl, const QString &currentVersion, QObject *parent)
    : QObject(parent), m_repoUrl(repoUrl), m_currentVersion(currentVersion) {

    m_networkManager = new QNetworkAccessManager(this);
}

void AppUpdater::checkForUpdates() {
    QUrl url(m_repoUrl + "/releases/latest");
    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        onCheckFinished(reply);
    });
}

void AppUpdater::onCheckFinished(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        emit updateCheckFailed(reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isNull() || !doc.isObject()) {
        emit updateCheckFailed("Failed to parse release JSON.");
        reply->deleteLater();
        return;
    }

    QJsonObject release = doc.object();
    QString latestVersion = release["tag_name"].toString().remove(0, 1); // Remove 'v' prefix
    QString releaseNotes = release["body"].toString();

    // Simple version comparison
    if (latestVersion > m_currentVersion) {
        QJsonArray assets = release["assets"].toArray();
        for (const QJsonValue &value : assets) {
            QJsonObject asset = value.toObject();
            QString assetName = asset["name"].toString();
            if (assetName.endsWith(".exe")) { // Assuming installer is .exe
                QUrl downloadUrl(asset["browser_download_url"].toString());
                emit updateAvailable(latestVersion, releaseNotes, downloadUrl);
                reply->deleteLater();
                return;
            }
        }
        emit updateCheckFailed("No suitable installer found in the latest release.");
    } else {
        emit noUpdateAvailable();
    }

    reply->deleteLater();
}

void AppUpdater::downloadAndInstall(const QUrl &downloadUrl) {
    QNetworkRequest request(downloadUrl);
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::downloadProgress, this, &AppUpdater::downloadProgress);
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        onDownloadFinished(reply);
    });
}

void AppUpdater::onDownloadFinished(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        emit updateCheckFailed("Failed to download update: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString installerPath = tempPath + "/LzyDownloader-Setup.exe";

    QFile installerFile(installerPath);
    if (!installerFile.open(QIODevice::WriteOnly)) {
        emit updateCheckFailed("Failed to save installer.");
        reply->deleteLater();
        return;
    }

    installerFile.write(reply->readAll());
    installerFile.close();
    reply->deleteLater();

    emit downloadFinished();

    // Run the installer silently
    QStringList args;
    args << "/S"; // Silent install
    args << "/D=" + QDir::toNativeSeparators(QCoreApplication::applicationDirPath());

    QProcess::startDetached(installerPath, args);
    QCoreApplication::quit();
}
