#include "core/version.h"
#include "MainWindow.h"
#include "StartTab.h"
#include "ActiveDownloadsTab.h"
#include "MainWindowUiBuilder.h" // Include the new UI builder
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
#include "ui/DownloadSectionsDialog.h"
#include "ToggleSwitch.h"
#include "utils/BinaryFinder.h"

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
#include <QPushButton>
#include <QStandardPaths>
#include <QDateTime>

const QString REPO_URL = "https://api.github.com/repos/vincentwetzel/LzyDownloader";
const QString GITHUB_PROJECT_URL = "https://github.com/vincentwetzel/LzyDownloader";
const QString DEVELOPER_DISCORD_URL_PART1 = "https://discord.gg/";
const QString DEVELOPER_DISCORD_URL_PART2 = "NfWaqK";
const QString DEVELOPER_DISCORD_URL_PART3 = "gYRG";

MainWindow::MainWindow(ExtractorJsonParser *extractorJsonParser, QWidget *parent)
    : QMainWindow(parent), m_configManager(nullptr), m_archiveManager(nullptr), m_downloadManager(nullptr),
      m_appUpdater(nullptr), m_urlValidator(nullptr), m_startupWorker(nullptr), m_startupThread(nullptr),
      m_extractorJsonParser(extractorJsonParser), m_runtimeExtractor(nullptr), m_uiBuilder(nullptr),
      m_clipboard(nullptr), m_startTab(nullptr), m_activeDownloadsTab(nullptr),
      m_advancedSettingsTab(nullptr), m_trayIcon(nullptr), m_trayMenu(nullptr),
      m_silentUpdateCheck(false), m_lastAutoPasteTimestamp(0)
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
    m_appUpdater = new AppUpdater(REPO_URL, QString(APP_VERSION_STRING), this);
    m_urlValidator = new UrlValidator(m_configManager, this);
    m_clipboard = QApplication::clipboard(); // Initialize QClipboard

    // --- Dynamic Binary Discovery ---
    QMap<QString, QString> foundBinaries = BinaryFinder::findAllBinaries();
    for (auto it = foundBinaries.constBegin(); it != foundBinaries.constEnd(); ++it) {
        QString configKey = it.key() + "_path";
        QString currentPath = m_configManager->get("Binaries", configKey).toString();
        // If current path is empty or invalid, update it
        if (currentPath.isEmpty() || !QFile::exists(currentPath)) {
            if (!it.value().isEmpty()) {
                m_configManager->set("Binaries", configKey, it.value());
            }
        }
    }
    m_configManager->save();

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
    m_uiBuilder = new MainWindowUiBuilder(m_configManager, this); // Initialize UI builder
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
    connect(m_downloadManager, &DownloadManager::downloadPaused,
            m_activeDownloadsTab, &ActiveDownloadsTab::onDownloadPaused);
    connect(m_downloadManager, &DownloadManager::downloadResumed,
            m_activeDownloadsTab, &ActiveDownloadsTab::onDownloadResumed);
    connect(m_downloadManager, &DownloadManager::downloadFinalPathReady,
            m_activeDownloadsTab, &ActiveDownloadsTab::onDownloadFinalPathReady);
    connect(m_downloadManager, &DownloadManager::playlistExpansionStarted,
            m_activeDownloadsTab, &ActiveDownloadsTab::addExpandingPlaylist);
    connect(m_downloadManager, &DownloadManager::playlistExpansionFinished,
            m_activeDownloadsTab, &ActiveDownloadsTab::removeExpandingPlaylist); // Corrected: removeExpandingPlaylist
    connect(m_downloadManager, &DownloadManager::queueFinished, this, &MainWindow::onQueueFinished);
    connect(m_downloadManager, &DownloadManager::totalSpeedUpdated, this, &MainWindow::updateTotalSpeed);
    connect(m_downloadManager, &DownloadManager::videoQualityWarning, this, &MainWindow::onVideoQualityWarning);
    connect(m_downloadManager, &DownloadManager::downloadStatsUpdated, this, &MainWindow::onDownloadStatsUpdated);
    
    // Connect duplicate detection signal to StartTab
    connect(m_downloadManager, &DownloadManager::duplicateDownloadDetected, m_startTab, &StartTab::onDuplicateDownloadDetected);

    // Connect yt-dlp error popup signal
    connect(m_downloadManager, &DownloadManager::ytDlpErrorPopupRequested, this, &MainWindow::onYtDlpErrorPopup);

    // Connect download sections signal
    connect(m_downloadManager, &DownloadManager::downloadSectionsRequested, this, &MainWindow::onDownloadSectionsRequested);

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
    // FIXME: Re-enable this connection once the corresponding signal and slot are declared in their headers.
    // See comments in ActiveDownloadsTab.cpp and DownloadManager.cpp for details.
    // connect(m_activeDownloadsTab, &ActiveDownloadsTab::itemCleared,
    //         m_downloadManager, &DownloadManager::onItemCleared);

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
            msgBox.setInformativeText("LzyDownloader requires these tools to function correctly.\n\n"
                                      "Install them with your preferred package manager or use the 'Advanced Settings -> External Binaries' page "
                                      "to browse to their installed locations.");

            QPushButton *fixButton = msgBox.addButton("Take Me There", QMessageBox::ActionRole);
            msgBox.addButton(QMessageBox::Ok);
            msgBox.exec();

            if (msgBox.clickedButton() == fixButton) {
                m_uiBuilder->tabWidget()->setCurrentWidget(m_advancedSettingsTab);
                m_advancedSettingsTab->navigateToCategory("External Binaries");
            }
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
    setWindowTitle("LzyDownloader v" + QString(APP_VERSION_STRING));

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    m_startTab = new StartTab(m_configManager, m_extractorJsonParser, this);
    m_activeDownloadsTab = new ActiveDownloadsTab(m_configManager, this);
    m_advancedSettingsTab = new AdvancedSettingsTab(m_configManager, this);
    SortingTab *sortingTab = new SortingTab(m_configManager, this);

    m_uiBuilder->build(this, mainLayout, m_startTab, m_activeDownloadsTab, m_advancedSettingsTab, sortingTab);

    // Connect signals from the builder's widgets
    connect(m_uiBuilder->exitAfterSwitch(), &ToggleSwitch::toggled, this, [this](bool checked){
        m_configManager->set("General", "exit_after", checked);
        m_configManager->save();
    });

    connect(m_startTab, &StartTab::downloadRequested, this, &MainWindow::onDownloadRequested);
    connect(m_startTab, &StartTab::navigateToExternalBinaries, this, [this]() {
        m_uiBuilder->tabWidget()->setCurrentWidget(m_advancedSettingsTab);
    });
    connect(m_advancedSettingsTab, &AdvancedSettingsTab::themeChanged, this, &MainWindow::applyTheme);
}

