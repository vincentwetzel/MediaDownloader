#include "SortingRuleDialog.h"
#include "utils/YtDlpJsonParser.h" // Corrected include path
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMenu>
#include <QTextEdit>
#include <algorithm> // For std::sort
#include <QStackedWidget>
#include <QLineEdit>
#include <QDir>

// A simple widget for editing a single condition
class ConditionWidget : public QWidget {
public:
    ConditionWidget(QWidget *parent = nullptr) : QWidget(parent) {
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        QHBoxLayout *topLayout = new QHBoxLayout();

        m_fieldCombo = new QComboBox(this);
        m_fieldCombo->addItems({"Uploader", "Title", "Playlist Title", "Duration (seconds)", "Album", "ID"});
        m_fieldCombo->setToolTip("Select the metadata field to examine.");

        m_operatorCombo = new QComboBox(this);
        m_operatorCombo->setToolTip("Select the comparison operator.");
        // Items will be populated by onFieldChanged

        m_valueInputSingle = new QLineEdit(this);
        m_valueInputMulti = new QTextEdit(this);
        m_valueInputMulti->setAcceptRichText(false);
        m_valueInputMulti->setMinimumHeight(60);

        m_valueStackedWidget = new QStackedWidget(this);
        m_valueStackedWidget->addWidget(m_valueInputSingle);
        m_valueStackedWidget->addWidget(m_valueInputMulti);

        topLayout->addWidget(m_fieldCombo);
        topLayout->addWidget(m_operatorCombo);

        mainLayout->addLayout(topLayout);
        mainLayout->addWidget(m_valueStackedWidget);

        connect(m_fieldCombo, &QComboBox::currentTextChanged, this, &ConditionWidget::onFieldChanged);
        connect(m_operatorCombo, &QComboBox::currentTextChanged, this, &ConditionWidget::onOperatorChanged);

        // Initial state setup
        onFieldChanged(m_fieldCombo->currentText());
    }

    QVariantMap getCondition() const {
        QVariantMap condition;
        condition["field"] = m_fieldCombo->currentText();
        condition["operator"] = m_operatorCombo->currentText();
        if (m_operatorCombo->currentText() == "Is One Of") {
            condition["value"] = m_valueInputMulti->toPlainText();
        } else {
            condition["value"] = m_valueInputSingle->text();
        }
        return condition;
    }

    void setCondition(const QVariantMap &condition) {
        m_fieldCombo->setCurrentText(condition["field"].toString());
        onFieldChanged(condition["field"].toString()); // Set up operators

        QString op = condition["operator"].toString();
        if (op == "Equals") {
            op = "Is";
        }
        m_operatorCombo->setCurrentText(op);
        onOperatorChanged(op); // Set up correct widget

        if (op == "Is One Of") {
            m_valueInputMulti->setPlainText(condition["value"].toString());
        } else {
            m_valueInputSingle->setText(condition["value"].toString());
        }
    }

    QString getValueText() const {
        if (m_operatorCombo->currentText() == "Is One Of") {
            return m_valueInputMulti->toPlainText();
        }
        return m_valueInputSingle->text();
    }

    void setValueText(const QString &text) {
        if (m_operatorCombo->currentText() == "Is One Of") {
            m_valueInputMulti->setPlainText(text);
        } else {
            m_valueInputSingle->setText(text);
        }
    }

    QString getOperatorText() const {
        return m_operatorCombo->currentText();
    }


private slots:
    void onFieldChanged(const QString &field) {
        QString currentOperator = m_operatorCombo->currentText();
        m_operatorCombo->clear();
        if (field == "Duration (seconds)") {
            m_operatorCombo->addItems({"Is", "Greater Than", "Less Than"});
        } else {
            m_operatorCombo->addItems({"Contains", "Is", "Starts With", "Ends With", "Is One Of"});
        }
        // Try to restore the previously selected operator if it's still valid
        int index = m_operatorCombo->findText(currentOperator);
        if (index != -1) {
            m_operatorCombo->setCurrentIndex(index);
        } else {
            m_operatorCombo->setCurrentIndex(0); // Select first available operator
        }
        onOperatorChanged(m_operatorCombo->currentText());
    }

    void onOperatorChanged(const QString &op) {
        if (op == "Is One Of") {
            m_valueInputMulti->setPlaceholderText("Enter one value per line.");
            m_valueInputMulti->setToolTip("Enter one value per line. The condition will match if the field is an exact match to any of the lines.");
            m_valueStackedWidget->setCurrentWidget(m_valueInputMulti);
        } else {
            QString placeholder = "Enter value.";
            if (m_fieldCombo->currentText() == "Duration (seconds)") {
                placeholder = "Enter a number (e.g., 300 for 5 minutes).";
            }
            m_valueInputSingle->setPlaceholderText(placeholder);
            m_valueInputSingle->setToolTip("Enter the value to compare against.");

            // When switching from multi-line, copy the first line to the single-line input
            if (m_valueStackedWidget->currentWidget() == m_valueInputMulti) {
                QString firstLine = m_valueInputMulti->toPlainText().split('\n').first();
                m_valueInputSingle->setText(firstLine);
            }
            m_valueStackedWidget->setCurrentWidget(m_valueInputSingle);
        }
    }

private:
    QComboBox *m_fieldCombo;
    QComboBox *m_operatorCombo;
    QStackedWidget *m_valueStackedWidget;
    QLineEdit *m_valueInputSingle;
    QTextEdit *m_valueInputMulti;
};

