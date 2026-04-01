#include "MetadataPage.h"
#include "core/ConfigManager.h"
#include "ui/ToggleSwitch.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QSignalBlocker>

MetadataPage::MetadataPage(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *metadataGroup = new QGroupBox("Metadata / Thumbnails", this);
    QFormLayout *metadataLayout = new QFormLayout(metadataGroup);

    m_embedMetadataCheck = new ToggleSwitch(this);
    m_embedThumbnailCheck = new ToggleSwitch(this);
    m_highQualityThumbnailCheck = new ToggleSwitch(this);
    m_cropThumbnailCheck = new ToggleSwitch(this);
    m_generateFolderJpgCheck = new ToggleSwitch(this);
    m_convertThumbnailsCombo = new QComboBox(this);
    m_convertThumbnailsCombo->addItems({"None", "jpg", "png"});

    metadataLayout->addRow("Embed metadata", m_embedMetadataCheck);
    metadataLayout->addRow("Embed thumbnail", m_embedThumbnailCheck);
    metadataLayout->addRow("Use high-quality thumbnail converter", m_highQualityThumbnailCheck);
    metadataLayout->addRow("Crop audio thumbnails to square", m_cropThumbnailCheck);
    metadataLayout->addRow("Generate folder.jpg for audio playlists", m_generateFolderJpgCheck);
    metadataLayout->addRow("Convert thumbnails to:", m_convertThumbnailsCombo);

    layout->addWidget(metadataGroup);
    layout->addStretch();

    connect(m_embedMetadataCheck, &ToggleSwitch::toggled, this, &MetadataPage::onEmbedMetadataToggled);
    connect(m_embedThumbnailCheck, &ToggleSwitch::toggled, this, &MetadataPage::onEmbedThumbnailToggled);
    connect(m_highQualityThumbnailCheck, &ToggleSwitch::toggled, this, &MetadataPage::onHighQualityThumbnailToggled);
    connect(m_cropThumbnailCheck, &ToggleSwitch::toggled, this, &MetadataPage::onCropThumbnailToggled);
    connect(m_generateFolderJpgCheck, &ToggleSwitch::toggled, this, &MetadataPage::onGenerateFolderJpgToggled);
    connect(m_convertThumbnailsCombo, &QComboBox::currentTextChanged, this, &MetadataPage::onConvertThumbnailsChanged);
    connect(m_configManager, &ConfigManager::settingChanged, this, &MetadataPage::handleConfigSettingChanged);
}

void MetadataPage::loadSettings() {
    QSignalBlocker b1(m_embedMetadataCheck);
    QSignalBlocker b2(m_embedThumbnailCheck);
    QSignalBlocker b3(m_highQualityThumbnailCheck);
    QSignalBlocker b4(m_convertThumbnailsCombo);
    QSignalBlocker b5(m_cropThumbnailCheck);
    QSignalBlocker b6(m_generateFolderJpgCheck);

    m_embedMetadataCheck->setChecked(m_configManager->get("Metadata", "embed_metadata", true).toBool());
    m_embedThumbnailCheck->setChecked(m_configManager->get("Metadata", "embed_thumbnail", true).toBool());
    m_highQualityThumbnailCheck->setChecked(m_configManager->get("Metadata", "high_quality_thumbnail", true).toBool());
    m_cropThumbnailCheck->setChecked(m_configManager->get("Metadata", "crop_artwork_to_square", true).toBool());
    m_generateFolderJpgCheck->setChecked(m_configManager->get("Metadata", "generate_folder_jpg", false).toBool());
    m_convertThumbnailsCombo->setCurrentText(m_configManager->get("Metadata", "convert_thumbnail_to", "jpg").toString());
}
void MetadataPage::onEmbedMetadataToggled(bool c) { m_configManager->set("Metadata", "embed_metadata", c); }
void MetadataPage::onEmbedThumbnailToggled(bool c) { m_configManager->set("Metadata", "embed_thumbnail", c); }
void MetadataPage::onHighQualityThumbnailToggled(bool c) { m_configManager->set("Metadata", "high_quality_thumbnail", c); }
void MetadataPage::onCropThumbnailToggled(bool c) { m_configManager->set("Metadata", "crop_artwork_to_square", c); }
void MetadataPage::onGenerateFolderJpgToggled(bool c) { m_configManager->set("Metadata", "generate_folder_jpg", c); }
void MetadataPage::onConvertThumbnailsChanged(const QString &text) { m_configManager->set("Metadata", "convert_thumbnail_to", text); }
void MetadataPage::handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value) { /* Handled seamlessly through toggles mapping */ }