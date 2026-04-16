#include "DownloadSectionsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QComboBox>
#include <QStackedWidget>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QRegularExpression>

DownloadSectionsDialog::DownloadSectionsDialog(const QVariantMap &infoDict, QWidget *parent)
    : QDialog(parent)
{
    if (infoDict.contains("chapters") && infoDict["chapters"].typeId() == QMetaType::QVariantList) {
        m_chapters = infoDict["chapters"].toList();
    }
    setupUi();
    addSectionWidget(); // Start with one section by default
}

DownloadSectionsDialog::~DownloadSectionsDialog() = default;

void DownloadSectionsDialog::setupUi()
{
    setWindowTitle("Download Sections");
    setMinimumSize(600, 400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *descriptionLabel = new QLabel(
        "Define one or more sections to download. Each section can be a time range or a chapter.\n"
        "yt-dlp will download only these parts of the video. For time ranges, you can leave a field blank "
        "to download from the beginning or to the very end.", this);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setToolTip("Use the 'Add Section' button to define multiple parts to download.");
    mainLayout->addWidget(descriptionLabel);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::StyledPanel);

    QWidget *scrollWidget = new QWidget();
    m_sectionsLayout = new QVBoxLayout(scrollWidget);
    m_sectionsLayout->setSpacing(10);
    m_sectionsLayout->addStretch();

    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *addSectionButton = new QPushButton("Add Section", this);
    addSectionButton->setToolTip("Add another time range or chapter to download.");

    QPushButton *okButton = new QPushButton("OK", this);
    okButton->setDefault(true);
    QPushButton *cancelButton = new QPushButton("Cancel", this);

    buttonLayout->addWidget(addSectionButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(buttonLayout);

    connect(addSectionButton, &QPushButton::clicked, this, &DownloadSectionsDialog::addSectionWidget);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void DownloadSectionsDialog::addSectionWidget()
{
    QWidget *sectionWidget = createSectionWidget();
    m_sectionsLayout->insertWidget(m_sectionsLayout->count() - 1, sectionWidget);
}

void DownloadSectionsDialog::removeSectionWidget(QWidget *sectionWidget)
{
    m_sectionsLayout->removeWidget(sectionWidget);
    sectionWidget->deleteLater();
}

QWidget* DownloadSectionsDialog::createSectionWidget()
{
    QFrame *frame = new QFrame(this);
    frame->setFrameShape(QFrame::StyledPanel);

    QHBoxLayout *layout = new QHBoxLayout(frame);

    QComboBox *typeCombo = new QComboBox(frame);
    typeCombo->setObjectName("typeCombo");
    typeCombo->setToolTip("Choose whether to define the section by time or by chapter name.");
    typeCombo->addItem("Time Range");
    typeCombo->addItem("Chapter");
    typeCombo->setFixedWidth(120);
    if (m_chapters.isEmpty()) {
        if (auto *model = qobject_cast<QStandardItemModel*>(typeCombo->model())) {
            model->item(1)->setEnabled(false);
        }
        typeCombo->setToolTip("Choose whether to define the section by time. Chapters not available for this video.");
    }

    QStackedWidget *stack = new QStackedWidget(frame);
    stack->setObjectName("stack");

    // Time Range Widget
    QWidget *timeWidget = new QWidget(frame);
    QHBoxLayout *timeLayout = new QHBoxLayout(timeWidget);
    timeLayout->setContentsMargins(0, 0, 0, 0);
    QLineEdit *startEdit = new QLineEdit(timeWidget);
    startEdit->setObjectName("startEdit");
    startEdit->setPlaceholderText("HH:MM:SS");
    startEdit->setToolTip("Enter the start time (HH:MM:SS). Leave blank to start from the beginning.");
    QLabel *toLabel = new QLabel(" to ", timeWidget);
    QLineEdit *endEdit = new QLineEdit(timeWidget);
    endEdit->setObjectName("endEdit");
    endEdit->setPlaceholderText("HH:MM:SS");
    endEdit->setToolTip("Enter the end time (HH:MM:SS). Leave blank to download to the end.");
    timeLayout->addWidget(new QLabel("From:", timeWidget));
    timeLayout->addWidget(startEdit);
    timeLayout->addWidget(toLabel);
    timeLayout->addWidget(endEdit);
    timeLayout->addStretch();
    stack->addWidget(timeWidget);

    // Chapter Widget
    QWidget *chapterWidget = new QWidget(frame);
    QHBoxLayout *chapterLayout = new QHBoxLayout(chapterWidget);
    chapterLayout->setContentsMargins(0, 0, 0, 0);
    QComboBox *chapterCombo = new QComboBox(chapterWidget);
    chapterCombo->setObjectName("chapterCombo");
    chapterCombo->setToolTip("Select a chapter to download.");
    for (const QVariant &chapter : m_chapters) {
        QVariantMap chapterMap = chapter.toMap();
        if (chapterMap.contains("title")) {
            chapterCombo->addItem(chapterMap["title"].toString());
        }
    }
    chapterLayout->addWidget(new QLabel("Chapter:", chapterWidget));
    chapterLayout->addWidget(chapterCombo);
    chapterLayout->addStretch();
    stack->addWidget(chapterWidget);

    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), stack, &QStackedWidget::setCurrentIndex);

    QPushButton *removeButton = new QPushButton("Remove", frame);
    removeButton->setToolTip("Remove this section from the download.");
    connect(removeButton, &QPushButton::clicked, this, [this, frame] {
        removeSectionWidget(frame);
    });

    layout->addWidget(typeCombo);
    layout->addWidget(stack);
    layout->addWidget(removeButton);

    return frame;
}

