#include "DownloadItemWidget.h"
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>

DownloadItemWidget::DownloadItemWidget(const QVariantMap &itemData, QWidget *parent)
    : QWidget(parent), m_itemData(itemData) {
    setupUi();
}

QString DownloadItemWidget::getId() const {
    return m_itemData["id"].toString();
}

QVariantMap DownloadItemWidget::getItemData() const {
    return m_itemData;
}

void DownloadItemWidget::setupUi() {
    QHBoxLayout *mainLayout = new QHBoxLayout(this);

    m_titleLabel = new QLabel(m_itemData["url"].toString(), this);
    m_titleLabel->setWordWrap(true);

    m_statusLabel = new QLabel("Queued", this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);

    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->addWidget(m_titleLabel);
    infoLayout->addWidget(m_statusLabel);
    infoLayout->addWidget(m_progressBar);

    m_cancelButton = new QPushButton("Cancel", this);
    m_retryButton = new QPushButton("Retry", this);
    m_openFolderButton = new QPushButton("Open Folder", this);

    m_retryButton->hide();
    m_openFolderButton->hide();

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_retryButton);
    buttonLayout->addWidget(m_openFolderButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(infoLayout, 1);
    mainLayout->addLayout(buttonLayout);

    connect(m_cancelButton, &QPushButton::clicked, this, &DownloadItemWidget::onCancelClicked);
    connect(m_retryButton, &QPushButton::clicked, this, &DownloadItemWidget::onRetryClicked);
    connect(m_openFolderButton, &QPushButton::clicked, this, &DownloadItemWidget::onOpenContainingFolderClicked);
}

void DownloadItemWidget::updateProgress(const QVariantMap &progressData) {
    if (progressData.contains("title")) {
        m_titleLabel->setText(progressData["title"].toString());
    }
    if (progressData.contains("status")) {
        m_statusLabel->setText(progressData["status"].toString());
    }
    if (progressData.contains("progress")) {
        int progress = progressData["progress"].toInt();
        if (progress < 0) {
            m_progressBar->setRange(0, 0); // Indeterminate
        } else {
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(progress);
        }
    }
}

void DownloadItemWidget::setFinalPath(const QString &path) {
    m_itemData["final_path"] = path;
    m_openFolderButton->show();
}

void DownloadItemWidget::setFinished(bool success, const QString &message) {
    m_cancelButton->hide();
    m_statusLabel->setText(message);
    if (!success) {
        m_retryButton->show();
    }
}

void DownloadItemWidget::setCancelled() {
    m_cancelButton->hide();
    m_retryButton->show();
    m_statusLabel->setText("Cancelled");
}

void DownloadItemWidget::onCancelClicked() {
    emit cancelRequested(getId());
}

void DownloadItemWidget::onRetryClicked() {
    emit retryRequested(m_itemData);
}

void DownloadItemWidget::onOpenContainingFolderClicked() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_itemData["final_path"].toString()).path()));
}
