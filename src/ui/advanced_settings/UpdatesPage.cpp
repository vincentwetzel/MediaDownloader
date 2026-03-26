#include "UpdatesPage.h"
#include "core/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

UpdatesPage::UpdatesPage(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *updatesGroup = new QGroupBox("Updates", this);
    QFormLayout *updatesLayout = new QFormLayout(updatesGroup);

    m_ytDlpVersionLabel = new QLabel("yt-dlp version: Unknown", this);
    m_updateYtDlpButton = new QPushButton("Update yt-dlp", this);
    m_updateYtDlpButton->setObjectName("updateYtDlpButton"); // Make discoverable to MainWindow
    updatesLayout->addRow(m_ytDlpVersionLabel, m_updateYtDlpButton);

    m_galleryDlVersionLabel = new QLabel("gallery-dl version: Unknown", this);
    m_updateGalleryDlButton = new QPushButton("Update gallery-dl", this);
    m_updateGalleryDlButton->setObjectName("updateGalleryDlButton"); // Make discoverable to MainWindow
    updatesLayout->addRow(m_galleryDlVersionLabel, m_updateGalleryDlButton);

    layout->addWidget(updatesGroup);
    layout->addStretch();
}

void UpdatesPage::setYtDlpVersion(const QString &version) { m_ytDlpVersionLabel->setText("yt-dlp version: " + version); }
void UpdatesPage::setGalleryDlVersion(const QString &version) { m_galleryDlVersionLabel->setText("gallery-dl version: " + version); }