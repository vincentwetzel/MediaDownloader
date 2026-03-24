#include "AdvancedSettingsTab.h"
#include "utils/BrowserUtils.h"
#include "ToggleSwitch.h"
#include <QLineEdit>
#include <QVBoxLayout>
#include <QDebug>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QProcess>
#include <QMap>
#include <algorithm>
#include <QComboBox>
#include <QPushButton>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>

AdvancedSettingsTab::AdvancedSettingsTab(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    setupUI();
    populateSubtitleLanguages();
    loadSettings();

    connect(m_configManager, &ConfigManager::settingChanged,
            this, &AdvancedSettingsTab::handleConfigSettingChanged);

    updateSubtitleFormatAvailability(m_embedSubtitlesCheck->isChecked());

    m_cookieCheckProcess = new QProcess(this);
    connect(m_cookieCheckProcess, &QProcess::started, this, &AdvancedSettingsTab::onCookieCheckProcessStarted);
    connect(m_cookieCheckProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AdvancedSettingsTab::onCookieCheckProcessFinished);
    connect(m_cookieCheckProcess, &QProcess::errorOccurred,
            this, &AdvancedSettingsTab::onCookieCheckProcessErrorOccurred);
    connect(m_cookieCheckProcess, &QProcess::readyReadStandardOutput, this, &AdvancedSettingsTab::onCookieCheckProcessReadyReadStandardOutput);
    connect(m_cookieCheckProcess, &QProcess::readyReadStandardError, this, &AdvancedSettingsTab::onCookieCheckProcessReadyReadStandardError);


    m_cookieCheckTimeoutTimer = new QTimer(this);
    m_cookieCheckTimeoutTimer->setSingleShot(true);
    connect(m_cookieCheckTimeoutTimer, &QTimer::timeout, this, &AdvancedSettingsTab::onCookieCheckTimeout);
}

AdvancedSettingsTab::~AdvancedSettingsTab() {
    if (m_cookieCheckProcess->state() != QProcess::NotRunning) {
        m_cookieCheckProcess->terminate();
        m_cookieCheckProcess->waitForFinished(1000); // Wait a bit for it to terminate
    }
}

void AdvancedSettingsTab::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *contentLayout = new QHBoxLayout();

    m_categoryList = new QListWidget(this);
    m_categoryList->setFixedWidth(180);
    m_categoryList->setStyleSheet(R"(
        QListWidget {
            border: none;
            border-right: 1px solid palette(mid);
            outline: 0px;
            background-color: transparent;
        }

        QListWidget::item {
            padding: 8px 12px;
            margin: 1px 4px;
            border-radius: 5px;
            color: palette(text);
        }

        QListWidget::item:hover {
            background-color: palette(alternate-base);
        }

        QListWidget::item:selected {
            background-color: palette(highlight);
            color: palette(highlighted-text);
            font-weight: bold;
        }
    )");
    contentLayout->addWidget(m_categoryList);

    m_stackedWidget = new QStackedWidget(this);
    contentLayout->addWidget(m_stackedWidget);

    mainLayout->addLayout(contentLayout, 1); // Set stretch factor to 1

    // Create and add pages
    m_categoryList->addItem("Configuration");
    m_stackedWidget->addWidget(createConfigurationPage());

    m_categoryList->addItem("Video Settings");
    m_stackedWidget->addWidget(createVideoSettingsPage());

    m_categoryList->addItem("Audio Settings");
    m_stackedWidget->addWidget(createAudioSettingsPage());

    m_categoryList->addItem("Authentication");
    m_stackedWidget->addWidget(createAuthenticationPage());

    m_categoryList->addItem("Output Templates");
    m_stackedWidget->addWidget(createOutputTemplatePage());

    m_categoryList->addItem("Download Options");
    m_stackedWidget->addWidget(createDownloadOptionsPage());

    m_categoryList->addItem("Metadata");
    m_stackedWidget->addWidget(createMetadataPage());

    m_categoryList->addItem("Subtitles");
    m_stackedWidget->addWidget(createSubtitlesPage());

    m_categoryList->addItem("Updates");
    m_stackedWidget->addWidget(createUpdatesPage());

    connect(m_categoryList, &QListWidget::currentRowChanged, m_stackedWidget, &QStackedWidget::setCurrentIndex);

    m_categoryList->setCurrentRow(0);

    // Add Restore Defaults button at the bottom
    m_restoreDefaultsButton = new QPushButton("Restore defaults", this);
    m_restoreDefaultsButton->setToolTip("Click this button to reset all settings on this tab back to their original, default values.");
    mainLayout->addWidget(m_restoreDefaultsButton, 0, Qt::AlignRight);
    connect(m_restoreDefaultsButton, &QPushButton::clicked, this, &AdvancedSettingsTab::restoreDefaults);
}

QWidget* AdvancedSettingsTab::createConfigurationPage() {
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *configGroup = new QGroupBox("Configuration", this);
    configGroup->setToolTip("General application settings including download locations and theme.");
    QFormLayout *configLayout = new QFormLayout(configGroup);

    m_completedDirInput = new QLineEdit(this);
    m_completedDirInput->setReadOnly(true);
    m_completedDirInput->setToolTip("This is where your finished downloads will be saved. Click 'Browse' to change it.");
    m_browseCompletedBtn = new QPushButton("Browse...", this);
    m_browseCompletedBtn->setToolTip("Click to choose a different folder for your completed downloads.");
    QHBoxLayout *completedLayout = new QHBoxLayout();
    completedLayout->addWidget(m_completedDirInput);
    completedLayout->addWidget(m_browseCompletedBtn);
    configLayout->addRow("Output folder:", completedLayout);

    m_tempDirInput = new QLineEdit(this);
    m_tempDirInput->setReadOnly(true);
    m_tempDirInput->setToolTip("This is a temporary folder used during downloads. You usually don't need to change this.");
    m_browseTempBtn = new QPushButton("Browse...", this);
    m_browseTempBtn->setToolTip("Click to choose a different temporary folder for downloads.");
    QHBoxLayout *tempLayout = new QHBoxLayout();
    tempLayout->addWidget(m_tempDirInput);
    tempLayout->addWidget(m_browseTempBtn);
    configLayout->addRow("Temporary folder:", tempLayout);

    m_themeCombo = new QComboBox(this);
    m_themeCombo->setToolTip("Choose the visual style of the application: 'System' (matches your computer's setting), 'Light', or 'Dark'.");
    m_themeCombo->addItems({"System", "Light", "Dark"});
    configLayout->addRow("Theme:", m_themeCombo);

    layout->addWidget(configGroup);
    layout->addStretch();

    connect(m_browseCompletedBtn, &QPushButton::clicked, this, &AdvancedSettingsTab::selectCompletedDir);
    connect(m_browseTempBtn, &QPushButton::clicked, this, &AdvancedSettingsTab::selectTempDir);
    connect(m_themeCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onThemeChanged);

    return page;
}

