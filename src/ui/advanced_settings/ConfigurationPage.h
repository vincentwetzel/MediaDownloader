#pragma once
#include <QWidget>
#include <QVariant>

class ConfigManager;
class QLineEdit;
class QPushButton;
class QComboBox;
class QCheckBox;

class ConfigurationPage : public QWidget {
    Q_OBJECT
public:
    explicit ConfigurationPage(ConfigManager *configManager, QWidget *parent = nullptr);

public slots:
    void loadSettings();

signals:
    void themeChanged(const QString &text);

private slots:
    void selectCompletedDir();
    void selectTempDir();
    void onThemeChanged(const QString &text);
    void handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value);
    void onEnableApiServerToggled(int state);

private:
    ConfigManager *m_configManager;
    QLineEdit *m_completedDirInput;
    QPushButton *m_browseCompletedBtn;
    QLineEdit *m_tempDirInput;
    QPushButton *m_browseTempBtn;
    QComboBox *m_themeCombo;
    QCheckBox *m_enableApiServerCheck;
};