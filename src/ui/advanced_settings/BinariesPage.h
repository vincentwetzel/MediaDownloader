#pragma once
#include <QWidget>
#include <QVariant>

class ConfigManager;
class QLineEdit;
class QPushButton;
class QFormLayout;

class BinariesPage : public QWidget {
    Q_OBJECT
public:
    explicit BinariesPage(ConfigManager *configManager, QWidget *parent = nullptr);
public slots:
    void loadSettings();
private slots:
    void browseYtDlp();
    void browseFfmpeg();
    void browseFfprobe();
    void browseGalleryDl();
    void browseAria2c();
    void handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value);
private:
    void setupRow(const QString &labelText, QLineEdit *&input, QPushButton *&btn, QFormLayout *layout);
    QString browseBinary(const QString &title);

    ConfigManager *m_configManager;
    QLineEdit *m_ytDlpInput;      QPushButton *m_ytDlpBtn;
    QLineEdit *m_ffmpegInput;     QPushButton *m_ffmpegBtn;
    QLineEdit *m_ffprobeInput;    QPushButton *m_ffprobeBtn;
    QLineEdit *m_galleryDlInput;  QPushButton *m_galleryDlBtn;
    QLineEdit *m_aria2cInput;     QPushButton *m_aria2cBtn;
};