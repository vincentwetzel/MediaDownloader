#include "MainWindow.h"
#include "StartTab.h"
#include "ActiveDownloadsTab.h"
#include "AdvancedSettingsTab.h"
#include "SortingTab.h"

// Include full definitions for forward-declared classes
#include "core/ConfigManager.h"
#include "core/ArchiveManager.h"
#include "core/DownloadManager.h"
#include "core/AppUpdater.h"
#include "core/UrlValidator.h"
#include "core/UpdateStatus.h" // Include UpdateStatus enum
#include "core/StartupWorker.h"
#include "utils/ExtractorJsonParser.h"
#include "YtDlpJsonExtractor.h"
#include "core/ProcessUtils.h"
#include "ui/RuntimeSelectionDialog.h"
#include "ui/FormatSelectionDialog.h"
#include "ToggleSwitch.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QStatusBar>
#include <QLabel>
#include <QDebug>
#include <QMessageBox>
#include <QTimer>
#include <QFile>
#include <QCoreApplication>
#include <QIcon>
#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QPixmap>
#include <QImageReader>
#include <QFileDialog>
#include <QEvent>
#include <QStyleHints>
#include <QComboBox>
#include <QSizePolicy>
#include <QStandardPaths>

const QString APP_VERSION = "1.0.0";
const QString REPO_URL = "https://api.github.com/repos/vincentwetzel/MediaDownloader";
const QString GITHUB_PROJECT_URL = "https://github.com/vincentwetzel/MediaDownloader";
const QString DEVELOPER_DISCORD_URL_PART1 = "https://discord.gg/";
const QString DEVELOPER_DISCORD_URL_PART2 = "NfWaqK";
const QString DEVELOPER_DISCORD_URL_PART3 = "gYRG";

