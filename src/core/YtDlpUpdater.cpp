#include "YtDlpUpdater.h"
#include <algorithm>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QJsonArray>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>
#include <QThread>

YtDlpUpdater::YtDlpUpdater(QObject *parent) : QObject(parent), m_process(nullptr) {
    m_networkManager = new QNetworkAccessManager(this);
    m_currentLocalVersion = "0.0.0";
    m_cachedVersion = loadStoredVersion();
}

YtDlpUpdater::~YtDlpUpdater() {
    stop();
}

void YtDlpUpdater::checkForUpdates() {
    fetchVersion();
}

void YtDlpUpdater::stop() {
    if (m_process) {
        // Disconnect from our slots to prevent calls on a deleted object
        disconnect(m_process, nullptr, this, nullptr);
        if (m_process->state() == QProcess::Running) {
            m_process->terminate();
        }
        // The process will self-delete on finish, so we just null our pointer
        m_process = nullptr;
    }

    // Abort all network replies
    for (auto reply : m_networkManager->findChildren<QNetworkReply*>()) {
        if (reply->isRunning()) {
            reply->abort();
        }
    }
}

void YtDlpUpdater::fetchVersion() {
    m_process = new QProcess(); // No parent
    connect(m_process, &QProcess::finished, this, &YtDlpUpdater::onVersionFetchFinished);
    // Ensure the process deletes itself when it's done.
    connect(m_process, &QProcess::finished, m_process, &QObject::deleteLater);

    QString appDir = QCoreApplication::applicationDirPath();
    m_process->start(appDir + "/yt-dlp.exe", {"--version"});
}

void YtDlpUpdater::onReleaseCheckFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->error() != QNetworkReply::OperationCanceledError) {
            emit updateFinished(Updater::UpdateStatus::Error, "Update check failed: " + reply->errorString());
        }
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject release = doc.object();

    QString remoteVersionTag = release["tag_name"].toString();
    QString normalizedRemoteVersion = normalizeVersion(remoteVersionTag);
    QString comparisonVersion = m_cachedVersion.isEmpty() ? m_currentLocalVersion : m_cachedVersion;
    QString normalizedComparisonVersion = normalizeVersion(comparisonVersion);

    if (!isVersionNewer(normalizedComparisonVersion, normalizedRemoteVersion)) {
        emit updateFinished(Updater::UpdateStatus::UpToDate, QString("yt-dlp is already up to date (%1)." ).arg(comparisonVersion));
        reply->deleteLater();
        return;
    }

    QJsonArray assets = release["assets"].toArray();
    QUrl downloadUrl;
    for (const QJsonValue &value : assets) {
        QJsonObject asset = value.toObject();
        if (asset["name"].toString() == "yt-dlp.exe") {
            downloadUrl = QUrl(asset["browser_download_url"].toString());
            break;
        }
    }

    if (downloadUrl.isValid()) {
        QNetworkRequest request(downloadUrl);
        request.setHeader(QNetworkRequest::UserAgentHeader, "MediaDownloader");
        QNetworkReply *dlReply = m_networkManager->get(request);
        dlReply->setProperty("newVersion", remoteVersionTag);
        connect(dlReply, &QNetworkReply::finished, this, &YtDlpUpdater::onDownloadFinished);
    } else {
        emit updateFinished(Updater::UpdateStatus::Error, "yt-dlp.exe not found in release assets.");
    }

    reply->deleteLater();
}

void YtDlpUpdater::onDownloadFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->error() != QNetworkReply::OperationCanceledError) {
            emit updateFinished(Updater::UpdateStatus::Error, "Download failed: " + reply->errorString());
        }
        reply->deleteLater();
        return;
    }

    QString newVersion = reply->property("newVersion").toString();
    QString appDir = QCoreApplication::applicationDirPath();
    QString targetPath = appDir + "/yt-dlp.exe";

    QFile file(targetPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(reply->readAll());
        file.close();
        m_currentLocalVersion = newVersion;
        m_cachedVersion = newVersion;
        saveStoredVersion(newVersion);
        emit versionFetched(m_currentLocalVersion);
        emit updateFinished(Updater::UpdateStatus::UpdateAvailable, QString("yt-dlp updated successfully to %1.").arg(newVersion));
    } else {
        emit updateFinished(Updater::UpdateStatus::Error, "Failed to write file: " + file.errorString());
    }

    reply->deleteLater();
}

void YtDlpUpdater::onVersionFetchFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (exitStatus == QProcess::CrashExit) {
        // If we're here, it's likely because stop() was called, and the process was terminated.
        // We don't need to do anything else. The process will self-delete.
        return;
    }

    QProcess *process = qobject_cast<QProcess*>(sender());
    if (exitCode == 0) {
        QString versionOutput = process->readAllStandardOutput().trimmed();
        m_currentLocalVersion = versionOutput;
        m_cachedVersion = m_currentLocalVersion;
        saveStoredVersion(m_currentLocalVersion);
        emit versionFetched(m_currentLocalVersion);

        QString repo = "yt-dlp/yt-dlp-nightly-builds";
        QUrl url("https://api.github.com/repos/" + repo + "/releases/latest");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, "MediaDownloader");
        QNetworkReply *reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &YtDlpUpdater::onReleaseCheckFinished);
    } else {
        emit versionFetched("Error");
        qWarning() << "Failed to fetch yt-dlp version:" << process->readAllStandardError();
        emit updateFinished(Updater::UpdateStatus::Error, "Failed to determine local yt-dlp version.");
    }
    m_process = nullptr; // The process will self-delete, so we clear our pointer.
}

QString YtDlpUpdater::normalizeVersion(const QString &version) const {
    QString trimmed = version.trimmed();
    if (trimmed.isEmpty()) return QString();
    QRegularExpression regex(R"(\d+(?:\.\d+)*)");
    QRegularExpressionMatch match = regex.match(trimmed);
    return match.hasMatch() ? match.captured(0) : trimmed;
}

bool YtDlpUpdater::isVersionNewer(const QString &localVersion, const QString &remoteVersion) const {
    QStringList localParts = localVersion.split('.', Qt::SkipEmptyParts);
    QStringList remoteParts = remoteVersion.split('.', Qt::SkipEmptyParts);
    for (int i = 0; i < std::min(localParts.size(), remoteParts.size()); ++i) {
        int localNum = localParts[i].toInt();
        int remoteNum = remoteParts[i].toInt();
        if (remoteNum > localNum) return true;
        if (remoteNum < localNum) return false;
    }
    return remoteParts.size() > localParts.size();
}

QString YtDlpUpdater::storedVersionPath() const {
    return QCoreApplication::applicationDirPath() + "/yt-dlp.version";
}

QString YtDlpUpdater::loadStoredVersion() const {
    QFile file(storedVersionPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    QString version = QString::fromUtf8(file.readAll()).trimmed();
    return version;
}

void YtDlpUpdater::saveStoredVersion(const QString &version) const {
    QFile file(storedVersionPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(version.toUtf8());
    } else {
        qWarning() << "Failed to persist yt-dlp version cache:" << file.errorString();
    }
}
