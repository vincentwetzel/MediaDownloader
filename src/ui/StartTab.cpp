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
#include <QPushButton>
#include <QDebug> // Include QDebug for debugging
#include <QPalette>

StartTab::StartTab(ConfigManager *configManager, ExtractorJsonParser *extractorJsonParser, QWidget *parent)
    : QWidget(parent), m_configManager(configManager), m_extractorJsonParser(extractorJsonParser), m_typeSelectionDialog(nullptr) {
    m_ytDlpArgsBuilder = new YtDlpArgsBuilder();
    m_galleryDlArgsBuilder = new GalleryDlArgsBuilder(m_configManager);
    connect(m_extractorJsonParser, &ExtractorJsonParser::extractorsReady, this, &StartTab::onExtractorsReady);
    connect(m_configManager, &ConfigManager::settingChanged, this, &StartTab::updateCommandPreview);
    setupUI(); // UI elements are created here
    loadSettings();
    if (m_urlInput) { // Added null check
        m_urlInput->setEnabled(false);
        m_urlInput->setPlaceholderText("Waiting for startup checks to finish...");
        m_urlInput->installEventFilter(this);
    } else {
        qCritical() << "CRITICAL ERROR: m_urlInput is null in StartTab constructor after setupUI!";
    }
    updateCommandPreview();
}

void StartTab::onExtractorsReady()
{
    if (m_urlInput) { // Added null check
        m_urlInput->setEnabled(true);
        m_urlInput->setPlaceholderText("Paste one or more media URLs (one per line)...");
    }
}

bool StartTab::tryAutoPasteFromClipboard()
{
    return checkClipboardForUrl();
}

void StartTab::focusUrlInput()
{
    if (m_urlInput) {
        m_urlInput->setFocus(Qt::OtherFocusReason);
    }
}

void StartTab::focusInEvent(QFocusEvent *event)
{
    if (event->reason() == Qt::MouseFocusReason)
    {
        checkClipboardForUrl();
    }
    QWidget::focusInEvent(event);
}

bool StartTab::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_urlInput) {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn) {
            checkClipboardForUrl();
        }
    }
    return QWidget::eventFilter(obj, event);
}

void StartTab::onClipboardChangedWhileDialogIsOpen()
{
    if (!m_typeSelectionDialog) return;

    const QClipboard *clipboard = QGuiApplication::clipboard();
    QString text = clipboard->text().trimmed();
    if (text.isEmpty()) return;

    QString firstUrlStr = text.split('\n', Qt::SkipEmptyParts).first().trimmed();
    if (!firstUrlStr.startsWith("http://", Qt::CaseInsensitive) && !firstUrlStr.startsWith("https://", Qt::CaseInsensitive)) {
        firstUrlStr = "https://" + firstUrlStr;
    }
    QUrl url(firstUrlStr);
    QString host = url.host().toLower();

    if (host.isEmpty()) return;

    QJsonObject ytDlpExtractors = m_extractorJsonParser->getYtDlpExtractors();
    bool inYtDlp = false;
    for (auto it = ytDlpExtractors.begin(); it != ytDlpExtractors.end(); ++it) {
        QJsonArray domains = it.value().toObject()["domains"].toArray();
        for (const QJsonValue &domainValue : domains) {
            QString domainStr = domainValue.toString().toLower();
            if (host == domainStr || host.endsWith("." + domainStr)) {
                inYtDlp = true;
                break;
            }
        }
        if (inYtDlp) break;
    }

    if (!inYtDlp) {
        m_typeSelectionDialog->reject();
    }
}

void StartTab::onTypeSelectionDialogFinished(int /*result*/)
{
    if (!m_typeSelectionDialog) return;

    QObject::disconnect(QGuiApplication::clipboard(), &QClipboard::changed, this, &StartTab::onClipboardChangedWhileDialogIsOpen);

    QAbstractButton *clickedButton = m_typeSelectionDialog->clickedButton();
    if (clickedButton) {
        QString btnText = clickedButton->text();
        QString dataValue;
        if (btnText == "Video") dataValue = "video";
        else if (btnText == "Audio Only") dataValue = "audio";
        else if (btnText == "View Formats") dataValue = "formats";

        if (!dataValue.isEmpty()) {
            int index = m_downloadTypeCombo->findData(dataValue);
            if (index != -1) {
                m_downloadTypeCombo->setCurrentIndex(index);
            }
        }
    }

    m_typeSelectionDialog->deleteLater();
    m_typeSelectionDialog = nullptr;
}

