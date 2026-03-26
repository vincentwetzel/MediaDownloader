#pragma once
#include <QWidget>
#include <QVariant>

class ConfigManager;
class ToggleSwitch;
class QComboBox;

class DownloadOptionsPage : public QWidget {
    Q_OBJECT
public:
    explicit DownloadOptionsPage(ConfigManager *configManager, QWidget *parent = nullptr);
public slots:
    void loadSettings();
private slots:
    void onExternalDownloaderToggled(bool checked);
    void onSponsorBlockToggled(bool checked);
    void onEmbedChaptersToggled(bool checked);
    void onAutoPasteModeChanged(int index);
    void onSingleLineCommandPreviewToggled(bool checked);
    void onRestrictFilenamesToggled(bool checked);
    void handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value);
private:
    ConfigManager *m_configManager;
    ToggleSwitch *m_externalDownloaderCheck, *m_sponsorBlockCheck, *m_embedChaptersCheck, *m_singleLineCommandPreviewCheck, *m_restrictFilenamesCheck;
    QComboBox *m_autoPasteModeCombo;
};