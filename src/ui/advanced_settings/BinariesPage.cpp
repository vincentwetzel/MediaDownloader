#include "BinariesPage.h"

#include "core/ConfigManager.h"
#include "core/ProcessUtils.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QTextCursor>
#include <QTimer>

BinariesPage::BinariesPage(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *scrollWidget = new QWidget(scrollArea);
    QVBoxLayout *layout = new QVBoxLayout(scrollWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *group = new QGroupBox("External Binaries", scrollWidget);
    group->setToolTip("Review detected binary locations, browse for custom executables, or launch an installer option.");
    QVBoxLayout *groupLayout = new QVBoxLayout(group);

    QLabel *introLabel = new QLabel(
        "<b>REQUIRED:</b> yt-dlp, ffmpeg, ffprobe, deno<br>"
        "<b>OPTIONAL:</b> gallery-dl, aria2c<br><br>"
        "<b>Browse</b> sets a manual path override. <b>Clear</b> reverts to auto-detection.<br>"
        "<b>Install</b> opens package-manager or manual-download options (refresh after installing).",
        scrollWidget);
    introLabel->setWordWrap(true);
    introLabel->setTextFormat(Qt::RichText);
    introLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    groupLayout->addWidget(introLabel);

    QPushButton *refreshButton = new QPushButton("Refresh All Statuses", scrollWidget);
    refreshButton->setToolTip("Re-scan configured paths and auto-detected binaries.");
    connect(refreshButton, &QPushButton::clicked, this, [this, refreshButton]() {
        refreshButton->setEnabled(false);
        for (auto it = m_statusLabels.constBegin(); it != m_statusLabels.constEnd(); ++it) {
            it.value()->setText("<b>Status:</b> Refreshing...");
        }
        for (auto it = m_installButtons.constBegin(); it != m_installButtons.constEnd(); ++it) {
            it.value()->setEnabled(false);
        }

        // Clear the binary resolution cache so stale "Not Found" entries
        // from before external installs are purged.
        ProcessUtils::clearCache();

        QTimer::singleShot(150, this, [this, refreshButton]() {
            loadSettings();
            refreshButton->setEnabled(true);
        });
    });
    groupLayout->addWidget(refreshButton, 0, Qt::AlignRight);

    setupRow(groupLayout, "yt-dlp", "yt-dlp", "yt-dlp_path", "https://github.com/yt-dlp/yt-dlp/releases/tag/nightly", false);
    setupRow(groupLayout, "ffmpeg", "ffmpeg", "ffmpeg_path", "https://ffmpeg.org/download.html", false);
    setupRow(groupLayout, "ffprobe", "ffprobe", "ffprobe_path", "https://ffmpeg.org/download.html", false);
    setupRow(groupLayout, "deno", "deno", "deno_path", "https://deno.com/", false);
    setupRow(groupLayout, "gallery-dl", "gallery-dl", "gallery-dl_path", "https://github.com/mikf/gallery-dl", true);
    setupRow(groupLayout, "aria2c", "aria2c", "aria2c_path", "https://github.com/aria2/aria2/releases", true);

    layout->addWidget(group);
    layout->addStretch();

    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    connect(m_configManager, &ConfigManager::settingChanged, this, &BinariesPage::handleConfigSettingChanged);
}

void BinariesPage::setupRow(QVBoxLayout *layout,
                            const QString &binaryName,
                            const QString &labelText,
                            const QString &configKey,
                            const QString &manualUrl,
                            bool optional) {
    m_configKeys.insert(binaryName, configKey);
    m_manualUrls.insert(binaryName, manualUrl);
    m_displayNames.insert(binaryName, labelText);
    if (optional) {
        m_optionalBinaries.insert(binaryName);
    }

    QGroupBox *rowGroup = new QGroupBox(this);
    QString title = optional ? QString("%1 (Optional)").arg(labelText) : labelText;
    rowGroup->setTitle(title);

    QFont groupFont = rowGroup->font();
    groupFont.setBold(true);
    rowGroup->setFont(groupFont);

    QHBoxLayout *rowLayout = new QHBoxLayout(rowGroup);
    rowLayout->setSpacing(16);

    QLabel *statusLabel = new QLabel(rowGroup);
    statusLabel->setWordWrap(true);
    statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusLabel->setToolTip(QString("Shows whether %1 was found automatically, configured manually, or is missing.").arg(labelText));
    statusLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    QSizePolicy labelPolicy = statusLabel->sizePolicy();
    labelPolicy.setHorizontalPolicy(QSizePolicy::Expanding);
    labelPolicy.setVerticalPolicy(QSizePolicy::Minimum);
    statusLabel->setSizePolicy(labelPolicy);

    QPushButton *browseButton = new QPushButton("Browse...", rowGroup);
    QPushButton *clearButton = new QPushButton("Clear", rowGroup);
    QPushButton *installButton = new QPushButton("Install...", rowGroup);

    QFont childFont = browseButton->font();
    childFont.setBold(false);
    browseButton->setFont(childFont);
    clearButton->setFont(childFont);
    installButton->setFont(childFont);
    statusLabel->setFont(childFont);

    browseButton->setToolTip(QString("Choose a specific %1 executable from disk to set a manual override.").arg(labelText));
    clearButton->setToolTip("Clear the manual path and revert to auto-detection.");
    installButton->setToolTip(QString("Open installer options for %1.").arg(labelText));

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    buttonLayout->addWidget(browseButton);
    buttonLayout->addWidget(clearButton);
    buttonLayout->addWidget(installButton);

    QVBoxLayout *rightCol = new QVBoxLayout();
    rightCol->addLayout(buttonLayout);
    rightCol->addStretch();

    rowLayout->addWidget(statusLabel, 1);
    rowLayout->addLayout(rightCol);

    layout->addWidget(rowGroup);

    m_statusLabels.insert(binaryName, statusLabel);
    m_installButtons.insert(binaryName, installButton);

    connect(browseButton, &QPushButton::clicked, this, [this, binaryName]() { browseBinaryFor(binaryName); });
    connect(clearButton, &QPushButton::clicked, this, [this, binaryName]() {
        saveBinaryOverride(binaryName, "");
    });
    connect(installButton, &QPushButton::clicked, this, [this, binaryName]() { installBinaryFor(binaryName); });
}

QString BinariesPage::browseBinary(const QString &title) const {
    QString filter;
#ifdef Q_OS_WIN
    filter = "Executables (*.exe);;All Files (*.*)";
#else
    filter = "All Files (*)";
#endif
    return QFileDialog::getOpenFileName(const_cast<BinariesPage *>(this), title, QString(), filter);
}

void BinariesPage::browseBinaryFor(const QString &binaryName) {
    const QString path = browseBinary(QString("Select %1 executable").arg(displayName(binaryName)));
    if (!path.isEmpty()) {
        saveBinaryOverride(binaryName, path);
    }
}

void BinariesPage::installBinaryFor(const QString &binaryName) {
    QDialog dialog(this);
    dialog.setWindowTitle(QString("Install %1").arg(displayName(binaryName)));
    dialog.setToolTip(QString("Choose how to install %1.").arg(displayName(binaryName)));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    const QList<InstallOption> options = buildInstallOptions(binaryName);

    QLabel *infoLabel = new QLabel(&dialog);
    if (options.isEmpty()) {
        infoLabel->setText(QString("No supported package managers were detected for %1. Use Manual Download instead.").arg(displayName(binaryName)));
    } else {
        infoLabel->setText(QString("Select an installation method for %1. Package-manager options were detected from this system.").arg(displayName(binaryName)));
    }
    infoLabel->setWordWrap(true);
    infoLabel->setToolTip("Package-manager commands are launched through the system shell (cmd.exe on Windows).");
    layout->addWidget(infoLabel);

    QComboBox *optionsCombo = new QComboBox(&dialog);
    optionsCombo->setToolTip("Choose a detected package-manager command.");
    for (const InstallOption &option : options) {
        optionsCombo->addItem(option.label);
    }
    layout->addWidget(optionsCombo);

    QLabel *descriptionLabel = new QLabel(&dialog);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setToolTip("Summary of the selected install method.");
    layout->addWidget(descriptionLabel);

    QLabel *commandLabel = new QLabel(&dialog);
    commandLabel->setWordWrap(true);
    commandLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    commandLabel->setToolTip("Command that will be launched when you choose Run Install Command.");
    layout->addWidget(commandLabel);

    auto updateSelectionText = [&]() {
        if (options.isEmpty()) {
            descriptionLabel->setText("No supported package manager was detected. Use Manual Download instead.");
            commandLabel->setText("Command: Not available");
            return;
        }

        const InstallOption &option = options.at(optionsCombo->currentIndex());
        descriptionLabel->setText(option.description);
        commandLabel->setText(QString("Command: %1").arg(commandPreview(option)));
    };

    connect(optionsCombo, &QComboBox::currentIndexChanged, &dialog, updateSelectionText);
    updateSelectionText();

    QDialogButtonBox *buttons = new QDialogButtonBox(&dialog);
    QPushButton *runButton = buttons->addButton("Run Install Command", QDialogButtonBox::AcceptRole);
    QPushButton *manualButton = buttons->addButton("Manual Download", QDialogButtonBox::ActionRole);
    QPushButton *cancelButton = buttons->addButton(QDialogButtonBox::Cancel);
    runButton->setToolTip("Launch the selected package-manager command in a detached process.");
    manualButton->setToolTip("Open the official download page and show manual placement instructions.");
    cancelButton->setToolTip("Close this installer dialog.");
    runButton->setEnabled(!options.isEmpty());
    layout->addWidget(buttons);

    connect(runButton, &QPushButton::clicked, &dialog, [&]() {
        const InstallOption &option = options.at(optionsCombo->currentIndex());

        QDialog progressDialog(&dialog);
        progressDialog.setWindowTitle(QString("Installing %1").arg(displayName(binaryName)));
        progressDialog.resize(600, 400);

        QVBoxLayout *pLayout = new QVBoxLayout(&progressDialog);
        QTextEdit *outputEdit = new QTextEdit(&progressDialog);
        outputEdit->setReadOnly(true);
        outputEdit->setFontFamily("Courier New");
        pLayout->addWidget(outputEdit);

        QDialogButtonBox *pButtons = new QDialogButtonBox(QDialogButtonBox::Close, &progressDialog);
        QPushButton *closeBtn = pButtons->button(QDialogButtonBox::Close);
        closeBtn->setEnabled(false);
        pLayout->addWidget(pButtons);

        connect(pButtons, &QDialogButtonBox::rejected, &progressDialog, &QDialog::reject);

        QProcess *process = new QProcess(&progressDialog);
        process->setProcessChannelMode(QProcess::MergedChannels);

        // For WindowsApps execution-alias stubs (winget, pip from MS Store) we
        // must NOT invoke the full path directly — the 0-byte stubs crash when
        // executed that way. Instead we prepend the WindowsApps directory to
        // PATH and launch with the bare program name so the shell resolves the
        // alias correctly.
        bool isAlias = option.extraData.value("is_windows_apps_alias").toBool();

        if (isAlias) {
            // Prepend WindowsApps directory to PATH
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            const QString localAppData = env.value("LOCALAPPDATA");
            if (!localAppData.isEmpty()) {
                const QString windowsAppsPath = localAppData + "/Microsoft/WindowsApps";
                QString currentPath = env.value("PATH");
                env.insert("PATH", windowsAppsPath + ";" + currentPath);
                process->setProcessEnvironment(env);
            }
        }

        // Build the command string
        QString fullCommand = option.program;
        for (const QString &arg : option.arguments) {
            if (arg.contains(' ')) {
                fullCommand += " \"" + arg + "\"";
            } else {
                fullCommand += " " + arg;
            }
        }

        connect(process, &QProcess::readyReadStandardOutput, [&]() {
            QString output = QString::fromLocal8Bit(process->readAllStandardOutput());
            outputEdit->moveCursor(QTextCursor::End);
            outputEdit->insertPlainText(output);
            outputEdit->moveCursor(QTextCursor::End);
        });

        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [&](int exitCode, QProcess::ExitStatus exitStatus) {
            closeBtn->setEnabled(true);
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                outputEdit->moveCursor(QTextCursor::End);
                outputEdit->insertPlainText("\n--- Installation completed successfully. ---\n");

                // Clear cache and refresh status to use the newly installed binary
                ProcessUtils::clearCache();
                this->refreshBinaryStatus(binaryName);
                if (binaryName == "ffmpeg") this->refreshBinaryStatus("ffprobe");
                else if (binaryName == "ffprobe") this->refreshBinaryStatus("ffmpeg");

                QMessageBox::information(&progressDialog, "Install Successful", QString("%1 has been installed successfully.").arg(displayName(binaryName)));
            } else {
                outputEdit->moveCursor(QTextCursor::End);
                outputEdit->insertPlainText(QString("\n--- Installation failed with exit code %1. ---\n").arg(exitCode));
                QMessageBox::warning(&progressDialog, "Install Failed", QString("The installation of %1 failed. Please check the output log.").arg(displayName(binaryName)));
            }
        });

        connect(process, &QProcess::errorOccurred, [&](QProcess::ProcessError error) {
            closeBtn->setEnabled(true);
            outputEdit->moveCursor(QTextCursor::End);
            outputEdit->insertPlainText(QString("\n--- Process error: %1 ---\n").arg(process->errorString()));
        });

        outputEdit->insertPlainText(QString("Running command: %1\n\n").arg(fullCommand));
#ifdef Q_OS_WIN
        process->start("cmd", {"/C", fullCommand});
#else
        process->start("/bin/sh", {"-c", fullCommand});
#endif

        progressDialog.exec();
        dialog.accept();
    });

    connect(manualButton, &QPushButton::clicked, &dialog, [&]() {
        QDesktopServices::openUrl(QUrl(m_manualUrls.value(binaryName)));
        QMessageBox::information(
            &dialog,
            "Manual Download",
            QString("The official download page for %1 was opened in your browser.\n\n"
                    "After downloading, place the executable somewhere permanent and use Browse to point LzyDownloader to it.")
                .arg(displayName(binaryName)));
    });

    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
}