MainWindow::MainWindow(ExtractorJsonParser *extractorJsonParser, QWidget *parent)
    : QMainWindow(parent),
      m_configManager(nullptr),
      m_archiveManager(nullptr),
      m_downloadManager(nullptr),
      m_appUpdater(nullptr),
      m_urlValidator(nullptr),
      m_startupWorker(nullptr),
      m_startupThread(nullptr),
      m_extractorJsonParser(extractorJsonParser),
      m_runtimeExtractor(nullptr),
      m_clipboard(nullptr),
      m_tabWidget(nullptr),
      m_activeDownloadsTab(nullptr),
      m_advancedSettingsTab(nullptr),
      m_startTab(nullptr),
      m_speedLabel(nullptr),
      m_queuedDownloadsLabel(nullptr),
      m_activeDownloadsLabel(nullptr),
      m_completedDownloadsLabel(nullptr),
      m_trayIcon(nullptr),
      m_trayMenu(nullptr),
      m_silentUpdateCheck(false)
{
    // Initialize core components
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty()) {
        configDir = QCoreApplication::applicationDirPath(); // Fallback
    }
    QDir().mkpath(configDir);
    QString configPath = QDir(configDir).filePath("settings.ini");
    qInfo() << "Using settings file at:" << configPath;

    m_configManager = new ConfigManager(configPath, this);
    m_archiveManager = new ArchiveManager(m_configManager, this);
    m_downloadManager = new DownloadManager(m_configManager, this);
    m_appUpdater = new AppUpdater(REPO_URL, APP_VERSION, this);
    m_urlValidator = new UrlValidator(m_configManager, this);
    m_clipboard = QApplication::clipboard(); // Initialize QClipboard

    // Create worker and thread but do not parent the worker to MainWindow
    m_startupWorker = new StartupWorker(m_configManager, m_extractorJsonParser, nullptr);
    m_startupThread = new QThread(this);

    m_runtimeExtractor = new YtDlpJsonExtractor(this);
    connect(m_runtimeExtractor, &YtDlpJsonExtractor::extractionSuccess, this,
            [this](const QString &, const QString &, const QList<DownloadTarget> &, const QString &, const QMap<QString, QString> &, const QVariantMap &metadata) {
                onRuntimeInfoReady(metadata);
            });
    connect(m_runtimeExtractor, &YtDlpJsonExtractor::extractionFailed, this, &MainWindow::onRuntimeInfoError);

    // Apply theme before UI setup
    applyTheme(m_configManager->get("General", "theme", "System").toString());

    setupUI();
    setupTrayIcon();

    // Ensure completed downloads directory is set
    QString completedDownloadsDir = m_configManager->get("Paths", "completed_downloads_directory").toString();
    if (completedDownloadsDir.isEmpty()) {
        QMessageBox::information(this, "Setup Required",
                                 "Please select a directory for completed downloads. This will also set up a temporary downloads directory.");
        QString selectedDir = QFileDialog::getExistingDirectory(this, "Select Completed Downloads Directory",
                                                                QDir::homePath(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!selectedDir.isEmpty()) {
            if (m_configManager->set("Paths", "completed_downloads_directory", selectedDir)) {
                m_configManager->save();
            }
            completedDownloadsDir = selectedDir;
            QMessageBox::information(this, "Directory Set",
                                     QString("Completed downloads directory set to:\n%1\n\nTemporary downloads directory set to:\n%2")
                                         .arg(completedDownloadsDir)
                                         .arg(QDir(completedDownloadsDir).filePath("temp_downloads")));
        } else {
            QMessageBox::warning(this, "Directory Not Set",
                                 "No completed downloads directory was selected. Please set it in Advanced Settings to enable downloads.");
        }
    }

    // Ensure temporary downloads directory is set if completed is available but temp is not
    QString temporaryDownloadsDir = m_configManager->get("Paths", "temporary_downloads_directory").toString();
    if (!completedDownloadsDir.isEmpty() && temporaryDownloadsDir.isEmpty()) {
        QString defaultTempDir = QDir(completedDownloadsDir).filePath("temp_downloads");
        if (m_configManager->set("Paths", "temporary_downloads_directory", defaultTempDir)) {
            m_configManager->save();
            qInfo() << "Automatically set missing temporary_downloads_directory to" << defaultTempDir;
        }
    }

    // Connect signals
    connect(m_downloadManager, &DownloadManager::downloadAddedToQueue,
            m_activeDownloadsTab, &ActiveDownloadsTab::addDownloadItem);
    connect(m_downloadManager, &DownloadManager::downloadProgress,
            m_activeDownloadsTab, &ActiveDownloadsTab::updateDownloadProgress);
    connect(m_downloadManager, &DownloadManager::downloadFinished,
            m_activeDownloadsTab, &ActiveDownloadsTab::onDownloadFinished);
    connect(m_downloadManager, &DownloadManager::downloadCancelled,
            m_activeDownloadsTab, &ActiveDownloadsTab::onDownloadCancelled);
    connect(m_downloadManager, &DownloadManager::downloadStarted,
            m_activeDownloadsTab, [this](const QString &id){
                m_activeDownloadsTab->setDownloadStatus(id, "Downloading...");
            });
    connect(m_downloadManager, &DownloadManager::downloadPaused,
            m_activeDownloadsTab, &ActiveDownloadsTab::onDownloadPaused);
    connect(m_downloadManager, &DownloadManager::downloadResumed,
            m_activeDownloadsTab, &ActiveDownloadsTab::onDownloadResumed);
    connect(m_downloadManager, &DownloadManager::downloadFinalPathReady,
            m_activeDownloadsTab, &ActiveDownloadsTab::onDownloadFinalPathReady);
    connect(m_downloadManager, &DownloadManager::playlistExpansionStarted,
            m_activeDownloadsTab, &ActiveDownloadsTab::addExpandingPlaylist);
    connect(m_downloadManager, &DownloadManager::playlistExpansionFinished,
            m_activeDownloadsTab, &ActiveDownloadsTab::removeExpandingPlaylist);
    connect(m_downloadManager, &DownloadManager::queueFinished, this, &MainWindow::onQueueFinished);
    connect(m_downloadManager, &DownloadManager::totalSpeedUpdated, this, &MainWindow::updateTotalSpeed);
    connect(m_downloadManager, &DownloadManager::videoQualityWarning, this, &MainWindow::onVideoQualityWarning);
    connect(m_downloadManager, &DownloadManager::downloadStatsUpdated, this, &MainWindow::onDownloadStatsUpdated);

    // Handle requests for runtime format selection
    connect(m_downloadManager, &DownloadManager::formatSelectionRequested, this, 
        [this](const QString &url, const QVariantMap &options, const QVariantMap &infoDict) {
            FormatSelectionDialog dialog(infoDict, options, this);
            if (dialog.exec() == QDialog::Accepted) {
                QStringList selectedFormats = dialog.getSelectedFormatIds();
                if (!selectedFormats.isEmpty()) {
                    // The dialog allows selecting multiple formats, and the user expects
                    // each to be enqueued as a separate download.
                    for (const QString &formatId : selectedFormats) {
                        QVariantMap newOptions = options;
                        newOptions["format"] = formatId;
                        m_downloadManager->enqueueDownload(url, newOptions);
                    }
                    m_tabWidget->setCurrentWidget(m_activeDownloadsTab);
                }
            }
        });

    connect(m_activeDownloadsTab, &ActiveDownloadsTab::cancelDownloadRequested,
            m_downloadManager, &DownloadManager::cancelDownload);
    connect(m_activeDownloadsTab, &ActiveDownloadsTab::retryDownloadRequested,
            m_downloadManager, &DownloadManager::retryDownload);
    connect(m_activeDownloadsTab, &ActiveDownloadsTab::resumeDownloadRequested,
            m_downloadManager, &DownloadManager::resumeDownload);
    connect(m_activeDownloadsTab, &ActiveDownloadsTab::pauseDownloadRequested,
            m_downloadManager, &DownloadManager::pauseDownload);
    connect(m_activeDownloadsTab, &ActiveDownloadsTab::unpauseDownloadRequested,
            m_downloadManager, &DownloadManager::unpauseDownload);
    connect(m_activeDownloadsTab, &ActiveDownloadsTab::moveDownloadUpRequested,
            m_downloadManager, &DownloadManager::moveDownloadUp);
    connect(m_activeDownloadsTab, &ActiveDownloadsTab::moveDownloadDownRequested,
            m_downloadManager, &DownloadManager::moveDownloadDown);

    connect(m_urlValidator, &UrlValidator::validationFinished, this, &MainWindow::onValidationFinished);

    // Setup startup worker threading
    m_startupWorker->moveToThread(m_startupThread);
    connect(m_startupThread, &QThread::started, m_startupWorker, &StartupWorker::start);
    connect(m_startupWorker, &StartupWorker::finished, m_startupThread, &QThread::quit);
    // DO NOT connect deleteLater here. It will be handled manually in the destructor.
    connect(m_startupWorker, &StartupWorker::binariesChecked, this, [this](const QStringList &missingBinaries){
        if (!missingBinaries.isEmpty()) {
            QMessageBox msgBox(this);
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setWindowTitle("Missing Required Binaries");
            msgBox.setText("The following required binaries could not be found:\n" + missingBinaries.join(", "));
            msgBox.setInformativeText("MediaDownloader requires these tools to function correctly.\n\n"
                                      "You can download them from their official websites and place them in the 'bin' folder next to MediaDownloader.exe, "
                                      "or specify their custom installed locations in 'Advanced Settings -> External Binaries'.");
            msgBox.exec();
        }
    });
    connect(m_startupWorker, &StartupWorker::ytDlpVersionFetched, this, &MainWindow::setYtDlpVersion);
    connect(m_startupWorker, &StartupWorker::galleryDlVersionFetched, m_advancedSettingsTab, &AdvancedSettingsTab::setGalleryDlVersion);

    // Connect clipboard signal
    connect(m_clipboard, &QClipboard::changed, this, &MainWindow::onClipboardChanged);

    startStartupChecks();
}

