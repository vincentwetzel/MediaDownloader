#ifndef APPUPDATER_H
#define APPUPDATER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>

class AppUpdater : public QObject {
    Q_OBJECT

public:
    explicit AppUpdater(const QString &repoUrl, const QString &currentVersion, QObject *parent = nullptr);
    void checkForUpdates();
    void downloadAndInstall(const QUrl &downloadUrl);

signals:
    void updateAvailable(const QString &latestVersion, const QString &releaseNotes, const QUrl &downloadUrl);
    void noUpdateAvailable();
    void updateCheckFailed(const QString &error);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished();

private slots:
    void onCheckFinished(QNetworkReply *reply);
    void onDownloadFinished(QNetworkReply *reply);

private:
    QString m_repoUrl;
    QString m_currentVersion;
    QNetworkAccessManager *m_networkManager;
};

#endif // APPUPDATER_H