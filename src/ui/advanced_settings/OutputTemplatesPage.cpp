#include "OutputTemplatesPage.h"
#include "core/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QMessageBox>
#include <QProcess>
#include <QSignalBlocker>

OutputTemplatesPage::OutputTemplatesPage(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *ytDlpGroup = new QGroupBox("yt-dlp Filename Template", this);
    QHBoxLayout *ytDlpControlsLayout = new QHBoxLayout(ytDlpGroup);
    ytDlpControlsLayout->addWidget(new QLabel("Filename Pattern:"));
    m_ytDlpOutputTemplateInput = new QLineEdit(this);
    ytDlpControlsLayout->addWidget(m_ytDlpOutputTemplateInput);
    m_ytDlpTemplateTokensCombo = new QComboBox(this);
    m_ytDlpTemplateTokensCombo->addItem("Insert token...", "");
    m_ytDlpTemplateTokensCombo->addItems({"%(title)s", "%(uploader)s", "%(upload_date>%Y-%m-%d)s", "%(id)s", "%(ext)s", "%(playlist_index)s", "%(playlist_title)s"});
    ytDlpControlsLayout->addWidget(m_ytDlpTemplateTokensCombo);
    m_saveYtDlpTemplateButton = new QPushButton("Save", this);
    ytDlpControlsLayout->addWidget(m_saveYtDlpTemplateButton);
    QPushButton *resetYtDlpButton = new QPushButton("Reset", this);
    ytDlpControlsLayout->addWidget(resetYtDlpButton);
    layout->addWidget(ytDlpGroup);

    QGroupBox *galleryDlGroup = new QGroupBox("gallery-dl Filename Template", this);
    QHBoxLayout *galleryDlControlsLayout = new QHBoxLayout(galleryDlGroup);
    galleryDlControlsLayout->addWidget(new QLabel("Filename Pattern:"));
    m_galleryDlOutputTemplateInput = new QLineEdit(this);
    galleryDlControlsLayout->addWidget(m_galleryDlOutputTemplateInput);
    m_galleryDlTemplateTokensCombo = new QComboBox(this);
    m_galleryDlTemplateTokensCombo->addItem("Insert token...", "");
    m_galleryDlTemplateTokensCombo->addItems({"{filename}.{extension}", "{id}", "{author[name]}", "{date:%Y-%m-%d}"});
    galleryDlControlsLayout->addWidget(m_galleryDlTemplateTokensCombo);
    m_saveGalleryDlTemplateButton = new QPushButton("Save", this);
    galleryDlControlsLayout->addWidget(m_saveGalleryDlTemplateButton);
    QPushButton *resetGalleryDlButton = new QPushButton("Reset", this);
    galleryDlControlsLayout->addWidget(resetGalleryDlButton);
    layout->addWidget(galleryDlGroup);

    layout->addStretch();

    connect(resetYtDlpButton, &QPushButton::clicked, this, [this]() {
        m_ytDlpOutputTemplateInput->setText(m_configManager->getDefault("General", "output_template").toString());
        QMessageBox::information(this, "Template Reset", "Filename pattern has been reset to default.");
    });
    connect(resetGalleryDlButton, &QPushButton::clicked, this, [this]() {
        m_galleryDlOutputTemplateInput->setText(m_configManager->getDefault("General", "gallery_output_template").toString());
        m_configManager->set("General", "gallery_output_template", m_galleryDlOutputTemplateInput->text());
        QMessageBox::information(this, "Template Reset", "Filename pattern has been reset to default.");
    });

    connect(m_saveYtDlpTemplateButton, &QPushButton::clicked, this, &OutputTemplatesPage::validateAndSaveYtDlpTemplate);
    connect(m_ytDlpTemplateTokensCombo, QOverload<int>::of(&QComboBox::activated), this, &OutputTemplatesPage::insertYtDlpTemplateToken);
    connect(m_saveGalleryDlTemplateButton, &QPushButton::clicked, this, &OutputTemplatesPage::validateAndSaveGalleryDlTemplate);
    connect(m_galleryDlTemplateTokensCombo, QOverload<int>::of(&QComboBox::activated), this, &OutputTemplatesPage::insertGalleryDlTemplateToken);
    connect(m_configManager, &ConfigManager::settingChanged, this, &OutputTemplatesPage::handleConfigSettingChanged);
}

void OutputTemplatesPage::loadSettings() {
    QSignalBlocker b1(m_ytDlpOutputTemplateInput);
    QSignalBlocker b2(m_galleryDlOutputTemplateInput);

    m_ytDlpOutputTemplateInput->setText(m_configManager->get("General", "output_template").toString());
    m_galleryDlOutputTemplateInput->setText(m_configManager->get("General", "gallery_output_template").toString());
}

void OutputTemplatesPage::validateAndSaveYtDlpTemplate() {
    QString templateStr = m_ytDlpOutputTemplateInput->text();
    if (templateStr.isEmpty()) { QMessageBox::warning(this, "Invalid Template", "Template cannot be empty."); return; }
    m_configManager->set("General", "output_template", templateStr);
    QMessageBox::information(this, "Saved", "Output filename pattern saved.");
}

void OutputTemplatesPage::validateAndSaveGalleryDlTemplate() {
    QString templateStr = m_galleryDlOutputTemplateInput->text();
    if (templateStr.isEmpty()) { QMessageBox::warning(this, "Invalid Template", "Template cannot be empty."); return; }
    m_configManager->set("General", "gallery_output_template", templateStr);
    QMessageBox::information(this, "Saved", "Output filename pattern saved.");
}

void OutputTemplatesPage::insertYtDlpTemplateToken(int index) { if (index > 0) m_ytDlpOutputTemplateInput->insert(m_ytDlpTemplateTokensCombo->itemText(index)); m_ytDlpTemplateTokensCombo->setCurrentIndex(0); }
void OutputTemplatesPage::insertGalleryDlTemplateToken(int index) { if (index > 0) m_galleryDlOutputTemplateInput->insert(m_galleryDlTemplateTokensCombo->itemText(index)); m_galleryDlTemplateTokensCombo->setCurrentIndex(0); }
void OutputTemplatesPage::handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value) {
    if (section == "General" && key == "output_template") m_ytDlpOutputTemplateInput->setText(value.toString());
    else if (section == "General" && key == "gallery_output_template") m_galleryDlOutputTemplateInput->setText(value.toString());
}