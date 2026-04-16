#pragma once

#include <QDialog>
#include <QVariantMap>
#include <QList>

class QVBoxLayout;
class QWidget;
class QScrollArea;

class DownloadSectionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadSectionsDialog(const QVariantMap &infoDict, QWidget *parent = nullptr);
    ~DownloadSectionsDialog() override;

    QString getSectionsString() const;
    QString getFilenameLabel() const;

private slots:
    void addSectionWidget();

private:
    void setupUi();
    QWidget* createSectionWidget();
    void removeSectionWidget(QWidget* sectionWidget);

    QVBoxLayout *m_sectionsLayout;
    QVariantList m_chapters;
};
