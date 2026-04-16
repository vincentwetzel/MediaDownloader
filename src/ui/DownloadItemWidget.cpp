#include "DownloadItemWidget.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>
#include <QProgressBar>

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

    // Thumbnail label on the left side
    m_thumbnailLabel = new QLabel(this);
    m_thumbnailLabel->setFixedSize(80, 60);
    m_thumbnailLabel->setStyleSheet("QLabel { background-color: palette(mid); border-radius: 4px; }");
    m_thumbnailLabel->setAlignment(Qt::AlignCenter);
    m_thumbnailLabel->setToolTip("Thumbnail preview of the media being downloaded.");
    m_thumbnailLabel->setScaledContents(false);

    m_titleLabel = new QLabel(m_itemData["url"].toString(), this);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setToolTip("The URL or title of the media being downloaded.");

    m_statusLabel = new QLabel("Queued", this);
    m_statusLabel->setToolTip("Current status of this download.");

    m_progressBar = new ProgressLabelBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setToolTip("Progress for the currently active stream or processing stage.");

    m_overallProgressLabel = new QLabel("Overall progress", this);
    m_overallProgressLabel->setToolTip("Overall progress across all primary streams in this download.");
    m_overallProgressLabel->hide();

    m_overallProgressBar = new QProgressBar(this);
    m_overallProgressBar->setRange(0, 100);
    m_overallProgressBar->setValue(0);
    m_overallProgressBar->setTextVisible(false);
    m_overallProgressBar->setMaximumHeight(8);
    m_overallProgressBar->setToolTip("Overall progress across all primary streams in this download.");
    m_overallProgressBar->hide();

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
    infoLayout->addWidget(m_overallProgressLabel);
    infoLayout->addWidget(m_overallProgressBar);

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
    mainLayout->addWidget(m_thumbnailLabel);
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
    if (m_isFinished) {
        return; // Ignore delayed progress signals if already finished
    }

    if (progressData.contains("title")) {
        m_titleLabel->setText(progressData["title"].toString());
    }

    if (progressData.contains("status")) {
        m_statusLabel->setStyleSheet("");
        QString statusText = progressData["status"].toString();
        
        if (statusText == "Downloading...") {
            const QString type = m_itemData.value("options").toMap().value("type").toString();
            if (type == "audio") {
                statusText = "Downloading audio...";
            } else if (type == "gallery") {
                statusText = "Downloading gallery...";
            }
        }

        m_statusLabel->setText(statusText);
    }
    if (progressData.contains("progress")) {
        int progress = progressData["progress"].toInt();
        if (progress < 0) {
            // Indeterminate state (queued/starting) - colorless/default
            m_progressBar->setRange(0, 0);
            m_progressBar->setStyleSheet("");
            m_progressBar->setProgressText("");
        } else if (progress == 100 && (m_statusLabel->text().contains("Processing", Qt::CaseInsensitive) ||
                                       m_statusLabel->text().contains("Merging", Qt::CaseInsensitive) ||
                                       m_statusLabel->text().contains("Post", Qt::CaseInsensitive) ||
                                       m_statusLabel->text().contains("Extracting", Qt::CaseInsensitive) ||
                                       m_statusLabel->text().contains("Converting", Qt::CaseInsensitive) ||
                                       m_statusLabel->text().contains("Applying", Qt::CaseInsensitive) ||
                                       m_statusLabel->text().contains("Fixing", Qt::CaseInsensitive) ||
                                       m_statusLabel->text().contains("Verifying", Qt::CaseInsensitive) ||
                                       m_statusLabel->text().contains("Moving", Qt::CaseInsensitive) ||
                                       m_statusLabel->text().contains("Copying", Qt::CaseInsensitive) ||
                                       m_statusLabel->text().contains("Embedding", Qt::CaseInsensitive) ||
                                       m_statusLabel->text() == "Complete")) {
            // Still in post-processing / finalizing phase - teal
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(100);
            m_progressBar->setStyleSheet("QProgressBar::chunk { background-color: #008080; }");
            m_progressBar->setProgressText("Finalizing...");
        } else {
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(progress);

            // Actively downloading - light blue for all active transfers
            m_progressBar->setStyleSheet("QProgressBar::chunk { background-color: #3b82f6; }");

            // Build centered progress text: percentage + size + speed + ETA
            QStringList parts;
            parts << QString("%1%").arg(progress);

            if (progressData.contains("downloaded_size") && progressData.contains("total_size")) {
                parts << QString("%1/%2").arg(progressData["downloaded_size"].toString(), progressData["total_size"].toString());
            }
            if (progressData.contains("speed")) {
                parts << progressData["speed"].toString();
            }
            if (progressData.contains("eta")) {
                parts << QString("ETA %1").arg(progressData["eta"].toString());
            }
            m_progressBar->setProgressText(parts.join("  "));
        }
    }
    if (progressData.contains("overall_progress")) {
        const int overallProgress = qRound(progressData["overall_progress"].toDouble());
        m_overallProgressBar->show();
        m_overallProgressLabel->show();
        m_overallProgressBar->setRange(0, 100);
        m_overallProgressBar->setValue(overallProgress);
        m_overallProgressBar->setStyleSheet("QProgressBar::chunk { background-color: #64748b; }");

        QString overallLabel = QString("Overall %1%").arg(overallProgress);
        if (progressData.contains("overall_downloaded_size") && progressData.contains("overall_total_size")) {
            overallLabel += QString("  %1/%2").arg(progressData["overall_downloaded_size"].toString(), progressData["overall_total_size"].toString());
        }
        m_overallProgressLabel->setText(overallLabel);
    } else if (progressData.contains("progress") && progressData["progress"].toInt() < 0) {
        m_overallProgressBar->hide();
        m_overallProgressLabel->hide();
    }

    if (progressData.contains("thumbnail_path")) {
        setThumbnail(progressData["thumbnail_path"].toString());
    }
}