MainWindow::~MainWindow() {
    if (m_startupWorker) {
        QMetaObject::invokeMethod(m_startupWorker, "deleteLater", Qt::QueuedConnection);
    }
    if (m_startupThread && m_startupThread->isRunning()) {
        m_startupThread->quit();
        m_startupThread->wait();
    }
}

void MainWindow::setupUI() {
    setWindowTitle("MediaDownloader v" + APP_VERSION);
    resize(800, 600);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget);
    
    QComboBox *languageCombo = new QComboBox(this);
    languageCombo->setToolTip("Select Application Language");
    QStringList languagesWithFlags = {
        "🇺🇸 English", "🇪🇸 Spanish", "🇵🇹 Portuguese", "🇷🇺 Russian", "🇩🇪 German", "🇫🇷 French",
        "🇮🇹 Italian", "🇨🇳 Mandarin", "🇮🇳 Hindi", "🇧🇩 Bengali", "🇯🇵 Japanese", "🇵🇰 Western Punjabi",
        "🇹🇷 Turkish", "🇻🇳 Vietnamese", "🇭🇰 Yue Chinese", "🇪🇬 Egyptian Arabic", "🇨🇳 Wu Chinese",
        "🇮🇳 Marathi", "🇮🇳 Telugu", "🇰🇷 Korean", "🇮🇳 Tamil", "🇵🇰 Urdu", "🇮🇩 Indonesian",
        "🇮🇩 Javanese", "🇮🇷 Iranian Persian", "🇳🇬 Hausa", "🇮🇳 Gujarati", "🇱🇧 Levantine Arabic",
        "🇮🇳 Bhojpuri"
    };
    languageCombo->addItems(languagesWithFlags);
    
    QString savedLang = m_configManager->get("General", "language", "🇺🇸 English").toString();
    int index = languageCombo->findText(savedLang);
    if (index >= 0) languageCombo->setCurrentIndex(index);
    m_tabWidget->setCornerWidget(languageCombo, Qt::TopRightCorner);
    connect(languageCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (m_configManager->get("General", "language", "🇺🇸 English").toString() != text) {
            m_configManager->set("General", "language", text);
            QMessageBox::information(this, "Language Changed", "Language changed to " + text + ".\n\nPlease restart the application for changes to take full effect.");
        }
    });

    m_startTab = new StartTab(m_configManager, m_extractorJsonParser, this);
    m_activeDownloadsTab = new ActiveDownloadsTab(m_configManager, this);
    m_advancedSettingsTab = new AdvancedSettingsTab(m_configManager, this);
    SortingTab *sortingTab = new SortingTab(m_configManager, this);

    m_tabWidget->addTab(m_startTab, "Start Download");
    m_tabWidget->addTab(m_activeDownloadsTab, "Active Downloads");
    m_tabWidget->addTab(sortingTab, "Sorting Rules");
    m_tabWidget->addTab(m_advancedSettingsTab, "Advanced Settings");

    // Dynamically adjust size policies to prevent hidden tabs from forcing a large minimum window width.
    // This ensures that only the currently visible tab influences the window's minimum size.
    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            if (QWidget *widget = m_tabWidget->widget(i)) {
                widget->setSizePolicy(i == index ? QSizePolicy::Preferred : QSizePolicy::Ignored, QSizePolicy::Preferred);
            }
        }
        m_tabWidget->currentWidget()->updateGeometry();
    });

    // Set initial state for all but the first tab
    for (int i = 1; i < m_tabWidget->count(); ++i) {
        if (QWidget *widget = m_tabWidget->widget(i)) {
            widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        }
    }

    QVBoxLayout *footerContainer = new QVBoxLayout();
    QHBoxLayout *footerTopRow = new QHBoxLayout();
    QLabel *githubLink = new QLabel(QString("<a href=\"%1\">Source Code</a>").arg(GITHUB_PROJECT_URL));
    githubLink->setOpenExternalLinks(true);
    githubLink->setToolTip("Visit the project's source code repository on GitHub.");

    QLabel *discordLink = new QLabel(QString("<a href=\"%1%2%3\"><img src=\":/ui/assets/discord.png\" alt=\"Developer Discord\" width=\"24\" height=\"24\"></a>")
                                         .arg(DEVELOPER_DISCORD_URL_PART1)
                                         .arg(DEVELOPER_DISCORD_URL_PART2)
                                         .arg(DEVELOPER_DISCORD_URL_PART3));
    discordLink->setOpenExternalLinks(true);
    discordLink->setToolTip("Join the developer Discord server.");

    m_queuedDownloadsLabel = new QLabel("Queued: 0", this);
    m_queuedDownloadsLabel->setToolTip("Number of downloads waiting to start.");
    m_activeDownloadsLabel = new QLabel("Active: 0", this);
    m_activeDownloadsLabel->setToolTip("Number of currently active downloads.");
    m_completedDownloadsLabel = new QLabel("Completed: 0", this);
    m_completedDownloadsLabel->setToolTip("Number of successfully completed downloads.");
    m_speedLabel = new QLabel("Current Speed: 0.00 MB/s", this);
    m_speedLabel->setToolTip("Total download speed across all active transfers.");

    QLabel *exitAfterLabel = new QLabel("Exit after all downloads complete:", this);
    ToggleSwitch *exitAfterSwitch = new ToggleSwitch(this);
    exitAfterSwitch->setToolTip("If switched on, the application will automatically close once all your downloads are finished.");
    exitAfterSwitch->setChecked(m_configManager->get("General", "exit_after", false).toBool());

    connect(exitAfterSwitch, &ToggleSwitch::toggled, this, [this](bool checked){
        m_configManager->set("General", "exit_after", checked);
        m_configManager->save();
    });

    footerTopRow->addWidget(githubLink);
    footerTopRow->addSpacing(20);
    footerTopRow->addWidget(discordLink);
    footerTopRow->addStretch();
    footerTopRow->addWidget(exitAfterLabel);
    footerTopRow->addWidget(exitAfterSwitch);

    QHBoxLayout *footerBottomRow = new QHBoxLayout();
    footerBottomRow->addStretch();
    footerBottomRow->addWidget(m_queuedDownloadsLabel);
    footerBottomRow->addSpacing(10);
    footerBottomRow->addWidget(m_activeDownloadsLabel);
    footerBottomRow->addSpacing(10);
    footerBottomRow->addWidget(m_completedDownloadsLabel);
    footerBottomRow->addSpacing(20);
    footerBottomRow->addWidget(m_speedLabel);

    footerContainer->addLayout(footerTopRow);
    footerContainer->addLayout(footerBottomRow);
    mainLayout->addLayout(footerContainer);

    connect(m_startTab, &StartTab::downloadRequested, this, &MainWindow::onDownloadRequested);
    connect(m_advancedSettingsTab, &AdvancedSettingsTab::themeChanged, this, &MainWindow::applyTheme);
}

