#include "OutputTemplatesPage.h"
#include "core/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QMessageBox>
#include <QProcess>
#include <QSignalBlocker>
#include "core/ProcessUtils.h"

OutputTemplatesPage::OutputTemplatesPage(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QStringList ytDlpTokens = {
        "%(title)s", "%(uploader)s", "%(upload_date>%Y-%m-%d)s", "%(id)s", "%(ext)s",
        "%(playlist_index)s", "%(playlist_title)s", "%(channel)s", "%(channel_id)s",
        "%(duration)s", "%(view_count)s", "%(like_count)s", "%(comment_count)s",
        "%(age_limit)s", "%(genre)s", "%(format_id)s", "%(format)s", "%(format_note)s",
        "%(resolution)s", "%(width)s", "%(height)s", "%(fps)s", "%(vcodec)s", "%(acodec)s",
        "%(abr)s", "%(vbr)s", "%(tbr)s", "%(filesize)s", "%(epoch)s", "%(autonumber)s"
    };

    QGroupBox *ytDlpGroup = new QGroupBox("yt-dlp Filename Templates", this);
    QGridLayout *ytDlpLayout = new QGridLayout(ytDlpGroup);

    ytDlpLayout->addWidget(new QLabel("Video Pattern:"), 0, 0);
    m_videoOutputTemplateInput = new QLineEdit(this);
    ytDlpLayout->addWidget(m_videoOutputTemplateInput, 0, 1);
    m_videoTemplateTokensCombo = new QComboBox(this);
    m_videoTemplateTokensCombo->addItem("Insert token...", "");
    m_videoTemplateTokensCombo->addItems(ytDlpTokens);
    ytDlpLayout->addWidget(m_videoTemplateTokensCombo, 0, 2);
    m_saveVideoTemplateButton = new QPushButton("Save", this);
    ytDlpLayout->addWidget(m_saveVideoTemplateButton, 0, 3);
    QPushButton *resetVideoButton = new QPushButton("Reset", this);
    ytDlpLayout->addWidget(resetVideoButton, 0, 4);

    ytDlpLayout->addWidget(new QLabel("Audio Pattern:"), 1, 0);
    m_audioOutputTemplateInput = new QLineEdit(this);
    ytDlpLayout->addWidget(m_audioOutputTemplateInput, 1, 1);
    m_audioTemplateTokensCombo = new QComboBox(this);
    m_audioTemplateTokensCombo->addItem("Insert token...", "");
    m_audioTemplateTokensCombo->addItems(ytDlpTokens);
    ytDlpLayout->addWidget(m_audioTemplateTokensCombo, 1, 2);
    m_saveAudioTemplateButton = new QPushButton("Save", this);
    ytDlpLayout->addWidget(m_saveAudioTemplateButton, 1, 3);
    QPushButton *resetAudioButton = new QPushButton("Reset", this);
    ytDlpLayout->addWidget(resetAudioButton, 1, 4);
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

    connect(resetVideoButton, &QPushButton::clicked, this, [this]() {
        m_videoOutputTemplateInput->setText(m_configManager->getDefault("General", "output_template").toString());
        QMessageBox::information(this, "Template Reset", "Video filename pattern has been reset to default.");
    });
    connect(resetAudioButton, &QPushButton::clicked, this, [this]() {
        m_audioOutputTemplateInput->setText(m_configManager->getDefault("General", "output_template").toString());
        QMessageBox::information(this, "Template Reset", "Audio filename pattern has been reset to default.");
    });
    connect(resetGalleryDlButton, &QPushButton::clicked, this, [this]() {
        m_galleryDlOutputTemplateInput->setText(m_configManager->getDefault("General", "gallery_output_template").toString());
        m_configManager->set("General", "gallery_output_template", m_galleryDlOutputTemplateInput->text());
        QMessageBox::information(this, "Template Reset", "Filename pattern has been reset to default.");
    });

    connect(m_saveVideoTemplateButton, &QPushButton::clicked, this, &OutputTemplatesPage::validateAndSaveVideoTemplate);
    connect(m_videoTemplateTokensCombo, QOverload<int>::of(&QComboBox::activated), this, &OutputTemplatesPage::insertVideoTemplateToken);
    connect(m_saveAudioTemplateButton, &QPushButton::clicked, this, &OutputTemplatesPage::validateAndSaveAudioTemplate);
    connect(m_audioTemplateTokensCombo, QOverload<int>::of(&QComboBox::activated), this, &OutputTemplatesPage::insertAudioTemplateToken);
    connect(m_saveGalleryDlTemplateButton, &QPushButton::clicked, this, &OutputTemplatesPage::validateAndSaveGalleryDlTemplate);
    connect(m_galleryDlTemplateTokensCombo, QOverload<int>::of(&QComboBox::activated), this, &OutputTemplatesPage::insertGalleryDlTemplateToken);
    connect(m_configManager, &ConfigManager::settingChanged, this, &OutputTemplatesPage::handleConfigSettingChanged);
}