bool StartTab::checkClipboardForUrl()
{
    if (!m_urlInput || !m_urlInput->isEnabled()) {
        return false;
    }

    const QClipboard *clipboard = QGuiApplication::clipboard();
    const QString text = clipboard->text().trimmed();
    if (text.isEmpty() || m_urlInput->toPlainText().trimmed() == text) {
        return false;
    }

    QJsonObject ytDlpExtractors = m_extractorJsonParser->getYtDlpExtractors();
    QJsonObject galleryDlExtractors = m_extractorJsonParser->getGalleryDlExtractors();

    if (ytDlpExtractors.isEmpty() && galleryDlExtractors.isEmpty()) {
        return false;
    }

    QString firstUrlStr = text.split('\n', Qt::SkipEmptyParts).first().trimmed();
    if (!firstUrlStr.startsWith("http://", Qt::CaseInsensitive) && !firstUrlStr.startsWith("https://", Qt::CaseInsensitive)) {
        firstUrlStr = "https://" + firstUrlStr;
    }
    QUrl url(firstUrlStr);
    QString host = url.host().toLower();

    if (host.isEmpty()) {
        return false;
    }

    bool inYtDlp = false;
    for (auto it = ytDlpExtractors.begin(); it != ytDlpExtractors.end(); ++it) {
        QJsonArray domains = it.value().toObject()["domains"].toArray();
        for (const QJsonValue &domainValue : domains) {
            QString domainStr = domainValue.toString().toLower();
            if (host == domainStr || host.endsWith("." + domainStr)) {
                inYtDlp = true;
                break;
            }
        }
        if (inYtDlp) break;
    }

    bool inGalleryDl = false;
    for (auto it = galleryDlExtractors.begin(); it != galleryDlExtractors.end(); ++it) {
        QJsonArray domains = it.value().toObject()["domains"].toArray();
        for (const QJsonValue &domainValue : domains) {
            QString domainStr = domainValue.toString().toLower();
            if (host == domainStr || host.endsWith("." + domainStr)) {
                inGalleryDl = true;
                break;
            }
        }
        if (inGalleryDl) break;
    }

    if (inYtDlp || inGalleryDl) {
        m_urlInput->setText(text);

        if (inGalleryDl && !inYtDlp) {
            // URL is exclusively for gallery-dl, switch to "Gallery"
            int galleryIndex = m_downloadTypeCombo->findData("gallery");
            if (galleryIndex != -1) {
                m_downloadTypeCombo->setCurrentIndex(galleryIndex);
            }
        } else if (inYtDlp && !inGalleryDl) {
            // URL is exclusively for yt-dlp, check if user has "Gallery" selected
            if (m_downloadTypeCombo->currentData().toString() == "gallery") {
                if (m_typeSelectionDialog) {
                    return true; // Dialog is already open
                }

                m_typeSelectionDialog = new QMessageBox(this);
                m_typeSelectionDialog->setWindowTitle("Download Type Selection");
                m_typeSelectionDialog->setText("The pasted URL is likely for video/audio.\nPlease select a download type:");
                m_typeSelectionDialog->setIcon(QMessageBox::Question);

                m_typeSelectionDialog->addButton("Video", QMessageBox::ActionRole);
                m_typeSelectionDialog->addButton("Audio Only", QMessageBox::ActionRole);
                m_typeSelectionDialog->addButton("View Formats", QMessageBox::ActionRole);
                m_typeSelectionDialog->addButton(QMessageBox::Cancel);

                connect(m_typeSelectionDialog, &QDialog::finished, this, &StartTab::onTypeSelectionDialogFinished);
                connect(QGuiApplication::clipboard(), &QClipboard::changed, this, &StartTab::onClipboardChangedWhileDialogIsOpen);

                m_typeSelectionDialog->open();
            }
        }
        // If in both, do nothing and let the user decide.

        return true;
    }

    return false;
}

