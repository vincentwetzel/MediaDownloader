#include "SortingTab.h"
#include "SortingRuleDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QHeaderView>
#include <QDebug>
#include <QDir>

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
    m_rulesTable->setItem(row, 2, new QTableWidgetItem(ruleMap["applies_to"].toString()));

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
    if (size == 0) {
        return;
    }

    for (int i = 0; i < size; ++i) {
        QString key = QString("rule_%1").arg(i);
        QString jsonString = m_configManager->get("SortingRules", key).toString();
        QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());

        if (doc.isObject()) {
            int row = m_rulesTable->rowCount();
            m_rulesTable->insertRow(row);
            populateRow(row, doc.object().toVariantMap());
        } else {
            qWarning() << "Skipping invalid or empty sorting rule for key" << key;
        }
    }
}

void SortingTab::saveRules() {
    m_configManager->set("SortingRules", "size", 0);
    m_configManager->save();
    m_configManager->set("SortingRules", "size", m_rulesTable->rowCount());
    for (int i = 0; i < m_rulesTable->rowCount(); ++i) {
        QVariantMap ruleMap = m_rulesTable->item(i, 0)->data(Qt::UserRole).toMap();
        QJsonObject jsonObj = QJsonObject::fromVariantMap(ruleMap);
        QString jsonString = QJsonDocument(jsonObj).toJson(QJsonDocument::Compact);
        m_configManager->set("SortingRules", QString("rule_%1").arg(i), jsonString);
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