QWidget* AdvancedSettingsTab::createVideoSettingsPage() {
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *videoGroup = new QGroupBox("Default Video Settings", this);
    videoGroup->setToolTip("Set the default video download options. These can be overridden on the Start tab.");
    m_videoLayout = new QFormLayout(videoGroup);

    m_videoQualityCombo = new QComboBox(this);
    m_videoQualityCombo->setToolTip("Pick the picture quality for your video, like '1080p' for high definition or '360p' for smaller files. 'best' tries to get the highest quality available.");
    m_videoQualityCombo->addItems({"best", "2160p", "1440p", "1080p", "720p", "480p", "360p", "240p", "144p", "worst"});
    m_videoLayout->addRow("Quality:", m_videoQualityCombo);

    m_videoCodecCombo = new QComboBox(this);
    m_videoCodecCombo->setToolTip("Choose the video format (codec). This affects file size and compatibility. H.264 is common, H.265 is newer and smaller, AV1/VP9 are often used for web videos.");
    m_videoCodecCombo->addItems({"Default", "H.264 (AVC)", "H.265 (HEVC)", "VP9", "AV1", "ProRes (Archive)", "Theora"});
    m_videoLayout->addRow("Codec:", m_videoCodecCombo);

    m_videoExtLabel = new QLabel("Extension:", this);
    m_videoExtLabel->setToolTip("Select the file type for your video, like '.mp4' (very common) or '.mkv' (supports more features). This changes automatically based on the video codec you pick.");
    m_videoExtCombo = new QComboBox(this);
    m_videoExtCombo->setToolTip("Select the file type for your video, like '.mp4' (very common) or '.mkv' (supports more features). This changes automatically based on the video codec you pick.");
    m_videoExtCombo->addItems({"mp4", "mkv", "webm"});
    m_videoLayout->addRow(m_videoExtLabel, m_videoExtCombo);

    m_videoAudioCodecCombo = new QComboBox(this);
    m_videoAudioCodecCombo->setToolTip("Choose the audio format (codec) that will be included in your video file. AAC is common, Opus is good for web, MP3 is widely supported.");
    m_videoAudioCodecCombo->addItems({"Default", "AAC", "Opus", "Vorbis", "MP3", "FLAC", "PCM"});
    m_videoLayout->addRow("Audio Codec:", m_videoAudioCodecCombo);

    layout->addWidget(videoGroup);
    layout->addStretch();

    connect(m_videoQualityCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onVideoQualityChanged);
    connect(m_videoCodecCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onVideoCodecChanged);
    connect(m_videoExtCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onVideoExtChanged);
    connect(m_videoAudioCodecCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onVideoAudioCodecChanged);

    return page;
}

QWidget* AdvancedSettingsTab::createAudioSettingsPage() {
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *audioGroup = new QGroupBox("Default Audio Settings", this);
    audioGroup->setToolTip("Set the default audio-only download options. These can be overridden on the Start tab.");
    m_audioLayout = new QFormLayout(audioGroup);

    m_audioQualityCombo = new QComboBox(this);
    m_audioQualityCombo->setToolTip("Pick the sound quality for your audio, like '320k' for high quality or '64k' for smaller files. 'best' tries to get the highest quality available.");
    m_audioQualityCombo->addItems({"best", "320k", "256k", "192k", "128k", "96k", "64k", "32k", "worst"});
    m_audioLayout->addRow("Quality:", m_audioQualityCombo);

    m_audioCodecCombo = new QComboBox(this);
    m_audioCodecCombo->setToolTip("Choose the audio format (codec). Opus is modern and efficient, MP3 is very common, FLAC is for lossless quality.");
    m_audioCodecCombo->addItems({"Default", "Opus", "AAC", "Vorbis", "MP3", "FLAC", "WAV", "ALAC", "AC3", "EAC3", "DTS", "PCM"});
    m_audioLayout->addRow("Codec:", m_audioCodecCombo);

    m_audioExtLabel = new QLabel("Extension:", this);
    m_audioExtLabel->setToolTip("Select the file type for your audio, like '.mp3' (very common) or '.opus' (modern and small). This changes automatically based on the audio codec you pick.");
    m_audioExtCombo = new QComboBox(this);
    m_audioExtCombo->setToolTip("Select the file type for your audio, like '.mp3' (very common) or '.opus' (modern and small). This changes automatically based on the audio codec you pick.");
    m_audioExtCombo->addItems({"mp3", "m4a", "opus", "wav", "flac"});
    m_audioLayout->addRow(m_audioExtLabel, m_audioExtCombo);

    layout->addWidget(audioGroup);
    layout->addStretch();

    connect(m_audioQualityCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onAudioQualityChanged);
    connect(m_audioCodecCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onAudioCodecChanged);
    connect(m_audioExtCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onAudioExtChanged);

    return page;
}

QWidget* AdvancedSettingsTab::createAuthenticationPage() {
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *authGroup = new QGroupBox("Authentication Access", this);
    authGroup->setToolTip("Settings for accessing content that requires login, using browser cookies.");
    QFormLayout *authLayout = new QFormLayout(authGroup);

    QStringList installedBrowsers = BrowserUtils::getInstalledBrowsers();
    QStringList orderedBrowsers;

    int firefoxIndex = -1;
    for (int i = 0; i < installedBrowsers.size(); ++i) {
        if (installedBrowsers.at(i).compare("Firefox", Qt::CaseInsensitive) == 0) {
            firefoxIndex = i;
            break;
        }
    }
    if (firefoxIndex != -1) {
        orderedBrowsers.append(installedBrowsers.takeAt(firefoxIndex));
    }

    int chromeIndex = -1;
    for (int i = 0; i < installedBrowsers.size(); ++i) {
        if (installedBrowsers.at(i).compare("Chrome", Qt::CaseInsensitive) == 0) {
            chromeIndex = i;
            break;
        }
    }
    if (chromeIndex != -1) {
        orderedBrowsers.append(installedBrowsers.takeAt(chromeIndex));
    }

    std::sort(installedBrowsers.begin(), installedBrowsers.end(), [](const QString &s1, const QString &s2){
        return s1.compare(s2, Qt::CaseInsensitive) < 0;
    });

    orderedBrowsers.append(installedBrowsers);
    orderedBrowsers.append("None");

    m_cookiesBrowserCombo = new QComboBox(this);
    m_cookiesBrowserCombo->setToolTip("Choose a web browser to get your login cookies from. This lets you download content that requires you to be logged in.");
    m_cookiesBrowserCombo->addItems(orderedBrowsers);
    authLayout->addRow("Cookies from browser:", m_cookiesBrowserCombo);

    layout->addWidget(authGroup);
    layout->addStretch();

    connect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onCookiesBrowserChanged);

    return page;
}