void StartTab::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addStretch();
    m_openDownloadsFolderButton = new QPushButton("Open Downloads Folder", this);
    m_openDownloadsFolderButton->setToolTip("Click here to open the folder where all your finished downloads are saved.");
    topLayout->addWidget(m_openDownloadsFolderButton);
    mainLayout->addLayout(topLayout);

    QHBoxLayout *inputSectionLayout = new QHBoxLayout();

    QVBoxLayout *urlInputLayout = new QVBoxLayout();
    QLabel *urlLabel = new QLabel("Video/Playlist URL(s):", this);
    urlLabel->setToolTip("Enter the URLs of the videos or playlists you want to download.");
    urlInputLayout->addWidget(urlLabel);

    m_urlInput = new QTextEdit(this);
    m_urlInput->setPlaceholderText("Paste one or more media URLs (one per line)...");
    m_urlInput->setToolTip("Paste the web address (URL) of the video or audio you want to download here. You can paste multiple links, just put each one on a new line.");
    m_urlInput->setMinimumHeight(100);
    urlInputLayout->addWidget(m_urlInput);
    applyUrlInputStyleSheet();
    inputSectionLayout->addLayout(urlInputLayout, 70);

    QVBoxLayout *actionColumnLayout = new QVBoxLayout();
    actionColumnLayout->setSpacing(10);
    actionColumnLayout->setAlignment(Qt::AlignTop);

    actionColumnLayout->addSpacing(25);

    m_downloadButton = new QPushButton("Download Video", this);
    m_downloadButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_downloadButton->setStyleSheet("QPushButton { font-size: 16px; font-weight: bold; background-color: #0078d7; color: white; border-radius: 5px; padding: 10px; } QPushButton:hover { background-color: #005a9e; } QPushButton:pressed { background-color: #004578; }");
    m_downloadButton->setCursor(Qt::PointingHandCursor);
    m_downloadButton->setToolTip("Click this button to start downloading the media based on your settings.");
    actionColumnLayout->addWidget(m_downloadButton);

    QLabel *downloadTypeLabel = new QLabel("Download Type:", this);
    downloadTypeLabel->setToolTip("Choose if you want to download the video, only the audio, a gallery of images, or just see what formats are available.");
    m_downloadTypeCombo = new QComboBox(this);
    m_downloadTypeCombo->addItem("Video", "video");
    m_downloadTypeCombo->addItem("Audio Only", "audio");
    m_downloadTypeCombo->addItem("Gallery", "gallery");
    m_downloadTypeCombo->addItem("View Formats", "formats");
    m_downloadTypeCombo->setToolTip("Choose if you want to download the video, only the audio, a gallery of images, or just see what formats are available.");
    actionColumnLayout->addWidget(downloadTypeLabel);
    actionColumnLayout->addWidget(m_downloadTypeCombo);

    actionColumnLayout->addStretch();

    inputSectionLayout->addLayout(actionColumnLayout, 30);
    mainLayout->addLayout(inputSectionLayout);

    QHBoxLayout *operationalControlsLayout = new QHBoxLayout();
    operationalControlsLayout->setSpacing(20);
    operationalControlsLayout->setAlignment(Qt::AlignLeft);

    QLabel *playlistLabel = new QLabel("Playlist Handling:", this);
    playlistLabel->setToolTip("How should the app handle links that are part of a playlist (like a YouTube playlist)?");
    m_playlistLogicCombo = new QComboBox(this);
    m_playlistLogicCombo->setToolTip("How should the app handle links that are part of a playlist (like a YouTube playlist)? 'Ask' will prompt you, 'Download All' will get everything, 'Download Single' will only get the video you linked.");
    m_playlistLogicCombo->addItems({"Ask", "Download All (no prompt)", "Download Single (ignore playlist)"});
    operationalControlsLayout->addWidget(playlistLabel);
    operationalControlsLayout->addWidget(m_playlistLogicCombo);

    QLabel *maxConcurrentLabel = new QLabel("Max Concurrent:", this);
    maxConcurrentLabel->setToolTip("Set how many downloads can happen at the same time. More downloads might slow down your internet.");
    m_maxConcurrentCombo = new QComboBox(this);
    m_maxConcurrentCombo->setToolTip("Set how many downloads can happen at the same time. More downloads might slow down your internet. '1 (short sleep)' and '1 (long sleep)' are for very slow connections or to avoid detection.");
    m_maxConcurrentCombo->addItems({"1", "2", "3", "4", "5", "6", "7", "8", "1 (short sleep)", "1 (long sleep)"});
    operationalControlsLayout->addWidget(maxConcurrentLabel);
    operationalControlsLayout->addWidget(m_maxConcurrentCombo);

    QLabel *rateLimitLabel = new QLabel("Rate Limit:", this);
    rateLimitLabel->setToolTip("Limit the download speed so it doesn't use up all your internet bandwidth.");
    m_rateLimitCombo = new QComboBox(this);
    m_rateLimitCombo->setToolTip("Limit the download speed so it doesn't use up all your internet bandwidth. 'Unlimited' uses full speed, others set a maximum speed.");
    m_rateLimitCombo->addItems({"Unlimited", "50 KB/s", "100 KB/s", "250 KB/s", "500 KB/s", "1 MB/s", "2 MB/s", "5 MB/s", "10 MB/s", "20 MB/s", "50 MB/s"});
    operationalControlsLayout->addWidget(rateLimitLabel);
    operationalControlsLayout->addWidget(m_rateLimitCombo);

    operationalControlsLayout->addStretch();

    QVBoxLayout *checkboxLayout = new QVBoxLayout();
    m_overrideDuplicateCheck = new QCheckBox("Override duplicate download check", this);
    m_overrideDuplicateCheck->setToolTip("If checked, allows downloading a URL even if it's already in your download history. Use this if you want to download the same file again.");
    checkboxLayout->addWidget(m_overrideDuplicateCheck);

    m_exitAfterDownloadsCheck = new QCheckBox("Exit after all downloads complete", this);
    m_exitAfterDownloadsCheck->setToolTip("If checked, the application will automatically close once all your downloads are finished.");
    checkboxLayout->addWidget(m_exitAfterDownloadsCheck);
    operationalControlsLayout->addLayout(checkboxLayout);

    mainLayout->addLayout(operationalControlsLayout);

    mainLayout->addStretch();

    QLabel *commandPreviewLabel = new QLabel("Command Preview:", this);
    commandPreviewLabel->setToolTip("Shows the exact command that will be executed.");
    mainLayout->addWidget(commandPreviewLabel);

    m_commandPreview = new QTextEdit(this);
    m_commandPreview->setReadOnly(true);
    m_commandPreview->setFontFamily("Courier New");
    m_commandPreview->setPlaceholderText("The generated command will be shown here...");
    m_commandPreview->setFixedHeight(120);
    m_commandPreview->setToolTip("The exact command-line arguments that will be passed to the downloader.");
    mainLayout->addWidget(m_commandPreview);
    applyCommandPreviewStyleSheet(); // Initial application of stylesheet

    connect(m_downloadButton, &QPushButton::clicked, this, &StartTab::onDownloadButtonClicked);
    connect(m_openDownloadsFolderButton, &QPushButton::clicked, this, &StartTab::openDownloadsFolder);
    connect(m_downloadTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StartTab::onDownloadTypeChanged);

    connect(m_urlInput, &QTextEdit::textChanged, this, &StartTab::updateCommandPreview);
    connect(m_downloadTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StartTab::updateCommandPreview);
    connect(m_playlistLogicCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StartTab::updateCommandPreview);
    connect(m_maxConcurrentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StartTab::updateCommandPreview);
    connect(m_rateLimitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StartTab::updateCommandPreview);
    connect(m_overrideDuplicateCheck, &QCheckBox::stateChanged, this, &StartTab::updateCommandPreview);
}

