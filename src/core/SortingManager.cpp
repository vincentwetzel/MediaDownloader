#include "SortingManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDate>

SortingManager::SortingManager(ConfigManager *configManager, QObject *parent)
    : QObject(parent), m_configManager(configManager) {
}

QString SortingManager::getSortedDirectory(const QVariantMap &videoMetadata, const QVariantMap &downloadOptions) {
    int size = m_configManager->get("SortingRules", "size", 0).toInt();
    if (size == 0) {
        // No rules to process, return default directory
        QString baseDir = m_configManager->get("Paths", "completed_downloads_directory").toString();
        if (baseDir.isEmpty()) {
            baseDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        }
        return baseDir;
    }

    // Determine which key format to use ("rule_N" or "N")
    bool useNewFormat = m_configManager->get("SortingRules", "rule_0").isValid();

    for (int i = 0; i < size; ++i) {
        QString key = useNewFormat ? QString("rule_%1").arg(i) : QString::number(i);
        QString jsonString = m_configManager->get("SortingRules", key).toString();
        if (jsonString.isEmpty()) {
            continue; // Skip if the rule is missing for some reason
        }

        QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
        QJsonObject rule = doc.object();

        // 1. Check if the rule applies to this download type
        QString appliesTo = rule["applies_to"].toString("All Downloads");
        QString downloadType = downloadOptions.value("type", "video").toString();
        bool isPlaylist = videoMetadata.contains("playlist_index") && videoMetadata["playlist_index"].toInt() != -1;

        bool typeMatch = false;
        if (appliesTo == "All Downloads") {
            typeMatch = true;
        } else if (appliesTo == "Video Downloads" && downloadType == "video" && !isPlaylist) {
            typeMatch = true;
        } else if (appliesTo == "Audio Downloads" && downloadType == "audio" && !isPlaylist) {
            typeMatch = true;
        } else if (appliesTo == "Gallery Downloads" && downloadType == "gallery") {
            typeMatch = true;
        } else if (appliesTo == "Video Playlist Downloads" && downloadType == "video" && isPlaylist) {
            typeMatch = true;
        } else if (appliesTo == "Audio Playlist Downloads" && downloadType == "audio" && isPlaylist) {
            typeMatch = true;
        }

        if (!typeMatch) {
            continue; // Skip to the next rule
        }

        // 2. Check if all conditions match
        QJsonArray conditions = rule["conditions"].toArray();
        bool allConditionsMatch = true;
        for (const QJsonValue &condValue : conditions) {
            QJsonObject condition = condValue.toObject();
            QString field = condition["field"].toString();
            QString op = condition["operator"].toString();
            QString value = condition["value"].toString();

            QVariant metadataValue;
            if (field == "Duration (seconds)") {
                metadataValue = videoMetadata.value("duration");
            } else {
                metadataValue = videoMetadata.value(field.toLower());
            }

            bool match = false;
            if (field == "Duration (seconds)") {
                bool ok;
                int durationValue = metadataValue.toInt();
                int conditionValue = value.toInt(&ok);
                if (ok) {
                    if (op == "Is") {
                        match = (durationValue == conditionValue);
                    } else if (op == "Greater Than") {
                        match = (durationValue > conditionValue);
                    } else if (op == "Less Than") {
                        match = (durationValue < conditionValue);
                    }
                }
            } else {
                if (op == "Contains") {
                    match = metadataValue.toString().contains(value, Qt::CaseInsensitive);
                } else if (op == "Is") {
                    match = metadataValue.toString().compare(value, Qt::CaseInsensitive) == 0;
                } else if (op == "Starts With") {
                    match = metadataValue.toString().startsWith(value, Qt::CaseInsensitive);
                } else if (op == "Ends With") {
                    match = metadataValue.toString().endsWith(value, Qt::CaseInsensitive);
                } else if (op == "Is One Of") {
                    QStringList values = value.split('\n', Qt::SkipEmptyParts);
                    for (const QString &v : values) {
                        if (metadataValue.toString().compare(v.trimmed(), Qt::CaseInsensitive) == 0) {
                            match = true;
                            break;
                        }
                    }
                }
            }

            if (!match) {
                allConditionsMatch = false;
                break; // One condition failed, no need to check others
            }
        }

        // 3. If all conditions match, construct the directory path
        if (allConditionsMatch) {
            QString targetFolder = rule["target_folder"].toString();
            QString subfolderPattern = rule["subfolder_pattern"].toString();

            QString finalSubfolder;
            if (!subfolderPattern.isEmpty()) {
                finalSubfolder = parseAndReplaceTokens(subfolderPattern, videoMetadata);
            }

            return QDir(targetFolder).filePath(finalSubfolder);
        }
    }

    // No rule matched, return default directory
    QString baseDir = m_configManager->get("Paths", "completed_downloads_directory").toString();
    if (baseDir.isEmpty()) {
        baseDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    }
    return baseDir;
}

QString SortingManager::parseAndReplaceTokens(const QString &pattern, const QVariantMap &metadata) {
    QString result = pattern;

    // Handle special date tokens first
    if (metadata.contains("upload_date")) {
        QString dateStr = metadata["upload_date"].toString(); // YYYYMMDD
        if (dateStr.length() == 8) {
            result.replace("{upload_year}", dateStr.left(4), Qt::CaseInsensitive);
            result.replace("{upload_month}", dateStr.mid(4, 2), Qt::CaseInsensitive);
            result.replace("{upload_day}", dateStr.right(2), Qt::CaseInsensitive);
        }
    }

    // Use regex to find all {token} patterns
    QRegularExpression re("\\{([^}]+)\\}");
    auto it = re.globalMatch(pattern);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString token = match.captured(0); // e.g., "{title}"
        QString key = match.captured(1);   // e.g., "title"

        if (metadata.contains(key)) {
            result.replace(token, sanitize(metadata[key].toString()), Qt::CaseInsensitive);
        }
    }

    return result;
}

QString SortingManager::sanitize(const QString &name) {
    QString sanitized = name;
    // Remove illegal characters for Windows/Unix paths
    sanitized.remove(QRegularExpression("[<>:\"/\\\\|?*]"));
    return sanitized.trimmed();
}