QWidget* AdvancedSettingsTab::createOutputTemplatePage() {
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    // yt-dlp Group
    QGroupBox *ytDlpGroup = new QGroupBox("yt-dlp Filename Template", this);
    ytDlpGroup->setToolTip("Define how downloaded files from yt-dlp are named using special tokens.");
    QVBoxLayout *ytDlpLayout = new QVBoxLayout(ytDlpGroup);
    QHBoxLayout *ytDlpControlsLayout = new QHBoxLayout();
    QLabel *ytPatternLabel = new QLabel("Filename Pattern:", this);
    ytPatternLabel->setToolTip("This is how your downloaded files will be named. You can use special 'tokens' (like %(title)s) to automatically include information about the video.");
    ytDlpControlsLayout->addWidget(ytPatternLabel);
    m_ytDlpOutputTemplateInput = new QLineEdit(this);
    m_ytDlpOutputTemplateInput->setToolTip("This is how your downloaded files will be named. You can use special 'tokens' (like %(title)s) to automatically include information about the video.");
    ytDlpControlsLayout->addWidget(m_ytDlpOutputTemplateInput);
    m_ytDlpTemplateTokensCombo = new QComboBox(this);
    m_ytDlpTemplateTokensCombo->setToolTip("Select a token from this list to insert it into your output template. Tokens are placeholders that get replaced with actual video information.");
    m_ytDlpTemplateTokensCombo->addItem("Insert token...", "");
    m_ytDlpTemplateTokensCombo->addItem("Title", "%(title)s");
    m_ytDlpTemplateTokensCombo->addItem("Uploader", "%(uploader)s");
    m_ytDlpTemplateTokensCombo->addItem("Upload Date (YYYY-MM-DD)", "%(upload_date>%Y-%m-%d)s");
    m_ytDlpTemplateTokensCombo->addItem("Video ID", "%(id)s");
	m_ytDlpTemplateTokensCombo->addItem("Extension", "%(ext)s");
    m_ytDlpTemplateTokensCombo->addItem("Playlist Index", "%(playlist_index)s");
    m_ytDlpTemplateTokensCombo->addItem("Playlist Title", "%(playlist_title)s");
    m_ytDlpTemplateTokensCombo->addItem("Playlist Title", "%(playlist_title)s");
    ytDlpControlsLayout->addWidget(m_ytDlpTemplateTokensCombo);
    m_saveYtDlpTemplateButton = new QPushButton("Save", this);
    m_saveYtDlpTemplateButton->setToolTip("Click to save your custom filename template. The app will check if it's valid first.");
    m_saveYtDlpTemplateButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_saveYtDlpTemplateButton->setFixedWidth(60);
    ytDlpControlsLayout->addWidget(m_saveYtDlpTemplateButton);
    QPushButton *resetYtDlpButton = new QPushButton("Reset", this);
    resetYtDlpButton->setToolTip("Restores the filename pattern to its default value.");
		connect(resetYtDlpButton, &QPushButton::clicked, this, [this]() {
        m_ytDlpOutputTemplateInput->setText(m_configManager->get("General", "output_template").toString());
        QMessageBox::information(this, "Template Reset", "Filename pattern has been reset to default.");
    });
    ytDlpControlsLayout->addWidget(resetYtDlpButton);
    ytDlpLayout->addLayout(ytDlpControlsLayout);
    layout->addWidget(ytDlpGroup);


    // gallery-dl Group
    QGroupBox *galleryDlGroup = new QGroupBox("gallery-dl Filename Template", this);
    galleryDlGroup->setToolTip("Define how downloaded files from gallery-dl are named using special tokens.");
    QVBoxLayout *galleryDlLayout = new QVBoxLayout(galleryDlGroup);
    QHBoxLayout *galleryDlControlsLayout = new QHBoxLayout();
    QLabel *galleryPatternLabel = new QLabel("Filename Pattern:", this);
    galleryPatternLabel->setToolTip("This is how your downloaded files will be named. You can use special 'tokens' (like {filename}) to automatically include information about the image.");
    galleryDlControlsLayout->addWidget(galleryPatternLabel);
    m_galleryDlOutputTemplateInput = new QLineEdit(this);
    m_galleryDlOutputTemplateInput->setToolTip("This is how your downloaded files will be named. You can use special 'tokens' (like {filename}) to automatically include information about the image.");
    galleryDlControlsLayout->addWidget(m_galleryDlOutputTemplateInput);
    m_galleryDlTemplateTokensCombo = new QComboBox(this);
    m_galleryDlTemplateTokensCombo->setToolTip("Select a token from this list to insert it into your output template. Tokens are placeholders that get replaced with actual image information.");
    m_galleryDlTemplateTokensCombo->addItem("Insert token...", "");
    m_galleryDlTemplateTokensCombo->addItem("Filename", "{filename}.{extension}");
    m_galleryDlTemplateTokensCombo->addItem("ID", "{id}");
    m_galleryDlTemplateTokensCombo->addItem("Author", "{author[name]}");
    m_galleryDlTemplateTokensCombo->addItem("Date (YYYY-MM-DD)", "{date:%Y-%m-%d}");
    galleryDlControlsLayout->addWidget(m_galleryDlTemplateTokensCombo);
    m_saveGalleryDlTemplateButton = new QPushButton("Save", this);
    m_saveGalleryDlTemplateButton->setToolTip("Click to save your custom filename template.");
    m_saveGalleryDlTemplateButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_saveGalleryDlTemplateButton->setFixedWidth(60);
    galleryDlControlsLayout->addWidget(m_saveGalleryDlTemplateButton);
    QPushButton *resetGalleryDlButton = new QPushButton("Reset", this);
    resetGalleryDlButton->setToolTip("Restores the filename pattern to its default value.");
    resetGalleryDlButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    resetGalleryDlButton->setFixedWidth(60);
    connect(resetGalleryDlButton, &QPushButton::clicked, this, [this]() {
        m_configManager->set("General", "gallery_output_template", m_configManager->getDefault("General", "gallery_output_template").toString());
        m_galleryDlOutputTemplateInput->setText(m_configManager->get("General", "gallery_output_template").toString());
        QMessageBox::information(this, "Template Reset", "Filename pattern has been reset to default.");
    });
    galleryDlControlsLayout->addWidget(resetGalleryDlButton);
    galleryDlLayout->addLayout(galleryDlControlsLayout);
    layout->addWidget(galleryDlGroup);

    layout->addStretch();

    connect(m_saveYtDlpTemplateButton, &QPushButton::clicked, this, &AdvancedSettingsTab::validateAndSaveYtDlpTemplate);
    connect(m_ytDlpTemplateTokensCombo, QOverload<int>::of(&QComboBox::activated), this, &AdvancedSettingsTab::insertYtDlpTemplateToken);

    connect(m_saveGalleryDlTemplateButton, &QPushButton::clicked, this, &AdvancedSettingsTab::validateAndSaveGalleryDlTemplate);
    connect(m_galleryDlTemplateTokensCombo, QOverload<int>::of(&QComboBox::activated), this, &AdvancedSettingsTab::insertGalleryDlTemplateToken);

    return page;
}

QWidget* AdvancedSettingsTab::createDownloadOptionsPage() {
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *downloadOptionsGroup = new QGroupBox("Download Options", this);
    downloadOptionsGroup->setToolTip("Control various aspects of the download process.");
    QFormLayout *downloadOptionsLayout = new QFormLayout(downloadOptionsGroup);

    m_externalDownloaderCheck = new ToggleSwitch(this);
    m_externalDownloaderCheck->setToolTip("Check this box to use 'aria2c' for potentially faster downloads, especially for large files. It needs to be in your 'bin' folder.");
    downloadOptionsLayout->addRow("External Downloader (aria2c)", m_externalDownloaderCheck);

    m_sponsorBlockCheck = new ToggleSwitch(this);
    m_sponsorBlockCheck->setToolTip("If checked, the downloader will try to automatically skip sponsored parts, intros, outros, and other non-content segments in videos (where data is available).");
    downloadOptionsLayout->addRow("Enable SponsorBlock", m_sponsorBlockCheck);

    m_embedChaptersCheck = new ToggleSwitch(this);
    m_embedChaptersCheck->setToolTip("If checked, chapter markers (if available) will be saved inside the video file, allowing you to jump to different sections.");
    downloadOptionsLayout->addRow("Embed video chapters", m_embedChaptersCheck);

    // Replaced ToggleSwitch with QComboBox for auto-paste functionality
    m_autoPasteModeCombo = new QComboBox(this);
    m_autoPasteModeCombo->setToolTip("Choose how the application handles URLs copied to your clipboard.");
    m_autoPasteModeCombo->addItem("Disabled", 0);
    m_autoPasteModeCombo->addItem("Auto-paste on app focus or hover", 1);
    m_autoPasteModeCombo->addItem("Auto-paste on new URL in clipboard (does not require app focus)", 2);
    m_autoPasteModeCombo->addItem("Auto-paste & enqueue on app focus", 3);
    m_autoPasteModeCombo->addItem("Auto-paste & enqueue on new URL in clipboard (does not require app focus)", 4);
    downloadOptionsLayout->addRow("Auto-paste URL behavior:", m_autoPasteModeCombo);

    m_singleLineCommandPreviewCheck = new ToggleSwitch(this);
    m_singleLineCommandPreviewCheck->setToolTip("If checked, the command preview on the Start tab will be a single line. Otherwise, it will be multi-line.");
    downloadOptionsLayout->addRow("Single-line command preview", m_singleLineCommandPreviewCheck);

    m_restrictFilenamesCheck = new ToggleSwitch(this);
    m_restrictFilenamesCheck->setToolTip("If checked, filenames will be restricted to ASCII characters, and spaces will be replaced with underscores.");
    downloadOptionsLayout->addRow("Restrict filenames", m_restrictFilenamesCheck);

    layout->addWidget(downloadOptionsGroup);
    layout->addStretch();

    connect(m_externalDownloaderCheck, &ToggleSwitch::toggled, this, &AdvancedSettingsTab::onExternalDownloaderToggled);
    connect(m_sponsorBlockCheck, &ToggleSwitch::toggled, this, &AdvancedSettingsTab::onSponsorBlockToggled);
    connect(m_embedChaptersCheck, &ToggleSwitch::toggled, this, &AdvancedSettingsTab::onEmbedChaptersToggled);
    // Connect the new QComboBox
    connect(m_autoPasteModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AdvancedSettingsTab::onAutoPasteModeChanged);
    connect(m_singleLineCommandPreviewCheck, &ToggleSwitch::toggled, this, &AdvancedSettingsTab::onSingleLineCommandPreviewToggled);
    connect(m_restrictFilenamesCheck, &ToggleSwitch::toggled, this, &AdvancedSettingsTab::onRestrictFilenamesToggled);

    return page;
}

