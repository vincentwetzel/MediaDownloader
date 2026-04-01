#include "BinariesPage.h"
#include "core/ConfigManager.h"
#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QSignalBlocker>
#include "core/ProcessUtils.h"

BinariesPage::BinariesPage(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *group = new QGroupBox("External Binaries Paths", this);
    QFormLayout *formLayout = new QFormLayout(group);

    setupRow("yt-dlp:", m_ytDlpInput, m_ytDlpBtn, formLayout);
    setupRow("ffmpeg:", m_ffmpegInput, m_ffmpegBtn, formLayout);
    setupRow("ffprobe:", m_ffprobeInput, m_ffprobeBtn, formLayout);
    setupRow("gallery-dl:", m_galleryDlInput, m_galleryDlBtn, formLayout);
    setupRow("aria2c:", m_aria2cInput, m_aria2cBtn, formLayout);

    QLabel *infoLabel = new QLabel("Leave blank to use bundled binaries or auto-detect from system PATH.", this);
    infoLabel->setStyleSheet("color: gray; font-style: italic;");
    infoLabel->setWordWrap(true);
    formLayout->addRow(infoLabel);

    layout->addWidget(group);
    layout->addStretch();

    connect(m_ytDlpBtn, &QPushButton::clicked, this, &BinariesPage::browseYtDlp);
    connect(m_ffmpegBtn, &QPushButton::clicked, this, &BinariesPage::browseFfmpeg);
    connect(m_ffprobeBtn, &QPushButton::clicked, this, &BinariesPage::browseFfprobe);
    connect(m_galleryDlBtn, &QPushButton::clicked, this, &BinariesPage::browseGalleryDl);
    connect(m_aria2cBtn, &QPushButton::clicked, this, &BinariesPage::browseAria2c);

    connect(m_ytDlpInput, &QLineEdit::editingFinished, this, [this]() { m_configManager->set("Binaries", "yt_dlp_path", m_ytDlpInput->text()); });
    connect(m_ffmpegInput, &QLineEdit::editingFinished, this, [this]() { m_configManager->set("Binaries", "ffmpeg_path", m_ffmpegInput->text()); });
    connect(m_ffprobeInput, &QLineEdit::editingFinished, this, [this]() { m_configManager->set("Binaries", "ffprobe_path", m_ffprobeInput->text()); });
    connect(m_galleryDlInput, &QLineEdit::editingFinished, this, [this]() { m_configManager->set("Binaries", "gallery_dl_path", m_galleryDlInput->text()); });
    connect(m_aria2cInput, &QLineEdit::editingFinished, this, [this]() { m_configManager->set("Binaries", "aria2c_path", m_aria2cInput->text()); });

    connect(m_configManager, &ConfigManager::settingChanged, this, &BinariesPage::handleConfigSettingChanged);
}

void BinariesPage::setupRow(const QString &labelText, QLineEdit *&input, QPushButton *&btn, QFormLayout *layout) {
    QHBoxLayout *rowLayout = new QHBoxLayout();
    input = new QLineEdit(this);
    input->setPlaceholderText("Auto-detect / Bundled");
    btn = new QPushButton("Browse...", this);
    rowLayout->addWidget(input);
    rowLayout->addWidget(btn);
    layout->addRow(labelText, rowLayout);
}

QString BinariesPage::browseBinary(const QString &title) {
    QString filter;
#ifdef Q_OS_WIN
    filter = "Executables (*.exe);;All Files (*.*)";
#else
    filter = "All Files (*)";
#endif
    return QFileDialog::getOpenFileName(this, title, "", filter);
}

void BinariesPage::browseYtDlp()     { QString p = browseBinary("Select yt-dlp executable");    if(!p.isEmpty()) { m_ytDlpInput->setText(p);     m_configManager->set("Binaries", "yt_dlp_path", p); } }
void BinariesPage::browseFfmpeg()    { QString p = browseBinary("Select ffmpeg executable");    if(!p.isEmpty()) { m_ffmpegInput->setText(p);    m_configManager->set("Binaries", "ffmpeg_path", p); } }
void BinariesPage::browseFfprobe()   { QString p = browseBinary("Select ffprobe executable");   if(!p.isEmpty()) { m_ffprobeInput->setText(p);   m_configManager->set("Binaries", "ffprobe_path", p); } }
void BinariesPage::browseGalleryDl() { QString p = browseBinary("Select gallery-dl executable");if(!p.isEmpty()) { m_galleryDlInput->setText(p); m_configManager->set("Binaries", "gallery_dl_path", p); } }
void BinariesPage::browseAria2c()    { QString p = browseBinary("Select aria2c executable");    if(!p.isEmpty()) { m_aria2cInput->setText(p);    m_configManager->set("Binaries", "aria2c_path", p); } }

void BinariesPage::loadSettings() {
    QSignalBlocker b1(m_ytDlpInput); QSignalBlocker b2(m_ffmpegInput); QSignalBlocker b3(m_ffprobeInput); QSignalBlocker b4(m_galleryDlInput); QSignalBlocker b5(m_aria2cInput);
    m_ytDlpInput->setText(m_configManager->get("Binaries", "yt_dlp_path", "").toString());
    m_ffmpegInput->setText(m_configManager->get("Binaries", "ffmpeg_path", "").toString());
    m_ffprobeInput->setText(m_configManager->get("Binaries", "ffprobe_path", "").toString());
    m_galleryDlInput->setText(m_configManager->get("Binaries", "gallery_dl_path", "").toString());
    m_aria2cInput->setText(m_configManager->get("Binaries", "aria2c_path", "").toString());
    
    auto setPlaceholder = [&](QLineEdit* input, const QString& name) {
        ProcessUtils::FoundBinary binary = ProcessUtils::findBinary(name, m_configManager);
        if (binary.source == "Not Found") {
            input->setPlaceholderText("Not found! Please browse or install.");
        } else {
            input->setPlaceholderText(binary.source + ": " + binary.path);
        }
    };
    setPlaceholder(m_ytDlpInput, "yt-dlp");
    setPlaceholder(m_ffmpegInput, "ffmpeg");
    setPlaceholder(m_ffprobeInput, "ffprobe");
    setPlaceholder(m_galleryDlInput, "gallery-dl");
    setPlaceholder(m_aria2cInput, "aria2c");
}

void BinariesPage::handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value) {
    if (section == "Binaries") {
        if (key == "yt_dlp_path") m_ytDlpInput->setText(value.toString());
        else if (key == "ffmpeg_path") m_ffmpegInput->setText(value.toString());
        else if (key == "ffprobe_path") m_ffprobeInput->setText(value.toString());
        else if (key == "gallery_dl_path") m_galleryDlInput->setText(value.toString());
        else if (key == "aria2c_path") m_aria2cInput->setText(value.toString());
    }
}