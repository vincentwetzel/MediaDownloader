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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
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
    m_configManager = new ConfigManager("settings.ini", this);
    m_archiveManager = new ArchiveManager(m_configManager, this);
    m_downloadManager = new DownloadManager(m_configManager, this);
    m_appUpdater = new AppUpdater(REPO_URL, APP_VERSION, this);
    m_urlValidator = new UrlValidator(m_configManager, this);
    m_clipboard = QApplication::clipboard(); // Initialize QClipboard

    // Create worker and thread but do not parent the worker to MainWindow
    m_startupWorker = new StartupWorker(m_configManager, m_extractorJsonParser, nullptr);
    m_startupThread = new QThread(this);

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

    connect(m_activeDownloadsTab, &ActiveDownloadsTab::cancelDownloadRequested,
            m_downloadManager, &DownloadManager::cancelDownload);
    connect(m_activeDownloadsTab, &ActiveDownloadsTab::retryDownloadRequested,
            m_downloadManager, &DownloadManager::retryDownload);
    connect(m_activeDownloadsTab, &ActiveDownloadsTab::resumeDownloadRequested,
            m_downloadManager, &DownloadManager::resumeDownload);

    connect(m_urlValidator, &UrlValidator::validationFinished, this, &MainWindow::onValidationFinished);

    // Setup startup worker threading
    m_startupWorker->moveToThread(m_startupThread);
    connect(m_startupThread, &QThread::started, m_startupWorker, &StartupWorker::start);
    connect(m_startupWorker, &StartupWorker::finished, m_startupThread, &QThread::quit);
    // DO NOT connect deleteLater here. It will be handled manually in the destructor.
    connect(m_startupWorker, &StartupWorker::binariesChecked, this, [this](const QStringList &missingBinaries){
        if (!missingBinaries.isEmpty()) {
            QMessageBox::warning(this, "Missing Binaries",
                                 "The following required binaries are missing from the application directory:\n\n" +
                                 missingBinaries.join("\n") +
                                 "\n\nPlease ensure they are placed in the same folder as MediaDownloader.exe.");
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

    m_startTab = new StartTab(m_configManager, m_extractorJsonParser, this);
    m_activeDownloadsTab = new ActiveDownloadsTab(m_configManager, this);
    m_advancedSettingsTab = new AdvancedSettingsTab(m_configManager, this);
    SortingTab *sortingTab = new SortingTab(m_configManager, this);

    m_tabWidget->addTab(m_startTab, "Start Download");
    m_tabWidget->addTab(m_activeDownloadsTab, "Active Downloads");
    m_tabWidget->addTab(sortingTab, "Sorting Rules");
    m_tabWidget->addTab(m_advancedSettingsTab, "Advanced Settings");

    QHBoxLayout *footerLayout = new QHBoxLayout();
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

    footerLayout->addWidget(githubLink);
    footerLayout->addSpacing(20);
    footerLayout->addWidget(discordLink);
    footerLayout->addStretch();
    footerLayout->addWidget(m_queuedDownloadsLabel);
    footerLayout->addSpacing(10);
    footerLayout->addWidget(m_activeDownloadsLabel);
    footerLayout->addSpacing(10);
    footerLayout->addWidget(m_completedDownloadsLabel);
    footerLayout->addSpacing(10);
    footerLayout->addWidget(m_speedLabel);
    mainLayout->addLayout(footerLayout);

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
    if (themeName == "Dark") {
        qApp->setStyle(QStyleFactory::create("Fusion"));
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
        qApp->setPalette(darkPalette);
    } else if (themeName == "Light") {
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setPalette(QApplication::style()->standardPalette());
    } else { // System
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setPalette(QApplication::style()->standardPalette());
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
