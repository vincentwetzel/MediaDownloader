#include "ActiveDownloadsTab.h"
#include "DownloadItemWidget.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QMessageBox>
#include <QProcess>

ActiveDownloadsTab::ActiveDownloadsTab(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    setupUi();
}

void ActiveDownloadsTab::setupUi() {
    m_downloadsLayout = new QVBoxLayout(this);
    m_downloadsLayout->addStretch();
}

void ActiveDownloadsTab::addDownloadItem(const QVariantMap &itemData) {
    DownloadItemWidget *itemWidget = new DownloadItemWidget(itemData, this);
    m_downloadsLayout->insertWidget(m_downloadsLayout->count() - 1, itemWidget);
    m_downloadItems[itemData["id"].toString()] = itemWidget;

    connect(itemWidget, &DownloadItemWidget::cancelRequested, this, &ActiveDownloadsTab::cancelDownloadRequested);
    connect(itemWidget, &DownloadItemWidget::retryRequested, this, &ActiveDownloadsTab::retryDownloadRequested);
    connect(itemWidget, &DownloadItemWidget::resumeRequested, this, &ActiveDownloadsTab::resumeDownloadRequested);
}

void ActiveDownloadsTab::updateDownloadProgress(const QString &id, const QVariantMap &progressData) {
    if (m_downloadItems.contains(id)) {
        m_downloadItems[id]->updateProgress(progressData);
    }
}

void ActiveDownloadsTab::onDownloadFinished(const QString &id, bool success, const QString &message) {
    if (m_downloadItems.contains(id)) {
        m_downloadItems[id]->setFinished(success, message);
    }
}

void ActiveDownloadsTab::onDownloadCancelled(const QString &id) {
    if (m_downloadItems.contains(id)) {
        m_downloadItems[id]->setCancelled();
    }
}

void ActiveDownloadsTab::onDownloadFinalPathReady(const QString &id, const QString &path) {
    if (m_downloadItems.contains(id)) {
        m_downloadItems[id]->setFinalPath(path);
    }
}

void ActiveDownloadsTab::setDownloadStatus(const QString &id, const QString &status) {
    if (m_downloadItems.contains(id)) {
        m_downloadItems[id]->updateProgress({{"status", status}});
    }
}

void ActiveDownloadsTab::addExpandingPlaylist(const QString &url) {
    // UI for this can be added later
}

void ActiveDownloadsTab::removeExpandingPlaylist(const QString &url, int count) {
    // UI for this can be added later
}
