#include "SortingTab.h"
#include "SortingRuleDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QLabel>
#include <QHeaderView>
#include <QDebug>
#include <QDir>
#include <QTableWidget>
#include <QPushButton>

SortingTab::SortingTab(ConfigManager *configManager, QWidget *parent)
    : QWidget(parent), m_configManager(configManager) {
    setupUI();
    loadRules();
    updatePriorityNumbers();
}

void SortingTab::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    QLabel *descriptionLabel = new QLabel("Organize your downloaded files automatically! Create rules to move files to specific folders based on their properties (like title, uploader, or file type).", this);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setToolTip("This section allows you to set up rules for automatically sorting your downloaded files into different folders.");
    mainLayout->addWidget(descriptionLabel);

    m_rulesTable = new QTableWidget(this);
    m_rulesTable->setToolTip("This table shows all your active sorting rules. When a download finishes, the app checks these rules from top to bottom.");
    m_rulesTable->setColumnCount(6);
    m_rulesTable->setHorizontalHeaderLabels({"#", "Name", "Applies To", "Condition", "Target Path", "Subfolder"});
    m_rulesTable->verticalHeader()->setVisible(false);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rulesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_rulesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rulesTable->setSortingEnabled(false);
    mainLayout->addWidget(m_rulesTable);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_addButton = new QPushButton("Add Rule", this);
    m_addButton->setToolTip("Click to create a new sorting rule.");
    m_editButton = new QPushButton("Edit Rule", this);
    m_editButton->setToolTip("Select a rule from the list and click here to change it.");
    m_deleteButton = new QPushButton("Delete Rule", this);
    m_deleteButton->setToolTip("Select a rule from the list and click here to delete it.");
    m_moveUpButton = new QPushButton("Move Up", this);
    m_moveUpButton->setToolTip("Move the selected rule up in priority.");
    m_moveDownButton = new QPushButton("Move Down", this);
    m_moveDownButton->setToolTip("Move the selected rule down in priority.");

    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_moveUpButton);
    buttonLayout->addWidget(m_moveDownButton);

    mainLayout->addLayout(buttonLayout);

    connect(m_addButton, &QPushButton::clicked, this, &SortingTab::addRule);
    connect(m_editButton, &QPushButton::clicked, this, &SortingTab::editRule);
    connect(m_deleteButton, &QPushButton::clicked, this, &SortingTab::deleteRule);
    connect(m_moveUpButton, &QPushButton::clicked, this, &SortingTab::moveRuleUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &SortingTab::moveRuleDown);
    connect(m_rulesTable, &QTableWidget::itemDoubleClicked, this, &SortingTab::editRule);
}

void SortingTab::populateRow(int row, const QVariantMap &ruleMap) {
    QTableWidgetItem *priorityItem = new QTableWidgetItem("");
    // Store the map directly in the user role
    priorityItem->setData(Qt::UserRole, ruleMap);
    m_rulesTable->setItem(row, 0, priorityItem);

    m_rulesTable->setItem(row, 1, new QTableWidgetItem(ruleMap["name"].toString()));

    QString appliesToUI = ruleMap["applies_to"].toString();
    if (appliesToUI == "video") appliesToUI = "Video Downloads";
    else if (appliesToUI == "audio") appliesToUI = "Audio Downloads";
    else if (appliesToUI == "gallery") appliesToUI = "Gallery Downloads";
    else if (appliesToUI == "video_playlist") appliesToUI = "Video Playlist Downloads";
    else if (appliesToUI == "audio_playlist") appliesToUI = "Audio Playlist Downloads";
    else if (appliesToUI == "any" || appliesToUI == "all") appliesToUI = "All Downloads";
    m_rulesTable->setItem(row, 2, new QTableWidgetItem(appliesToUI));

    QString conditionText = "No conditions";
    QVariantList conditions = ruleMap["conditions"].toList();
    if (!conditions.isEmpty()) {
        QVariantMap firstCondition = conditions.first().toMap();
        QString field = firstCondition["field"].toString();
        QString op = firstCondition["operator"].toString();
        QString value = firstCondition["value"].toString();

        if (op == "Is One Of") {
            int valueCount = value.split('\n', Qt::SkipEmptyParts).size();
            conditionText = QString("%1 is one of [%2 values]").arg(field).arg(valueCount);
        } else {
            conditionText = QString("%1 %2 \"%3\"").arg(field).arg(op.toLower()).arg(value);
        }

        if (conditions.size() > 1) {
            conditionText += QString(" (+%1 more)").arg(conditions.size() - 1);
        }
    }
    m_rulesTable->setItem(row, 3, new QTableWidgetItem(conditionText));
    m_rulesTable->setItem(row, 4, new QTableWidgetItem(QDir::toNativeSeparators(ruleMap["target_folder"].toString())));
    m_rulesTable->setItem(row, 5, new QTableWidgetItem(ruleMap["subfolder_pattern"].toString()));
}

