#include "DownloadOptionsPage.h"
#include "core/ConfigManager.h"
#include "core/ProcessUtils.h"
#include "ui/ToggleSwitch.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSignalBlocker>

DownloadOptionsPage::DownloadOptionsPage(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *downloadOptionsGroup = new QGroupBox("Download Options", this);
    QFormLayout *downloadOptionsLayout = new QFormLayout(downloadOptionsGroup);

    m_externalDownloaderCombo = new QComboBox(this);
    m_externalDownloaderCombo->addItem("yt-dlp (default)", "ytdlp");
    m_externalDownloaderCombo->addItem("aria2c", "aria2c");
    m_externalDownloaderCombo->setToolTip("Choose the downloader to use for downloads.\n"
                                          "yt-dlp: Default downloader built into yt-dlp.\n"
                                          "aria2c: External downloader for faster multi-connection downloads.");
    
    m_sponsorBlockCheck = new ToggleSwitch(this);
    m_embedChaptersCheck = new ToggleSwitch(this);
    m_splitChaptersCheck = new ToggleSwitch(this);
    m_downloadSectionsCheck = new ToggleSwitch(this);
    m_singleLineCommandPreviewCheck = new ToggleSwitch(this);
    m_restrictFilenamesCheck = new ToggleSwitch(this);
    m_autoClearCompletedCheck = new ToggleSwitch(this);
    m_autoPasteModeCombo = new QComboBox(this);
    m_autoPasteModeCombo->addItems({
        "Disabled",
        "Auto-paste on app focus (no enqueue)",
        "Auto-paste on new URL in clipboard (no enqueue)",
        "Auto-paste & enqueue on app focus",
        "Auto-paste & enqueue on new URL in clipboard"
    });
    m_autoPasteModeCombo->setToolTip("Controls when URLs are auto-pasted from clipboard.\n"
                                      "Enqueue modes will only add NEW URLs (duplicates are prevented).");

    m_geoProxyInput = new QLineEdit(this);
    m_geoProxyInput->setPlaceholderText("e.g., http://proxy.server:port");

    downloadOptionsLayout->addRow("External Downloader:", m_externalDownloaderCombo);
    downloadOptionsLayout->addRow("Enable SponsorBlock", m_sponsorBlockCheck);
    downloadOptionsLayout->addRow("Embed video chapters", m_embedChaptersCheck);
    downloadOptionsLayout->addRow("Split chapters into separate files", m_splitChaptersCheck);
    downloadOptionsLayout->addRow("Enable Download Sections", m_downloadSectionsCheck);
    downloadOptionsLayout->addRow("Auto-clear completed downloads", m_autoClearCompletedCheck);
    downloadOptionsLayout->addRow("Auto-paste URL behavior:", m_autoPasteModeCombo);
    downloadOptionsLayout->addRow("Single-line command preview", m_singleLineCommandPreviewCheck);
    downloadOptionsLayout->addRow("Restrict filenames", m_restrictFilenamesCheck);
    downloadOptionsLayout->addRow("Geo-verification proxy:", m_geoProxyInput);

    layout->addWidget(downloadOptionsGroup);
    layout->addStretch();

    connect(m_externalDownloaderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DownloadOptionsPage::onExternalDownloaderChanged);
    connect(m_sponsorBlockCheck, &ToggleSwitch::toggled, this, &DownloadOptionsPage::onSponsorBlockToggled);
    connect(m_embedChaptersCheck, &ToggleSwitch::toggled, this, &DownloadOptionsPage::onEmbedChaptersToggled);
    connect(m_splitChaptersCheck, &ToggleSwitch::toggled, this, &DownloadOptionsPage::onSplitChaptersToggled);
    connect(m_downloadSectionsCheck, &ToggleSwitch::toggled, this, &DownloadOptionsPage::onDownloadSectionsToggled);
    connect(m_autoPasteModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DownloadOptionsPage::onAutoPasteModeChanged);
    connect(m_singleLineCommandPreviewCheck, &ToggleSwitch::toggled, this, &DownloadOptionsPage::onSingleLineCommandPreviewToggled);
    connect(m_restrictFilenamesCheck, &ToggleSwitch::toggled, this, &DownloadOptionsPage::onRestrictFilenamesToggled);
    connect(m_autoClearCompletedCheck, &ToggleSwitch::toggled, this, &DownloadOptionsPage::onAutoClearCompletedToggled);
    connect(m_geoProxyInput, &QLineEdit::editingFinished, this, &DownloadOptionsPage::onGeoProxyChanged);
    connect(m_configManager, &ConfigManager::settingChanged, this, &DownloadOptionsPage::handleConfigSettingChanged);
}

