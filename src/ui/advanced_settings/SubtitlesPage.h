#pragma once
#include <QWidget>
#include <QVariant>

class ConfigManager;
class ToggleSwitch;
class QComboBox;

class SubtitlesPage : public QWidget {
    Q_OBJECT
public:
    explicit SubtitlesPage(ConfigManager *configManager, QWidget *parent = nullptr);
public slots:
    void loadSettings();
private slots:
    void onSubtitleLanguageChanged(const QString &text);
    void onEmbedSubtitlesToggled(bool checked);
    void onWriteSubtitlesToggled(bool checked);
    void onIncludeAutoSubtitlesToggled(bool checked);
    void onSubtitleFormatChanged(const QString &text);
    void handleConfigSettingChanged(const QString &section, const QString &key, const QVariant &value);
private:
    void updateSubtitleFormatAvailability(bool embedSubtitlesChecked);

    ConfigManager *m_configManager;
    QComboBox *m_subtitleLanguageCombo, *m_subtitleFormatCombo;
    ToggleSwitch *m_embedSubtitlesCheck, *m_writeSubtitlesCheck, *m_includeAutoSubtitlesCheck;
};