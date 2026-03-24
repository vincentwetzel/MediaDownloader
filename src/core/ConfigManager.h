#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QVariant>

class ConfigManager : public QObject {
    Q_OBJECT

public:
    explicit ConfigManager(const QString &filePath, QObject *parent = nullptr);
    QVariant get(const QString &section, const QString &key, const QVariant &defaultValue = QVariant());
    bool set(const QString &section, const QString &key, const QVariant &value);
    void save();
    QString getConfigDir() const;
    void setDefaults();
    QVariant getDefault(const QString &section, const QString &key);

signals:
    void settingChanged(const QString &section, const QString &key, const QVariant &value);

private:
    void initializeDefaultSettings();

    QSettings *m_settings;
    QMap<QString, QMap<QString, QVariant>> m_defaultSettings;
};

#endif // CONFIGMANAGER_H
