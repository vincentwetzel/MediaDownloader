#ifndef ADVANCEDSETTINGSTAB_H
#define ADVANCEDSETTINGSTAB_H

#include "ToggleSwitch.h"
#include "core/ConfigManager.h"
#include <QtWidgets/QWidget>
#include <QProcess>
#include <QTimer>

class QListWidget;
class QStackedWidget;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QFormLayout;

class AdvancedSettingsTab : public QWidget {
    Q_OBJECT

public:
    explicit AdvancedSettingsTab(ConfigManager *configManager, QWidget *parent = nullptr);
    ~AdvancedSettingsTab();

    QWidget* createOutputTemplatePage();
signals:
    void themeChanged(const QString &themeName);

public slots:
    void setGalleryDlVersion(const QString &version);
    void setYtDlpVersion(const QString &version);

private slots:
    void selectCompletedDir();
    void selectTempDir();
    void restoreDefaults();
    void validateAndSaveYtDlpTemplate();
    void validateAndSaveGalleryDlTemplate();
    void insertYtDlpTemplateToken(int index);
    void insertGalleryDlTemplateToken(int index);
    void handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value);

    // Auto-save slots
    void onThemeChanged(const QString &text);
    void onCookiesBrowserChanged(const QString &text);
    void onCookieCheckProcessStarted();
    void onCookieCheckProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onCookieCheckProcessErrorOccurred(QProcess::ProcessError error);
    void onCookieCheckProcessReadyReadStandardOutput();
    void onCookieCheckProcessReadyReadStandardError();
    void onCookieCheckTimeout();
    void onExternalDownloaderToggled(bool checked);
    void onSponsorBlockToggled(bool checked);
    void onEmbedChaptersToggled(bool checked);
    void onAutoPasteModeChanged(int index); // Changed from onAutoPasteOnFocusToggled
    void onSingleLineCommandPreviewToggled(bool checked);
    void onRestrictFilenamesToggled(bool checked);
    void onEmbedMetadataToggled(bool checked);
    void onEmbedThumbnailToggled(bool checked);
    void onHighQualityThumbnailToggled(bool checked);
    void onConvertThumbnailsChanged(const QString &text);
    void onSubtitleLanguageChanged(const QString &text);
    void onEmbedSubtitlesToggled(bool checked);
    void onWriteSubtitlesToggled(bool checked);
    void onIncludeAutoSubtitlesToggled(bool checked);
    void onSubtitleFormatChanged(const QString &text);

    // Video/Audio slots
    void onVideoQualityChanged(const QString &text);
    void onVideoCodecChanged(const QString &text);
    void onVideoExtChanged(const QString &text);
    void onVideoAudioCodecChanged(const QString &text);
    void onAudioQualityChanged(const QString &text);
    void onAudioCodecChanged(const QString &text);
    void onAudioExtChanged(const QString &text);
    void updateVideoOptions();
    void updateAudioOptions();

private:
    void setupUI();
    void loadSettings();
    void populateSubtitleLanguages();
    void updateSubtitleFormatAvailability(bool embedSubtitlesChecked);

    // Page creation methods
    QWidget* createConfigurationPage();
    QWidget* createVideoSettingsPage();
    QWidget* createAudioSettingsPage();
    QWidget* createAuthenticationPage();
    QWidget* createOutputTemplatesPage();
    QWidget* createDownloadOptionsPage();
    QWidget* createMetadataPage();
    QWidget* createSubtitlesPage();
    QWidget* createUpdatesPage();


    ConfigManager *m_configManager;

    QListWidget *m_categoryList;
    QStackedWidget *m_stackedWidget;

    // --- Widgets ---

    // Configuration
    QLineEdit *m_completedDirInput;
    QPushButton *m_browseCompletedBtn;
    QLineEdit *m_tempDirInput;
    QPushButton *m_browseTempBtn;
    QComboBox *m_themeCombo;

    // Video Settings
    QFormLayout *m_videoLayout;
    QComboBox *m_videoQualityCombo;
    QComboBox *m_videoCodecCombo;
    QLabel *m_videoExtLabel;
    QComboBox *m_videoExtCombo;
    QComboBox *m_videoAudioCodecCombo;

    // Audio Settings
    QFormLayout *m_audioLayout;
    QComboBox *m_audioQualityCombo;
    QComboBox *m_audioCodecCombo;
    QLabel *m_audioExtLabel;
    QComboBox *m_audioExtCombo;

    // Authentication
    QComboBox *m_cookiesBrowserCombo;
    QProcess *m_cookieCheckProcess;
    QTimer *m_cookieCheckTimeoutTimer;
    QString m_lastSavedBrowser;


    // Output Template
    QLineEdit *m_ytDlpOutputTemplateInput;
    QPushButton *m_saveYtDlpTemplateButton;
    QComboBox *m_ytDlpTemplateTokensCombo;
    QLineEdit *m_galleryDlOutputTemplateInput;
    QPushButton *m_saveGalleryDlTemplateButton;
    QComboBox *m_galleryDlTemplateTokensCombo;


    // Download Options
    ToggleSwitch *m_externalDownloaderCheck;
    ToggleSwitch *m_sponsorBlockCheck;
    ToggleSwitch *m_embedChaptersCheck;
    QComboBox *m_autoPasteModeCombo; // Changed from ToggleSwitch *m_autoPasteOnFocusCheck;
    ToggleSwitch *m_singleLineCommandPreviewCheck;
    ToggleSwitch *m_restrictFilenamesCheck;

    // Metadata / Thumbnails

    ToggleSwitch *m_embedMetadataCheck;
    ToggleSwitch *m_embedThumbnailCheck;
    ToggleSwitch *m_highQualityThumbnailCheck;
    QComboBox *m_convertThumbnailsCombo;

    // Subtitles
    QComboBox *m_subtitleLanguageCombo;
    ToggleSwitch *m_embedSubtitlesCheck;
    ToggleSwitch *m_writeSubtitlesCheck;
    ToggleSwitch *m_includeAutoSubtitlesCheck;
    QComboBox *m_subtitleFormatCombo;

    // Updates
    QLabel *m_ytDlpVersionLabel;
    QLabel *m_ytDlpUpdateStatusLabel;
    QPushButton *m_updateYtDlpButton;
    QLabel *m_galleryDlVersionLabel;
    QLabel *m_galleryDlUpdateStatusLabel;
    QPushButton *m_updateGalleryDlButton;


    // Restore Defaults
    QPushButton *m_restoreDefaultsButton;
};

#endif // ADVANCEDSETTINGSTAB_H
