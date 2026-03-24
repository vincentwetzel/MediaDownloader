#ifndef PLAYLISTEXPANDER_H
#define PLAYLISTEXPANDER_H

#include <QObject>
#include <QProcess>
#include <QVariantMap>
#include <QList> // Include QList
#include "ConfigManager.h"

class PlaylistExpander : public QObject {
    Q_OBJECT

public:
    explicit PlaylistExpander(const QString &url, ConfigManager *configManager, QObject *parent = nullptr);
    void startExpansion(const QString &playlistLogic); // Modified to accept playlistLogic

signals:
    void expansionFinished(const QString &originalUrl, const QList<QVariantMap> &expandedItems, const QString &error);
    void playlistDetected(const QString &url, int itemCount, const QVariantMap &options, const QList<QVariantMap> &expandedItems); // New signal with options and expandedItems

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QString m_url;
    ConfigManager *m_configManager;
    QProcess *m_process;
    QString m_currentPlaylistLogic; // Store the playlist logic for onProcessFinished
    QVariantMap m_options; // Store options to pass back with playlistDetected
};

#endif // PLAYLISTEXPANDER_H