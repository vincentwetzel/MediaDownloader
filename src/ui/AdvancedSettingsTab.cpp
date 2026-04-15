#include "AdvancedSettingsTab.h"
#include "advanced_settings/ConfigurationPage.h"
#include "advanced_settings/VideoSettingsPage.h"
#include "advanced_settings/AudioSettingsPage.h"
#include "advanced_settings/LivestreamSettingsPage.h"
#include "advanced_settings/AuthenticationPage.h"
#include "advanced_settings/OutputTemplatesPage.h"
#include "advanced_settings/DownloadOptionsPage.h"
#include "advanced_settings/MetadataPage.h"
#include "advanced_settings/SubtitlesPage.h"
#include "advanced_settings/UpdatesPage.h"
#include "advanced_settings/BinariesPage.h"
#include "core/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QAbstractItemView>
#include <QStackedWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QPalette>
#include <QApplication>
#include <QSizePolicy>
#include <QEvent>
#include <array>

AdvancedSettingsTab::AdvancedSettingsTab(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    setupUI();
    loadSettings();
}

AdvancedSettingsTab::~AdvancedSettingsTab() {}

static QString buildCategoryListStyleSheet(const QPalette &palette) {
    const QString baseColor = palette.color(QPalette::Base).name();
    const QString midColor = palette.color(QPalette::Mid).name();
    const QString textColor = palette.color(QPalette::Text).name();
    const QString highlightColor = palette.color(QPalette::Highlight).name();
    const QString highlightedTextColor = palette.color(QPalette::HighlightedText).name();

    return QString(R"(
        QListWidget {
            background: %1;
            border: none;
            border-right: 1px solid %2;
            padding-left: 6px;
            padding-right: 6px;
        }
        QListWidget::item {
            border-radius: 5px;
            padding: 6px 10px;
            margin: 2px 0;
            color: %3;
        }
        QListWidget::item:selected {
            background: %4;
            color: %5;
            font-weight: 600;
        }
    )").arg(baseColor, midColor, textColor, highlightColor, highlightedTextColor);
}

void AdvancedSettingsTab::applyCategoryListStyleSheet() {
    if (!m_categoryList) {
        return;
    }
    // Use the application's global palette because setting a stylesheet on a widget
    // can interfere with its local palette inheritance.
    m_categoryList->setStyleSheet(buildCategoryListStyleSheet(QApplication::palette()));
}

void AdvancedSettingsTab::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *contentLayout = new QHBoxLayout();

    m_categoryList = new QListWidget(this);
    m_categoryList->setFixedWidth(190);
    contentLayout->addWidget(m_categoryList);

    m_stackedWidget = new QStackedWidget(this);
    contentLayout->addWidget(m_stackedWidget);

    mainLayout->addLayout(contentLayout, 1);

    m_categoryList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_categoryList->setUniformItemSizes(true);
    m_categoryList->setSpacing(2);
    applyCategoryListStyleSheet();
    m_categoryList->setToolTip("Switch between advanced setting sections.");
    auto addPage = [this](const QString &title, QWidget *page, const QString &tooltip) {
        auto *item = new QListWidgetItem(title);
        item->setToolTip(tooltip);
        m_categoryList->addItem(item);
        m_stackedWidget->addWidget(page);
    };

    ConfigurationPage* configPage = new ConfigurationPage(m_configManager, this);
    connect(configPage, &ConfigurationPage::themeChanged, this, &AdvancedSettingsTab::onThemeChanged);

    struct PageDescriptor {
        QString title;
        QWidget *page;
        QString tooltip;
    };

    const std::array<PageDescriptor, 11> descriptors = {{
        { "Configuration", configPage, "General application settings, download locations, and theme." },
        { "Video Settings", new VideoSettingsPage(m_configManager, this), "Control video codec, resolution, and formats." },
        { "Audio Settings", new AudioSettingsPage(m_configManager, this), "Adjust audio codecs, quality, and extensions." },
        { "Livestream Settings", new LivestreamSettingsPage(m_configManager, this), "Configure livestream recording, quality, and post-download conversion." },
        { "Authentication", new AuthenticationPage(m_configManager, this), "Manage browser cookies and credential access." },
        { "Output Templates", new OutputTemplatesPage(m_configManager, this), "Define how downloaded files are named and organized." },
        { "Download Options", new DownloadOptionsPage(m_configManager, this), "Set concurrency, rate limits, and temporary directory behavior." },
        { "Metadata", new MetadataPage(m_configManager, this), "Embed metadata, artwork, and thumbnails into media." },
        { "Subtitles", new SubtitlesPage(m_configManager, this), "Subtitle languages, formats, and embedding behavior." },
        { "External Binaries", new BinariesPage(m_configManager, this), "Manage paths to external dependencies like yt-dlp and ffmpeg." },
        { "Updates", new UpdatesPage(m_configManager, this), "Check for new versions of yt-dlp, gallery-dl, and the app." }
    }};

    for (const auto &descriptor : descriptors) {
        addPage(descriptor.title, descriptor.page, descriptor.tooltip);
    }

    // Dynamically adjust size policies to prevent hidden tabs from forcing a large minimum window width
    connect(m_categoryList, &QListWidget::currentRowChanged, this, [this](int index) {
        for (int i = 0; i < m_stackedWidget->count(); ++i) {
            QWidget *page = m_stackedWidget->widget(i);
            if (i == index) {
                page->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            } else {
                page->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
            }
        }
        m_stackedWidget->setCurrentIndex(index);
    });
    m_categoryList->setCurrentRow(0);

    m_restoreDefaultsButton = new QPushButton("Restore defaults", this);
    m_restoreDefaultsButton->setToolTip("Reset all advanced settings to defaults.");
    mainLayout->addWidget(m_restoreDefaultsButton, 0, Qt::AlignRight);
    connect(m_restoreDefaultsButton, &QPushButton::clicked, this, &AdvancedSettingsTab::restoreDefaults);
}

void AdvancedSettingsTab::loadSettings() {
    for (int i = 0; i < m_stackedWidget->count(); ++i) {
        QWidget *page = m_stackedWidget->widget(i);
        // Invoke dynamically so we don't need to manually cast/know the types 
        QMetaObject::invokeMethod(page, "loadSettings");
    }
}

void AdvancedSettingsTab::restoreDefaults() {
    if (QMessageBox::question(this, "Restore Defaults",
        "Are you sure you want to restore all settings to their default values?\nThis cannot be undone.",
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_configManager->resetToDefaults();
        loadSettings();
        QMessageBox::information(this, "Defaults Restored", "Settings have been restored to defaults.");
    }
}

void AdvancedSettingsTab::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange) {
        applyCategoryListStyleSheet();
    }
    QWidget::changeEvent(event);
}

void AdvancedSettingsTab::setGalleryDlVersion(const QString &version) {
    if (auto page = m_stackedWidget->findChild<UpdatesPage*>()) {
        page->setGalleryDlVersion(version);
    }
}

void AdvancedSettingsTab::setYtDlpVersion(const QString &version) {
    if (auto page = m_stackedWidget->findChild<UpdatesPage*>()) {
        page->setYtDlpVersion(version);
    }
}

void AdvancedSettingsTab::navigateToCategory(const QString &categoryTitle) {
    QList<QListWidgetItem *> items = m_categoryList->findItems(categoryTitle, Qt::MatchExactly);
    if (!items.isEmpty()) {
        m_categoryList->setCurrentItem(items.first());
    }
}

void AdvancedSettingsTab::onThemeChanged(const QString &themeName) {
    emit themeChanged(themeName);
}
