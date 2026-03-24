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

private slots:
    void onCancelClicked();
    void onRetryClicked();
    void onOpenContainingFolderClicked();

private:
    void setupUi();

    QVariantMap m_itemData;
    QLabel *m_titleLabel;
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;
    QPushButton *m_cancelButton;
    QPushButton *m_retryButton;
    QPushButton *m_openFolderButton;
};

#endif // DOWNLOADITEMWIDGET_H