void SortingTab::loadRules() {
    m_rulesTable->setRowCount(0);
    int size = m_configManager->get("SortingRules", "size", 0).toInt();

    bool rulesPurged = false;

    for (int i = 0; i < size; ++i) {
        QString key = QString("rule_%1").arg(i);
        QVariantMap ruleMap;
        
        ruleMap["name"] = m_configManager->get("SortingRules", key + "_name").toString();
        
        QString rawAppliesTo = m_configManager->get("SortingRules", key + "_applies_to").toString();
        QString appliesTo = rawAppliesTo;
        if (appliesTo.compare("Video Downloads", Qt::CaseInsensitive) == 0) appliesTo = "video";
        else if (appliesTo.compare("Audio Downloads", Qt::CaseInsensitive) == 0) appliesTo = "audio";
        else if (appliesTo.compare("Gallery Downloads", Qt::CaseInsensitive) == 0) appliesTo = "gallery";
        else if (appliesTo.compare("Video Playlist Downloads", Qt::CaseInsensitive) == 0) appliesTo = "video_playlist";
        else if (appliesTo.compare("Audio Playlist Downloads", Qt::CaseInsensitive) == 0) appliesTo = "audio_playlist";
        else if (appliesTo.compare("All Downloads", Qt::CaseInsensitive) == 0) appliesTo = "any";
        else if (appliesTo == "all") appliesTo = "any";
        
        if (rawAppliesTo != appliesTo) {
            rulesPurged = true; // Force save to upgrade legacy config rules
        }
        ruleMap["applies_to"] = appliesTo;
        
        ruleMap["target_folder"] = m_configManager->get("SortingRules", key + "_target_folder").toString();
        ruleMap["subfolder_pattern"] = m_configManager->get("SortingRules", key + "_subfolder_pattern").toString();

        int condSize = m_configManager->get("SortingRules", key + "_conditions_size", 0).toInt();
        QVariantList conditions;
        for (int j = 0; j < condSize; ++j) {
            QVariantMap cond;
            QString condKey = key + QString("_condition_%1").arg(j);
            cond["field"] = m_configManager->get("SortingRules", condKey + "_field").toString();
            cond["operator"] = m_configManager->get("SortingRules", condKey + "_operator").toString();
            cond["value"] = m_configManager->get("SortingRules", condKey + "_value").toString();
            conditions.append(cond);
        }
        ruleMap["conditions"] = conditions;

        // Discard legacy JSON formats or invalid/empty rules
        if (ruleMap["name"].toString().isEmpty() || ruleMap["target_folder"].toString().isEmpty()) {
            qWarning() << "Skipping invalid or legacy sorting rule for key" << key;
            rulesPurged = true;
            continue;
        }

        int row = m_rulesTable->rowCount();
        m_rulesTable->insertRow(row);
        populateRow(row, ruleMap);
    }

    // Check for detached garbage past 'size'
    if (!m_configManager->get("SortingRules", QString("rule_%1").arg(size)).isNull() ||
        !m_configManager->get("SortingRules", QString("rule_%1_name").arg(size)).isNull()) {
        rulesPurged = true;
    }

    if (rulesPurged) {
        qInfo() << "Purging invalid/legacy rules from settings.";
        saveRules();
    }
}

void SortingTab::saveRules() {
    int oldSize = m_configManager->get("SortingRules", "size", 0).toInt();
    int newSize = m_rulesTable->rowCount();
    
    m_configManager->set("SortingRules", "size", newSize);
    for (int i = 0; i < newSize; ++i) {
        QVariantMap ruleMap = m_rulesTable->item(i, 0)->data(Qt::UserRole).toMap();
        QString baseKey = QString("rule_%1").arg(i);
        
        // Purge old JSON string key
        m_configManager->remove("SortingRules", baseKey);
        
        // Save strictly in flat properties
        m_configManager->set("SortingRules", baseKey + "_name", ruleMap["name"]);
        m_configManager->set("SortingRules", baseKey + "_applies_to", ruleMap["applies_to"]);
        m_configManager->set("SortingRules", baseKey + "_target_folder", ruleMap["target_folder"]);
        m_configManager->set("SortingRules", baseKey + "_subfolder_pattern", ruleMap["subfolder_pattern"]);
        
        QVariantList conditions = ruleMap["conditions"].toList();
        int oldCondSize = m_configManager->get("SortingRules", baseKey + "_conditions_size", 0).toInt();
        m_configManager->set("SortingRules", baseKey + "_conditions_size", conditions.size());
        
        for (int j = 0; j < conditions.size(); ++j) {
            QVariantMap cond = conditions[j].toMap();
            QString condKey = baseKey + QString("_condition_%1").arg(j);
            m_configManager->set("SortingRules", condKey + "_field", cond["field"]);
            m_configManager->set("SortingRules", condKey + "_operator", cond["operator"]);
            m_configManager->set("SortingRules", condKey + "_value", cond["value"]);
        }
        
        // Purge leftover conditions if the rule shrunk
        for (int j = conditions.size(); j < oldCondSize; ++j) {
            QString condKey = baseKey + QString("_condition_%1").arg(j);
            m_configManager->remove("SortingRules", condKey + "_field");
            m_configManager->remove("SortingRules", condKey + "_operator");
            m_configManager->remove("SortingRules", condKey + "_value");
        }
    }
    
    // Purge leftover rules entirely, checking up to a large bound to catch detached legacy keys
    int cleanupLimit = qMax(oldSize, 100);
    for (int i = newSize; i < cleanupLimit; ++i) {
        QString baseKey = QString("rule_%1").arg(i);
        
        m_configManager->remove("SortingRules", baseKey);
        m_configManager->remove("SortingRules", baseKey + "_name");
        m_configManager->remove("SortingRules", baseKey + "_applies_to");
        m_configManager->remove("SortingRules", baseKey + "_target_folder");
        m_configManager->remove("SortingRules", baseKey + "_subfolder_pattern");
        
        int oldCondSize = m_configManager->get("SortingRules", baseKey + "_conditions_size", 0).toInt();
        m_configManager->remove("SortingRules", baseKey + "_conditions_size");
        for (int j = 0; j < qMax(oldCondSize, 20); ++j) {
            QString condKey = baseKey + QString("_condition_%1").arg(j);
            m_configManager->remove("SortingRules", condKey + "_field");
            m_configManager->remove("SortingRules", condKey + "_operator");
            m_configManager->remove("SortingRules", condKey + "_value");
        }
    }
    
    m_configManager->save();
}

