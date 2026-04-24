#pragma once
#include <QWidget>
#include <QVariant>

class ConfigManager;
class ToggleSwitch;
class QComboBox;

class MetadataPage : public QWidget {
    Q_OBJECT
public:
    explicit MetadataPage(ConfigManager *configManager, QWidget *parent = nullptr);
public slots:
    void loadSettings();
private slots:
    void onEmbedMetadataToggled(bool checked);
    void onEmbedThumbnailToggled(bool checked);
    void onHighQualityThumbnailToggled(bool checked);
    void onCropThumbnailToggled(bool checked);
    void onGenerateFolderJpgToggled(bool checked);
    void onForcePlaylistAsAlbumToggled(bool checked);
    void onConvertThumbnailsChanged(const QString &text);
    void handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value);
private:
    ConfigManager *m_configManager;
    ToggleSwitch *m_embedMetadataCheck, *m_embedThumbnailCheck, *m_highQualityThumbnailCheck, *m_cropThumbnailCheck, *m_generateFolderJpgCheck, *m_forcePlaylistAsAlbumSwitch;
    QComboBox *m_convertThumbnailsCombo;
};