void StartTab::applyCommandPreviewStyleSheet() {
    if (!m_commandPreview) {
        return;
    }

    const QPalette palette = this->palette();
    const QString borderColor = palette.color(QPalette::Mid).name();
    const QString backgroundColor = palette.color(QPalette::AlternateBase).name(); // Use alternate base for distinction
    const QString textColor = palette.color(QPalette::Text).name();

    const QString style = QStringLiteral(R"(
        QTextEdit {
            padding: 5px;
            border: 1px solid %1;
            border-radius: 4px;
            background-color: %2;
            color: %3;
        }
    )").arg(borderColor, backgroundColor, textColor);

    m_commandPreview->setStyleSheet(style);
}

void StartTab::applyUrlInputStyleSheet() {
    if (!m_urlInput) {
        return;
    }

    const QPalette palette = this->palette();
    const QString borderColor = palette.color(QPalette::Mid).name();
    const QString backgroundColor = palette.color(QPalette::Base).name();
    const QString textColor = palette.color(QPalette::Text).name();
    const QString focusColor = palette.color(QPalette::Highlight).name();

    const QString style = QStringLiteral(R"(
        QTextEdit {
            font-size: 14px;
            padding: 5px;
            border: 1px solid %1;
            border-radius: 4px;
            background-color: %2;
            color: %3;
        }
        QTextEdit:focus {
            border: 1px solid %4;
        }
    )").arg(borderColor, backgroundColor, textColor, focusColor);

    m_urlInput->setStyleSheet(style);
}