void OutputTemplatesPage::loadSettings() {
    QSignalBlocker b1(m_videoOutputTemplateInput);
    QSignalBlocker b2(m_audioOutputTemplateInput);
    QSignalBlocker b3(m_galleryDlOutputTemplateInput);

    QString videoTpl = m_configManager->get("General", "output_template_video").toString();
    if (videoTpl.isEmpty()) videoTpl = m_configManager->get("General", "output_template").toString();
    m_videoOutputTemplateInput->setText(videoTpl);

    QString audioTpl = m_configManager->get("General", "output_template_audio").toString();
    if (audioTpl.isEmpty()) audioTpl = m_configManager->get("General", "output_template").toString();
    m_audioOutputTemplateInput->setText(audioTpl);

    m_galleryDlOutputTemplateInput->setText(m_configManager->get("General", "gallery_output_template").toString());
}

void OutputTemplatesPage::validateAndSaveVideoTemplate() {
    QString templateStr = m_videoOutputTemplateInput->text();
    if (templateStr.isEmpty()) { QMessageBox::warning(this, "Invalid Template", "Template cannot be empty."); return; }

    QProcess process;
    ProcessUtils::setProcessEnvironment(process);
    process.start(ProcessUtils::findBinary("yt-dlp", m_configManager).path, QStringList() << "-o" << templateStr << "dummy:");
    process.waitForFinished(2000);
    QString err = process.readAllStandardError();
    if (err.contains("error:", Qt::CaseInsensitive) && (err.contains("template", Qt::CaseInsensitive) || err.contains("missing", Qt::CaseInsensitive))) {
        QMessageBox::warning(this, "Invalid Template", "yt-dlp rejected the template:\n" + err.trimmed());
        return;
    }

    m_configManager->set("General", "output_template_video", templateStr);
    QMessageBox::information(this, "Saved", "Video output filename pattern saved.");
}

void OutputTemplatesPage::validateAndSaveAudioTemplate() {
    QString templateStr = m_audioOutputTemplateInput->text();
    if (templateStr.isEmpty()) { QMessageBox::warning(this, "Invalid Template", "Template cannot be empty."); return; }

    QProcess process;
    ProcessUtils::setProcessEnvironment(process);
    process.start(ProcessUtils::findBinary("yt-dlp", m_configManager).path, QStringList() << "-o" << templateStr << "dummy:");
    process.waitForFinished(2000);
    QString err = process.readAllStandardError();
    if (err.contains("error:", Qt::CaseInsensitive) && (err.contains("template", Qt::CaseInsensitive) || err.contains("missing", Qt::CaseInsensitive))) {
        QMessageBox::warning(this, "Invalid Template", "yt-dlp rejected the template:\n" + err.trimmed());
        return;
    }

    m_configManager->set("General", "output_template_audio", templateStr);
    QMessageBox::information(this, "Saved", "Audio output filename pattern saved.");
}

void OutputTemplatesPage::validateAndSaveGalleryDlTemplate() {
    QString templateStr = m_galleryDlOutputTemplateInput->text();
    if (templateStr.isEmpty()) { QMessageBox::warning(this, "Invalid Template", "Template cannot be empty."); return; }
    m_configManager->set("General", "gallery_output_template", templateStr);
    QMessageBox::information(this, "Saved", "Output filename pattern saved.");
}

void OutputTemplatesPage::insertVideoTemplateToken(int index) { if (index > 0) m_videoOutputTemplateInput->insert(m_videoTemplateTokensCombo->itemText(index)); m_videoTemplateTokensCombo->setCurrentIndex(0); }
void OutputTemplatesPage::insertAudioTemplateToken(int index) { if (index > 0) m_audioOutputTemplateInput->insert(m_audioTemplateTokensCombo->itemText(index)); m_audioTemplateTokensCombo->setCurrentIndex(0); }
void OutputTemplatesPage::insertGalleryDlTemplateToken(int index) { if (index > 0) m_galleryDlOutputTemplateInput->insert(m_galleryDlTemplateTokensCombo->itemText(index)); m_galleryDlTemplateTokensCombo->setCurrentIndex(0); }
void OutputTemplatesPage::handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value) {
    if (section == "General" && key == "output_template_video") m_videoOutputTemplateInput->setText(value.toString());
    else if (section == "General" && key == "output_template_audio") m_audioOutputTemplateInput->setText(value.toString());
    else if (section == "General" && key == "gallery_output_template") m_galleryDlOutputTemplateInput->setText(value.toString());
}