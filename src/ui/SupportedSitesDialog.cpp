#include "SupportedSitesDialog.h"
#include <QHeaderView>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QLabel>

SupportedSitesDialog::SupportedSitesDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Supported Sites"));
    resize(600, 700);

    auto* layout = new QVBoxLayout(this);

    auto* descLabel = new QLabel(tr("Search for a domain to see what type of media can be downloaded:"), this);
    layout->addWidget(descLabel);

    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("Search domains (e.g. youtube.com, deviantart)..."));
    m_searchBox->setClearButtonEnabled(true);
    layout->addWidget(m_searchBox);

    m_tableWidget = new QTableWidget(0, 2, this);
    m_tableWidget->setHorizontalHeaderLabels({tr("Domain"), tr("Supported Media")});
    m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->verticalHeader()->setVisible(false);
    layout->addWidget(m_tableWidget);

    connect(m_searchBox, &QLineEdit::textChanged, this, &SupportedSitesDialog::filterSites);

    loadExtractors();
    populateTable();
}

void SupportedSitesDialog::loadExtractors() {
    auto parseFile = [this](const QString& fileName, int flag) {
        QFile file(QCoreApplication::applicationDirPath() + "/" + fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            QJsonObject root = doc.object();
            for (auto it = root.begin(); it != root.end(); ++it) {
                if (it.value().isObject()) {
                    QJsonObject extObj = it.value().toObject();
                    if (extObj.contains("domains") && extObj["domains"].isArray()) {
                        for (const QJsonValue& val : extObj["domains"].toArray()) {
                            if (val.isString()) {
                                m_domainMap[val.toString()] |= flag; // Automatically alphabetized by QMap
                            }
                        }
                    }
                }
            }
        }
    };

    parseFile("extractors_yt-dlp.json", 1);
    parseFile("extractors_gallery-dl.json", 2);
}

void SupportedSitesDialog::populateTable() {
    m_tableWidget->setUpdatesEnabled(false);
    m_tableWidget->setRowCount(m_domainMap.size());
    int row = 0;
    for (auto it = m_domainMap.begin(); it != m_domainMap.end(); ++it) {
        m_tableWidget->setItem(row, 0, new QTableWidgetItem(it.key()));
        
        QString typeStr = (it.value() == 3) ? tr("Video/Audio, Image Gallery") : 
                          (it.value() == 2) ? tr("Image Gallery") : tr("Video/Audio");
        m_tableWidget->setItem(row, 1, new QTableWidgetItem(typeStr));
        row++;
    }
    m_tableWidget->setUpdatesEnabled(true);
}

void SupportedSitesDialog::filterSites(const QString& text) {
    for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
        QTableWidgetItem* item = m_tableWidget->item(i, 0);
        if (item) {
            bool match = item->text().contains(text, Qt::CaseInsensitive);
            m_tableWidget->setRowHidden(i, !match);
        }
    }
}