void BinariesPage::saveBinaryOverride(const QString &binaryName, const QString &path) {
    const QString configKey = m_configKeys.value(binaryName);
    if (configKey.isEmpty()) {
        return;
    }

    if (path.isEmpty()) {
        m_configManager->remove("Binaries", configKey);
    } else {
        m_configManager->set("Binaries", configKey, path);
    }
    m_configManager->save(); // Explicitly save changes to disk
    
    // Clear cache so the new path is used immediately
    ProcessUtils::clearCache();
}

void BinariesPage::loadSettings() {
    for (auto it = m_statusLabels.constBegin(); it != m_statusLabels.constEnd(); ++it) {
        refreshBinaryStatus(it.key());
    }
}

void BinariesPage::handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value) {
    if (section != "Binaries") {
        return;
    }

    const QString binaryName = m_configKeys.key(key);
    if (binaryName.isEmpty() || !m_statusLabels.contains(binaryName)) {
        return;
    }

    refreshBinaryStatus(binaryName);
}

void BinariesPage::refreshBinaryStatus(const QString &binaryName) {
    QLabel *statusLabel = m_statusLabels.value(binaryName);
    QPushButton *installButton = m_installButtons.value(binaryName);
    if (!statusLabel || !installButton) {
        return;
    }

    // Use resolveBinary for fresh lookup (bypasses cache)
    const ProcessUtils::FoundBinary foundBinary = ProcessUtils::resolveBinary(binaryName, m_configManager);
    
    qDebug() << "[BinariesPage] refreshBinaryStatus:" << binaryName << "- source:" << foundBinary.source << "| path:" << foundBinary.path;

    QString displayPath = foundBinary.path.toHtmlEscaped();
    // Insert a zero-width space after path separators so QLabel can properly word-wrap long paths without spaces
    displayPath.replace("\\", QString("\\") + QChar(0x200B));
    displayPath.replace("/", QString("/") + QChar(0x200B));

    if (foundBinary.source == "Not Found") {
        const bool isOptional = m_optionalBinaries.contains(binaryName);
        const QString requiredText = isOptional ? "This is an optional enhancement." : "This binary is required for core functionality.";

        QString statusPrefix = isOptional
            ? "⚠️ <span style='color: #d97706;'>Missing</span>"
            : "❌ <span style='color: #dc2626;'>Missing</span>";

        statusLabel->setText(QString("<b>Status:</b> %1. %2").arg(statusPrefix, requiredText));
        installButton->setEnabled(true);
    } else if (foundBinary.source == "Invalid Custom") {
        const bool isOptional = m_optionalBinaries.contains(binaryName);
        QString statusPrefix = isOptional
            ? "⚠️ <span style='color: #d97706;'>Not Found</span>"
            : "❌ <span style='color: #dc2626;'>Not Found</span>";
        statusLabel->setText(QString("<b>Status:</b> %1 (invalid manual override)<br><b>Path:</b> %2").arg(statusPrefix, displayPath));
        installButton->setEnabled(true);
    } else if (foundBinary.source == "Custom") {
        QString statusPrefix = "✅ <span style='color: #16a34a;'>Found</span>";
        statusLabel->setText(QString("<b>Status:</b> %1 (manual override)<br><b>Path:</b> %2").arg(statusPrefix, displayPath));
        installButton->setEnabled(false);
    } else {
        QString statusPrefix = "✅ <span style='color: #16a34a;'>Found</span>";
        statusLabel->setText(QString("<b>Status:</b> %1 (auto-detected via %2)<br><b>Path:</b> %3").arg(statusPrefix, foundBinary.source, displayPath));
        installButton->setEnabled(false);
    }
}

