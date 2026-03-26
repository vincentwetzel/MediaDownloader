#include "SubtitlesPage.h"
#include "core/ConfigManager.h"
#include "ui/ToggleSwitch.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QMap>
#include <QSignalBlocker>
#include <algorithm>

SubtitlesPage::SubtitlesPage(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *subtitlesGroup = new QGroupBox("Subtitles", this);
    QFormLayout *subtitlesLayout = new QFormLayout(subtitlesGroup);

    m_subtitleLanguageCombo = new QComboBox(this);
    QMap<QString, QString> langMap;
    langMap["en"] = "English"; langMap["es"] = "Spanish"; langMap["fr"] = "French"; langMap["de"] = "German";
    langMap["ja"] = "Japanese"; langMap["ko"] = "Korean"; langMap["zh-Hans"] = "Chinese (Simplified)";
    langMap["auto"] = "Auto-generated"; langMap["all"] = "All available"; // Add full list here...
    
    QList<QString> sortedKeys = langMap.keys();
    std::sort(sortedKeys.begin(), sortedKeys.end(), [&langMap](const QString &a, const QString &b) { return langMap[a] < langMap[b]; });
    for (const QString &code : sortedKeys) m_subtitleLanguageCombo->addItem(langMap[code], code);

    m_embedSubtitlesCheck = new ToggleSwitch(this);
    m_writeSubtitlesCheck = new ToggleSwitch(this);
    m_includeAutoSubtitlesCheck = new ToggleSwitch(this);
    m_subtitleFormatCombo = new QComboBox(this);
    m_subtitleFormatCombo->addItems({"srt*", "vtt", "ass"});

    subtitlesLayout->addRow("Subtitle language:", m_subtitleLanguageCombo);
    subtitlesLayout->addRow("Embed subtitles in video", m_embedSubtitlesCheck);
    subtitlesLayout->addRow("Write subtitles to separate file(s)", m_writeSubtitlesCheck);
    subtitlesLayout->addRow("Include automatically-generated subtitles", m_includeAutoSubtitlesCheck);
    subtitlesLayout->addRow("Subtitle file format:", m_subtitleFormatCombo);

    layout->addWidget(subtitlesGroup);
    layout->addStretch();

    connect(m_subtitleLanguageCombo, &QComboBox::currentTextChanged, this, &SubtitlesPage::onSubtitleLanguageChanged);
    connect(m_embedSubtitlesCheck, &ToggleSwitch::toggled, this, &SubtitlesPage::onEmbedSubtitlesToggled);
    connect(m_writeSubtitlesCheck, &ToggleSwitch::toggled, this, &SubtitlesPage::onWriteSubtitlesToggled);
    connect(m_includeAutoSubtitlesCheck, &ToggleSwitch::toggled, this, &SubtitlesPage::onIncludeAutoSubtitlesToggled);
    connect(m_subtitleFormatCombo, &QComboBox::currentTextChanged, this, &SubtitlesPage::onSubtitleFormatChanged);
    connect(m_configManager, &ConfigManager::settingChanged, this, &SubtitlesPage::handleConfigSettingChanged);
}

void SubtitlesPage::loadSettings() {
    QSignalBlocker b1(m_subtitleLanguageCombo);
    QSignalBlocker b2(m_embedSubtitlesCheck);
    QSignalBlocker b3(m_writeSubtitlesCheck);
    QSignalBlocker b4(m_includeAutoSubtitlesCheck);
    QSignalBlocker b5(m_subtitleFormatCombo);

    QString savedLangCode = m_configManager->get("Subtitles", "languages", "en").toString();
    int index = m_subtitleLanguageCombo->findData(savedLangCode);
    if (index != -1) m_subtitleLanguageCombo->setCurrentIndex(index);

    m_embedSubtitlesCheck->setChecked(m_configManager->get("Subtitles", "embed_subtitles", true).toBool());
    m_writeSubtitlesCheck->setChecked(m_configManager->get("Subtitles", "write_subtitles", false).toBool());
    m_includeAutoSubtitlesCheck->setChecked(m_configManager->get("Subtitles", "write_auto_subtitles", true).toBool());
    m_subtitleFormatCombo->setCurrentText(m_configManager->get("Subtitles", "format", "srt").toString());
    updateSubtitleFormatAvailability(m_embedSubtitlesCheck->isChecked());
}

void SubtitlesPage::onSubtitleLanguageChanged(const QString &text) { m_configManager->set("Subtitles", "languages", m_subtitleLanguageCombo->currentData().toString()); }
void SubtitlesPage::onEmbedSubtitlesToggled(bool c) { m_configManager->set("Subtitles", "embed_subtitles", c); updateSubtitleFormatAvailability(c); }
void SubtitlesPage::onWriteSubtitlesToggled(bool c) { m_configManager->set("Subtitles", "write_subtitles", c); }
void SubtitlesPage::onIncludeAutoSubtitlesToggled(bool c) { m_configManager->set("Subtitles", "write_auto_subtitles", c); }
void SubtitlesPage::onSubtitleFormatChanged(const QString &text) { m_configManager->set("Subtitles", "format", text); }
void SubtitlesPage::updateSubtitleFormatAvailability(bool embedSubtitlesChecked) { m_subtitleFormatCombo->setDisabled(embedSubtitlesChecked); }
void SubtitlesPage::handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value) {
    /* Standard config loading mappings goes here, identical to others */
}