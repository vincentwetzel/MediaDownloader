#pragma once
#include <QWidget>
#include <QVariant>

class ConfigManager;
class ToggleSwitch;
class QComboBox;
class QLineEdit;

class DownloadOptionsPage : public QWidget {
    Q_OBJECT
public:
    explicit DownloadOptionsPage(ConfigManager *configManager, QWidget *parent = nullptr);
public slots:
    void loadSettings();
private slots:
    void onExternalDownloaderChanged(int index);
    void onSponsorBlockToggled(bool checked);
    void onEmbedChaptersToggled(bool checked);
    void onSplitChaptersToggled(bool checked);
    void onDownloadSectionsToggled(bool checked);
    void onAutoPasteModeChanged(int index);
    void onSingleLineCommandPreviewToggled(bool checked);
    void onRestrictFilenamesToggled(bool checked);
    void onGeoProxyChanged();
    void onAutoClearCompletedToggled(bool checked);
    void handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value);
private:
    ConfigManager *m_configManager;
    QComboBox *m_externalDownloaderCombo;
    ToggleSwitch *m_sponsorBlockCheck, *m_embedChaptersCheck, *m_splitChaptersCheck, *m_downloadSectionsCheck, *m_singleLineCommandPreviewCheck, *m_restrictFilenamesCheck, *m_autoClearCompletedCheck;
    QComboBox *m_autoPasteModeCombo;
    QLineEdit *m_geoProxyInput;
};