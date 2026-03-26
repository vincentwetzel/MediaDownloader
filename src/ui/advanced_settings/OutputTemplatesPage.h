#pragma once
#include <QWidget>
#include <QVariant>

class ConfigManager;
class QLineEdit;
class QComboBox;
class QPushButton;

class OutputTemplatesPage : public QWidget {
    Q_OBJECT
public:
    explicit OutputTemplatesPage(ConfigManager *configManager, QWidget *parent = nullptr);
public slots:
    void loadSettings();
private slots:
    void validateAndSaveYtDlpTemplate();
    void validateAndSaveGalleryDlTemplate();
    void insertYtDlpTemplateToken(int index);
    void insertGalleryDlTemplateToken(int index);
    void handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value);
private:
    ConfigManager *m_configManager;
    QLineEdit *m_ytDlpOutputTemplateInput;
    QComboBox *m_ytDlpTemplateTokensCombo;
    QPushButton *m_saveYtDlpTemplateButton;
    QLineEdit *m_galleryDlOutputTemplateInput;
    QComboBox *m_galleryDlTemplateTokensCombo;
    QPushButton *m_saveGalleryDlTemplateButton;
};