#ifndef ACTIVEDOWNLOADSTAB_H
#define ACTIVEDOWNLOADSTAB_H

#include <QWidget>
#include <QMap>
#include <QVariantMap>

class QVBoxLayout;
class DownloadItemWidget;
class ConfigManager;

class ActiveDownloadsTab : public QWidget {
    Q_OBJECT

public:
    explicit ActiveDownloadsTab(ConfigManager *configManager, QWidget *parent = nullptr);

public slots:
    void addDownloadItem(const QVariantMap &itemData);
    void updateDownloadProgress(const QString &id, const QVariantMap &progressData);
    void onDownloadFinished(const QString &id, bool success, const QString &message);
    void onDownloadCancelled(const QString &id);
    void onDownloadFinalPathReady(const QString &id, const QString &path);
    void setDownloadStatus(const QString &id, const QString &status);
    void addExpandingPlaylist(const QString &url);
    void removeExpandingPlaylist(const QString &url, int count);

signals:
    void cancelDownloadRequested(const QString &id);
    void retryDownloadRequested(const QVariantMap &itemData);
    void resumeDownloadRequested(const QVariantMap &itemData);

private:
    void setupUi();

    ConfigManager *m_configManager;
    QVBoxLayout *m_downloadsLayout;
    QMap<QString, DownloadItemWidget*> m_downloadItems;
    QMap<QString, QWidget*> m_expandingPlaylists;
};

#endif // ACTIVEDOWNLOADSTAB_H