QWidget* AdvancedSettingsTab::createMetadataPage() {
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *metadataGroup = new QGroupBox("Metadata / Thumbnails", this);
    metadataGroup->setToolTip("Settings for embedding metadata and thumbnails into downloaded files.");
    QFormLayout *metadataLayout = new QFormLayout(metadataGroup);

    m_embedMetadataCheck = new ToggleSwitch(this);
    m_embedMetadataCheck->setToolTip("If checked, information like title, artist, and description will be saved directly into the downloaded file.");
    metadataLayout->addRow("Embed metadata", m_embedMetadataCheck);

    m_embedThumbnailCheck = new ToggleSwitch(this);
    m_embedThumbnailCheck->setToolTip("If checked, the video's cover image (thumbnail) will be saved inside the downloaded file.");
    metadataLayout->addRow("Embed thumbnail", m_embedThumbnailCheck);

    m_highQualityThumbnailCheck = new ToggleSwitch(this);
    m_highQualityThumbnailCheck->setToolTip("If checked, the app will try to get the best possible quality for the embedded thumbnail.");
    metadataLayout->addRow("Use high-quality thumbnail converter", m_highQualityThumbnailCheck);

    m_convertThumbnailsCombo = new QComboBox(this);
    m_convertThumbnailsCombo->setToolTip("Choose a format to convert the thumbnail to, or 'None' to keep the original format. This is useful if you want a specific image file type.");
    m_convertThumbnailsCombo->addItems({"None", "jpg", "png"});
    metadataLayout->addRow("Convert thumbnails to:", m_convertThumbnailsCombo);

    layout->addWidget(metadataGroup);
    layout->addStretch();

    connect(m_embedMetadataCheck, &ToggleSwitch::toggled, this, &AdvancedSettingsTab::onEmbedMetadataToggled);
    connect(m_embedThumbnailCheck, &ToggleSwitch::toggled, this, &AdvancedSettingsTab::onEmbedThumbnailToggled);
    connect(m_highQualityThumbnailCheck, &ToggleSwitch::toggled, this, &AdvancedSettingsTab::onHighQualityThumbnailToggled);
    connect(m_convertThumbnailsCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onConvertThumbnailsChanged);

    return page;
}

QWidget* AdvancedSettingsTab::createSubtitlesPage() {
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *subtitlesGroup = new QGroupBox("Subtitles", this);
    subtitlesGroup->setToolTip("These settings control how subtitles are handled for your video downloads.");
    QFormLayout *subtitlesLayout = new QFormLayout(subtitlesGroup);

    m_subtitleLanguageCombo = new QComboBox(this);
    m_subtitleLanguageCombo->setToolTip("Select the language for subtitles. Use full words like 'English' instead of 'en'.");
    subtitlesLayout->addRow("Subtitle language:", m_subtitleLanguageCombo);

    m_embedSubtitlesCheck = new ToggleSwitch(this);
    m_embedSubtitlesCheck->setToolTip("If checked, subtitles will be permanently added inside the video file itself, so they always play with the video.");
    subtitlesLayout->addRow("Embed subtitles in video", m_embedSubtitlesCheck);

    m_writeSubtitlesCheck = new ToggleSwitch(this);
    m_writeSubtitlesCheck->setToolTip("If checked, the app will download available subtitles as a separate file alongside the video.");
    subtitlesLayout->addRow("Write subtitles to separate file(s)", m_writeSubtitlesCheck);

    m_includeAutoSubtitlesCheck = new ToggleSwitch(this);
    m_includeAutoSubtitlesCheck->setToolTip("If checked, the app will also download subtitles that were automatically created (e.g., by YouTube).");
    subtitlesLayout->addRow("Include automatically-generated subtitles", m_includeAutoSubtitlesCheck);

    m_subtitleFormatCombo = new QComboBox(this);
    m_subtitleFormatCombo->setToolTip("Choose the file format for your subtitles (e.g., 'srt' is a common text-based format). This option is greyed out if 'Embed subtitles in video' is selected.");
    m_subtitleFormatCombo->addItems({"srt*", "vtt", "ass"});
    subtitlesLayout->addRow("Subtitle file format:", m_subtitleFormatCombo);

    layout->addWidget(subtitlesGroup);
    layout->addStretch();

    connect(m_subtitleLanguageCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onSubtitleLanguageChanged);
    connect(m_embedSubtitlesCheck, &ToggleSwitch::toggled, this, &AdvancedSettingsTab::onEmbedSubtitlesToggled);
    connect(m_writeSubtitlesCheck, &ToggleSwitch::toggled, this, &AdvancedSettingsTab::onWriteSubtitlesToggled);
    connect(m_includeAutoSubtitlesCheck, &ToggleSwitch::toggled, this, &AdvancedSettingsTab::onIncludeAutoSubtitlesToggled);
    connect(m_subtitleFormatCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onSubtitleFormatChanged);

    return page;
}

QWidget* AdvancedSettingsTab::createUpdatesPage() {
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *updatesGroup = new QGroupBox("Updates", this);
    updatesGroup->setToolTip("Manage updates for the application and its core tools.");
    QFormLayout *updatesLayout = new QFormLayout(updatesGroup);

    m_ytDlpVersionLabel = new QLabel("yt-dlp version: Unknown", this);
    m_ytDlpVersionLabel->setToolTip("Shows the current version of the 'yt-dlp' tool.");
    m_updateYtDlpButton = new QPushButton("Update yt-dlp", this);
    m_updateYtDlpButton->setToolTip("Click to update the 'yt-dlp' tool, which is the engine that does the actual downloading. It will always update to the latest nightly build.");
    QHBoxLayout *ytDlpUpdateLayout = new QHBoxLayout();
    ytDlpUpdateLayout->addWidget(m_ytDlpVersionLabel);
    ytDlpUpdateLayout->addWidget(m_updateYtDlpButton);
    updatesLayout->addRow(ytDlpUpdateLayout);

    m_ytDlpUpdateStatusLabel = new QLabel(this);
    m_ytDlpUpdateStatusLabel->setToolTip("Displays the status of the yt-dlp update process.");
    updatesLayout->addRow(m_ytDlpUpdateStatusLabel);

    m_galleryDlVersionLabel = new QLabel("gallery-dl version: Unknown", this);
    m_galleryDlVersionLabel->setToolTip("Shows the current version of the 'gallery-dl' tool installed.");
    m_updateGalleryDlButton = new QPushButton("Update gallery-dl", this);
    m_updateGalleryDlButton->setToolTip("Click to update the 'gallery-dl' tool, used for downloading image galleries.");
    QHBoxLayout *galleryDlUpdateLayout = new QHBoxLayout();
    galleryDlUpdateLayout->addWidget(m_galleryDlVersionLabel);
    galleryDlUpdateLayout->addWidget(m_updateGalleryDlButton);
    updatesLayout->addRow(galleryDlUpdateLayout);

    m_galleryDlUpdateStatusLabel = new QLabel(this);
    m_galleryDlUpdateStatusLabel->setToolTip("Displays the status of the gallery-dl update process.");
    updatesLayout->addRow(m_galleryDlUpdateStatusLabel);

    layout->addWidget(updatesGroup);
    layout->addStretch();

    return page;
}