QList<BinariesPage::InstallOption> BinariesPage::buildInstallOptions(const QString &binaryName) const {
    QList<InstallOption> options;
    const QString display = displayName(binaryName);

    // OS-gating so users don't see macOS/Linux-only managers on Windows and vice versa.
    auto isWindows = []() -> bool {
#ifdef Q_OS_WIN
        return true;
#else
        return false;
#endif
    };
    auto isMacOS = []() -> bool {
#ifdef Q_OS_MACOS
        return true;
#else
        return false;
#endif
    };
    auto isLinux = []() -> bool {
#ifdef Q_OS_LINUX
        return true;
#else
        return false;
#endif
    };

    auto addOptionIfPresent = [&](const QString &program, const QStringList &arguments, const QString &description) {
        QString programPath;
        bool isWindowsAppsAlias = false;

        // First try standard PATH lookup
        QString foundPath = QStandardPaths::findExecutable(program);
        if (!foundPath.isEmpty()) {
            programPath = foundPath;
        }

        // On Windows, many tools live in the WindowsApps execution-alias directory
        // which is NOT in PATH. These are 0-byte stubs that only work through shell
        // alias resolution — they crash if invoked by full path directly.
        // Mark them so the launch code can handle them correctly.
        if (programPath.isEmpty()) {
            const QString localAppData = QProcessEnvironment::systemEnvironment().value("LOCALAPPDATA");
            if (!localAppData.isEmpty()) {
                const QString windowsApps = localAppData + "/Microsoft/WindowsApps/" + program + ".exe";
                if (QFile::exists(windowsApps)) {
                    // Don't store the full path — keep the bare name and flag as alias.
                    // The launch code will prepend WindowsApps to PATH so the alias
                    // resolves correctly via shell magic.
                    programPath = program;
                    isWindowsAppsAlias = true;
                }
            }
        }

        if (programPath.isEmpty()) {
            return;
        }

        InstallOption option;
        option.label = QString("%1 (%2)").arg(program, display);
        option.description = description;
        option.program = programPath;
        option.arguments = arguments;
        option.extraData["is_windows_apps_alias"] = isWindowsAppsAlias;
        options.append(option);
    };

    if (binaryName == "yt-dlp") {
        addOptionIfPresent("pip", {"install", "-U", "--pre", "yt-dlp"}, "Install or upgrade yt-dlp (nightly) with pip.");
        if (isWindows()) addOptionIfPresent("winget", {"install", "yt-dlp", "--accept-package-agreements", "--accept-source-agreements"}, "Install yt-dlp with WinGet. Note: This installs the stable version, as a nightly package is not available.");
        if (isWindows()) addOptionIfPresent("choco", {"install", "-y", "yt-dlp"}, "Install yt-dlp with Chocolatey. Note: This installs the stable version, as a nightly package is not available.");
        if (isWindows()) addOptionIfPresent("scoop", {"install", "yt-dlp-nightly"}, "Install yt-dlp (nightly) with Scoop. Requires the 'extras' bucket (`scoop bucket add extras`).");
        if (isMacOS() || isLinux()) addOptionIfPresent("brew", {"install", "yt-dlp", "--HEAD"}, "Install yt-dlp (latest from master) with Homebrew.");
    } else if (binaryName == "ffmpeg" || binaryName == "ffprobe") {
        if (isWindows()) addOptionIfPresent("winget", {"install", "ffmpeg", "--accept-package-agreements", "--accept-source-agreements"}, "Install FFmpeg (includes ffprobe) with WinGet.");
        if (isWindows()) addOptionIfPresent("choco", {"install", "-y", "ffmpeg"}, "Install FFmpeg (includes ffprobe) with Chocolatey.");
        if (isWindows()) addOptionIfPresent("scoop", {"install", "ffmpeg"}, "Install FFmpeg (includes ffprobe) with Scoop.");
        if (isLinux()) addOptionIfPresent("apt", {"install", "-y", "ffmpeg"}, "Install FFmpeg (includes ffprobe) with apt.");
        if (isLinux()) addOptionIfPresent("dnf", {"install", "-y", "ffmpeg"}, "Install FFmpeg (includes ffprobe) with dnf.");
        if (isLinux()) addOptionIfPresent("pacman", {"-S", "--noconfirm", "ffmpeg"}, "Install FFmpeg (includes ffprobe) with pacman.");
        if (isMacOS() || isLinux()) addOptionIfPresent("brew", {"install", "ffmpeg"}, "Install FFmpeg (includes ffprobe) with Homebrew.");
    } else if (binaryName == "gallery-dl") {
        addOptionIfPresent("pip", {"install", "-U", "gallery-dl"}, "Install or upgrade gallery-dl with pip.");
        if (isWindows()) addOptionIfPresent("winget", {"install", "gallery-dl", "--accept-package-agreements", "--accept-source-agreements"}, "Install gallery-dl with WinGet.");
        if (isWindows()) addOptionIfPresent("choco", {"install", "-y", "gallery-dl"}, "Install gallery-dl with Chocolatey.");
        if (isWindows()) addOptionIfPresent("scoop", {"install", "gallery-dl"}, "Install gallery-dl with Scoop.");
        if (isMacOS() || isLinux()) addOptionIfPresent("brew", {"install", "gallery-dl"}, "Install gallery-dl with Homebrew.");
    } else if (binaryName == "aria2c") {
        if (isWindows()) addOptionIfPresent("winget", {"install", "aria2", "--accept-package-agreements", "--accept-source-agreements"}, "Install aria2 with WinGet.");
        if (isWindows()) addOptionIfPresent("choco", {"install", "-y", "aria2"}, "Install aria2 with Chocolatey.");
        if (isWindows()) addOptionIfPresent("scoop", {"install", "aria2"}, "Install aria2 with Scoop.");
        if (isLinux()) addOptionIfPresent("apt", {"install", "-y", "aria2"}, "Install aria2 with apt.");
        if (isLinux()) addOptionIfPresent("dnf", {"install", "-y", "aria2"}, "Install aria2 with dnf.");
        if (isLinux()) addOptionIfPresent("pacman", {"-S", "--noconfirm", "aria2"}, "Install aria2 with pacman.");
        if (isMacOS() || isLinux()) addOptionIfPresent("brew", {"install", "aria2"}, "Install aria2 with Homebrew.");
    } else if (binaryName == "deno") {
        if (isWindows()) addOptionIfPresent("winget", {"install", "deno", "--accept-package-agreements", "--accept-source-agreements"}, "Install Deno with WinGet.");
        if (isWindows()) addOptionIfPresent("choco", {"install", "-y", "deno"}, "Install Deno with Chocolatey.");
        if (isWindows()) addOptionIfPresent("scoop", {"install", "deno"}, "Install Deno with Scoop.");
        if (isMacOS() || isLinux()) addOptionIfPresent("brew", {"install", "deno"}, "Install Deno with Homebrew.");
    }

    return options;
}

QString BinariesPage::commandPreview(const InstallOption &option) const {
    return option.program + " " + option.arguments.join(' ');
}

QString BinariesPage::displayName(const QString &binaryName) const {
    return m_displayNames.value(binaryName, binaryName);
}
