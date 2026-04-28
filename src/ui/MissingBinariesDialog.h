#pragma once

#include <QDialog>
#include <QHash>
#include <QStringList>

class BinariesPage;
class ConfigManager;
class QLabel;
class QPushButton;

class MissingBinariesDialog : public QDialog {
    Q_OBJECT

public:
    explicit MissingBinariesDialog(const QStringList &binaryNames,
                                   ConfigManager *configManager,
                                   BinariesPage *binariesPage,
                                   QWidget *parent = nullptr);

    bool allBinariesResolved() const;

private:
    struct BinaryRow {
        QLabel *statusLabel = nullptr;
        QPushButton *installButton = nullptr;
        QPushButton *browseButton = nullptr;
    };

    void refreshStatuses();
    QStringList unresolvedBinaries() const;
    static QStringList normalizedBinaryList(const QStringList &binaryNames);

    QStringList m_binaryNames;
    ConfigManager *m_configManager;
    BinariesPage *m_binariesPage;
    QHash<QString, BinaryRow> m_rows;
    QPushButton *m_doneButton;
};
