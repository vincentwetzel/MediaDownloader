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
    m_defaultSettings["Video"]["video_quality"] = "best";
    m_defaultSettings["Video"]["video_codec"] = "H.264";
    m_defaultSettings["Video"]["video_extension"] = "mp4";
    m_defaultSettings["Video"]["video_audio_codec"] = "AAC";
    m_defaultSettings["Video"]["video_multistreams"] = "Default Stream";
    m_defaultSettings["Audio"]["audio_quality"] = "best";
    m_defaultSettings["Audio"]["audio_codec"] = "Opus";
    m_defaultSettings["Audio"]["audio_extension"] = "opus";
    m_defaultSettings["Audio"]["audio_multistreams"] = "Default Stream";
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
    QString fullKey = section + "/" + key;
    if (m_settings->contains(fullKey) && m_settings->value(fullKey) == value) {
        return true;
    }
    m_settings->setValue(fullKey, value);
    emit settingChanged(section, key, value);
    return true;
}

void ConfigManager::remove(const QString &section, const QString &key) {
    QString fullKey = section + "/" + key;
    if (m_settings->contains(fullKey)) {
        m_settings->remove(fullKey);
        emit settingChanged(section, key, QVariant());
    }
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
    // --- 1. Define what to preserve ---
    const QList<QPair<QString, QString>> keysToPreserve = {
        {"Paths", "completed_downloads_directory"},
        {"Paths", "temporary_downloads_directory"},
        {"General", "theme"},
        {"General", "output_template"},
        {"General", "output_template_video"},
        {"General", "output_template_audio"},
        {"General", "gallery_output_template"},
        {"Binaries", "yt-dlp_path"},
        {"Binaries", "ffmpeg_path"},
        {"Binaries", "ffprobe_path"},
        {"Binaries", "gallery-dl_path"},
        {"Binaries", "aria2c_path"},
        {"Binaries", "deno_path"}
    };
    const QStringList groupsToPreserve = {"SortingRules", "MainWindow", "UI", "Geometry"};

    // --- 2. Preserve individual keys ---
    QMap<QString, QVariant> preservedValues;
    for (const auto& keyPair : keysToPreserve) {
        QVariant value = get(keyPair.first, keyPair.second);
        if (!value.isNull() && !value.toString().isEmpty()) {
            preservedValues.insert(keyPair.first + "/" + keyPair.second, value);
        }
    }

    // --- 3. Preserve whole groups ---
    QMap<QString, QVariantMap> preservedGroups;
    for (const QString& groupName : groupsToPreserve) {
        QVariantMap groupValues;
        m_settings->beginGroup(groupName);
        for (const QString &key : m_settings->childKeys()) {
            groupValues[key] = m_settings->value(key);
        }
        m_settings->endGroup();
        if (!groupValues.isEmpty()) {
            preservedGroups.insert(groupName, groupValues);
        }
    }

    // --- 4. Clear and apply defaults ---
    m_settings->clear();
    m_settings->sync();

    bool oldState = blockSignals(true);
    setDefaults();

    // --- 5. Restore preserved values ---
    for (auto it = preservedValues.constBegin(); it != preservedValues.constEnd(); ++it) {
        m_settings->setValue(it.key(), it.value());
    }

    for (auto it = preservedGroups.constBegin(); it != preservedGroups.constEnd(); ++it) {
        m_settings->beginGroup(it.key());
        const QVariantMap& groupValues = it.value();
        for (auto it2 = groupValues.constBegin(); it2 != groupValues.constEnd(); ++it2) {
            m_settings->setValue(it2.key(), it2.value());
        }
        m_settings->endGroup();
    }

    blockSignals(oldState);

    save();
    emit settingsReset();
}