void AdvancedSettingsTab::populateSubtitleLanguages() {
    disconnect(m_subtitleLanguageCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onSubtitleLanguageChanged);

    QMap<QString, QString> langMap;
    langMap["en"] = "English";
    langMap["es"] = "Spanish";
    langMap["fr"] = "French";
    langMap["de"] = "German";
    langMap["it"] = "Italian";
    langMap["pt"] = "Portuguese";
    langMap["ru"] = "Russian";
    langMap["ja"] = "Japanese";
    langMap["ko"] = "Korean";
    langMap["zh-Hans"] = "Chinese (Simplified)";
    langMap["zh-Hant"] = "Chinese (Traditional)";
    langMap["ar"] = "Arabic";
    langMap["hi"] = "Hindi";
    langMap["bn"] = "Bengali";
    langMap["pa"] = "Punjabi";
    langMap["jv"] = "Javanese";
    langMap["ms"] = "Malay";
    langMap["te"] = "Telugu";
    langMap["vi"] = "Vietnamese";
    langMap["ta"] = "Tamil";
    langMap["mr"] = "Marathi";
    langMap["ur"] = "Urdu";
    langMap["tr"] = "Turkish";
    langMap["pl"] = "Polish";
    langMap["nl"] = "Dutch";
    langMap["th"] = "Thai";
    langMap["sv"] = "Swedish";
    langMap["da"] = "Danish";
    langMap["fi"] = "Finnish";
    langMap["no"] = "Norwegian";
    langMap["el"] = "Greek";
    langMap["he"] = "Hebrew";
    langMap["hu"] = "Hungarian";
    langMap["cs"] = "Czech";
    langMap["ro"] = "Romanian";
    langMap["id"] = "Indonesian";
    langMap["uk"] = "Ukrainian";
    langMap["bg"] = "Bulgarian";
    langMap["hr"] = "Croatian";
    langMap["sr"] = "Serbian";
    langMap["sk"] = "Slovak";
    langMap["sl"] = "Slovenian";
    langMap["et"] = "Estonian";
    langMap["lv"] = "Latvian";
    langMap["lt"] = "Lithuanian";
    langMap["fa"] = "Persian";
    langMap["tl"] = "Tagalog";
    langMap["sw"] = "Swahili";
    langMap["am"] = "Amharic";
    langMap["ne"] = "Nepali";
    langMap["si"] = "Sinhala";
    langMap["km"] = "Khmer";
    langMap["lo"] = "Lao";
    langMap["my"] = "Burmese";
    langMap["ka"] = "Georgian";
    langMap["az"] = "Azerbaijani";
    langMap["uz"] = "Uzbek";
    langMap["kk"] = "Kazakh";
    langMap["ky"] = "Kyrgyz";
    langMap["tg"] = "Tajik";
    langMap["ug"] = "Uyghur";
    langMap["mn"] = "Mongolian";
    langMap["dz"] = "Dzongkha";
    langMap["bo"] = "Tibetan";
    langMap["ps"] = "Pashto";
    langMap["sd"] = "Sindhi";
    langMap["ku"] = "Kurdish";
    langMap["ha"] = "Hausa";
    langMap["yo"] = "Yoruba";
    langMap["ig"] = "Igbo";
    langMap["ff"] = "Fulah";
    langMap["om"] = "Oromo";
    langMap["so"] = "Somali";
    langMap["zu"] = "Zulu";
    langMap["xh"] = "Xhosa";
    langMap["af"] = "Afrikaans";
    langMap["sq"] = "Albanian";
    langMap["hy"] = "Armenian";
    langMap["eu"] = "Basque";
    langMap["be"] = "Belarusian";
    langMap["bs"] = "Bosnian";
    langMap["ca"] = "Catalan";
    langMap["co"] = "Corsican";
    langMap["cy"] = "Welsh";
    langMap["eo"] = "Esperanto";
    langMap["gl"] = "Galician";
    langMap["ht"] = "Haitian Creole";
    langMap["is"] = "Icelandic";
    langMap["ga"] = "Irish";
    langMap["la"] = "Latin";
    langMap["lb"] = "Luxembourgish";
    langMap["mk"] = "Macedonian";
    langMap["mg"] = "Malagasy";
    langMap["mt"] = "Maltese";
    langMap["mi"] = "Maori";
    langMap["mo"] = "Moldavian";
    langMap["gd"] = "Scottish Gaelic";
    langMap["sn"] = "Shona";
    langMap["st"] = "Sesotho";
    langMap["su"] = "Sundanese";
    langMap["tk"] = "Turkmen";
    langMap["yi"] = "Yiddish";
    langMap["auto"] = "Auto-generated";
    langMap["all"] = "All available";

    QList<QString> sortedKeys = langMap.keys();
    std::sort(sortedKeys.begin(), sortedKeys.end(), [&langMap](const QString &a, const QString &b) {
        return langMap[a] < langMap[b];
    });

    for (const QString &code : sortedKeys) {
        m_subtitleLanguageCombo->addItem(langMap[code], code);
    }

    connect(m_subtitleLanguageCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onSubtitleLanguageChanged);
}

void AdvancedSettingsTab::loadSettings() {
    // Configuration
    m_completedDirInput->setText(m_configManager->get("Paths", "completed_downloads_directory").toString());
    m_tempDirInput->setText(m_configManager->get("Paths", "temporary_downloads_directory").toString());
    m_themeCombo->setCurrentText(m_configManager->get("General", "theme", "System").toString());

    // Video Settings
    m_videoQualityCombo->setCurrentText(m_configManager->get("Video", "video_quality", m_configManager->getDefault("Video", "video_quality")).toString());
    m_videoCodecCombo->setCurrentText(m_configManager->get("Video", "video_codec", m_configManager->getDefault("Video", "video_codec")).toString());
    m_videoExtCombo->setCurrentText(m_configManager->get("Video", "video_extension", m_configManager->getDefault("Video", "video_extension")).toString());
    m_videoAudioCodecCombo->setCurrentText(m_configManager->get("Video", "video_audio_codec", m_configManager->getDefault("Video", "video_audio_codec")).toString());

    // Audio Settings
    m_audioQualityCombo->setCurrentText(m_configManager->get("Audio", "audio_quality", m_configManager->getDefault("Audio", "audio_quality")).toString());
    m_audioCodecCombo->setCurrentText(m_configManager->get("Audio", "audio_codec", m_configManager->getDefault("Audio", "audio_codec")).toString());
    m_audioExtCombo->setCurrentText(m_configManager->get("Audio", "audio_extension", m_configManager->getDefault("Audio", "audio_extension")).toString());

    // Authentication Access
    disconnect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onCookiesBrowserChanged);
    m_lastSavedBrowser = m_configManager->get("General", "cookies_from_browser", "None").toString();
    if (m_lastSavedBrowser == "None") {
        m_lastSavedBrowser = m_configManager->get("General", "gallery_cookies_from_browser", "None").toString();
    }
    m_cookiesBrowserCombo->setCurrentText(m_lastSavedBrowser);
    connect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onCookiesBrowserChanged);

    // Output Template
    m_ytDlpOutputTemplateInput->setText(m_configManager->get("General", "output_template").toString());
    m_galleryDlOutputTemplateInput->setText(m_configManager->get("General", "gallery_output_template").toString());

    // Download Options
    m_externalDownloaderCheck->setChecked(m_configManager->get("Metadata", "use_aria2c", true).toBool());
    m_sponsorBlockCheck->setChecked(m_configManager->get("General", "sponsorblock", false).toBool());
    m_embedChaptersCheck->setChecked(m_configManager->get("Metadata", "embed_chapters", true).toBool());
    // Updated to use the new QComboBox
    m_autoPasteModeCombo->setCurrentIndex(m_configManager->get("General", "auto_paste_mode", 0).toInt());
    m_singleLineCommandPreviewCheck->setChecked(m_configManager->get("General", "single_line_preview", false).toBool());
    m_restrictFilenamesCheck->setChecked(m_configManager->get("General", "restrict_filenames", false).toBool());

    // Metadata / Thumbnails
    m_embedMetadataCheck->setChecked(m_configManager->get("Metadata", "embed_metadata", true).toBool());
    m_embedThumbnailCheck->setChecked(m_configManager->get("Metadata", "embed_thumbnail", true).toBool());
    m_highQualityThumbnailCheck->setChecked(m_configManager->get("Metadata", "high_quality_thumbnail", true).toBool());
    m_convertThumbnailsCombo->setCurrentText(m_configManager->get("Metadata", "convert_thumbnail_to", "jpg").toString());

    // Subtitles
    QString savedLangCode = m_configManager->get("Subtitles", "languages", "en").toString();
    int index = m_subtitleLanguageCombo->findData(savedLangCode);
    if (index != -1) {
        m_subtitleLanguageCombo->setCurrentIndex(index);
    } else {
        m_subtitleLanguageCombo->setCurrentText("English");
    }

    m_embedSubtitlesCheck->setChecked(m_configManager->get("Subtitles", "embed_subtitles", true).toBool());
    m_writeSubtitlesCheck->setChecked(m_configManager->get("Subtitles", "write_subtitles", false).toBool());
    m_includeAutoSubtitlesCheck->setChecked(m_configManager->get("Subtitles", "write_auto_subtitles", true).toBool());
    m_subtitleFormatCombo->setCurrentText(m_configManager->get("Subtitles", "format", "srt").toString());

    updateVideoOptions();
    updateAudioOptions();
}

