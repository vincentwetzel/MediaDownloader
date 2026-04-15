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

    QVBoxLayout *urlInputLayout = new QVBoxLayout();
    QLabel *urlLabel = new QLabel("Video/Playlist URL(s):", parentWidget);
    urlLabel->setToolTip("Enter the URLs of the videos or playlists you want to download.");
    urlInputLayout->addWidget(urlLabel);

    m_urlInput = new QTextEdit(parentWidget);
    m_urlInput->setPlaceholderText("Paste one or more media URLs (one per line)...");
    m_urlInput->setToolTip("Paste the web address (URL) of the video or audio you want to download here. You can paste multiple links, just put each one on a new line.");
    m_urlInput->setMinimumHeight(100);
    urlInputLayout->addWidget(m_urlInput);
    applyUrlInputStyleSheet(m_urlInput);
    inputSectionLayout->addLayout(urlInputLayout, 70);

    QVBoxLayout *actionColumnLayout = new QVBoxLayout();
    actionColumnLayout->setSpacing(10);

    // Initialize missing UI Elements
    m_downloadTypeCombo = new QComboBox(parentWidget);
    m_downloadTypeCombo->addItem("Video", "video");
    m_downloadTypeCombo->addItem("Audio Only", "audio");
    m_downloadTypeCombo->addItem("Gallery", "gallery");
    m_downloadTypeCombo->addItem("View Formats", "formats");
    m_downloadTypeCombo->setToolTip("Select the type of download.");
    actionColumnLayout->addWidget(m_downloadTypeCombo);

    m_downloadButton = new QPushButton("Download Video", parentWidget);
    m_downloadButton->setMinimumHeight(40);
    actionColumnLayout->addWidget(m_downloadButton);

    QFormLayout *settingsLayout = new QFormLayout();
    
    m_playlistLogicCombo = new QComboBox(parentWidget);
    m_playlistLogicCombo->addItems({"Ask", "Download All (no prompt)", "Download Single (ignore playlist)"});
    settingsLayout->addRow("Playlist Logic:", m_playlistLogicCombo);

    m_maxConcurrentCombo = new QComboBox(parentWidget);
    m_maxConcurrentCombo->addItems({"1", "2", "3", "4", "5", "6", "7", "8", "1 (short sleep)", "1 (long sleep)"});
    settingsLayout->addRow("Max Concurrent:", m_maxConcurrentCombo);

    m_rateLimitCombo = new QComboBox(parentWidget);
    m_rateLimitCombo->addItems({"Unlimited", "500 KB/s", "1 MB/s", "2 MB/s", "5 MB/s", "10 MB/s"});
    settingsLayout->addRow("Rate Limit:", m_rateLimitCombo);

    m_overrideDuplicateCheck = new ToggleSwitch(parentWidget);
    settingsLayout->addRow("Override Archive:", m_overrideDuplicateCheck);

    actionColumnLayout->addLayout(settingsLayout);
    inputSectionLayout->addLayout(actionColumnLayout, 30);
    mainLayout->addLayout(inputSectionLayout);

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