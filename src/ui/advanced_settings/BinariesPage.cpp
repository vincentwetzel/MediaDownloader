#include "BinariesPage.h"

#include "core/ConfigManager.h"
#include "core/ProcessUtils.h"

#include <QComboBox>
#include <QCoreApplication>
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
        "<b>Browse</b> sets a manual path override. <b>Clear Path</b> reverts to auto-detection.<br>"
        "<b>Install</b> opens package-manager or official website download options.<br><br>"
        "<span style='color: #d97706;'><b>Note:</b> If you install tools via package managers (winget, scoop, chocolatey, etc.), "
        "you must <b>restart LzyDownloader</b> for it to detect the updated system PATH. "
        "(The built-in installer will do this automatically.)</span>",
        scrollWidget);
    introLabel->setWordWrap(true);
    introLabel->setTextFormat(Qt::RichText);
    introLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    groupLayout->addWidget(introLabel);

    QPushButton *refreshButton = new QPushButton("Refresh All Statuses", scrollWidget);
    refreshButton->setToolTip("Re-scan configured paths and auto-detected binaries.\nIf a newly installed package manager tool is still missing, restart the app to refresh the system PATH.");
    connect(refreshButton, &QPushButton::clicked, this, [this, refreshButton]() {
        refreshButton->setEnabled(false);
        for (auto it = m_statusLabels.constBegin(); it != m_statusLabels.constEnd(); ++it) {
            it.value()->setText("<b>Status:</b> Refreshing...");
        }
        for (auto it = m_installButtons.constBegin(); it != m_installButtons.constEnd(); ++it) {
            it.value()->setEnabled(false);
        }
        for (auto it = m_updateButtons.constBegin(); it != m_updateButtons.constEnd(); ++it) {
            it.value()->setEnabled(false);
        }
        for (const QString& binaryName : m_configKeys.keys()) {
            if (QPushButton *clearBtn = this->findChild<QPushButton*>(binaryName + "_clearButton")) {
                clearBtn->setEnabled(false);
            }
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

    setupRow(groupLayout, "yt-dlp", "yt-dlp", "yt-dlp_path", "https://github.com/yt-dlp/yt-dlp-nightly-builds/releases/latest", false);
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

    QLabel *versionLabel = new QLabel("Version: Unknown", rowGroup);
    versionLabel->setWordWrap(true);
    versionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    versionLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    QString description;
    if (binaryName == "yt-dlp") description = "Downloads video and audio from online platforms.";
    else if (binaryName == "ffmpeg") description = "Merges and converts media formats.";
    else if (binaryName == "ffprobe") description = "Analyzes media files and metadata.";
    else if (binaryName == "deno") description = "Executes JavaScript for solving anti-bot challenges.";
    else if (binaryName == "gallery-dl") description = "Downloads image galleries.";
    else if (binaryName == "aria2c") description = "Accelerates downloads using multiple connections.";

    QLabel *descLabel = new QLabel(QString("<i>%1</i>").arg(description), rowGroup);
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    QVBoxLayout *leftCol = new QVBoxLayout();
    leftCol->addWidget(descLabel);
    leftCol->addWidget(statusLabel);
    leftCol->addWidget(versionLabel);
    leftCol->addStretch();

    QPushButton *browseButton = new QPushButton("Browse...", rowGroup);
    QPushButton *clearButton = new QPushButton("Clear Path", rowGroup);
    clearButton->setObjectName(binaryName + "_clearButton");
    QPushButton *installButton = new QPushButton("Install...", rowGroup);
    QPushButton *updateButton = new QPushButton("Update", rowGroup);

    if (binaryName == "yt-dlp") {
        updateButton->setObjectName("updateYtDlpButton");
        updateButton->setToolTip("Check for and install yt-dlp updates.");
    } else if (binaryName == "gallery-dl") {
        updateButton->setObjectName("updateGalleryDlButton");
        updateButton->setToolTip("Check for and install gallery-dl updates.");
    }

    QFont childFont = browseButton->font();
    childFont.setBold(false);
    browseButton->setFont(childFont);
    clearButton->setFont(childFont);
    installButton->setFont(childFont);
    updateButton->setFont(childFont);
    statusLabel->setFont(childFont);
    versionLabel->setFont(childFont);
    descLabel->setFont(childFont);

    browseButton->setToolTip(QString("Choose a specific %1 executable from disk to set a manual override.").arg(labelText));
    clearButton->setToolTip("Clear the manual path override and revert to auto-detection.");
    installButton->setToolTip(QString("Open installer options for %1.").arg(labelText));

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    buttonLayout->addWidget(browseButton);
    buttonLayout->addWidget(clearButton);
    buttonLayout->addWidget(installButton);
    buttonLayout->addWidget(updateButton);

    QVBoxLayout *rightCol = new QVBoxLayout();
    rightCol->addLayout(buttonLayout);
    rightCol->addStretch();

    rowLayout->addLayout(leftCol, 1);
    rowLayout->addLayout(rightCol);

    layout->addWidget(rowGroup);

    m_statusLabels.insert(binaryName, statusLabel);
    m_versionLabels.insert(binaryName, versionLabel);
    m_installButtons.insert(binaryName, installButton);
    m_updateButtons.insert(binaryName, updateButton);

    connect(browseButton, &QPushButton::clicked, this, [this, binaryName]() { browseBinaryFor(binaryName); });
    connect(clearButton, &QPushButton::clicked, this, [this, binaryName]() {
        saveBinaryOverride(binaryName, "");
    });
    connect(installButton, &QPushButton::clicked, this, [this, binaryName]() { installBinaryFor(binaryName); });
}

void BinariesPage::setYtDlpVersion(const QString &version) {
    // Deprecated: BinariesPage now autonomously fetches its own versions
    // Ignoring this signal prevents StartupWorker from overwriting the UI with stale errors
}

void BinariesPage::setGalleryDlVersion(const QString &version) {
    // Deprecated: BinariesPage now autonomously fetches its own versions
    // Ignoring this signal prevents StartupWorker from overwriting the UI with stale errors
}

void BinariesPage::fetchBinaryVersion(const QString &binaryName, const QString &path) {
    if (path.isEmpty() || !m_versionLabels.contains(binaryName)) {
        return;
    }

    m_versionLabels[binaryName]->setText("Version: Fetching...");

    QProcess *process = new QProcess(this);
    ProcessUtils::setProcessEnvironment(*process);
    process->setProcessChannelMode(QProcess::MergedChannels);

#ifdef Q_OS_WIN
    // cmd.exe seamlessly resolves WindowsApps execution aliases (winget stubs)
    process->setProgram("cmd.exe");
    QString argVersion = (binaryName == "ffmpeg" || binaryName == "ffprobe") ? "-version" : "--version";
    process->setArguments({"/c", path, argVersion});
#else
    process->setProgram(path);
    if (binaryName == "ffmpeg" || binaryName == "ffprobe") {
        process->setArguments({"-version"});
    } else {
        process->setArguments({"--version"});
    }
#endif
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, [this, process, binaryName](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            QString output = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
            QString firstLine = output.split('\n').first().trimmed();
            if (firstLine.length() > 65) firstLine = firstLine.left(62) + "...";
            m_versionLabels[binaryName]->setText("Version: " + firstLine);
        } else {
            m_versionLabels[binaryName]->setText("Version: Error");
        }
    });
    
    connect(process, &QProcess::errorOccurred, this, [this, binaryName](QProcess::ProcessError) {
        m_versionLabels[binaryName]->setText("Version: Error");
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), process, &QObject::deleteLater);

    process->start();
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

    QList<InstallOption> options = buildInstallOptions(binaryName);

    QLabel *infoLabel = new QLabel(&dialog);
    if (options.isEmpty()) {
        infoLabel->setText(QString("No supported package managers were detected for %1. Please use the 'Download from Official Website' option.").arg(displayName(binaryName)));
    } else {
        infoLabel->setText(QString("Select an installation method for %1. Package-manager options were detected from this system.").arg(displayName(binaryName)));
    }
    infoLabel->setWordWrap(true);
    infoLabel->setToolTip("Package-manager commands are launched through the system shell (cmd.exe on Windows).");
    layout->addWidget(infoLabel);

    InstallOption manualOpt;
    manualOpt.label = "Download from Official Website";
    manualOpt.description = "Open the official download page in your web browser and show manual placement instructions.";
    manualOpt.extraData["is_manual_download"] = true;
    options.append(manualOpt);

    QComboBox *optionsCombo = new QComboBox(&dialog);
    optionsCombo->setToolTip("Choose an installation method.");
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
        const InstallOption &option = options.at(optionsCombo->currentIndex());
        descriptionLabel->setText(option.description);
        if (option.extraData.value("is_manual_download").toBool()) {
            commandLabel->setText("Command: Opens in default web browser");
        } else {
            commandLabel->setText(QString("Command: %1").arg(commandPreview(option)));
        }
    };

    connect(optionsCombo, &QComboBox::currentIndexChanged, &dialog, updateSelectionText);
    updateSelectionText();

    QDialogButtonBox *buttons = new QDialogButtonBox(&dialog);
    QPushButton *runButton = buttons->addButton("Run Install", QDialogButtonBox::AcceptRole);
    QPushButton *cancelButton = buttons->addButton(QDialogButtonBox::Cancel);
    runButton->setToolTip("Execute the selected installation method.");
    cancelButton->setToolTip("Close this installer dialog.");
    layout->addWidget(buttons);

    connect(runButton, &QPushButton::clicked, &dialog, [&]() {
        const InstallOption &option = options.at(optionsCombo->currentIndex());

        if (option.extraData.value("is_manual_download").toBool()) {
            QDesktopServices::openUrl(QUrl(m_manualUrls.value(binaryName)));
            QMessageBox::information(
                &dialog,
                "Browser Download",
                QString("The official download page for %1 was opened in your browser.\n\n"
                        "After downloading, place the executable somewhere permanent and use Browse to point LzyDownloader to it.")
                    .arg(displayName(binaryName)));
            dialog.accept();
            return;
        }

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

        // For WindowsApps execution-alias stubs (for example winget from the MS Store) we
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

                if (option.extraData.contains("set_custom_path")) {
                    QString customPath = option.extraData.value("set_custom_path").toString();
                    if (QFile::exists(customPath)) {
#ifndef Q_OS_WIN
                        QFile::setPermissions(customPath, QFile::permissions(customPath) | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);
#endif
                        this->saveBinaryOverride(binaryName, customPath);
                    }
                }

                // Clear cache and refresh status to use the newly installed binary
                ProcessUtils::clearCache();
                this->refreshBinaryStatus(binaryName);
                if (binaryName == "ffmpeg") this->refreshBinaryStatus("ffprobe");
                else if (binaryName == "ffprobe") this->refreshBinaryStatus("ffmpeg");

                QMessageBox::information(&progressDialog, "Install Successful", 
                    QString("%1 has been installed successfully.\n\n"
                            "LzyDownloader will now restart to detect your updated system PATH "
                            "and apply the changes.").arg(displayName(binaryName)));

                // Safely restart the application via the OS shell and avoid a
                // single-instance lock race by adding a slight delay first.
                const QString rawAppPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
                const QFileInfo appInfo(rawAppPath);
                QString appPath = QDir::toNativeSeparators(appInfo.canonicalFilePath());
                if (appPath.isEmpty()) {
                    appPath = QDir::toNativeSeparators(appInfo.absoluteFilePath());
                }
#ifdef Q_OS_WIN
                if (appPath.startsWith("\\\\?\\")) {
                    appPath.remove(0, 4);
                }
#endif
                if (appPath.isEmpty() || !QFileInfo::exists(appPath)) {
                    QMessageBox::warning(
                        &progressDialog,
                        "Restart Failed",
                        QString("The installation finished, but LzyDownloader could not determine a valid restart path.\n\n"
                                "Expected executable:\n%1\n\n"
                                "Please restart the app manually.")
                            .arg(appPath.isEmpty() ? QStringLiteral("(empty path)") : appPath));
                    qWarning() << "[BinariesPage] Restart aborted: invalid application path. Raw:"
                               << rawAppPath << "| normalized:" << appPath;
                    return;
                }

                qInfo() << "[BinariesPage] Scheduling restart after install. Raw path:"
                        << rawAppPath << "| normalized:" << appPath;

#ifdef Q_OS_WIN
                // Ask Windows PowerShell to wait briefly, then ask Explorer to
                // open the app path. This is more tolerant than `cmd /c start`
                // and lets the relaunched process come from the shell path
                // instead of inheriting this process' stale PATH snapshot.
                QString psAppPath = appPath;
                psAppPath.replace('\'', "''");
                const QString restartCommand =
                    QString("Start-Sleep -Seconds 3; Start-Process explorer.exe -ArgumentList @('%1')")
                        .arg(psAppPath);
                const bool restartScheduled = QProcess::startDetached(
                    "powershell.exe",
                    {"-NoProfile", "-WindowStyle", "Hidden", "-Command", restartCommand},
                    QFileInfo(appPath).absolutePath());
#else
                const bool restartScheduled = QProcess::startDetached(
                    "sh",
                    {"-c", QString("sleep 3 && \"%1\" &").arg(appPath)},
                    appInfo.absolutePath());
#endif
                if (!restartScheduled) {
                    QMessageBox::warning(
                        &progressDialog,
                        "Restart Failed",
                        QString("The installation finished, but LzyDownloader could not relaunch itself.\n\n"
                                "Executable:\n%1\n\n"
                                "Please restart the app manually.")
                            .arg(appPath));
                    qWarning() << "[BinariesPage] Failed to schedule restart for:" << appPath;
                    return;
                }

                progressDialog.accept();
                QCoreApplication::quit();
            } else {
                outputEdit->moveCursor(QTextCursor::End);
                outputEdit->insertPlainText(QString("\n--- Installation failed with exit code %1. ---\n").arg(exitCode));
                
                QString logText = outputEdit->toPlainText();
                if (logText.contains("Permission denied", Qt::CaseInsensitive) || logText.contains("Access is denied", Qt::CaseInsensitive)) {
                    QMessageBox::warning(&progressDialog, "Permission Denied", 
                        QString("The installation of %1 failed due to insufficient permissions.\n\n"
                                "If you are trying to install or update in a protected system folder (like 'Program Files'), "
                                "please restart LzyDownloader as Administrator and try again.").arg(displayName(binaryName)));
                } else {
                    QMessageBox::warning(&progressDialog, "Install Failed", QString("The installation of %1 failed. Please check the output log.").arg(displayName(binaryName)));
                }
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
    QLabel *versionLabel = m_versionLabels.value(binaryName);
    QPushButton *updateButton = m_updateButtons.value(binaryName);
    QPushButton *clearButton = this->findChild<QPushButton*>(binaryName + "_clearButton");

    if (!statusLabel || !installButton || !versionLabel || !updateButton) {
        return;
    }

    // Use resolveBinary for fresh lookup (bypasses cache)
    const ProcessUtils::FoundBinary foundBinary = ProcessUtils::resolveBinary(binaryName, m_configManager);
    
    qDebug() << "[BinariesPage] refreshBinaryStatus:" << binaryName << "- source:" << foundBinary.source << "| path:" << foundBinary.path;

    QString displayPath = foundBinary.path.toHtmlEscaped();
    // Insert a zero-width space after path separators so QLabel can properly word-wrap long paths without spaces
    displayPath.replace("\\", QString("\\") + QChar(0x200B));
    displayPath.replace("/", QString("/") + QChar(0x200B));

    bool isInstalled = (foundBinary.source != "Not Found" && foundBinary.source != "Invalid Custom");
    bool hasCustomPath = !m_configManager->get("Binaries", m_configKeys.value(binaryName)).toString().isEmpty();
    bool supportsUpdate = (binaryName == "yt-dlp" || binaryName == "gallery-dl");

    // Re-enable in case they were disabled by "Refresh All Statuses"
    installButton->setEnabled(true);
    updateButton->setEnabled(true);
    if (clearButton) {
        clearButton->setEnabled(true);
    }

    // Standardize button visibility across all binaries
    installButton->setVisible(!isInstalled);
    updateButton->setVisible(isInstalled && supportsUpdate);
    if (clearButton) {
        clearButton->setVisible(hasCustomPath);
    }

    if (foundBinary.source == "Not Found") {
        const bool isOptional = m_optionalBinaries.contains(binaryName);
        const QString requiredText = isOptional ? "This is an optional enhancement." : "This binary is required for core functionality.";

        QString statusPrefix = isOptional
            ? "⚠️ <span style='color: #d97706;'>Missing</span>"
            : "❌ <span style='color: #dc2626;'>Missing</span>";

        statusLabel->setText(QString("<b>Status:</b> %1. %2").arg(statusPrefix, requiredText));
        versionLabel->setText("Version: Unknown");
    } else if (foundBinary.source == "Invalid Custom") {
        const bool isOptional = m_optionalBinaries.contains(binaryName);
        QString statusPrefix = isOptional
            ? "⚠️ <span style='color: #d97706;'>Not Found</span>"
            : "❌ <span style='color: #dc2626;'>Not Found</span>";
        statusLabel->setText(QString("<b>Status:</b> %1 (invalid manual override)<br><b>Path:</b> %2").arg(statusPrefix, displayPath));
        versionLabel->setText("Version: Unknown");
    } else if (foundBinary.source == "Custom") {
        QString statusPrefix = "✅ <span style='color: #16a34a;'>Found</span>";
        statusLabel->setText(QString("<b>Status:</b> %1 (manual override)<br><b>Path:</b> %2").arg(statusPrefix, displayPath));
        fetchBinaryVersion(binaryName, foundBinary.path);
    } else {
        QString statusPrefix = "✅ <span style='color: #16a34a;'>Found</span>";
        statusLabel->setText(QString("<b>Status:</b> %1 (auto-detected via %2)<br><b>Path:</b> %3").arg(statusPrefix, foundBinary.source, displayPath));
        fetchBinaryVersion(binaryName, foundBinary.path);
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
        // Direct GitHub nightly download via curl
        QString curlPath = QStandardPaths::findExecutable("curl");
        if (!curlPath.isEmpty()) {
            InstallOption opt;
            opt.label = "curl (yt-dlp nightly)";
            opt.extraData["is_windows_apps_alias"] = false;

            if (isWindows()) {
                const QString localAppData = QProcessEnvironment::systemEnvironment().value("LOCALAPPDATA");
                if (!localAppData.isEmpty()) {
                    QString targetPath = localAppData + "\\LzyDownloader\\bin\\yt-dlp.exe";
                    opt.description = "Recommended on Windows. Download the latest nightly yt-dlp standalone executable directly from GitHub to your user directory.";
                    opt.program = curlPath;
                    opt.arguments = {"-L", "--create-dirs", "-o", targetPath, "https://github.com/yt-dlp/yt-dlp-nightly-builds/releases/latest/download/yt-dlp.exe"};
                    opt.extraData["set_custom_path"] = targetPath;
                    options.append(opt);
                }
            } else {
                QString targetDir = QDir::homePath() + "/.local/bin";
                QString targetPath = targetDir + "/yt-dlp";
                QString downloadUrl = "https://github.com/yt-dlp/yt-dlp-nightly-builds/releases/latest/download/yt-dlp";
                if (isMacOS()) downloadUrl = "https://github.com/yt-dlp/yt-dlp-nightly-builds/releases/latest/download/yt-dlp_macos";
                else if (isLinux()) downloadUrl = "https://github.com/yt-dlp/yt-dlp-nightly-builds/releases/latest/download/yt-dlp_linux";

                opt.description = "Download the latest nightly yt-dlp executable directly from GitHub to ~/.local/bin.";
                opt.program = curlPath;
                opt.arguments = {"-L", "--create-dirs", "-o", targetPath, downloadUrl};
                opt.extraData["set_custom_path"] = targetPath;
                options.append(opt);
            }
        }

        if (isWindows()) addOptionIfPresent("scoop", {"install", "yt-dlp-nightly"}, "Install yt-dlp (nightly) with Scoop. Requires the 'extras' bucket (`scoop bucket add extras`).");
        if (isMacOS() || isLinux()) addOptionIfPresent("brew", {"install", "yt-dlp", "--HEAD"}, "Install yt-dlp (latest from master) with Homebrew.");
    } else if (binaryName == "ffmpeg" || binaryName == "ffprobe") {
        if (isWindows()) addOptionIfPresent("winget", {"install", "--id", "Gyan.FFmpeg", "--exact", "--accept-package-agreements", "--accept-source-agreements"}, "Install FFmpeg (includes ffprobe) with WinGet.");
        if (isWindows()) addOptionIfPresent("choco", {"install", "-y", "ffmpeg"}, "Install FFmpeg (includes ffprobe) with Chocolatey.");
        if (isWindows()) addOptionIfPresent("scoop", {"install", "ffmpeg"}, "Install FFmpeg (includes ffprobe) with Scoop.");
        if (isLinux()) addOptionIfPresent("apt", {"install", "-y", "ffmpeg"}, "Install FFmpeg (includes ffprobe) with apt.");
        if (isLinux()) addOptionIfPresent("dnf", {"install", "-y", "ffmpeg"}, "Install FFmpeg (includes ffprobe) with dnf.");
        if (isLinux()) addOptionIfPresent("pacman", {"-S", "--noconfirm", "ffmpeg"}, "Install FFmpeg (includes ffprobe) with pacman.");
        if (isMacOS() || isLinux()) addOptionIfPresent("brew", {"install", "ffmpeg"}, "Install FFmpeg (includes ffprobe) with Homebrew.");
    } else if (binaryName == "gallery-dl") {
        // Direct GitHub standalone download via curl (Windows only)
        if (isWindows()) {
            QString curlPath = QStandardPaths::findExecutable("curl");
            if (!curlPath.isEmpty()) {
                const QString localAppData = QProcessEnvironment::systemEnvironment().value("LOCALAPPDATA");
                if (!localAppData.isEmpty()) {
                    QString targetPath = localAppData + "\\LzyDownloader\\bin\\gallery-dl.exe";
                    InstallOption opt;
                    opt.label = "curl (gallery-dl standalone)";
                    opt.description = "Recommended on Windows. Download the latest gallery-dl standalone executable directly from GitHub to your user directory.";
                    opt.program = curlPath;
                    opt.arguments = {"-L", "--create-dirs", "-o", targetPath, "https://github.com/mikf/gallery-dl/releases/latest/download/gallery-dl.exe"};
                    opt.extraData["is_windows_apps_alias"] = false;
                    opt.extraData["set_custom_path"] = targetPath;
                    options.append(opt);
                }
            }
        }

        if (isWindows()) addOptionIfPresent("winget", {"install", "--id", "mikf.gallery-dl", "--exact", "--accept-package-agreements", "--accept-source-agreements"}, "Install gallery-dl (stable) with WinGet.");
        if (isWindows()) addOptionIfPresent("winget", {"install", "--id", "mikf.gallery-dl.Nightly", "--exact", "--accept-package-agreements", "--accept-source-agreements"}, "Install gallery-dl (nightly) with WinGet.");
        if (isWindows()) addOptionIfPresent("choco", {"install", "-y", "gallery-dl"}, "Install gallery-dl with Chocolatey.");
        if (isWindows()) addOptionIfPresent("scoop", {"install", "gallery-dl"}, "Install gallery-dl with Scoop.");
        if (isMacOS() || isLinux()) addOptionIfPresent("brew", {"install", "gallery-dl"}, "Install gallery-dl with Homebrew.");
    } else if (binaryName == "aria2c") {
        if (isWindows()) addOptionIfPresent("winget", {"install", "--id", "aria2.aria2", "--exact", "--accept-package-agreements", "--accept-source-agreements"}, "Install aria2 with WinGet.");
        if (isWindows()) addOptionIfPresent("choco", {"install", "-y", "aria2"}, "Install aria2 with Chocolatey.");
        if (isWindows()) addOptionIfPresent("scoop", {"install", "aria2"}, "Install aria2 with Scoop.");
        if (isLinux()) addOptionIfPresent("apt", {"install", "-y", "aria2"}, "Install aria2 with apt.");
        if (isLinux()) addOptionIfPresent("dnf", {"install", "-y", "aria2"}, "Install aria2 with dnf.");
        if (isLinux()) addOptionIfPresent("pacman", {"-S", "--noconfirm", "aria2"}, "Install aria2 with pacman.");
        if (isMacOS() || isLinux()) addOptionIfPresent("brew", {"install", "aria2"}, "Install aria2 with Homebrew.");
    } else if (binaryName == "deno") {
        if (isWindows()) addOptionIfPresent("winget", {"install", "--id", "DenoLand.Deno", "--exact", "--accept-package-agreements", "--accept-source-agreements"}, "Install Deno with WinGet.");
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
