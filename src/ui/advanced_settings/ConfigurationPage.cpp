#include "ConfigurationPage.h"
#include "core/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSignalBlocker>

ConfigurationPage::ConfigurationPage(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *configGroup = new QGroupBox("Configuration", this);
    configGroup->setToolTip("General application settings including download locations and theme.");
    QFormLayout *configLayout = new QFormLayout(configGroup);

    m_completedDirInput = new QLineEdit(this);
    m_completedDirInput->setReadOnly(true);
    m_completedDirInput->setToolTip("This is where your finished downloads will be saved. Click 'Browse' to change it.");
    m_browseCompletedBtn = new QPushButton("Browse...", this);
    m_browseCompletedBtn->setToolTip("Click to choose a different folder for your completed downloads.");
    QHBoxLayout *completedLayout = new QHBoxLayout();
    completedLayout->addWidget(m_completedDirInput);
    completedLayout->addWidget(m_browseCompletedBtn);
    configLayout->addRow("Output folder:", completedLayout);

    m_tempDirInput = new QLineEdit(this);
    m_tempDirInput->setReadOnly(true);
    m_tempDirInput->setToolTip("This is a temporary folder used during downloads. You usually don't need to change this.");
    m_browseTempBtn = new QPushButton("Browse...", this);
    m_browseTempBtn->setToolTip("Click to choose a different temporary folder for downloads.");
    QHBoxLayout *tempLayout = new QHBoxLayout();
    tempLayout->addWidget(m_tempDirInput);
    tempLayout->addWidget(m_browseTempBtn);
    configLayout->addRow("Temporary folder:", tempLayout);

    m_themeCombo = new QComboBox(this);
    m_themeCombo->setToolTip("Choose the visual style of the application: 'System' (matches your computer's setting), 'Light', or 'Dark'.");
    m_themeCombo->addItems({"System", "Light", "Dark"});
    configLayout->addRow("Theme:", m_themeCombo);

    layout->addWidget(configGroup);
    layout->addStretch();

    connect(m_browseCompletedBtn, &QPushButton::clicked, this, &ConfigurationPage::selectCompletedDir);
    connect(m_browseTempBtn, &QPushButton::clicked, this, &ConfigurationPage::selectTempDir);
    connect(m_themeCombo, &QComboBox::currentTextChanged, this, &ConfigurationPage::onThemeChanged);
    connect(m_configManager, &ConfigManager::settingChanged, this, &ConfigurationPage::handleConfigSettingChanged);
}

void ConfigurationPage::loadSettings() {
    QSignalBlocker b1(m_completedDirInput);
    QSignalBlocker b2(m_tempDirInput);
    QSignalBlocker b3(m_themeCombo);

    m_completedDirInput->setText(m_configManager->get("Paths", "completed_downloads_directory").toString());
    m_tempDirInput->setText(m_configManager->get("Paths", "temporary_downloads_directory").toString());
    m_themeCombo->setCurrentText(m_configManager->get("General", "theme", "System").toString());
}

void ConfigurationPage::selectCompletedDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Completed Downloads Directory", m_completedDirInput->text());
    if (!dir.isEmpty()) {
        m_configManager->set("Paths", "completed_downloads_directory", dir);
        m_configManager->save();
    }
}

void ConfigurationPage::selectTempDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Temporary Downloads Directory", m_tempDirInput->text());
    if (!dir.isEmpty()) {
        m_configManager->set("Paths", "temporary_downloads_directory", dir);
        m_configManager->save();
    }
}

void ConfigurationPage::onThemeChanged(const QString &text) {
    m_configManager->set("General", "theme", text);
    m_configManager->save();
    emit themeChanged(text);
}

void ConfigurationPage::handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value) {
    if (section == "Paths") {
        if (key == "completed_downloads_directory") m_completedDirInput->setText(value.toString());
        else if (key == "temporary_downloads_directory") m_tempDirInput->setText(value.toString());
    } else if (section == "General" && key == "theme") {
        disconnect(m_themeCombo, &QComboBox::currentTextChanged, this, &ConfigurationPage::onThemeChanged);
        m_themeCombo->setCurrentText(value.toString());
        connect(m_themeCombo, &QComboBox::currentTextChanged, this, &ConfigurationPage::onThemeChanged);
    }
}