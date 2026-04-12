#ifndef DOWNLOADITEMWIDGET_H
#define DOWNLOADITEMWIDGET_H

#include <QWidget>
#include <QVariantMap>

class QLabel;
class QProgressBar;
class QPushButton;

class DownloadItemWidget : public QWidget {
    Q_OBJECT

public:
    explicit DownloadItemWidget(const QVariantMap &itemData, QWidget *parent = nullptr);
    QString getId() const;
    QVariantMap getItemData() const;
    void updateProgress(const QVariantMap &progressData);
    void setFinalPath(const QString &path);
    void setFinished(bool success, const QString &message);
    void setCancelled();

signals:
    void cancelRequested(const QString &id);
    void retryRequested(const QVariantMap &itemData);
    void resumeRequested(const QVariantMap &itemData);
    void clearRequested(const QString &id);
    void pauseRequested(const QString &id);
    void unpauseRequested(const QString &id);
    void moveUpRequested(const QString &id);
    void moveDownRequested(const QString &id);

private slots:
    void onCancelClicked();
    void onRetryClicked();
    void onOpenContainingFolderClicked();
    void onPauseResumeClicked();
    void onMoveUpClicked();
    void onMoveDownClicked();

private:
    void setupUi();
    void setThumbnail(const QString &imagePath);

    QVariantMap m_itemData;
    QLabel *m_thumbnailLabel;
    QLabel *m_titleLabel;
    QLabel *m_statusLabel;
    QLabel *m_progressDetailsLabel;
    QProgressBar *m_progressBar;
    QPushButton *m_clearButton;
    QPushButton *m_pauseResumeButton;
    QPushButton *m_cancelButton;
    QPushButton *m_retryButton;
    QPushButton *m_openFolderButton;
    QPushButton *m_moveUpButton;
    QPushButton *m_moveDownButton;
    bool m_isFinished = false;
    bool m_isSuccessful = false;
    bool m_isPaused = false;

public:
    void setPaused(bool paused);
    bool isFinished() const { return m_isFinished; }
    bool isSuccessful() const { return m_isSuccessful; }
};

#endif // DOWNLOADITEMWIDGET_H
