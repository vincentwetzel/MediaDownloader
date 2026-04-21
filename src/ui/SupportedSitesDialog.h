#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QMap>
#include <QString>

class SupportedSitesDialog : public QDialog {
    Q_OBJECT
public:
    explicit SupportedSitesDialog(QWidget* parent = nullptr);

private slots:
    void filterSites(const QString& text);

private:
    void loadExtractors();
    void populateTable();

    QLineEdit* m_searchBox;
    QTableWidget* m_tableWidget;
    
    // Maps domain to an integer flag: 1 = Video/Audio, 2 = Gallery, 3 = Both
    QMap<QString, int> m_domainMap; 
};