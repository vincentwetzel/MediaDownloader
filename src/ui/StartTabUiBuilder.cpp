#include "StartTabUiBuilder.h"
#include "core/ConfigManager.h"
#include "ToggleSwitch.h"
#include <QLabel>
#include <QMessageBox>
#include <QDesktopServices>
#include <QDir>
#include <QFormLayout>
#include <QClipboard>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleFactory> // For QStyleHints
#include <QPushButton>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>

StartTabUiBuilder::StartTabUiBuilder(ConfigManager *configManager, QObject *parent)
    : QObject(parent), m_configManager(configManager),
      m_urlInput(nullptr), m_downloadButton(nullptr), m_downloadTypeCombo(nullptr),
      m_playlistLogicCombo(nullptr), m_maxConcurrentCombo(nullptr), m_rateLimitCombo(nullptr),
      m_overrideDuplicateCheck(nullptr), m_commandPreview(nullptr), m_openDownloadsFolderButton(nullptr)
{
}

void StartTabUiBuilder::build(QWidget *parentWidget, QVBoxLayout *mainLayout)
{
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QHBoxLayout *topLayout = new QHBoxLayout();

    QLabel *urlLabel = new QLabel("Video/Playlist URL(s):", parentWidget);
    urlLabel->setToolTip("Enter the URLs of the videos or playlists you want to download.");
    topLayout->addWidget(urlLabel);

    topLayout->addStretch();

    QPushButton *openTempFolderButton = new QPushButton("Open Temporary Folder", parentWidget);
    openTempFolderButton->setToolTip("Click here to open the folder where active downloads are temporarily stored.");
    connect(openTempFolderButton, &QPushButton::clicked, this, [this, parentWidget]() {
        QString tempDir = m_configManager->get("Paths", "temporary_downloads_directory").toString();
        if (tempDir.isEmpty() || !QDir(tempDir).exists()) {
            QMessageBox::warning(parentWidget, "Folder Not Found",
                                 "The temporary downloads directory is not set or does not exist.\n"
                                 "Please configure it in the Advanced Settings tab.");
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(tempDir));
    });
    topLayout->addWidget(openTempFolderButton);

    m_openDownloadsFolderButton = new QPushButton("Open Downloads Folder", parentWidget);
    m_openDownloadsFolderButton->setToolTip("Click here to open the folder where all your finished downloads are saved.");
    topLayout->addWidget(m_openDownloadsFolderButton);
    mainLayout->addLayout(topLayout);

    QHBoxLayout *inputSectionLayout = new QHBoxLayout();

    m_urlInput = new QTextEdit(parentWidget);
    m_urlInput->setPlaceholderText("Paste one or more media URLs (one per line)...");
    m_urlInput->setToolTip("Paste the web address (URL) of the video or audio you want to download here. You can paste multiple links, just put each one on a new line.");
    m_urlInput->setMinimumHeight(100);
    applyUrlInputStyleSheet(m_urlInput);
    inputSectionLayout->addWidget(m_urlInput, 70);

    QVBoxLayout *actionColumnLayout = new QVBoxLayout();
    actionColumnLayout->setSpacing(10);

    actionColumnLayout->addStretch();

    m_downloadButton = new QPushButton("Download Video", parentWidget);
    m_downloadButton->setMinimumHeight(100);
    actionColumnLayout->addWidget(m_downloadButton);

    actionColumnLayout->addStretch();

    QLabel *downloadTypeLabel = new QLabel("Download Type:", parentWidget);
    downloadTypeLabel->setToolTip("Select the type of download.");
    actionColumnLayout->addWidget(downloadTypeLabel);

    m_downloadTypeCombo = new QComboBox(parentWidget);
    m_downloadTypeCombo->addItem("Video", "video");
    m_downloadTypeCombo->addItem("Audio Only", "audio");
    m_downloadTypeCombo->addItem("Gallery", "gallery");
    m_downloadTypeCombo->addItem("View Formats", "formats");
    m_downloadTypeCombo->setToolTip("Select the type of download.");
    actionColumnLayout->addWidget(m_downloadTypeCombo);

    connect(m_downloadTypeCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (text == "Video") {
            m_downloadButton->setText("Download Video");
        } else if (text == "Audio Only") {
            m_downloadButton->setText("Download Audio");
        } else if (text == "Gallery") {
            m_downloadButton->setText("Download Gallery");
        } else if (text == "View Formats") {
            m_downloadButton->setText("View Formats");
        }
    });

    actionColumnLayout->addSpacing(20);

    inputSectionLayout->addLayout(actionColumnLayout, 30);
    mainLayout->addLayout(inputSectionLayout);

    QHBoxLayout *settingsLayout = new QHBoxLayout();

    QLabel *playlistLabel = new QLabel("Playlist Logic:", parentWidget);
    settingsLayout->addWidget(playlistLabel);
    m_playlistLogicCombo = new QComboBox(parentWidget);
    m_playlistLogicCombo->addItems({"Ask", "Download All (no prompt)", "Download Single (ignore playlist)"});
    m_playlistLogicCombo->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    m_playlistLogicCombo->setMinimumContentsLength(10);
    m_playlistLogicCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    settingsLayout->addWidget(m_playlistLogicCombo);

    QLabel *concurrentLabel = new QLabel("Max Concurrent:", parentWidget);
    settingsLayout->addWidget(concurrentLabel);
    m_maxConcurrentCombo = new QComboBox(parentWidget);
    m_maxConcurrentCombo->addItems({"1", "2", "3", "4", "5", "6", "7", "8", "1 (short sleep)", "1 (long sleep)"});
    m_maxConcurrentCombo->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    settingsLayout->addWidget(m_maxConcurrentCombo);

    QLabel *rateLabel = new QLabel("Rate Limit:", parentWidget);
    settingsLayout->addWidget(rateLabel);
    m_rateLimitCombo = new QComboBox(parentWidget);
    m_rateLimitCombo->addItems({"Unlimited", "500 KB/s", "1 MB/s", "2 MB/s", "5 MB/s", "10 MB/s"});
    m_rateLimitCombo->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    settingsLayout->addWidget(m_rateLimitCombo);

    QLabel *overrideLabel = new QLabel("Override Archive:", parentWidget);
    settingsLayout->addWidget(overrideLabel);
    m_overrideDuplicateCheck = new ToggleSwitch(parentWidget);
    settingsLayout->addWidget(m_overrideDuplicateCheck);

    mainLayout->addLayout(settingsLayout);

    QLabel *previewLabel = new QLabel("Command Preview:", parentWidget);
    mainLayout->addWidget(previewLabel);

    m_commandPreview = new QTextEdit(parentWidget);
    m_commandPreview->setReadOnly(true);
    m_commandPreview->setMaximumHeight(80);
    mainLayout->addWidget(m_commandPreview);
}

void StartTabUiBuilder::applyUrlInputStyleSheet(QTextEdit *urlInput)
{
    // Implement style sheet logic here, or leave empty if handled by global themes
}