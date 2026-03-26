#pragma once
#include <QWidget>
#include <QVariant>

class ConfigManager;
class QComboBox;
class QLabel;
class QFormLayout;

class VideoSettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit VideoSettingsPage(ConfigManager *configManager, QWidget *parent = nullptr);
public slots:
    void loadSettings();
private slots:
    void onVideoQualityChanged(const QString &text);
    void onVideoCodecChanged(const QString &text);
    void onVideoExtChanged(const QString &text);
    void onVideoAudioCodecChanged(const QString &text);
    void handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value);
private:
    void updateVideoOptions();
    ConfigManager *m_configManager;
    QComboBox *m_videoQualityCombo;
    QComboBox *m_videoCodecCombo;
    QLabel *m_videoExtLabel;
    QComboBox *m_videoExtCombo;
    QComboBox *m_videoAudioCodecCombo;
};