void AdvancedSettingsTab::setYtDlpVersion(const QString &version) {
    m_ytDlpVersionLabel->setText("yt-dlp version: " + version);
}

void AdvancedSettingsTab::selectCompletedDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Completed Downloads Directory",
                                                    m_completedDirInput->text());
    if (!dir.isEmpty()) {
        m_configManager->set("Paths", "completed_downloads_directory", dir);
    }
}

void AdvancedSettingsTab::selectTempDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Temporary Downloads Directory",
                                                    m_tempDirInput->text());
    if (!dir.isEmpty()) {
        m_configManager->set("Paths", "temporary_downloads_directory", dir);
    }
}

void AdvancedSettingsTab::setGalleryDlVersion(const QString &version) {
    m_galleryDlVersionLabel->setText("gallery-dl version: " + version);
}

void AdvancedSettingsTab::restoreDefaults() {
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Restore Defaults", "Are you sure you want to restore all settings to their defaults?",
                                  QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        m_configManager->setDefaults();
        loadSettings();
        QMessageBox::information(this, "Restored", "Default settings have been restored.");
    }
}

void AdvancedSettingsTab::validateAndSaveYtDlpTemplate() {
    QString templateStr = m_ytDlpOutputTemplateInput->text();
    if (templateStr.isEmpty()) {
        QMessageBox::warning(this, "Invalid Template", "Template cannot be empty.");
        return;
    }

    m_saveYtDlpTemplateButton->setEnabled(false);
    m_saveYtDlpTemplateButton->setText("Checking...");

    QProcess *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, templateStr](int exitCode, QProcess::ExitStatus exitStatus){
        m_saveYtDlpTemplateButton->setEnabled(true);
        m_saveYtDlpTemplateButton->setText("Save");

        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            m_configManager->set("General", "output_template", templateStr);
            QMessageBox::information(this, "Saved", "Output filename pattern saved.");
        } else {
            QString error = process->readAllStandardError();
            QMessageBox::warning(this, "Invalid Template", "The output template appears to be invalid:\n" + error);
        }
        process->deleteLater();
    });

    QStringList args;
    args << "--get-filename";
    args << "-o" << templateStr;
    args << "https://www.youtube.com/watch?v=dQw4w9WgXcQ";

    process->start("yt-dlp", args);
}

void AdvancedSettingsTab::validateAndSaveGalleryDlTemplate() {
    QString templateStr = m_galleryDlOutputTemplateInput->text();
    if (templateStr.isEmpty()) {
        QMessageBox::warning(this, "Invalid Template", "Template cannot be empty.");
        return;
    }
    m_configManager->set("General", "gallery_output_template", templateStr);
    QMessageBox::information(this, "Saved", "Output filename pattern saved.");
}

void AdvancedSettingsTab::insertYtDlpTemplateToken(int index) {
    if (index == 0) return;

    QString token = m_ytDlpTemplateTokensCombo->itemData(index).toString();
    if (!token.isEmpty()) {
        m_ytDlpOutputTemplateInput->insert(token);
        m_ytDlpOutputTemplateInput->setFocus();
    }
    m_ytDlpTemplateTokensCombo->setCurrentIndex(0);
}

void AdvancedSettingsTab::insertGalleryDlTemplateToken(int index) {
    if (index == 0) return;

    QString token = m_galleryDlTemplateTokensCombo->itemData(index).toString();
    if (!token.isEmpty()) {
        m_galleryDlOutputTemplateInput->insert(token);
        m_galleryDlOutputTemplateInput->setFocus();
    }
    m_galleryDlTemplateTokensCombo->setCurrentIndex(0);
}

void AdvancedSettingsTab::handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value) {
    if (section == "Paths") {
        if (key == "completed_downloads_directory") {
            m_completedDirInput->setText(value.toString());
        } else if (key == "temporary_downloads_directory") {
            m_tempDirInput->setText(value.toString());
        }
    } else if (section == "General") {
        if (key == "theme") {
            disconnect(m_themeCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onThemeChanged);
            m_themeCombo->setCurrentText(value.toString());
            connect(m_themeCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onThemeChanged);
        } else if (key == "cookies_from_browser" || key == "gallery_cookies_from_browser") {
            m_lastSavedBrowser = value.toString();
            if (m_cookiesBrowserCombo->currentText() != m_lastSavedBrowser) {
                disconnect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onCookiesBrowserChanged);
                m_cookiesBrowserCombo->setCurrentText(m_lastSavedBrowser);
                connect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onCookiesBrowserChanged);
            }
        } else if (key == "output_template") {
            m_ytDlpOutputTemplateInput->setText(value.toString());
        } else if (key == "gallery_output_template") {
            m_galleryDlOutputTemplateInput->setText(value.toString());
        } else if (key == "sponsorblock") {
            m_sponsorBlockCheck->setChecked(value.toBool());
        } else if (key == "auto_paste_mode") { // Updated key
            disconnect(m_autoPasteModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AdvancedSettingsTab::onAutoPasteModeChanged);
            m_autoPasteModeCombo->setCurrentIndex(value.toInt());
            connect(m_autoPasteModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AdvancedSettingsTab::onAutoPasteModeChanged);
        } else if (key == "single_line_preview") {
            m_singleLineCommandPreviewCheck->setChecked(value.toBool());
        } else if (key == "restrict_filenames") {
            m_restrictFilenamesCheck->setChecked(value.toBool());
        }
    } else if (section == "Video") {
        if (key == "quality") m_videoQualityCombo->setCurrentText(value.toString());
        else if (key == "codec") m_videoCodecCombo->setCurrentText(value.toString());
        else if (key == "extension") m_videoExtCombo->setCurrentText(value.toString());
        else if (key == "audio_codec") m_videoAudioCodecCombo->setCurrentText(value.toString());
    } else if (section == "Audio") {
        if (key == "quality") m_audioQualityCombo->setCurrentText(value.toString());
        else if (key == "codec") m_audioCodecCombo->setCurrentText(value.toString());
        else if (key == "extension") m_audioExtCombo->setCurrentText(value.toString());
    }
    else if (section == "Metadata") {
        if (key == "use_aria2c") {
            m_externalDownloaderCheck->setChecked(value.toBool());
        } else if (key == "embed_chapters") {
            m_embedChaptersCheck->setChecked(value.toBool());
        } else if (key == "embed_metadata") {
            m_embedMetadataCheck->setChecked(value.toBool());
        } else if (key == "embed_thumbnail") {
            m_embedThumbnailCheck->setChecked(value.toBool());
        } else if (key == "high_quality_thumbnail") {
            m_highQualityThumbnailCheck->setChecked(value.toBool());
        }
    } else if (section == "Subtitles") {
        if (key == "embed_subtitles") {
            m_embedSubtitlesCheck->setChecked(value.toBool());
            updateSubtitleFormatAvailability(value.toBool());
        } else if (key == "write_subtitles") {
            m_writeSubtitlesCheck->setChecked(value.toBool());
        } else if (key == "write_auto_subtitles") {
            m_includeAutoSubtitlesCheck->setChecked(value.toBool());
        }
    }
}