void MainWindow::setupTrayIcon() {
    m_trayIcon = new QSystemTrayIcon(QIcon(":/app-icon"), this);
    m_trayIcon->setToolTip("MediaDownloader");

    m_trayMenu = new QMenu(this);
    QAction *showAction = m_trayMenu->addAction("Show");
    connect(showAction, &QAction::triggered, this, &QWidget::showNormal);
    QAction *quitAction = m_trayMenu->addAction("Quit");
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!m_configManager) {
        event->accept();
        return;
    }

    QString tempDir = m_configManager->get("Paths", "temporary_downloads_directory").toString();
    if (!tempDir.isEmpty() && QDir(tempDir).exists()) {
        QStringList files = QDir(tempDir).entryList(QDir::Files);
        if (!files.isEmpty()) {
            QMessageBox::StandardButton reply;
            reply = QMessageBox::warning(this, "Temporary Files Found",
                                         "The temporary directory is not empty. This may indicate incomplete downloads. Do you want to exit anyway?",
                                         QMessageBox::Yes|QMessageBox::No);
            if (reply == QMessageBox::No) {
                event->ignore();
                return;
            }
        }
    }

    qInfo() << "Main window close requested; exiting application.";
    if (m_trayIcon && m_trayIcon->isVisible()) {
        m_trayIcon->hide();
    }
    event->accept();
}