QString DownloadSectionsDialog::getFilenameLabel() const
{
    auto sanitizeLabelPart = [](QString value) {
        value = value.trimmed();
        if (value.isEmpty()) {
            return QString();
        }

        value.replace(':', '-');
        value.replace('/', '-');
        value.replace('\\', '-');
        value.replace(' ', '_');
        value.remove(QRegularExpression(R"([<>:"/\\|?*])"));
        value.replace(QRegularExpression(R"(_{2,})"), "_");
        value.replace(QRegularExpression(R"(-{2,})"), "-");
        return value.left(40);
    };

    QStringList labels;
    for (int i = 0; i < m_sectionsLayout->count(); ++i) {
        QWidget *widget = m_sectionsLayout->itemAt(i)->widget();
        if (!widget) continue;

        QComboBox *typeCombo = widget->findChild<QComboBox*>("typeCombo");
        QStackedWidget *stack = widget->findChild<QStackedWidget*>("stack");
        if (!typeCombo || !stack) continue;

        if (typeCombo->currentText() == "Time Range") {
            QLineEdit *startEdit = stack->widget(0)->findChild<QLineEdit*>("startEdit");
            QLineEdit *endEdit = stack->widget(0)->findChild<QLineEdit*>("endEdit");
            if (!startEdit || !endEdit) continue;

            QString startTime = sanitizeLabelPart(startEdit->text());
            QString endTime = sanitizeLabelPart(endEdit->text());
            if (startTime.isEmpty() && endTime.isEmpty()) {
                continue;
            }

            if (startTime.isEmpty()) startTime = "start";
            if (endTime.isEmpty()) endTime = "end";
            labels.append(QString("%1_to_%2").arg(startTime, endTime));
        } else if (typeCombo->currentText() == "Chapter") {
            QComboBox *chapterCombo = stack->widget(1)->findChild<QComboBox*>("chapterCombo");
            if (!chapterCombo || chapterCombo->count() == 0) continue;

            QString chapterName = sanitizeLabelPart(chapterCombo->currentText());
            if (!chapterName.isEmpty()) {
                labels.append(QString("chapter_%1").arg(chapterName));
            }
        }
    }

    if (labels.isEmpty()) {
        return QString();
    }

    if (labels.size() == 1) {
        return labels.first();
    }

    QString label = labels.mid(0, 3).join("__");
    if (labels.size() > 3) {
        label += QString("__plus_%1_more").arg(labels.size() - 3);
    }
    return label.left(90);
}
QString DownloadSectionsDialog::getSectionsString() const
{
    QStringList sectionStrings;
    for (int i = 0; i < m_sectionsLayout->count(); ++i) {
        QWidget *widget = m_sectionsLayout->itemAt(i)->widget();
        if (!widget) continue;

        QComboBox *typeCombo = widget->findChild<QComboBox*>("typeCombo");
        QStackedWidget *stack = widget->findChild<QStackedWidget*>("stack");
        if (!typeCombo || !stack) continue;

        if (typeCombo->currentText() == "Time Range") {
            QLineEdit *startEdit = stack->widget(0)->findChild<QLineEdit*>("startEdit");
            QLineEdit *endEdit = stack->widget(0)->findChild<QLineEdit*>("endEdit");
            if (startEdit && endEdit) {
                QString startTime = startEdit->text().trimmed();
                QString endTime = endEdit->text().trimmed();

                // Don't add empty sections
                if (startTime.isEmpty() && endTime.isEmpty()) {
                    continue;
                }
                sectionStrings.append(QString("*%1-%2").arg(startTime, endTime)); // yt-dlp handles empty start/end correctly
            }
        } else if (typeCombo->currentText() == "Chapter") {
            QComboBox *chapterCombo = stack->widget(1)->findChild<QComboBox*>("chapterCombo");
            if (chapterCombo && chapterCombo->count() > 0) {
                QString chapterName = chapterCombo->currentText();
                // yt-dlp is sensitive to some characters, but we'll let it handle it.
                // We just need to make sure it's not empty.
                if (!chapterName.isEmpty()) {
                    // Chapter names can contain regex, so we might need to escape them.
                    // For now, we'll pass it directly. The user can use regex if they know how.
                    // A simple approach is to just use the name.
                    // For more complex names, yt-dlp recommends `*re:^Chapter Title$`
                    // For now, we'll just use the chapter name.
                    sectionStrings.append(QString("*%1").arg(chapterName));
                }
            }
        }
    }

    return sectionStrings.join('+');
}