SortingRuleDialog::SortingRuleDialog(QWidget *parent) : QDialog(parent) {
    setupUI();
}

SortingRuleDialog::SortingRuleDialog(const QVariantMap &rule, QWidget *parent) : QDialog(parent) {
    setupUI();
    setRule(rule);
}

void SortingRuleDialog::setupUI() {
    setWindowTitle("Sorting Rule");
    setMinimumWidth(500);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout();

    m_ruleNameInput = new QLineEdit(this);
    m_ruleNameInput->setToolTip("A descriptive name for this rule (e.g., 'Music Videos').");
    formLayout->addRow("Rule Name:", m_ruleNameInput);

    QHBoxLayout *targetFolderLayout = new QHBoxLayout();
    m_targetFolderInput = new QLineEdit(this);
    m_targetFolderInput->setToolTip("The main folder where matching files will be moved.");
    m_browseButton = new QPushButton("Browse...", this);
    m_browseButton->setToolTip("Browse to select the target folder.");
    targetFolderLayout->addWidget(m_targetFolderInput);
    targetFolderLayout->addWidget(m_browseButton);
    formLayout->addRow("Target Folder:", targetFolderLayout);

    QHBoxLayout *subfolderLayout = new QHBoxLayout();
    m_subfolderPatternInput = new QLineEdit(this);
    m_subfolderPatternInput->setToolTip("Optional. Create subfolders using media properties. Use the dropdown to insert placeholders.");

    m_tokenDropdown = new QComboBox(this);
    m_tokenDropdown->setToolTip("Insert a placeholder into the subfolder pattern.");
    m_tokenDropdown->addItem("Insert Token...");
    QStringList tokens = {"{title}", "{uploader}", "{id}", "{album}", "{upload_year}", "{upload_month}", "{upload_day}", "{playlist_title}"};
    m_tokenDropdown->addItems(tokens);

    subfolderLayout->addWidget(m_subfolderPatternInput);
    subfolderLayout->addWidget(m_tokenDropdown);
    formLayout->addRow("Subfolder Pattern:", subfolderLayout);

    m_appliesToDropdown = new QComboBox(this);
    m_appliesToDropdown->setToolTip("Choose which types of downloads this rule should apply to.");
    m_appliesToDropdown->addItems({"All Downloads", "Video Downloads", "Audio Downloads", "Gallery Downloads", "Video Playlist Downloads", "Audio Playlist Downloads"});
    formLayout->addRow("Rule Applies to:", m_appliesToDropdown);

    mainLayout->addLayout(formLayout);

    QHBoxLayout* conditionsHeaderLayout = new QHBoxLayout();
    QLabel *conditionsHeaderLabel = new QLabel("Conditions (All Must Match):");
    conditionsHeaderLabel->setToolTip("A list of conditions that must all be true for this sorting rule to trigger.");
    conditionsHeaderLayout->addWidget(conditionsHeaderLabel);
    conditionsHeaderLayout->addStretch();
    m_addConditionButton = new QPushButton("Add Condition", this);
    m_addConditionButton->setToolTip("Add a new condition to this rule. All conditions must be met for the rule to apply.");
    conditionsHeaderLayout->addWidget(m_addConditionButton);
    mainLayout->addLayout(conditionsHeaderLayout);

    m_conditionsList = new QListWidget(this);
    m_conditionsList->setToolTip("Add one or more conditions. A download must match ALL of them for this rule to apply.");
    mainLayout->addWidget(m_conditionsList);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SortingRuleDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    connect(m_browseButton, &QPushButton::clicked, this, &SortingRuleDialog::browseTargetFolder);
    connect(m_addConditionButton, &QPushButton::clicked, this, [this]() { addCondition(); });
    connect(m_tokenDropdown, &QComboBox::activated, this, [this](int index){
        if (index > 0) {
            insertToken(m_tokenDropdown->itemText(index));
            m_tokenDropdown->setCurrentIndex(0);
        }
    });
}

void SortingRuleDialog::setRule(const QVariantMap &rule) {
    m_ruleNameInput->setText(rule["name"].toString());
    m_targetFolderInput->setText(QDir::toNativeSeparators(rule["target_folder"].toString()));
    m_subfolderPatternInput->setText(rule["subfolder_pattern"].toString());

    if (rule.contains("applies_to")) {
        m_appliesToDropdown->setCurrentText(rule["applies_to"].toString());
    }

    m_conditionsList->clear();
    QVariantList conditions = rule["conditions"].toList();
    for (const QVariant &condVariant : conditions) {
        addCondition(condVariant.toMap());
    }
}

