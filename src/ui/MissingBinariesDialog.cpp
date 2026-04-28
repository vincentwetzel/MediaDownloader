#include "MissingBinariesDialog.h"

#include "advanced_settings/BinariesPage.h"
#include "core/ConfigManager.h"
#include "core/ProcessUtils.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

MissingBinariesDialog::MissingBinariesDialog(const QStringList &binaryNames,
                                             ConfigManager *configManager,
                                             BinariesPage *binariesPage,
                                             QWidget *parent)
    : QDialog(parent),
      m_binaryNames(normalizedBinaryList(binaryNames)),
      m_configManager(configManager),
      m_binariesPage(binariesPage),
      m_doneButton(nullptr)
{
    setWindowTitle("Set Up Required Tools");
    setMinimumWidth(640);
    setModal(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *introLabel = new QLabel(
        "LzyDownloader needs a few external tools before downloads can run. "
        "Install a missing tool or browse to an existing executable, then refresh the checklist.",
        this);
    introLabel->setWordWrap(true);
    introLabel->setToolTip("Explains why this setup dialog is being shown.");
    mainLayout->addWidget(introLabel);

    QGroupBox *toolsGroup = new QGroupBox("Missing tools", this);
    toolsGroup->setToolTip("Tools that were not found or have invalid configured paths.");
    QGridLayout *grid = new QGridLayout(toolsGroup);
    grid->setColumnStretch(1, 1);

    QLabel *toolHeader = new QLabel("Tool", toolsGroup);
    toolHeader->setToolTip("Name of the required external executable.");
    QLabel *statusHeader = new QLabel("Status", toolsGroup);
    statusHeader->setToolTip("Whether the executable was found after the latest scan.");
    QLabel *actionsHeader = new QLabel("Actions", toolsGroup);
    actionsHeader->setToolTip("Ways to install the tool or point LzyDownloader to an existing copy.");
    grid->addWidget(toolHeader, 0, 0);
    grid->addWidget(statusHeader, 0, 1);
    grid->addWidget(actionsHeader, 0, 2);

    int row = 1;
    for (const QString &binaryName : m_binaryNames) {
        QLabel *nameLabel = new QLabel(binaryName, toolsGroup);
        nameLabel->setToolTip(QString("External executable: %1").arg(binaryName));

        QLabel *statusLabel = new QLabel("Checking...", toolsGroup);
        statusLabel->setWordWrap(true);
        statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        statusLabel->setToolTip(QString("Current detection status for %1.").arg(binaryName));

        QPushButton *installButton = new QPushButton("Install...", toolsGroup);
        installButton->setToolTip(QString("Open installer options for %1.").arg(binaryName));
        installButton->setEnabled(m_binariesPage != nullptr);

        QPushButton *browseButton = new QPushButton("Browse...", toolsGroup);
        browseButton->setToolTip(QString("Select an existing %1 executable from disk.").arg(binaryName));
        browseButton->setEnabled(m_binariesPage != nullptr);

        QHBoxLayout *actionsLayout = new QHBoxLayout();
        actionsLayout->setContentsMargins(0, 0, 0, 0);
        actionsLayout->addWidget(installButton);
        actionsLayout->addWidget(browseButton);

        QWidget *actionsWidget = new QWidget(toolsGroup);
        actionsWidget->setLayout(actionsLayout);

        grid->addWidget(nameLabel, row, 0);
        grid->addWidget(statusLabel, row, 1);
        grid->addWidget(actionsWidget, row, 2);

        m_rows.insert(binaryName, {statusLabel, installButton, browseButton});

        connect(installButton, &QPushButton::clicked, this, [this, binaryName]() {
            if (m_binariesPage) {
                m_binariesPage->installBinaryFor(binaryName);
                refreshStatuses();
            }
        });
        connect(browseButton, &QPushButton::clicked, this, [this, binaryName]() {
            if (m_binariesPage) {
                m_binariesPage->browseBinaryFor(binaryName);
                refreshStatuses();
            }
        });

        ++row;
    }

    mainLayout->addWidget(toolsGroup);

    QHBoxLayout *utilityLayout = new QHBoxLayout();
    utilityLayout->addStretch();

    QPushButton *refreshButton = new QPushButton("Refresh", this);
    refreshButton->setToolTip("Re-scan configured paths and auto-detected binaries.");
    utilityLayout->addWidget(refreshButton);
    mainLayout->addLayout(utilityLayout);
    connect(refreshButton, &QPushButton::clicked, this, &MissingBinariesDialog::refreshStatuses);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    m_doneButton = buttonBox->addButton("Done", QDialogButtonBox::AcceptRole);
    QPushButton *laterButton = buttonBox->addButton("Later", QDialogButtonBox::RejectRole);
    m_doneButton->setToolTip("Continue once all listed tools are available.");
    laterButton->setToolTip("Close setup for now.");
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refreshStatuses();
}

bool MissingBinariesDialog::allBinariesResolved() const
{
    return unresolvedBinaries().isEmpty();
}

void MissingBinariesDialog::refreshStatuses()
{
    ProcessUtils::clearCache();

    for (const QString &binaryName : m_binaryNames) {
        BinaryRow row = m_rows.value(binaryName);
        if (!row.statusLabel) {
            continue;
        }

        const ProcessUtils::FoundBinary foundBinary = ProcessUtils::resolveBinary(binaryName, m_configManager);
        const bool resolved = foundBinary.source != "Not Found" && foundBinary.source != "Invalid Custom";

        if (resolved) {
            row.statusLabel->setText(QString("Found via %1\n%2")
                                         .arg(foundBinary.source, QFileInfo(foundBinary.path).absoluteFilePath()));
            if (row.installButton) {
                row.installButton->setVisible(false);
            }
        } else if (foundBinary.source == "Invalid Custom") {
            row.statusLabel->setText(QString("Invalid manual path\n%1").arg(foundBinary.path));
            if (row.installButton) {
                row.installButton->setVisible(true);
            }
        } else {
            row.statusLabel->setText("Missing");
            if (row.installButton) {
                row.installButton->setVisible(true);
            }
        }

        if (m_binariesPage) {
            m_binariesPage->refreshBinaryStatus(binaryName);
        }
    }

    if (m_doneButton) {
        const bool resolved = allBinariesResolved();
        m_doneButton->setEnabled(resolved);
        m_doneButton->setText(resolved ? "Done" : "Resolve Missing Tools");
    }
}

QStringList MissingBinariesDialog::unresolvedBinaries() const
{
    QStringList unresolved;
    for (const QString &binaryName : m_binaryNames) {
        const ProcessUtils::FoundBinary foundBinary = ProcessUtils::resolveBinary(binaryName, m_configManager);
        if (foundBinary.source == "Not Found" || foundBinary.source == "Invalid Custom") {
            unresolved << binaryName;
        }
    }
    return unresolved;
}

QStringList MissingBinariesDialog::normalizedBinaryList(const QStringList &binaryNames)
{
    QStringList normalized;
    QSet<QString> seen;
    for (const QString &binaryName : binaryNames) {
        const QString trimmed = binaryName.trimmed();
        if (!trimmed.isEmpty() && !seen.contains(trimmed)) {
            normalized << trimmed;
            seen.insert(trimmed);
        }
    }
    return normalized;
}
