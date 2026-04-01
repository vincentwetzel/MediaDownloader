#pragma once
#include <QWidget>
#include <QVariant>

class ConfigManager;
class QComboBox;
class QLabel;

class AudioSettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit AudioSettingsPage(ConfigManager *configManager, QWidget *parent = nullptr);
public slots:
    void loadSettings();
private slots:
    void onAudioQualityChanged(const QString &text);
    void onAudioCodecChanged(const QString &text);
    void onAudioExtChanged(const QString &text);
    void handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value);
private:
    void updateAudioOptions();
    bool isRuntimeSelectionMode() const;
    ConfigManager *m_configManager;
    QComboBox *m_audioQualityCombo;
    QLabel *m_audioCodecLabel;
    QComboBox *m_audioCodecCombo;
    QLabel *m_audioExtLabel;
    QComboBox *m_audioExtCombo;
    QLabel *m_runtimeHintLabel;
};