void DownloadItemWidget::setThumbnail(const QString &imagePath) {
    if (imagePath.isEmpty()) {
        return;
    }
    
    QFileInfo fileInfo(imagePath);
    if (!fileInfo.exists()) {
        return;
    }
    
    QPixmap pixmap(imagePath);
    if (pixmap.isNull()) {
        return;
    }
    
    // Scale the pixmap to fit the label while maintaining aspect ratio
    QPixmap scaled = pixmap.scaled(m_thumbnailLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_thumbnailLabel->setPixmap(scaled);
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

    if (!success) {
        m_retryButton->show();
        m_statusLabel->setStyleSheet("color: #dc2626;");
        m_progressBar->setStyleSheet("QProgressBar { color: #dc2626; }");
        m_progressBar->setProgressText("Download failed");
        m_overallProgressBar->hide();
        m_overallProgressLabel->hide();
    } else {
        m_statusLabel->setStyleSheet("");
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(100);
        m_progressBar->setStyleSheet("QProgressBar::chunk { background-color: #22c55e; }");
        m_progressBar->setProgressText("Complete");
        if (m_overallProgressBar->isVisible()) {
            m_overallProgressBar->setRange(0, 100);
            m_overallProgressBar->setValue(100);
            m_overallProgressBar->setStyleSheet("QProgressBar::chunk { background-color: #94a3b8; }");
            m_overallProgressLabel->setText("Overall 100%");
        }
    }
    m_statusLabel->setText(message);
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
    m_statusLabel->setStyleSheet("color: #dc2626;");
    m_statusLabel->setText("Cancelled");
    m_progressBar->setStyleSheet("QProgressBar { color: #dc2626; }");
    m_progressBar->setProgressText("Cancelled");
    m_overallProgressBar->hide();
    m_overallProgressLabel->hide();
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