void SortingTab::addRule() {
    SortingRuleDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QVariantMap rule = dialog.getRule();
        int row = m_rulesTable->rowCount();
        m_rulesTable->insertRow(row);
        populateRow(row, rule);
        updatePriorityNumbers();
        saveRules();
    }
}

void SortingTab::editRule() {
    int currentRow = m_rulesTable->currentRow();
    if (currentRow >= 0) {
        QVariantMap originalRule = m_rulesTable->item(currentRow, 0)->data(Qt::UserRole).toMap();
        SortingRuleDialog dialog(originalRule, this);

        if (dialog.exec() == QDialog::Accepted) {
            QVariantMap newRule = dialog.getRule();
            // Only update and save if the rule has actually changed.
            if (originalRule != newRule) {
                populateRow(currentRow, newRule);
                updatePriorityNumbers();
                saveRules();
                qDebug() << "Sorting rule changed, saving to disk.";
            } else {
                qDebug() << "Sorting rule unchanged, skipping save.";
            }
        }
    } else {
        QMessageBox::warning(this, "Edit Rule", "Please select a rule to edit.");
    }
}

void SortingTab::deleteRule() {
    int currentRow = m_rulesTable->currentRow();
    if (currentRow >= 0) {
        if (QMessageBox::question(this, "Delete Rule", "Are you sure you want to remove this rule?", QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes) {
            m_rulesTable->removeRow(currentRow);
            updatePriorityNumbers();
            saveRules();
        }
    } else {
        QMessageBox::warning(this, "Delete Rule", "Please select a rule to remove.");
    }
}

void SortingTab::moveRuleUp() {
    int currentRow = m_rulesTable->currentRow();
    if (currentRow > 0) {
        QVariantMap currentRule = m_rulesTable->item(currentRow, 0)->data(Qt::UserRole).toMap();
        QVariantMap aboveRule = m_rulesTable->item(currentRow - 1, 0)->data(Qt::UserRole).toMap();

        populateRow(currentRow, aboveRule);
        populateRow(currentRow - 1, currentRule);

        updatePriorityNumbers();
        m_rulesTable->setCurrentCell(currentRow - 1, 0);
        saveRules();
    } else if (currentRow == -1) {
        QMessageBox::warning(this, "Move Rule", "Please select a rule to move.");
    }
}

void SortingTab::moveRuleDown() {
    int currentRow = m_rulesTable->currentRow();
    if (currentRow >= 0 && currentRow < m_rulesTable->rowCount() - 1) {
        QVariantMap currentRule = m_rulesTable->item(currentRow, 0)->data(Qt::UserRole).toMap();
        QVariantMap belowRule = m_rulesTable->item(currentRow + 1, 0)->data(Qt::UserRole).toMap();

        populateRow(currentRow, belowRule);
        populateRow(currentRow + 1, currentRule);

        updatePriorityNumbers();
        m_rulesTable->setCurrentCell(currentRow + 1, 0);
        saveRules();
    } else if (currentRow == -1) {
        QMessageBox::warning(this, "Move Rule", "Please select a rule to move.");
    }
}

void SortingTab::updatePriorityNumbers() {
    for (int i = 0; i < m_rulesTable->rowCount(); ++i) {
        QTableWidgetItem *item = m_rulesTable->item(i, 0);
        if (item) {
            item->setText(QString::number(i + 1));
        } else {
            m_rulesTable->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        }
    }
}
