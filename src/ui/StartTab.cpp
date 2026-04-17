#include "StartTab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QUrl>
#include <QProcess>
#include <QDialog>
#include <QTextEdit>
#include <QGroupBox>
#include <QDesktopServices>
#include <QDir>
#include <QFormLayout>
#include <QCoreApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QFocusEvent>
#include <QEvent>
#include <QJsonArray>
#include <QSignalBlocker>
#include <QInputDialog>
#include <QFile>
#include <QStandardItemModel>
#include <QPushButton>
#include <QDebug> // Include QDebug for debugging
#include <QPalette>
#include "start_tab/StartTabUrlHandler.h"
#include "start_tab/StartTabDownloadActions.h"
#include "start_tab/StartTabCommandPreviewUpdater.h"
#include "ToggleSwitch.h"
#include "StartTabUiBuilder.h" // Include the new builder
#include "core/ProcessUtils.h"

StartTab::StartTab(ConfigManager *configManager, ExtractorJsonParser *extractorJsonParser, QWidget *parent) : QWidget(parent), m_configManager(configManager), m_extractorJsonParser(extractorJsonParser), m_uiBuilder(nullptr) {
    m_ytDlpArgsBuilder = new YtDlpArgsBuilder();
    m_galleryDlArgsBuilder = new GalleryDlArgsBuilder(m_configManager);

    connect(m_extractorJsonParser, &ExtractorJsonParser::extractorsReady, this, &StartTab::onExtractorsReady);
    connect(m_configManager, &ConfigManager::settingChanged, this, [this](const QString &section, const QString &/*key*/, const QVariant &/*value*/){
        // The command preview only cares about settings that influence the download args
        if (section != "SortingRules") {
            updateCommandPreview();
        }
        // updateDynamicUI is now a slot in StartTab, which delegates to m_downloadActions
        if (section == "Binaries") {
            updateDynamicUI();
        }
    });

    m_uiBuilder = new StartTabUiBuilder(m_configManager, this);
    m_urlHandler = new StartTabUrlHandler(m_configManager, m_extractorJsonParser, m_uiBuilder, this);
    m_downloadActions = new StartTabDownloadActions(m_configManager, m_uiBuilder, m_ytDlpArgsBuilder, m_galleryDlArgsBuilder, this);
    m_commandPreviewUpdater = new StartTabCommandPreviewUpdater(m_configManager, m_uiBuilder, m_ytDlpArgsBuilder, m_galleryDlArgsBuilder, this);

    connect(m_extractorJsonParser, &ExtractorJsonParser::extractorsReady, m_urlHandler, &StartTabUrlHandler::onExtractorsReady);
    connect(m_urlHandler, &StartTabUrlHandler::urlInputTextChanged, this, &StartTab::updateCommandPreview); // StartTab still needs to know for command preview
    connect(m_downloadActions, &StartTabDownloadActions::updateCommandPreview, this, &StartTab::updateCommandPreview); // Download actions might trigger preview update
    connect(m_downloadActions, &StartTabDownloadActions::downloadRequested, this, &StartTab::downloadRequested);
    connect(m_downloadActions, &StartTabDownloadActions::navigateToExternalBinaries, this, &StartTab::navigateToExternalBinaries);

    setupUI(); // UI elements are created here using the builder
    loadSettings();
    if (m_uiBuilder->urlInput()) { // Added null check
        m_uiBuilder->urlInput()->setEnabled(false);
        m_uiBuilder->urlInput()->setPlaceholderText("Waiting for startup checks to finish...");
        m_uiBuilder->urlInput()->installEventFilter(this);
    } else {
        qCritical() << "CRITICAL ERROR: m_urlInput is null in StartTab constructor after setupUI!";
    }
    m_downloadActions->updateDynamicUI(); // Initial call to set up dynamic UI

    updateCommandPreview(); // Call after UI is built and initial settings loaded
}

StartTab::~StartTab() {
    delete m_ytDlpArgsBuilder;
    delete m_galleryDlArgsBuilder;
    delete m_uiBuilder;
    delete m_urlHandler;
    delete m_downloadActions;
    delete m_commandPreviewUpdater;
}

void StartTab::onExtractorsReady() {
    m_urlHandler->onExtractorsReady();
}

bool StartTab::tryAutoPasteFromClipboard() {
    return m_urlHandler->tryAutoPasteFromClipboard();
}

void StartTab::focusUrlInput() {
    m_urlHandler->focusUrlInput();
}

void StartTab::onDownloadButtonClicked() {
    m_downloadActions->onDownloadButtonClicked();
}

void StartTab::focusInEvent(QFocusEvent *event) {
    m_urlHandler->handleFocusInEvent(event);
    QWidget::focusInEvent(event);
}

void StartTab::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this); // Use 'this' as parent for mainLayout
    m_uiBuilder->build(this, mainLayout); // Pass 'this' as parentWidget
    setLayout(mainLayout); // Set the layout for the StartTab widget
}

void StartTab::applyCommandPreviewStyleSheet() {
}

void StartTab::applyUrlInputStyleSheet() {
}

void StartTab::changeEvent(QEvent *event) {
    if (event && event->type() == QEvent::PaletteChange) {
        applyUrlInputStyleSheet();
        applyCommandPreviewStyleSheet(); // Re-apply command preview style on theme change
    }
    QWidget::changeEvent(event);
}

void StartTab::loadSettings() { // Use m_uiBuilder members
    QSignalBlocker b1(m_uiBuilder->playlistLogicCombo());
    QSignalBlocker b2(m_uiBuilder->maxConcurrentCombo());
    QSignalBlocker b3(m_uiBuilder->rateLimitCombo());
    QSignalBlocker b4(m_uiBuilder->overrideDuplicateCheck());

    if (m_uiBuilder->playlistLogicCombo())
        m_uiBuilder->playlistLogicCombo()->setCurrentText(m_configManager->get("General", "playlist_logic", "Ask").toString());
    if (m_uiBuilder->maxConcurrentCombo())
        m_uiBuilder->maxConcurrentCombo()->setCurrentText(m_configManager->get("General", "max_threads", "4").toString());
    if (m_uiBuilder->rateLimitCombo())
        m_uiBuilder->rateLimitCombo()->setCurrentText(m_configManager->get("General", "rate_limit", "Unlimited").toString());
    if (m_uiBuilder->overrideDuplicateCheck())
        m_uiBuilder->overrideDuplicateCheck()->setChecked(m_configManager->get("General", "override_archive", false).toBool());

}

bool StartTab::eventFilter(QObject *obj, QEvent *event) {
    return m_urlHandler->handleEventFilter(obj, event) || QWidget::eventFilter(obj, event);
}

void StartTab::updateCommandPreview()
{
    m_commandPreviewUpdater->updateCommandPreview();
}

void StartTab::updateDynamicUI() {
    m_downloadActions->updateDynamicUI();
}

void StartTab::onDuplicateDownloadDetected(const QString &url, const QString &reason)
{
    QMessageBox::warning(this, "Duplicate Download Detected",
                         QString("The following URL was not added to the queue:\n\n%1\n\nReason: %2")
                             .arg(url, reason));
}
