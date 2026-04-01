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
    m_titleLabel->setToolTip("The URL or title of the media being downloaded.");

    m_statusLabel = new QLabel("Queued", this);
    m_statusLabel->setToolTip("Current status of this download.");
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setToolTip("Download progress.");

    m_clearButton = new QPushButton("X", this);
    m_clearButton->setToolTip("Clear this download from the list.");
    m_clearButton->setFixedSize(20, 20);
    m_clearButton->setStyleSheet("QPushButton { font-weight: bold; color: red; border: none; } QPushButton:hover { background-color: rgba(150,150,150,0.3); }");
    m_clearButton->hide();

    QVBoxLayout *infoLayout = new QVBoxLayout();
    QHBoxLayout *titleLayout = new QHBoxLayout();
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_clearButton);
    infoLayout->addLayout(titleLayout);
    infoLayout->addWidget(m_statusLabel);
    infoLayout->addWidget(m_progressBar);

    m_pauseResumeButton = new QPushButton("Pause", this);
    m_pauseResumeButton->setToolTip("Pause or resume this download.");
    m_cancelButton = new QPushButton("Cancel", this);
    m_cancelButton->setToolTip("Cancel this download.");
    m_retryButton = new QPushButton("Retry", this);
    m_retryButton->setToolTip("Retry this failed or cancelled download.");
    m_openFolderButton = new QPushButton("Open Folder", this);
    m_openFolderButton->setToolTip("Open the folder where this file was saved.");

    m_retryButton->hide();
    m_openFolderButton->hide();

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_pauseResumeButton);
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_retryButton);
    buttonLayout->addWidget(m_openFolderButton);
    buttonLayout->addStretch();

    m_moveUpButton = new QPushButton("▲", this);
    m_moveUpButton->setToolTip("Move this download up in the queue.");
    m_moveUpButton->setFixedSize(20, 20);

    m_moveDownButton = new QPushButton("▼", this);
    m_moveDownButton->setToolTip("Move this download down in the queue.");
    m_moveDownButton->setFixedSize(20, 20);

    QVBoxLayout *moveLayout = new QVBoxLayout();
    moveLayout->addWidget(m_moveUpButton);
    moveLayout->addWidget(m_moveDownButton);
    moveLayout->setSpacing(0);
    moveLayout->setContentsMargins(0, 0, 5, 0);

    mainLayout->insertLayout(0, moveLayout);
    mainLayout->addLayout(infoLayout, 1);
    mainLayout->addLayout(buttonLayout);

    connect(m_cancelButton, &QPushButton::clicked, this, &DownloadItemWidget::onCancelClicked);
    connect(m_retryButton, &QPushButton::clicked, this, &DownloadItemWidget::onRetryClicked);
    connect(m_openFolderButton, &QPushButton::clicked, this, &DownloadItemWidget::onOpenContainingFolderClicked);
    connect(m_clearButton, &QPushButton::clicked, this, [this]() { emit clearRequested(getId()); });
    connect(m_pauseResumeButton, &QPushButton::clicked, this, &DownloadItemWidget::onPauseResumeClicked);
    connect(m_moveUpButton, &QPushButton::clicked, this, &DownloadItemWidget::onMoveUpClicked);
    connect(m_moveDownButton, &QPushButton::clicked, this, &DownloadItemWidget::onMoveDownClicked);
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
    m_pauseResumeButton->hide();
    m_cancelButton->hide();
    m_moveUpButton->hide();
    m_moveDownButton->hide();
    m_isFinished = true;
    m_isSuccessful = success;
    m_clearButton->show();
    m_statusLabel->setText(message);
    if (!success) {
        m_retryButton->show();
    }
}

void DownloadItemWidget::setCancelled() {
    m_pauseResumeButton->hide();
    m_cancelButton->hide();
    m_moveUpButton->hide();
    m_moveDownButton->hide();
    m_retryButton->show();
    m_isFinished = true;
    m_isSuccessful = false;
    m_clearButton->show();
    m_statusLabel->setText("Cancelled");
}

void DownloadItemWidget::setPaused(bool paused) {
    m_isPaused = paused;
    m_pauseResumeButton->setText(paused ? "Resume" : "Pause");
    if (paused) {
        m_statusLabel->setText("Paused");
    }
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

void DownloadItemWidget::onPauseResumeClicked() {
    if (m_isPaused) {
        emit unpauseRequested(getId());
    } else {
        emit pauseRequested(getId());
    }
}

void DownloadItemWidget::onMoveUpClicked() {
    emit moveUpRequested(getId());
}

void DownloadItemWidget::onMoveDownClicked() {
    emit moveDownRequested(getId());
}
