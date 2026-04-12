#pragma once

#include <QMap>
#include <QSet>
#include <QWidget>
#include <QVariant>

class ConfigManager;
class QLabel;
class QPushButton;
class QVBoxLayout;

class BinariesPage : public QWidget {
    Q_OBJECT

public:
    explicit BinariesPage(ConfigManager *configManager, QWidget *parent = nullptr);

public slots:
    void loadSettings();

private slots:
    void handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value);

private:
    struct InstallOption {
        QString label;
        QString description;
        QString program;
        QStringList arguments;
        QVariantMap extraData; // Flags for launch behaviour (e.g. is_windows_apps_alias)
    };

    void setupRow(QVBoxLayout *layout,
                  const QString &binaryName,
                  const QString &labelText,
                  const QString &configKey,
                  const QString &manualUrl,
                  bool optional = false);
    QString browseBinary(const QString &title) const;
    void browseBinaryFor(const QString &binaryName);
    void installBinaryFor(const QString &binaryName);
    void saveBinaryOverride(const QString &binaryName, const QString &path);
    void refreshBinaryStatus(const QString &binaryName);
    QString resolvedPathForBinary(const QString &binaryName) const;
    QList<InstallOption> buildInstallOptions(const QString &binaryName) const;
    QString commandPreview(const InstallOption &option) const;
    QString displayName(const QString &binaryName) const;

    ConfigManager *m_configManager;
    QMap<QString, QString> m_configKeys;
    QMap<QString, QString> m_manualUrls;
    QMap<QString, QString> m_displayNames;
    QSet<QString> m_optionalBinaries;
    QMap<QString, QLabel *> m_statusLabels;
    QMap<QString, QPushButton *> m_installButtons;
};
