#ifndef STARTTAB_H
#define STARTTAB_H

#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QVariantMap>
#include <QProcess>
#include <QSpinBox>
#include <QGroupBox>
#include <QMessageBox>
#include "core/ConfigManager.h"
#include "utils/ExtractorJsonParser.h"
#include "core/YtDlpArgsBuilder.h"
#include "core/GalleryDlArgsBuilder.h"

class QEvent;
class QFocusEvent;
class ToggleSwitch;

class StartTab : public QWidget {
    Q_OBJECT

public:
    explicit StartTab(ConfigManager *configManager, ExtractorJsonParser *extractorJsonParser, QWidget *parent = nullptr);
    bool tryAutoPasteFromClipboard();
    void focusUrlInput();

protected:
    void focusInEvent(QFocusEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void changeEvent(QEvent *event) override;

signals:
    void downloadRequested(const QString &url, const QVariantMap &options);

public slots: // Changed from private slots:
    void onDownloadButtonClicked();
    void onViewFormatsFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void openDownloadsFolder();
    void onDownloadTypeChanged(int index);
    void onExtractorsReady();
    void updateCommandPreview();

private slots:
    void onClipboardChangedWhileDialogIsOpen();
    void onTypeSelectionDialogFinished(int result);

private:
    void setupUI();
    void loadSettings();
    void checkFormats(const QString &url);
    QString resolveExecutablePath(const QString &name) const;
    bool checkClipboardForUrl();
    void applyUrlInputStyleSheet();
    void applyCommandPreviewStyleSheet(); // Added this line

    ConfigManager *m_configManager;
    ExtractorJsonParser *m_extractorJsonParser;
    YtDlpArgsBuilder *m_ytDlpArgsBuilder;
    GalleryDlArgsBuilder *m_galleryDlArgsBuilder;

    // UI Elements
    QTextEdit *m_urlInput;
    QPushButton *m_openDownloadsFolderButton;
    QComboBox *m_downloadTypeCombo;
    QPushButton *m_downloadButton;
    QTextEdit *m_commandPreview;

    // Operational Controls
    QComboBox *m_playlistLogicCombo;
    QComboBox *m_maxConcurrentCombo;
    QComboBox *m_rateLimitCombo;
    ToggleSwitch *m_overrideDuplicateCheck;

    QMessageBox *m_typeSelectionDialog = nullptr;
};

#endif // STARTTAB_H
