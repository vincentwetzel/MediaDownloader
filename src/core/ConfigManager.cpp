#include "ConfigManager.h"
#include <QDir>
#include <QCoreApplication>

ConfigManager::ConfigManager(const QString &filePath, QObject *parent)
    : QObject(parent) {
    QString configPath = QDir(QCoreApplication::applicationDirPath()).filePath(filePath);
    m_settings = new QSettings(configPath, QSettings::IniFormat, this);
    initializeDefaultSettings();
}

void ConfigManager::initializeDefaultSettings() {
    m_defaultSettings["General"]["output_template"] = "%(title)s [%(uploader)s][%(upload_date>%Y-%m-%d)s][%(id)s].%(ext)s";
    m_defaultSettings["General"]["gallery_output_template"] = "{author[name]}/{id}_{filename}.{extension}";
    m_defaultSettings["General"]["theme"] = "System";
    m_defaultSettings["General"]["cookies_from_browser"] = "None";
    m_defaultSettings["General"]["gallery_cookies_from_browser"] = "None";
    m_defaultSettings["General"]["sponsorblock"] = false;
    m_defaultSettings["General"]["auto_paste_mode"] = 0; // Changed from auto_paste_on_focus (bool) to auto_paste_mode (int)
    m_defaultSettings["General"]["single_line_preview"] = false;
    m_defaultSettings["General"]["restrict_filenames"] = false;
    m_defaultSettings["Video"]["video_quality"] = "1080p";
    m_defaultSettings["Video"]["video_codec"] = "Default";
    m_defaultSettings["Video"]["video_extension"] = "mp4";
    m_defaultSettings["Video"]["video_audio_codec"] = "AAC";
    m_defaultSettings["Audio"]["audio_quality"] = "best";
    m_defaultSettings["Audio"]["audio_codec"] = "Default";
    m_defaultSettings["Audio"]["audio_extension"] = "mp3";
    m_defaultSettings["Metadata"]["use_aria2c"] = true;
    m_defaultSettings["Metadata"]["embed_chapters"] = true;
    m_defaultSettings["Metadata"]["embed_metadata"] = true;
    m_defaultSettings["Metadata"]["embed_thumbnail"] = true;
    m_defaultSettings["Metadata"]["high_quality_thumbnail"] = true;
    m_defaultSettings["Metadata"]["convert_thumbnail_to"] = "jpg";
    m_defaultSettings["Subtitles"]["languages"] = "en";
    m_defaultSettings["Subtitles"]["embed_subtitles"] = true;
    m_defaultSettings["Subtitles"]["write_subtitles"] = false;
    m_defaultSettings["Subtitles"]["write_auto_subtitles"] = true;
    m_defaultSettings["Subtitles"]["format"] = "srt";
}

QVariant ConfigManager::get(const QString &section, const QString &key, const QVariant &defaultValue) {
    return m_settings->value(section + "/" + key, defaultValue);
}

bool ConfigManager::set(const QString &section, const QString &key, const QVariant &value) {
    m_settings->setValue(section + "/" + key, value);
    emit settingChanged(section, key, value);
    return true;
}

void ConfigManager::save() {
    m_settings->sync();
}

QString ConfigManager::getConfigDir() const {
    return QFileInfo(m_settings->fileName()).absolutePath();
}

void ConfigManager::setDefaults() {
    for (auto it = m_defaultSettings.constBegin(); it != m_defaultSettings.constEnd(); ++it) {
        for (auto it2 = it.value().constBegin(); it2 != it.value().constEnd(); ++it2) {
            set(it.key(), it2.key(), it2.value());
        }
    }
}

QVariant ConfigManager::getDefault(const QString &section, const QString &key) {
    return m_defaultSettings.value(section, {}).value(key);
}

void ConfigManager::resetToDefaults() {
    m_settings->clear();
    m_settings->sync();
    setDefaults();
    emit settingsReset();
}
