#include "PlaylistExpander.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

PlaylistExpander::PlaylistExpander(const QString &url, ConfigManager *configManager, QObject *parent)
    : QObject(parent), m_url(url), m_configManager(configManager) {

    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &PlaylistExpander::onProcessFinished);
}

void PlaylistExpander::startExpansion(const QString &playlistLogic) {
    m_currentPlaylistLogic = playlistLogic;
    m_options = property("options").toMap(); // Retrieve options stored earlier

    QStringList args;
    args << m_url;
    args << "--flat-playlist";
    args << "--dump-single-json";

    // Add cookies if configured
    QString cookiesBrowser = m_configManager->get("General", "cookies_from_browser", "None").toString();
    if (cookiesBrowser != "None") {
        args << "--cookies-from-browser" << cookiesBrowser;
    }

    m_process->start("yt-dlp", args);
}

void PlaylistExpander::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        emit expansionFinished(m_url, {}, "Failed to expand playlist: " + m_process->readAllStandardError());
        return;
    }

    QByteArray jsonData = m_process->readAllStandardOutput();
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);

    if (doc.isNull() || !doc.isObject()) {
        emit expansionFinished(m_url, {}, "Failed to parse playlist JSON.");
        return;
    }

    QJsonObject root = doc.object();
    QList<QVariantMap> expandedItems;
    bool isPlaylist = false;

    if (root.contains("entries") && root["entries"].isArray()) {
        // It's a playlist
        isPlaylist = true;
        QJsonArray entries = root["entries"].toArray();
        for (const QJsonValue &value : entries) {
            QJsonObject entry = value.toObject();
            if (entry.contains("url")) {
                QVariantMap item;
                item["url"] = entry["url"].toString();
                item["playlist_index"] = entry.value("playlist_index").toInt(-1);
                expandedItems.append(item);
            }
        }
    } else {
        // It's a single video
        QVariantMap item;
        item["url"] = m_url;
        item["playlist_index"] = -1;
        expandedItems.append(item);
    }

    if (isPlaylist && m_currentPlaylistLogic == "Ask") {
        emit playlistDetected(m_url, expandedItems.count(), m_options, expandedItems); // Emit expandedItems
    } else {
        emit expansionFinished(m_url, expandedItems, "");
    }
}