void MainWindow::setupTrayIcon() {
    m_trayIcon = new QSystemTrayIcon(QIcon(":/app-icon"), this);
    m_trayIcon->setToolTip("LzyDownloader");

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

    qInfo() << "Main window close requested; shutting down active background tasks before exit.";
    if (m_downloadManager) {
        m_downloadManager->shutdown();
    }
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

    // Enforce a cooldown period (5 seconds) to prevent rapid re-triggering
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastAutoPasteTimestamp < 5000) {
        return;
    }

    // Get the URL from clipboard to check before pasting
    const QString clipboardText = m_clipboard->text().trimmed();
    if (clipboardText.isEmpty()) {
        return;
    }

    // Check if this is the same URL we just auto-pasted
    if (clipboardText == m_lastAutoPastedUrl) {
        return;
    }

    // Try to auto-paste (this validates and sets the URL in the input)
    if (m_startTab->tryAutoPasteFromClipboard()) {
        // Update tracking
        m_lastAutoPastedUrl = clipboardText;
        m_lastAutoPasteTimestamp = QDateTime::currentMSecsSinceEpoch();

        // Switch to Start tab if not already there
        if (m_uiBuilder->tabWidget()->currentWidget() != m_startTab) {
            m_uiBuilder->tabWidget()->setCurrentWidget(m_startTab);
        }
        m_startTab->focusUrlInput();
        
        // Only auto-enqueue if forceEnqueue is true AND this is a new URL
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

    QVariantMap mutableOptions = options;
    bool overrideArchive = mutableOptions.value("override_archive", m_configManager->get("General", "override_archive", false)).toBool();

    if (!overrideArchive && m_archiveManager && m_archiveManager->isInArchive(url)) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Duplicate Download",
                                      QString("The following URL is already in your download history:\n%1\n\nDo you want to download it again?").arg(url),
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            return;
        }
        mutableOptions["override_archive"] = true;
    }

    bool runtimeVideo = m_configManager->get("Video", "video_multistreams", "Default Stream").toString() == "Select at Runtime" && mutableOptions.value("type").toString() == "video";
    bool runtimeAudio = m_configManager->get("Audio", "audio_multistreams", "Default Stream").toString() == "Select at Runtime"; // Audio tracks apply to video and audio types
    bool runtimeSubs = m_configManager->get("Subtitles", "languages", "en").toString().split(',').contains("runtime");

    if (runtimeVideo || runtimeAudio || runtimeSubs) {
        m_pendingUrl = url;
        m_pendingOptions = mutableOptions;
        statusBar()->showMessage("Fetching media info for runtime selection...");
        QString ytDlpPath = ProcessUtils::findBinary("yt-dlp", m_configManager).path;
        QStringList args;
        args << "--dump-json" << "--no-playlist" << url;
        m_runtimeExtractor->extract(ytDlpPath, args);
        return;
    }

    static QRegularExpression fastTrackRe(R"(^(https?://)?(www\.)?(youtube\.com|youtu\.be|music\.youtube\.com|tiktok\.com|instagram\.com|twitter\.com|x\.com)/)");
    if (fastTrackRe.match(url).hasMatch()) {
        m_downloadManager->enqueueDownload(url, mutableOptions); // Corrected: m_downloadManager
        m_uiBuilder->tabWidget()->setCurrentWidget(m_activeDownloadsTab);
        return;
    }

    m_pendingUrl = url;
    m_pendingOptions = mutableOptions;

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
            } // Corrected: m_downloadManager
            m_downloadManager->enqueueDownload(m_pendingUrl, opts); // Corrected: m_downloadManager
            m_uiBuilder->tabWidget()->setCurrentWidget(m_activeDownloadsTab);
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