bool MainWindow::event(QEvent *event)
{
    bool handled = QMainWindow::event(event);
    if (!m_configManager) {
        return handled;
    }

    int autoPasteMode = m_configManager->get("General", "auto_paste_mode", 0).toInt();

    if (event->type() == QEvent::WindowActivate || event->type() == QEvent::Enter) {
        if (autoPasteMode == 1) { // Auto-paste on app focus
            handleClipboardAutoPaste(false);
        } else if (autoPasteMode == 3) { // Auto-paste on app focus & enqueue
            handleClipboardAutoPaste(true);
        }
    }
    return handled;
}

void MainWindow::onClipboardChanged()
{
    if (!m_configManager) {
        return;
    }

    int autoPasteMode = m_configManager->get("General", "auto_paste_mode", 0).toInt();

    if (autoPasteMode == 2) { // Auto-paste on new URL in clipboard
        handleClipboardAutoPaste(false);
    } else if (autoPasteMode == 4) { // Auto-paste on new URL & enqueue
        handleClipboardAutoPaste(true);
    }
}

void MainWindow::handleClipboardAutoPaste(bool forceEnqueue)
{
    if (!m_startTab || !m_tabWidget || !m_startTab->isEnabled()) {
        return;
    }

    if (m_startTab->tryAutoPasteFromClipboard()) {
        if (m_tabWidget->currentWidget() != m_startTab) {
            m_tabWidget->setCurrentWidget(m_startTab);
        }
        m_startTab->focusUrlInput();
        if (forceEnqueue) {
            m_startTab->onDownloadButtonClicked(); // Trigger download
        }
    }
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        if (isVisible()) {
            hide();
        } else {
            showNormal();
            activateWindow();
        }
    }
}

