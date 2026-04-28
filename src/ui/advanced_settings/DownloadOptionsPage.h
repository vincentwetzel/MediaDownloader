#pragma once
#include <QWidget>
#include <QVariant>

class ConfigManager;
class ToggleSwitch;
class QComboBox;
class QLineEdit;
class QProcess;

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
    void onFfmpegCutEncoderChanged(int index);
    void onFfmpegCutCustomArgsChanged();
    void onAutoPasteModeChanged(int index);
    void onSingleLineCommandPreviewToggled(bool checked);
    void onRestrictFilenamesToggled(bool checked);
    void onGeoProxyChanged();
    void onAutoClearCompletedToggled(bool checked);
    void handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value);
private:
    void populateFfmpegCutEncoderCombo(const QStringList &visibleEncoderIds = {});
    void startHardwareEncoderProbe();
    void maybeApplyHardwareEncoderProbe();
    ConfigManager *m_configManager;
    QComboBox *m_externalDownloaderCombo;
    ToggleSwitch *m_sponsorBlockCheck, *m_embedChaptersCheck, *m_splitChaptersCheck, *m_downloadSectionsCheck, *m_singleLineCommandPreviewCheck, *m_restrictFilenamesCheck, *m_prefixPlaylistIndicesCheck, *m_autoClearCompletedCheck;
    QComboBox *m_ffmpegCutEncoderCombo;
    QLineEdit *m_ffmpegCutCustomArgsInput;
    QComboBox *m_autoPasteModeCombo;
    QLineEdit *m_geoProxyInput;
    QProcess *m_ffmpegEncoderProbe;
    QProcess *m_gpuProbe;
    QString m_ffmpegEncoderProbeOutput;
    QString m_gpuProbeOutput;
    bool m_ffmpegEncoderProbeFinished;
    bool m_gpuProbeFinished;
};
