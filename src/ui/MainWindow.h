#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QLabel>
#include <QCloseEvent>
#include <QVariantMap>
#include <QThread> // Include QThread
#include <QClipboard> // Include QClipboard

// Forward declarations
class QEvent;
class ConfigManager;
class ArchiveManager;
class DownloadManager;
class AppUpdater;
class UrlValidator;
class StartupWorker;
class ActiveDownloadsTab;
class AdvancedSettingsTab;
class StartTab;
class SortingTab;
class ExtractorJsonParser;
class YtDlpJsonExtractor;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(ExtractorJsonParser *extractorJsonParser, QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;

private slots:
    void onDownloadRequested(const QString &url, const QVariantMap &options);
    void onValidationFinished(bool isValid, const QString &error);
    void onQueueFinished();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onVideoQualityWarning(const QString &url, const QString &message);
    void applyTheme(const QString &themeName);
    void updateTotalSpeed(double speed);
    void onDownloadStatsUpdated(int queued, int active, int completed);
    void setYtDlpVersion(const QString &version);
    void onClipboardChanged(); // New slot for clipboard changes
        void onRuntimeInfoReady(const QVariantMap &info);
        void onRuntimeInfoError(const QString &error);

private:
    void setupUI();
    void setupTrayIcon();
    void checkBinaries();
    void startStartupChecks();
    void handleClipboardAutoPaste(bool forceEnqueue = false); // Modified to accept forceEnqueue

    ConfigManager *m_configManager;
    ArchiveManager *m_archiveManager;
    DownloadManager *m_downloadManager;
    AppUpdater *m_appUpdater;
    UrlValidator *m_urlValidator;
    StartupWorker *m_startupWorker;
    QThread *m_startupThread; // New thread for the startup worker
    ExtractorJsonParser *m_extractorJsonParser;
    YtDlpJsonExtractor *m_runtimeExtractor;
    QClipboard *m_clipboard; // New QClipboard member

    QTabWidget *m_tabWidget;
    ActiveDownloadsTab *m_activeDownloadsTab;
    AdvancedSettingsTab *m_advancedSettingsTab;
    StartTab *m_startTab;
    QLabel *m_speedLabel;
    QLabel *m_queuedDownloadsLabel;
    QLabel *m_activeDownloadsLabel;
    QLabel *m_completedDownloadsLabel;

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;

    QString m_pendingUrl;
    QVariantMap m_pendingOptions;
    bool m_silentUpdateCheck;
};

#endif // MAINWINDOW_H