void MainWindow::onDownloadRequested(const QString &url, const QVariantMap &options) {
    if (!m_pendingUrl.isEmpty()) {
        QMessageBox::warning(this, "Please Wait", "Currently fetching info for another download.");
        return;
    }

    bool runtimeVideo = m_configManager->get("Video", "video_multistreams", "Default Stream").toString() == "Select at Runtime" && options.value("type").toString() == "video";
    bool runtimeAudio = m_configManager->get("Audio", "audio_multistreams", "Default Stream").toString() == "Select at Runtime"; // Audio tracks apply to video and audio types
    bool runtimeSubs = m_configManager->get("Subtitles", "languages", "en").toString().split(',').contains("runtime");

    if (runtimeVideo || runtimeAudio || runtimeSubs) {
        m_pendingUrl = url;
        m_pendingOptions = options;
        statusBar()->showMessage("Fetching media info for runtime selection...");
        QString ytDlpPath = ProcessUtils::findBinary("yt-dlp", m_configManager).path;
        QStringList args;
        args << "--dump-json" << "--no-playlist" << url;
        m_runtimeExtractor->extract(ytDlpPath, args);
        return;
    }

    static QRegularExpression fastTrackRe(R"(^(https?://)?(www\.)?(youtube\.com|youtu\.be|music\.youtube\.com|tiktok\.com|instagram\.com|twitter\.com|x\.com)/)");
    if (fastTrackRe.match(url).hasMatch()) {
        m_downloadManager->enqueueDownload(url, options);
        m_tabWidget->setCurrentWidget(m_activeDownloadsTab);
        return;
    }

    m_pendingUrl = url;
    m_pendingOptions = options;

    m_urlValidator->validate(url);
}

void MainWindow::onRuntimeInfoReady(const QVariantMap &info) {
    statusBar()->clearMessage();
    bool runtimeVideo = m_configManager->get("Video", "video_multistreams", "Default Stream").toString() == "Select at Runtime" && m_pendingOptions.value("type").toString() == "video";
    bool runtimeAudio = m_configManager->get("Audio", "audio_multistreams", "Default Stream").toString() == "Select at Runtime";
    bool runtimeSubs = m_configManager->get("Subtitles", "languages", "en").toString().split(',').contains("runtime");

    RuntimeSelectionDialog dialog(info, runtimeVideo, runtimeAudio, runtimeSubs, this);
    if (dialog.exec() == QDialog::Accepted) {
        QVariantMap opts = m_pendingOptions;
        if (runtimeVideo) opts["runtime_video_format"] = dialog.getSelectedVideoFormat();
        if (runtimeAudio) opts["runtime_audio_format"] = dialog.getSelectedAudioFormat();
        if (runtimeSubs) {
            QStringList subs = dialog.getSelectedSubtitles();
            if (!subs.isEmpty()) opts["runtime_subtitles"] = subs.join(',');
        }
        m_downloadManager->enqueueDownload(m_pendingUrl, opts);
        m_tabWidget->setCurrentWidget(m_activeDownloadsTab);
    }
    m_pendingUrl.clear();
    m_pendingOptions.clear();
}

