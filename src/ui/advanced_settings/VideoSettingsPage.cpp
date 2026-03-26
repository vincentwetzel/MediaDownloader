#include "VideoSettingsPage.h"
#include "core/ConfigManager.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QSignalBlocker>

VideoSettingsPage::VideoSettingsPage(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *videoGroup = new QGroupBox("Default Video Settings", this);
    videoGroup->setToolTip("Set the default video download options. These can be overridden on the Start tab.");
    QFormLayout *videoLayout = new QFormLayout(videoGroup);

    m_videoQualityCombo = new QComboBox(this);
    m_videoQualityCombo->setToolTip("Pick the picture quality for your video, like '1080p' for high definition or '360p' for smaller files. 'best' tries to get the highest quality available.");
    m_videoQualityCombo->addItems({"best", "2160p", "1440p", "1080p", "720p", "480p", "360p", "240p", "144p", "worst"});
    videoLayout->addRow("Quality:", m_videoQualityCombo);

    m_videoCodecCombo = new QComboBox(this);
    m_videoCodecCombo->setToolTip("Choose the video format (codec). This affects file size and compatibility. H.264 is common, H.265 is newer and smaller, AV1/VP9 are often used for web videos.");
    m_videoCodecCombo->addItems({"Default", "H.264 (AVC)", "H.265 (HEVC)", "VP9", "AV1", "ProRes (Archive)", "Theora"});
    videoLayout->addRow("Codec:", m_videoCodecCombo);

    m_videoExtLabel = new QLabel("Extension:", this);
    m_videoExtLabel->setToolTip("Select the file type for your video... changes automatically based on codec.");
    m_videoExtCombo = new QComboBox(this);
    m_videoExtCombo->setToolTip("Select the file type for your video... changes automatically based on codec.");
    m_videoExtCombo->addItems({"mp4", "mkv", "webm"});
    videoLayout->addRow(m_videoExtLabel, m_videoExtCombo);

    m_videoAudioCodecCombo = new QComboBox(this);
    m_videoAudioCodecCombo->setToolTip("Choose the audio format (codec) that will be included in your video file.");
    m_videoAudioCodecCombo->addItems({"Default", "AAC", "Opus", "Vorbis", "MP3", "FLAC", "PCM"});
    videoLayout->addRow("Audio Codec:", m_videoAudioCodecCombo);

    layout->addWidget(videoGroup);
    layout->addStretch();

    connect(m_videoQualityCombo, &QComboBox::currentTextChanged, this, &VideoSettingsPage::onVideoQualityChanged);
    connect(m_videoCodecCombo, &QComboBox::currentTextChanged, this, &VideoSettingsPage::onVideoCodecChanged);
    connect(m_videoExtCombo, &QComboBox::currentTextChanged, this, &VideoSettingsPage::onVideoExtChanged);
    connect(m_videoAudioCodecCombo, &QComboBox::currentTextChanged, this, &VideoSettingsPage::onVideoAudioCodecChanged);
    connect(m_configManager, &ConfigManager::settingChanged, this, &VideoSettingsPage::handleConfigSettingChanged);
}

void VideoSettingsPage::loadSettings() {
    QSignalBlocker b1(m_videoQualityCombo);
    QSignalBlocker b2(m_videoCodecCombo);
    QSignalBlocker b3(m_videoExtCombo);
    QSignalBlocker b4(m_videoAudioCodecCombo);

    m_videoQualityCombo->setCurrentText(m_configManager->get("Video", "video_quality", m_configManager->getDefault("Video", "video_quality")).toString());
    m_videoCodecCombo->setCurrentText(m_configManager->get("Video", "video_codec", m_configManager->getDefault("Video", "video_codec")).toString());
    m_videoExtCombo->setCurrentText(m_configManager->get("Video", "video_extension", m_configManager->getDefault("Video", "video_extension")).toString());
    m_videoAudioCodecCombo->setCurrentText(m_configManager->get("Video", "video_audio_codec", m_configManager->getDefault("Video", "video_audio_codec")).toString());
    updateVideoOptions();
}

void VideoSettingsPage::onVideoQualityChanged(const QString &text) { m_configManager->set("Video", "video_quality", text); }
void VideoSettingsPage::onVideoCodecChanged(const QString &text) { m_configManager->set("Video", "video_codec", text); updateVideoOptions(); }
void VideoSettingsPage::onVideoExtChanged(const QString &text) { m_configManager->set("Video", "video_extension", text); }
void VideoSettingsPage::onVideoAudioCodecChanged(const QString &text) { m_configManager->set("Video", "video_audio_codec", text); }

void VideoSettingsPage::handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value) {
    if (section == "Video") {
        if (key == "quality" || key == "video_quality") m_videoQualityCombo->setCurrentText(value.toString());
        else if (key == "codec" || key == "video_codec") m_videoCodecCombo->setCurrentText(value.toString());
        else if (key == "extension" || key == "video_extension") m_videoExtCombo->setCurrentText(value.toString());
        else if (key == "audio_codec" || key == "video_audio_codec") m_videoAudioCodecCombo->setCurrentText(value.toString());
    }
}

void VideoSettingsPage::updateVideoOptions() {
    QString selectedVideoCodec = m_videoCodecCombo->currentText();
    bool isDefaultCodec = (selectedVideoCodec == "Default");
    m_videoExtLabel->setVisible(!isDefaultCodec);
    m_videoExtCombo->setVisible(!isDefaultCodec);
    if (isDefaultCodec) return;

    QString currentExt = m_videoExtCombo->currentText();
    m_videoExtCombo->clear();
    
    if (selectedVideoCodec == "AV1" || selectedVideoCodec == "VP9") m_videoExtCombo->addItems({"webm", "mkv"});
    else if (selectedVideoCodec == "H.264 (AVC)" || selectedVideoCodec == "H.265 (HEVC)") m_videoExtCombo->addItems({"mp4", "mkv"});
    else if (selectedVideoCodec == "ProRes (Archive)") m_videoExtCombo->addItem("mov");
    else if (selectedVideoCodec == "Theora") m_videoExtCombo->addItem("ogv");
    else m_videoExtCombo->addItems({"mp4", "mkv", "webm"});

    if (m_videoExtCombo->findText(currentExt) != -1) m_videoExtCombo->setCurrentText(currentExt);
    else m_videoExtCombo->setCurrentIndex(0);

    QString currentAudioCodec = m_videoAudioCodecCombo->currentText();
    m_videoAudioCodecCombo->clear();
    
    if (selectedVideoCodec == "AV1" || selectedVideoCodec == "VP9") m_videoAudioCodecCombo->addItems({"Default", "Opus", "Vorbis", "AAC"});
    else if (selectedVideoCodec == "H.264 (AVC)" || selectedVideoCodec == "H.265 (HEVC)") m_videoAudioCodecCombo->addItems({"Default", "AAC", "MP3", "FLAC", "PCM"});
    else if (selectedVideoCodec == "ProRes (Archive)") m_videoAudioCodecCombo->addItems({"Default", "PCM", "AAC"});
    else if (selectedVideoCodec == "Theora") m_videoAudioCodecCombo->addItems({"Default", "Vorbis"});
    else m_videoAudioCodecCombo->addItems({"Default", "AAC", "Opus", "Vorbis", "MP3", "FLAC", "PCM"});

    if (m_videoAudioCodecCombo->findText(currentAudioCodec) != -1) m_videoAudioCodecCombo->setCurrentText(currentAudioCodec);
    else m_videoAudioCodecCombo->setCurrentIndex(0);
}