void AdvancedSettingsTab::onThemeChanged(const QString &text) {
    m_configManager->set("General", "theme", text);
    emit themeChanged(text);
}

void AdvancedSettingsTab::onCookiesBrowserChanged(const QString &text) {
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
             << "onCookiesBrowserChanged triggered for" << text;

    if (text == m_lastSavedBrowser) {
        qDebug() << "Browser is the same as last saved. No action taken.";
        return;
    }

    if (text.compare("None", Qt::CaseInsensitive) == 0) {
        qDebug() << "Browser set to 'None'. Updating config.";
        m_lastSavedBrowser = text;
        m_configManager->set("General", "cookies_from_browser", text);
        m_configManager->set("General", "gallery_cookies_from_browser", text);
        return;
    }

    if (m_cookieCheckProcess->state() != QProcess::NotRunning) {
        qWarning() << "A cookie check is already in progress. Terminating previous process.";
        m_cookieCheckProcess->terminate();
    }

    setCursor(Qt::WaitCursor);
    m_cookiesBrowserCombo->setEnabled(false);

    QStringList args;
    args << "--cookies-from-browser" << text.toLower()
         << "--simulate"
         << "--verbose"
         << "https://www.youtube.com/watch?v=7x52ID-2H0E";

    // Find deno.exe and add it to the arguments if found
    QString denoPath;
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString denoExePath1 = QDir(appDir).filePath("deno.exe");
    const QString denoExePath2 = QDir(appDir).filePath("bin/deno.exe");

    if (QFile::exists(denoExePath1)) {
        denoPath = denoExePath1;
    } else if (QFile::exists(denoExePath2)) {
        denoPath = denoExePath2;
    }

    if (!denoPath.isEmpty()) {
        QString jsRuntimeArg = QString("deno:%1").arg(denoPath); // Corrected: Removed manual quotes
        args << "--js-runtimes" << jsRuntimeArg;
    } else {
        qWarning() << "deno.exe not found in" << appDir << "or" << QDir(appDir).filePath("bin")
                   << "- JS challenges may fail.";
    }

    qDebug().noquote() << "Starting cookie check process. Program: yt-dlp, Arguments:" << args;
    m_cookieCheckProcess->start("yt-dlp", args);
}

void AdvancedSettingsTab::onCookieCheckProcessStarted()
{
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
             << "Cookie check process started. Setting 30-second timeout.";
    m_cookieCheckTimeoutTimer->start(30000); // 30 seconds
}

void AdvancedSettingsTab::onCookieCheckProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    m_cookieCheckTimeoutTimer->stop();
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
             << "Cookie check process finished. Exit Code:" << exitCode
             << "Exit Status:" << (exitStatus == QProcess::NormalExit ? "Normal" : "Crashed");

    unsetCursor();
    m_cookiesBrowserCombo->setEnabled(true);

    QString selectedBrowser = m_cookiesBrowserCombo->currentText();

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        QString stderrOutput = m_cookieCheckProcess->readAllStandardError();
        qWarning().noquote() << "Cookie check failed. Stderr:\n" << stderrOutput;
        QString errorMessage;

        if (stderrOutput.contains("Unable to get cookie info", Qt::CaseInsensitive) ||
            stderrOutput.contains("database is locked", Qt::CaseInsensitive)) {
            errorMessage = QString("Failed to access cookies for %1. The browser may be running, which can lock the cookie database. Please close %1 and try again.").arg(selectedBrowser);
        } else {
            errorMessage = QString("An unexpected error occurred while checking cookies for %1.\n\nDetails:\n%2").arg(selectedBrowser, stderrOutput);
        }

        QMessageBox::warning(this, "Cookie Access Failed", errorMessage);

        disconnect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onCookiesBrowserChanged);
        m_cookiesBrowserCombo->setCurrentText(m_lastSavedBrowser);
        connect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onCookiesBrowserChanged);
        qDebug() << "Reverted selection to last saved browser:" << m_lastSavedBrowser;
    } else {
        m_lastSavedBrowser = selectedBrowser;
        m_configManager->set("General", "cookies_from_browser", selectedBrowser);
        m_configManager->set("General", "gallery_cookies_from_browser", selectedBrowser);
        qDebug() << "Cookie check successful. Saved new browser:" << selectedBrowser;
    }
}

void AdvancedSettingsTab::onCookieCheckProcessErrorOccurred(QProcess::ProcessError error) {
    m_cookieCheckTimeoutTimer->stop();
    qCritical() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
                << "Cookie check process error occurred. Error:" << m_cookieCheckProcess->errorString();

    if (m_cookieCheckProcess->state() == QProcess::NotRunning) {
        return; // Avoid showing message if process was terminated by timeout
    }

    unsetCursor();
    m_cookiesBrowserCombo->setEnabled(true);

    QString errorMessage;
    if (error == QProcess::FailedToStart) {
        errorMessage = "Failed to start yt-dlp. Please ensure 'yt-dlp.exe' is in the application directory or 'bin/' folder.";
    } else {
        errorMessage = QString("An unknown error occurred with the yt-dlp process: %1").arg(m_cookieCheckProcess->errorString());
    }

    QMessageBox::critical(this, "Process Error", errorMessage);

    disconnect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onCookiesBrowserChanged);
    m_cookiesBrowserCombo->setCurrentText(m_lastSavedBrowser);
    connect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onCookiesBrowserChanged);
    qDebug() << "Reverted selection to last saved browser due to timeout:" << m_lastSavedBrowser;
}

void AdvancedSettingsTab::onCookieCheckProcessReadyReadStandardOutput()
{
    qDebug().noquote() << "yt-dlp stdout:" << m_cookieCheckProcess->readAllStandardOutput();
}

void AdvancedSettingsTab::onCookieCheckProcessReadyReadStandardError()
{
    qWarning().noquote() << "yt-dlp stderr:" << m_cookieCheckProcess->readAllStandardError();
}

void AdvancedSettingsTab::onCookieCheckTimeout()
{
    qCritical() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
                << "Cookie check process timed out after 30 seconds. Terminating process.";
    m_cookieCheckProcess->terminate();

    unsetCursor();
    m_cookiesBrowserCombo->setEnabled(true);

    QMessageBox::warning(this, "Cookie Check Timed Out",
                         QString("The cookie check for %1 took too long to respond. This can happen if the browser is slow to start. Please try again.").arg(m_cookiesBrowserCombo->currentText()));

    disconnect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onCookiesBrowserChanged);
    m_cookiesBrowserCombo->setCurrentText(m_lastSavedBrowser);
    connect(m_cookiesBrowserCombo, &QComboBox::currentTextChanged, this, &AdvancedSettingsTab::onCookiesBrowserChanged);
    qDebug() << "Reverted selection to last saved browser due to timeout:" << m_lastSavedBrowser;
}

void AdvancedSettingsTab::onExternalDownloaderToggled(bool checked) {
    m_configManager->set("Metadata", "use_aria2c", checked);
}

void AdvancedSettingsTab::onSponsorBlockToggled(bool checked) {
    m_configManager->set("General", "sponsorblock", checked);
}

void AdvancedSettingsTab::onEmbedChaptersToggled(bool checked) {
    m_configManager->set("Metadata", "embed_chapters", checked);
}