void MainWindow::onRuntimeInfoError(const QString &error) {
    statusBar()->clearMessage();
    QMessageBox::warning(this, "Extraction Error", "Failed to fetch media info for runtime selection:\n" + error);
    m_pendingUrl.clear();
    m_pendingOptions.clear();
}

void MainWindow::onValidationFinished(bool isValid, const QString &error) {
    if (isValid) {
        m_downloadManager->enqueueDownload(m_pendingUrl, m_pendingOptions);
        m_tabWidget->setCurrentWidget(m_activeDownloadsTab);
    } else {
        QMessageBox::warning(this, "Invalid URL", "The URL could not be validated:\n" + error);
    }
    m_pendingUrl.clear();
    m_pendingOptions.clear();
}

void MainWindow::onQueueFinished() {
    if (!m_configManager) {
        return;
    }

    // Notify the user that the queue has finished
    if (m_trayIcon && m_trayIcon->isVisible()) {
        m_trayIcon->showMessage("Downloads Complete", "All queued media downloads have finished.", QSystemTrayIcon::Information, 3000);
    }

    bool exitAfter = m_pendingOptions.value("exit_after", m_configManager->get("General", "exit_after", false)).toBool();
    if (exitAfter) {
        QCoreApplication::quit();
    }
}

void MainWindow::startStartupChecks() {
    m_startupThread->start();
}

void MainWindow::onVideoQualityWarning(const QString &url, const QString &message) {
    QMessageBox::warning(this, "Low Quality Video",
                         QString("The following video was downloaded at a low quality:\n%1\n\n%2").arg(url, message));
}

void MainWindow::applyTheme(const QString &themeName) {
    qApp->setStyle(QStyleFactory::create("Fusion"));

    bool useDarkTheme = false;
    if (themeName == "Dark") {
        useDarkTheme = true;
    } else if (themeName == "System") {
        const auto colorScheme = qApp->styleHints()->colorScheme();
        useDarkTheme = (colorScheme == Qt::ColorScheme::Dark);
    }

    if (useDarkTheme) {
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        darkPalette.setColor(QPalette::Mid, QColor(40, 40, 40));
        qApp->setPalette(darkPalette);
    } else { // Light theme
        QPalette lightPalette(QColor(240, 240, 240));
        lightPalette.setColor(QPalette::WindowText, Qt::black);
        lightPalette.setColor(QPalette::Base, Qt::white);
        lightPalette.setColor(QPalette::AlternateBase, QColor(246, 246, 246));
        lightPalette.setColor(QPalette::ToolTipBase, Qt::white);
        lightPalette.setColor(QPalette::ToolTipText, Qt::black);
        lightPalette.setColor(QPalette::Text, Qt::black);
        lightPalette.setColor(QPalette::ButtonText, Qt::black);
        lightPalette.setColor(QPalette::BrightText, Qt::red);
        lightPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        lightPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        lightPalette.setColor(QPalette::HighlightedText, Qt::white);
        qApp->setPalette(lightPalette);
    }
}

void MainWindow::updateTotalSpeed(double speed) {
    double speedMb = speed / (1024.0 * 1024.0);
    m_speedLabel->setText(QString("Current Speed: %1 MB/s").arg(speedMb, 0, 'f', 2));
}

void MainWindow::onDownloadStatsUpdated(int queued, int active, int completed) {
    m_queuedDownloadsLabel->setText(QString("Queued: %1").arg(queued));
    m_activeDownloadsLabel->setText(QString("Active: %1").arg(active));
    m_completedDownloadsLabel->setText(QString("Completed: %1").arg(completed));
}

void MainWindow::setYtDlpVersion(const QString &version) {
    if (m_advancedSettingsTab) {
        m_advancedSettingsTab->setYtDlpVersion(version);
    }
}