void MainWindow::onDownloadSectionsRequested(const QString &url, const QVariantMap &options, const QVariantMap &infoJson)
{
    DownloadSectionsDialog dialog(infoJson, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString sections = dialog.getSectionsString();
        QString sectionLabel = dialog.getFilenameLabel();
        QVariantMap newOptions = options;
        newOptions["download_sections_set"] = true; // Mark as done to prevent looping
        if (!sections.isEmpty()) {
            newOptions["download_sections"] = sections;
        }
        if (!sectionLabel.isEmpty()) {
            newOptions["download_sections_label"] = sectionLabel;
        }
        // Re-enqueue with the new options
        m_downloadManager->enqueueDownload(url, newOptions);
        m_uiBuilder->tabWidget()->setCurrentWidget(m_activeDownloadsTab);
    } else {
        // User cancelled, do nothing. The download was never enqueued.
        qInfo() << "Download sections selection cancelled by user for" << url;
    }
}

void MainWindow::onValidationFinished(bool isValid, const QString &error) {
    if (isValid) {
        m_downloadManager->enqueueDownload(m_pendingUrl, m_pendingOptions); // Corrected: m_downloadManager
        m_uiBuilder->tabWidget()->setCurrentWidget(m_activeDownloadsTab);
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
        m_trayIcon->showMessage("Downloads Complete", "All queued media downloads have finished.",
                                QSystemTrayIcon::Information, 3000);
    }

    bool exitAfter = m_configManager->get("General", "exit_after", false).toBool();
    if (exitAfter) {
        qInfo() << "Queue finished and 'exit after' is enabled. Waiting 2 seconds before quitting to allow for final file cleanup.";
        QTimer::singleShot(2000, this, &QCoreApplication::quit);
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
    double speedMb = speed / (1024.0 * 1024.0); // Corrected: m_uiBuilder->speedLabel()
    m_uiBuilder->speedLabel()->setText(QString("Current Speed: %1 MB/s").arg(speedMb, 0, 'f', 2));
}

void MainWindow::onDownloadStatsUpdated(int queued, int active, int completed, int errors) {
    m_uiBuilder->queuedDownloadsLabel()->setText(QString("Queued: %1").arg(queued));
    m_uiBuilder->activeDownloadsLabel()->setText(QString("Active: %1").arg(active));
    m_uiBuilder->completedDownloadsLabel()->setText(QString("Completed: %1").arg(completed));
    m_uiBuilder->errorDownloadsLabel()->setText(QString("Errors: %1").arg(errors));
}

void MainWindow::onYtDlpErrorPopup(const QString &id, const QString &errorType, const QString &userMessage, const QString &rawError, const QVariantMap &itemData) {
    Q_UNUSED(id);

    // Clean up the raw error string to make it more user-friendly
    QString cleanError = rawError;
    if (cleanError.startsWith("ERROR: ")) {
        cleanError = cleanError.mid(7); // Remove "ERROR: "
    }
    // Remove "[extractor] " prefix if present (e.g. "[youtube] ")
    cleanError.remove(QRegularExpression("^\\[[^\\]]+\\]\\s*"));

    if (errorType == "scheduled_livestream") {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Scheduled Livestream");
        msgBox.setText(userMessage);
        if (!cleanError.isEmpty()) {
            msgBox.setInformativeText(cleanError);
        }
        msgBox.setIcon(QMessageBox::Information);

        QPushButton *waitButton = msgBox.addButton("Wait and Download When Available", QMessageBox::AcceptRole);
        msgBox.addButton(QMessageBox::Cancel);

        msgBox.exec();

        if (msgBox.clickedButton() == waitButton) {
            QVariantMap newItemData = itemData;
            QVariantMap options = newItemData["options"].toMap();
            options["wait_for_video"] = true;

            // Dynamically adjust wait time based on how far away the stream is.
            int minWait, maxWait;
            if (cleanError.contains("hour", Qt::CaseInsensitive)) {
                minWait = 1800; // 30 minutes
                maxWait = 3600; // 60 minutes
            } else if (cleanError.contains("second", Qt::CaseInsensitive)) {
                minWait = 5;    // 5 seconds
                maxWait = 15;   // 15 seconds
            } else { // Default to configured values for "minutes" or unknown units
                minWait = m_configManager->get("Livestream", "wait_for_video_min").toInt();
                maxWait = m_configManager->get("Livestream", "wait_for_video_max").toInt();
            }
            options["livestream_wait_min"] = minWait;
            options["livestream_wait_max"] = maxWait;

            newItemData["options"] = options;
            
            m_downloadManager->restartDownloadWithOptions(newItemData);
        }
    } else {
        // Generic error handling
        QString title;
        if (errorType == "private") title = "Private Video";
        else if (errorType == "unavailable") title = "Video Unavailable";
        else if (errorType == "geo_restricted") title = "Geo-Restricted Video";
        else if (errorType == "members_only") title = "Members-Only Video";
        else if (errorType == "age_restricted") title = "Age-Restricted Video";
        else if (errorType == "content_removed") title = "Content Removed";
        else title = "Download Error";

        QMessageBox msgBox(QMessageBox::Warning, title, userMessage, QMessageBox::Ok, this);
        if (!cleanError.isEmpty()) {
            msgBox.setInformativeText(cleanError);
        }
        msgBox.exec();
    }
}

void MainWindow::setYtDlpVersion(const QString &version) {
    if (m_advancedSettingsTab) {
        m_advancedSettingsTab->setYtDlpVersion(version);
    }
}