void AdvancedSettingsTab::onAutoPasteModeChanged(int index) {
    m_configManager->set("General", "auto_paste_mode", index);
}

void AdvancedSettingsTab::onSingleLineCommandPreviewToggled(bool checked) {
    m_configManager->set("General", "single_line_preview", checked);
}

void AdvancedSettingsTab::onRestrictFilenamesToggled(bool checked) {
    m_configManager->set("General", "restrict_filenames", checked);
}

void AdvancedSettingsTab::onEmbedMetadataToggled(bool checked) {
    m_configManager->set("Metadata", "embed_metadata", checked);
}

void AdvancedSettingsTab::onEmbedThumbnailToggled(bool checked) {
    m_configManager->set("Metadata", "embed_thumbnail", checked);
}

void AdvancedSettingsTab::onHighQualityThumbnailToggled(bool checked) {
    m_configManager->set("Metadata", "high_quality_thumbnail", checked);
}

void AdvancedSettingsTab::onConvertThumbnailsChanged(const QString &text) {
    m_configManager->set("Metadata", "convert_thumbnail_to", text);
}

void AdvancedSettingsTab::onSubtitleLanguageChanged(const QString &text) {
    QString langCode = m_subtitleLanguageCombo->currentData().toString();
    m_configManager->set("Subtitles", "languages", langCode);
}

void AdvancedSettingsTab::onEmbedSubtitlesToggled(bool checked) {
    m_configManager->set("Subtitles", "embed_subtitles", checked);
}

void AdvancedSettingsTab::onWriteSubtitlesToggled(bool checked) {
    m_configManager->set("Subtitles", "write_subtitles", checked);
}

void AdvancedSettingsTab::onIncludeAutoSubtitlesToggled(bool checked) {
    m_configManager->set("Subtitles", "write_auto_subtitles", checked);
}

void AdvancedSettingsTab::onSubtitleFormatChanged(const QString &text) {
    m_configManager->set("Subtitles", "format", text);
}

void AdvancedSettingsTab::updateSubtitleFormatAvailability(bool embedSubtitlesChecked) {
    m_subtitleFormatCombo->setDisabled(embedSubtitlesChecked);
    if (embedSubtitlesChecked) {
        m_subtitleFormatCombo->setToolTip("Subtitle file format is not applicable when subtitles are embedded in the video.");
    } else {
        m_subtitleFormatCombo->setToolTip("Choose the file format for your subtitles (e.g., 'srt' is a common text-based format).");
    }
}

void AdvancedSettingsTab::onVideoQualityChanged(const QString &text) {
    m_configManager->set("Video", "video_quality", text);
}

void AdvancedSettingsTab::onVideoCodecChanged(const QString &text) {
    m_configManager->set("Video", "video_codec", text);
    updateVideoOptions();
}

void AdvancedSettingsTab::onVideoExtChanged(const QString &text) {
    m_configManager->set("Video", "video_extension", text);
}

void AdvancedSettingsTab::onVideoAudioCodecChanged(const QString &text) {
    m_configManager->set("Video", "video_audio_codec", text);
}

void AdvancedSettingsTab::onAudioQualityChanged(const QString &text) {
    m_configManager->set("Audio", "audio_quality", text);
}

void AdvancedSettingsTab::onAudioCodecChanged(const QString &text) {
    m_configManager->set("Audio", "audio_codec", text);
    updateAudioOptions();
}

void AdvancedSettingsTab::onAudioExtChanged(const QString &text) {
    m_configManager->set("Audio", "audio_extension", text);
}

void AdvancedSettingsTab::updateVideoOptions() {
    QString selectedVideoCodec = m_videoCodecCombo->currentText();
    bool isDefaultCodec = (selectedVideoCodec == "Default");

    m_videoExtLabel->setVisible(!isDefaultCodec);
    m_videoExtCombo->setVisible(!isDefaultCodec);

    if (isDefaultCodec) {
        return;
    }

    QString currentExt = m_videoExtCombo->currentText();

    m_videoExtCombo->clear();
    if (selectedVideoCodec == "AV1" || selectedVideoCodec == "VP9") {
        m_videoExtCombo->addItems({"webm", "mkv"});
    } else if (selectedVideoCodec == "H.264 (AVC)" || selectedVideoCodec == "H.265 (HEVC)") {
        m_videoExtCombo->addItems({"mp4", "mkv"});
    } else if (selectedVideoCodec == "ProRes (Archive)") {
        m_videoExtCombo->addItem("mov");
    } else if (selectedVideoCodec == "Theora") {
        m_videoExtCombo->addItem("ogv");
    } else {
        m_videoExtCombo->addItems({"mp4", "mkv", "webm"});
    }

    if (m_videoExtCombo->findText(currentExt) != -1) {
        m_videoExtCombo->setCurrentText(currentExt);
    } else {
        m_videoExtCombo->setCurrentIndex(0);
    }

    QString currentAudioCodec = m_videoAudioCodecCombo->currentText();
    m_videoAudioCodecCombo->clear();
    if (selectedVideoCodec == "AV1" || selectedVideoCodec == "VP9") {
        m_videoAudioCodecCombo->addItems({"Default", "Opus", "Vorbis", "AAC"});
    } else if (selectedVideoCodec == "H.264 (AVC)" || selectedVideoCodec == "H.265 (HEVC)") {
        m_videoAudioCodecCombo->addItems({"Default", "AAC", "MP3", "FLAC", "PCM"});
    } else if (selectedVideoCodec == "ProRes (Archive)") {
        m_videoAudioCodecCombo->addItems({"Default", "PCM", "AAC"});
    } else if (selectedVideoCodec == "Theora") {
        m_videoAudioCodecCombo->addItems({"Default", "Vorbis"});
    } else {
        m_videoAudioCodecCombo->addItems({"Default", "AAC", "Opus", "Vorbis", "MP3", "FLAC", "PCM"});
    }

    if (m_videoAudioCodecCombo->findText(currentAudioCodec) != -1) {
        m_videoAudioCodecCombo->setCurrentText(currentAudioCodec);
    } else {
        m_videoAudioCodecCombo->setCurrentIndex(0);
    }
}

void AdvancedSettingsTab::updateAudioOptions() {
    QString selectedAudioCodec = m_audioCodecCombo->currentText();
    bool isDefaultCodec = (selectedAudioCodec == "Default");

    m_audioExtLabel->setVisible(!isDefaultCodec);
    m_audioExtCombo->setVisible(!isDefaultCodec);

    if (isDefaultCodec) {
        return;
    }

    QString currentExt = m_audioExtCombo->currentText();

    m_audioExtCombo->clear();
    if (selectedAudioCodec == "AAC") {
        m_audioExtCombo->addItems({"m4a", "aac"});
    } else if (selectedAudioCodec == "Opus") {
        m_audioExtCombo->addItem("opus");
    } else if (selectedAudioCodec == "Vorbis") {
        m_audioExtCombo->addItem("ogg");
    } else if (selectedAudioCodec == "MP3") {
        m_audioExtCombo->addItem("mp3");
    } else if (selectedAudioCodec == "FLAC") {
        m_audioExtCombo->addItem("flac");
    } else if (selectedAudioCodec == "WAV" || selectedAudioCodec == "PCM") {
        m_audioExtCombo->addItem("wav");
    } else if (selectedAudioCodec == "ALAC") {
        m_audioExtCombo->addItems({"m4a", "alac"});
    } else if (selectedAudioCodec == "AC3" || selectedAudioCodec == "EAC3" || selectedAudioCodec == "DTS") {
        m_audioExtCombo->addItems({"ac3", "eac3", "dts"});
    } else {
        m_audioExtCombo->addItems({"mp3", "m4a", "opus", "wav", "flac"});
    }

    if (m_audioExtCombo->findText(currentExt) != -1) {
        m_audioExtCombo->setCurrentText(currentExt);
    } else {
        m_audioExtCombo->setCurrentIndex(0);
    }
}
