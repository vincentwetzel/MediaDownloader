#pragma once
#include <QWidget>

class ConfigManager;
class QLabel;
class QPushButton;

class UpdatesPage : public QWidget {
    Q_OBJECT
public:
    explicit UpdatesPage(ConfigManager *configManager, QWidget *parent = nullptr);
public slots:
    void loadSettings() {}
    void setYtDlpVersion(const QString &version);
    void setGalleryDlVersion(const QString &version);
private:
    ConfigManager *m_configManager;
    QLabel *m_ytDlpVersionLabel;
    QPushButton *m_updateYtDlpButton;
    QLabel *m_galleryDlVersionLabel;
    QPushButton *m_updateGalleryDlButton;
};