void DownloadOptionsPage::loadSettings() {
    QSignalBlocker b1(m_externalDownloaderCombo);
    QSignalBlocker b2(m_sponsorBlockCheck);
    QSignalBlocker b3(m_embedChaptersCheck);
    QSignalBlocker b4(m_autoPasteModeCombo);
    QSignalBlocker b5(m_singleLineCommandPreviewCheck);
    QSignalBlocker b6(m_restrictFilenamesCheck);
    QSignalBlocker b7(m_splitChaptersCheck);
    QSignalBlocker b8(m_geoProxyInput);
    QSignalBlocker b9(m_autoClearCompletedCheck);    
    QSignalBlocker b10(m_downloadSectionsCheck);

    ProcessUtils::FoundBinary aria2Binary = ProcessUtils::findBinary("aria2c", m_configManager);
    bool aria2Available = (aria2Binary.source != "Not Found" && aria2Binary.source != "Invalid Custom" && !aria2Binary.path.isEmpty());
    
    bool useAria2c = m_configManager->get("Metadata", "use_aria2c", false).toBool();
    if (!aria2Available) {
        m_externalDownloaderCombo->setVisible(false);
        m_externalDownloaderCombo->setCurrentIndex(0); // yt-dlp
        if (useAria2c) {
            m_configManager->set("Metadata", "use_aria2c", false); // Auto-correct stale config
        }
    } else {
        m_externalDownloaderCombo->setVisible(true);
        m_externalDownloaderCombo->setCurrentIndex(useAria2c ? 1 : 0);
    }
    m_sponsorBlockCheck->setChecked(m_configManager->get("General", "sponsorblock", false).toBool());
    m_embedChaptersCheck->setChecked(m_configManager->get("Metadata", "embed_chapters", true).toBool());
    m_splitChaptersCheck->setChecked(m_configManager->get("DownloadOptions", "split_chapters", false).toBool());
    m_downloadSectionsCheck->setChecked(m_configManager->get("DownloadOptions", "download_sections_enabled", false).toBool());
    m_autoClearCompletedCheck->setChecked(m_configManager->get("DownloadOptions", "auto_clear_completed", false).toBool());
    m_autoPasteModeCombo->setCurrentIndex(m_configManager->get("General", "auto_paste_mode", 0).toInt());
    m_singleLineCommandPreviewCheck->setChecked(m_configManager->get("General", "single_line_preview", false).toBool());
    m_restrictFilenamesCheck->setChecked(m_configManager->get("General", "restrict_filenames", false).toBool());
    m_geoProxyInput->setText(m_configManager->get("DownloadOptions", "geo_verification_proxy", "").toString());
}

void DownloadOptionsPage::onExternalDownloaderChanged(int index) {
    bool useAria2c = (index == 1);
    m_configManager->set("Metadata", "use_aria2c", useAria2c);
}
void DownloadOptionsPage::onSponsorBlockToggled(bool c) { m_configManager->set("General", "sponsorblock", c); }
void DownloadOptionsPage::onEmbedChaptersToggled(bool c) { m_configManager->set("Metadata", "embed_chapters", c); }
void DownloadOptionsPage::onSplitChaptersToggled(bool c) { m_configManager->set("DownloadOptions", "split_chapters", c); }
void DownloadOptionsPage::onDownloadSectionsToggled(bool c) { m_configManager->set("DownloadOptions", "download_sections_enabled", c); }
void DownloadOptionsPage::onAutoPasteModeChanged(int index) { m_configManager->set("General", "auto_paste_mode", index); }
void DownloadOptionsPage::onSingleLineCommandPreviewToggled(bool c) { m_configManager->set("General", "single_line_preview", c); }
void DownloadOptionsPage::onRestrictFilenamesToggled(bool c) { m_configManager->set("General", "restrict_filenames", c); }
void DownloadOptionsPage::onAutoClearCompletedToggled(bool c) { m_configManager->set("DownloadOptions", "auto_clear_completed", c); }
void DownloadOptionsPage::onGeoProxyChanged() { m_configManager->set("DownloadOptions", "geo_verification_proxy", m_geoProxyInput->text()); }

void DownloadOptionsPage::handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value) {
    if (section == "General") {
        if (key == "sponsorblock") m_sponsorBlockCheck->setChecked(value.toBool());
        else if (key == "auto_paste_mode") m_autoPasteModeCombo->setCurrentIndex(value.toInt());
        else if (key == "single_line_preview") m_singleLineCommandPreviewCheck->setChecked(value.toBool());
        else if (key == "restrict_filenames") m_restrictFilenamesCheck->setChecked(value.toBool());
    } else if (section == "Metadata") {
        if (key == "use_aria2c") {
            bool useAria2c = value.toBool();
            m_externalDownloaderCombo->setCurrentIndex(useAria2c ? 1 : 0);
        }
        else if (key == "embed_chapters") m_embedChaptersCheck->setChecked(value.toBool());
    } else if (section == "DownloadOptions") {
        if (key == "split_chapters") m_splitChaptersCheck->setChecked(value.toBool());
        else if (key == "auto_clear_completed") m_autoClearCompletedCheck->setChecked(value.toBool());
        else if (key == "download_sections_enabled") m_downloadSectionsCheck->setChecked(value.toBool());
        else if (key == "geo_verification_proxy") m_geoProxyInput->setText(value.toString());
    }
}