void StartTab::changeEvent(QEvent *event) {
    if (event && event->type() == QEvent::PaletteChange) {
        applyUrlInputStyleSheet();
        applyCommandPreviewStyleSheet(); // Re-apply command preview style on theme change
    }
    QWidget::changeEvent(event);
}

void StartTab::loadSettings() {
    QSignalBlocker b1(m_playlistLogicCombo);
    QSignalBlocker b2(m_maxConcurrentCombo);
    QSignalBlocker b3(m_rateLimitCombo);
    QSignalBlocker b4(m_overrideDuplicateCheck);
    QSignalBlocker b5(m_exitAfterDownloadsCheck);

    if (m_playlistLogicCombo) // Added null check
        m_playlistLogicCombo->setCurrentText(m_configManager->get("General", "playlist_logic", "Ask").toString());
    if (m_maxConcurrentCombo) // Added null check
        m_maxConcurrentCombo->setCurrentText(m_configManager->get("General", "max_threads", "4").toString());
    if (m_rateLimitCombo) // Added null check
        m_rateLimitCombo->setCurrentText(m_configManager->get("General", "rate_limit", "Unlimited").toString());
    if (m_overrideDuplicateCheck) // Added null check
        m_overrideDuplicateCheck->setChecked(m_configManager->get("General", "override_archive", false).toBool());
    if (m_exitAfterDownloadsCheck) // Added null check
        m_exitAfterDownloadsCheck->setChecked(m_configManager->get("General", "exit_after", false).toBool());
}

void StartTab::onDownloadButtonClicked() {
    qDebug() << "StartTab::onDownloadButtonClicked called.";

    if (!m_urlInput) {
        qCritical() << "CRITICAL ERROR: m_urlInput is null in onDownloadButtonClicked!";
        return;
    }
    qDebug() << "Accessing m_urlInput...";
    QString urlStr = m_urlInput->toPlainText().trimmed();
    qDebug() << "m_urlInput accessed. urlStr:" << urlStr;

    if (urlStr.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter a valid URL(s).");
        return;
    }

    QStringList urls = urlStr.split('\n', Qt::SkipEmptyParts);
    if (urls.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter a valid URL(s).");
        return;
    }

    if (!m_playlistLogicCombo) {
        qCritical() << "CRITICAL ERROR: m_playlistLogicCombo is null in onDownloadButtonClicked!";
        return;
    }
    qDebug() << "Accessing m_playlistLogicCombo...";
    m_configManager->set("General", "playlist_logic", m_playlistLogicCombo->currentText());

    if (!m_maxConcurrentCombo) {
        qCritical() << "CRITICAL ERROR: m_maxConcurrentCombo is null in onDownloadButtonClicked!";
        return;
    }
    qDebug() << "Accessing m_maxConcurrentCombo...";
    m_configManager->set("General", "max_threads", m_maxConcurrentCombo->currentText());

    if (!m_rateLimitCombo) {
        qCritical() << "CRITICAL ERROR: m_rateLimitCombo is null in onDownloadButtonClicked!";
        return;
    }
    qDebug() << "Accessing m_rateLimitCombo...";
    m_configManager->set("General", "rate_limit", m_rateLimitCombo->currentText());

    if (!m_overrideDuplicateCheck) {
        qCritical() << "CRITICAL ERROR: m_overrideDuplicateCheck is null in onDownloadButtonClicked!";
        return;
    }
    qDebug() << "Accessing m_overrideDuplicateCheck...";
    m_configManager->set("General", "override_archive", m_overrideDuplicateCheck->isChecked());

    if (!m_exitAfterDownloadsCheck) {
        qCritical() << "CRITICAL ERROR: m_exitAfterDownloadsCheck is null in onDownloadButtonClicked!";
        return;
    }
    qDebug() << "Accessing m_exitAfterDownloadsCheck...";
    m_configManager->set("General", "exit_after", m_exitAfterDownloadsCheck->isChecked());
    m_configManager->save();

    if (!m_downloadTypeCombo) {
        qCritical() << "CRITICAL ERROR: m_downloadTypeCombo is null in onDownloadButtonClicked!";
        return;
    }
    qDebug() << "Accessing m_downloadTypeCombo...";
    QString type = m_downloadTypeCombo->currentData().toString();
    qDebug() << "m_downloadTypeCombo accessed. type:" << type;

    if (type == "formats") {
        checkFormats(urls.first());
        return;
    }

    QVariantMap options;
    options["type"] = type;

    for (const QString &singleUrl : urls) {
        QUrl url(singleUrl);
        if (!url.isValid()) {
            QMessageBox::warning(this, "Input Error", "The URL entered is invalid: " + singleUrl);
            continue;
        }
        emit downloadRequested(singleUrl, options);
    }

    m_urlInput->clear();
    qDebug() << "StartTab::onDownloadButtonClicked finished.";
}