QVariantMap SortingRuleDialog::getRule() const {
    QVariantMap rule;
    rule["name"] = m_ruleNameInput->text();
    rule["target_folder"] = QDir::cleanPath(m_targetFolderInput->text());
    rule["subfolder_pattern"] = m_subfolderPatternInput->text();
    rule["applies_to"] = m_appliesToDropdown->currentText();

    QVariantList conditions;
    for (int i = 0; i < m_conditionsList->count(); ++i) {
        QListWidgetItem *item = m_conditionsList->item(i);
        QWidget* itemWidget = m_conditionsList->itemWidget(item);
        for (QObject* child : itemWidget->children()) {
            if (auto widget = dynamic_cast<ConditionWidget*>(child)) {
                conditions.append(widget->getCondition());
                break;
            }
        }
    }
    rule["conditions"] = conditions;
    return rule;
}

void SortingRuleDialog::browseTargetFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Target Folder",
                                                    m_targetFolderInput->text(),
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        m_targetFolderInput->setText(QDir::toNativeSeparators(dir));
    }
}

void SortingRuleDialog::addCondition(const QVariantMap &condition) {
    QListWidgetItem *listItem = new QListWidgetItem(m_conditionsList);

    QWidget *containerWidget = new QWidget(m_conditionsList);
    QHBoxLayout *hbox = new QHBoxLayout(containerWidget);
    hbox->setContentsMargins(2, 2, 2, 2);

    ConditionWidget *conditionWidget = new ConditionWidget(containerWidget);
    if (!condition.isEmpty()) {
        conditionWidget->setCondition(condition);
    }

    QPushButton *removeButton = new QPushButton("Remove", containerWidget);
    removeButton->setStyleSheet("color: red;");
    removeButton->setToolTip("Remove this condition.");

    hbox->addWidget(conditionWidget);
    hbox->addWidget(removeButton);

    containerWidget->setLayout(hbox);
    listItem->setSizeHint(containerWidget->sizeHint());

    m_conditionsList->addItem(listItem);
    m_conditionsList->setItemWidget(listItem, containerWidget);

    connect(removeButton, &QPushButton::clicked, this, [this, listItem]() {
        int row = m_conditionsList->row(listItem);
        m_conditionsList->takeItem(row);
        delete listItem;
    });
}

void SortingRuleDialog::insertToken(const QString &token) {
    m_subfolderPatternInput->insert(token);
}

void SortingRuleDialog::accept() {
    if (m_ruleNameInput->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Rule Name cannot be empty.");
        return;
    }
    if (m_targetFolderInput->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Target Folder cannot be empty.");
        return;
    }

    // Sort "Is One Of" values alphabetically
    for (int i = 0; i < m_conditionsList->count(); ++i) {
        QListWidgetItem *item = m_conditionsList->item(i);
        QWidget* itemWidget = m_conditionsList->itemWidget(item);
        for (QObject* child : itemWidget->children()) {
            if (auto conditionWidget = dynamic_cast<ConditionWidget*>(child)) {
                if (conditionWidget->getOperatorText() == "Is One Of") {
                    QStringList values = conditionWidget->getValueText().split('\n', Qt::SkipEmptyParts);
                    std::sort(values.begin(), values.end(), [](const QString &s1, const QString &s2) {
                        return s1.toLower() < s2.toLower(); // Using toLower() for case-insensitive comparison
                    });
                    conditionWidget->setValueText(values.join('\n'));
                }
                break;
            }
        }
    }

    QString subfolderPattern = m_subfolderPatternInput->text();
    QString error;
    if (!validateSubfolderPattern(subfolderPattern, error)) {
        QMessageBox::warning(this, "Invalid Subfolder Pattern", error);
        return;
    }

    QDialog::accept();
}

bool SortingRuleDialog::validateSubfolderPattern(const QString &pattern, QString &error) const {
    QRegularExpression re("\\{([^}]+)\\}");
    auto it = re.globalMatch(pattern);

    const QSet<QString> validTokens = {
        "id", "title", "uploader", "uploader_id", "uploader_url",
        "upload_date", "license", "creator", "alt_title", "album",
        "display_id", "description", "tags", "categories", "duration",
        "channel", "channel_id", "channel_url", "extractor", "webpage_url",
        "playlist", "playlist_title", "playlist_id", "playlist_index",
        "artist", "track", "album_artist", "release_year", "release_date",
        // Custom tokens for date parts
        "upload_year", "upload_month", "upload_day"
    };

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString token = match.captured(1);
        if (!validTokens.contains(token.toLower())) {
            error = QString("The token '{%1}' is not a valid metadata field. Please check the spelling and try again.").arg(token);
            return false;
        }
    }
    return true;
}