void StartTab::onDownloadTypeChanged(int index)
{
    if (!m_downloadTypeCombo || !m_downloadButton) { // Added null checks
        qCritical() << "CRITICAL ERROR: m_downloadTypeCombo or m_downloadButton is null in onDownloadTypeChanged!";
        return;
    }
    QString type = m_downloadTypeCombo->itemData(index).toString();
    QString typeText = m_downloadTypeCombo->currentText();
    if (type == "formats") {
        m_downloadButton->setText("View Formats");
    } else {
        m_downloadButton->setText("Download " + typeText);
    }
    updateCommandPreview();
}

void StartTab::updateCommandPreview()
{
    qDebug() << "StartTab::updateCommandPreview called.";

    if (!m_urlInput) {
        qCritical() << "CRITICAL ERROR: m_urlInput is null in updateCommandPreview!";
        return;
    }
    QString url = m_urlInput->toPlainText().trimmed().split('\n').first();
    if (url.isEmpty()) {
        url = "[URL]";
    }

    QVariantMap options;
    if (!m_overrideDuplicateCheck) {
        qCritical() << "CRITICAL ERROR: m_overrideDuplicateCheck is null in updateCommandPreview!";
        return;
    }
    options["override_archive"] = m_overrideDuplicateCheck->isChecked();

    if (!m_rateLimitCombo) {
        qCritical() << "CRITICAL ERROR: m_rateLimitCombo is null in updateCommandPreview!";
        return;
    }
    options["rate_limit"] = m_rateLimitCombo->currentText();

    if (!m_downloadTypeCombo) {
        qCritical() << "CRITICAL ERROR: m_downloadTypeCombo is null in updateCommandPreview!";
        return;
    }
    QString downloadType = m_downloadTypeCombo->currentData().toString();
    if (downloadType == "gallery") {
        QString galleryDlPath = resolveExecutablePath("gallery-dl.exe");
        if (galleryDlPath.isEmpty()) {
            galleryDlPath = "gallery-dl.exe";
        }
        if (!m_galleryDlArgsBuilder) {
            qCritical() << "CRITICAL ERROR: m_galleryDlArgsBuilder is null in updateCommandPreview!";
            return;
        }
        QStringList args = m_galleryDlArgsBuilder->build(url, options);
        QString command = QDir::toNativeSeparators(galleryDlPath) + " " + args.join(" ");
        if (!m_commandPreview) {
            qCritical() << "CRITICAL ERROR: m_commandPreview is null in updateCommandPreview (gallery branch)!";
            return;
        }
        m_commandPreview->setText(command);
        return;
    }

    options["type"] = downloadType;
    if (!m_playlistLogicCombo) {
        qCritical() << "CRITICAL ERROR: m_playlistLogicCombo is null in updateCommandPreview!";
        return;
    }
    options["playlist_logic"] = m_playlistLogicCombo->currentText();

    if (!m_ytDlpArgsBuilder) {
        qCritical() << "CRITICAL ERROR: m_ytDlpArgsBuilder is null in updateCommandPreview!";
        return;
    }
    QStringList args = m_ytDlpArgsBuilder->build(m_configManager, url, options);

    QString ytDlpPath = resolveExecutablePath("yt-dlp.exe");
    if (ytDlpPath.isEmpty()) {
        ytDlpPath = "yt-dlp.exe";
    }

    if (!m_configManager) {
        qCritical() << "CRITICAL ERROR: m_configManager is null in updateCommandPreview!";
        return;
    }
    bool singleLine = m_configManager->get("General", "single_line_preview", false).toBool();

    if (!m_commandPreview) {
        qCritical() << "CRITICAL ERROR: m_commandPreview is null in updateCommandPreview!";
        return;
    }

    if (singleLine) {
        QString command = QDir::toNativeSeparators(ytDlpPath) + " " + args.join(" ");
        m_commandPreview->setText(command);
    } else {
        QString commandUrl = args.isEmpty() ? "" : args.takeFirst();
        QString command = QDir::toNativeSeparators(ytDlpPath) + " " + commandUrl + " \\\n    " + args.join(" \\\n    ");
        m_commandPreview->setText(command);
    }
    qDebug() << "StartTab::updateCommandPreview finished.";
}

QString StartTab::resolveExecutablePath(const QString &name) const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(name),
        QDir(QDir(appDir).filePath("bin")).filePath(name)
    };

    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }

    return QString();
}

void StartTab::checkFormats(const QString &url) {
    if (!m_downloadButton) { // Added null check
        qCritical() << "CRITICAL ERROR: m_downloadButton is null in checkFormats!";
        return;
    }
    m_downloadButton->setEnabled(false);
    m_downloadButton->setText("Checking...");

    QProcess *process = new QProcess(this);
    connect(process, &QProcess::finished, this, &StartTab::onViewFormatsFinished);

    QStringList args;
    args << url << "-F";

    QString cookiesBrowser = m_configManager->get("General", "cookies_from_browser", "None").toString();
    if (cookiesBrowser != "None") {
        args << "--cookies-from-browser" << cookiesBrowser;
    }

    const QString ytDlpPath = resolveExecutablePath("yt-dlp.exe");
    if (ytDlpPath.isEmpty()) {
        QMessageBox::critical(this, "Error", "yt-dlp.exe not found in application directory or bin/ subdirectory.");
        onViewFormatsFinished(1, QProcess::CrashExit);
        process->deleteLater();
        return;
    }

    process->start(ytDlpPath, args);
}

void StartTab::onViewFormatsFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QProcess *process = qobject_cast<QProcess*>(sender());
    if (m_downloadButton) { // Added null check
        m_downloadButton->setEnabled(true);
    }
    if (m_downloadTypeCombo) { // Added null check
        onDownloadTypeChanged(m_downloadTypeCombo->currentIndex());
    }

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        QString output = process->readAllStandardOutput();

        QDialog dialog(this);
        dialog.setWindowTitle("Available Formats");
        dialog.resize(600, 400);

        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        QTextEdit *textEdit = new QTextEdit(&dialog);
        textEdit->setReadOnly(true);
        textEdit->setFontFamily("Courier New");
        textEdit->setText(output);
        layout->addWidget(textEdit);

        QPushButton *closeButton = new QPushButton("Close", &dialog);
        connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
        layout->addWidget(closeButton);

        dialog.exec();
    } else {
        QString error = process->readAllStandardError();
        QMessageBox::critical(this, "Error", "Failed to retrieve formats:\n" + error);
    }

    process->deleteLater();
}

void StartTab::openDownloadsFolder() {
    QString completedDir = m_configManager->get("Paths", "completed_downloads_directory").toString();
    if (completedDir.isEmpty() || !QDir(completedDir).exists()) {
        QMessageBox::warning(this, "Folder Not Found",
                             "The completed downloads directory is not set or does not exist.\n"
                             "Please configure it in the Advanced Settings tab.");
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(completedDir));
}
