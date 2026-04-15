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
    qDebug() << "SortingManager::getSortedDirectory called.";
    qDebug() << "  videoMetadata keys:" << videoMetadata.keys();
    if (videoMetadata.contains("uploader")) {
        qDebug() << "  uploader value:" << videoMetadata["uploader"].toString();
    } else {
        qDebug() << "  uploader key NOT FOUND in metadata!";
    }
    qDebug() << "  downloadOptions type:" << downloadOptions.value("type", "video").toString();

    int size = m_configManager->get("SortingRules", "size", 0).toInt();
    qDebug() << "  SortingRules size:" << size;
    if (size == 0) {
        // No rules to process, return default directory
        QString baseDir = m_configManager->get("Paths", "completed_downloads_directory").toString();
        if (baseDir.isEmpty()) {
            baseDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        }
        return baseDir;
    }

    // Rules are stored with flat keys: rule_N_name, rule_N_applies_to, rule_N_target_folder, etc.
    for (int i = 0; i < size; ++i) {
        QString key = QString("rule_%1").arg(i);
        
        QString ruleName = m_configManager->get("SortingRules", key + "_name").toString();
        QVariant appliesToVar = m_configManager->get("SortingRules", key + "_applies_to");
        QString appliesTo = appliesToVar.isValid() ? appliesToVar.toString() : "All Downloads";
        QString targetFolder = m_configManager->get("SortingRules", key + "_target_folder").toString();
        QString subfolderPattern = m_configManager->get("SortingRules", key + "_subfolder_pattern").toString();
        
        // Load conditions
        int condSize = m_configManager->get("SortingRules", key + "_conditions_size", 0).toInt();
        QJsonArray conditionsArray;
        for (int j = 0; j < condSize; ++j) {
            QString condKey = key + QString("_condition_%1").arg(j);
            QJsonObject cond;
            cond["field"] = m_configManager->get("SortingRules", condKey + "_field").toString();
            cond["operator"] = m_configManager->get("SortingRules", condKey + "_operator").toString();
            cond["value"] = m_configManager->get("SortingRules", condKey + "_value").toString();
            conditionsArray.append(cond);
        }
        
        // Skip invalid rules
        if (ruleName.isEmpty() || targetFolder.isEmpty()) {
            qDebug() << "  Rule" << i << "(" << key << ") is invalid (empty name or target), skipping.";
            continue;
        }
        
        qDebug() << "  Checking rule" << i << "(" << ruleName << "), appliesTo:" << appliesTo << "targetFolder:" << targetFolder << "conditions:" << condSize;

        // 1. Check if the rule applies to this download type
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
        qDebug() << "    Conditions count:" << conditionsArray.size();
        bool allConditionsMatch = true;
        for (int c = 0; c < conditionsArray.size(); ++c) {
            QJsonObject condition = conditionsArray[c].toObject();
            QString field = condition["field"].toString();
            QString op = condition["operator"].toString();
            QString value = condition["value"].toString();

            QVariant metadataValue;
            if (field == "Duration (seconds)") {
                metadataValue = videoMetadata.value("duration");
            } else {
                metadataValue = videoMetadata.value(field.toLower());
            }

            qDebug() << "    Condition" << c << "- field:" << field << "(looking for key:" << field.toLower() << ")" << "op:" << op;
            qDebug() << "      metadataValue:" << metadataValue.toString() << "(isEmpty:" << metadataValue.toString().isEmpty() << ")";
            qDebug() << "      condition value:" << value.left(100);

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
                    qDebug() << "      Is One Of has" << values.size() << "values. First 5:" << values.mid(0, 5);
                    for (const QString &v : values) {
                        if (metadataValue.toString().compare(v.trimmed(), Qt::CaseInsensitive) == 0) {
                            match = true;
                            qDebug() << "      Is One Of MATCHED on:" << v.trimmed();
                            break;
                        }
                    }
                    if (!match) {
                        qDebug() << "      Is One Of did NOT match any value.";
                    }
                }
            }

            if (!match) {
                allConditionsMatch = false;
                qDebug() << "    Condition" << c << "FAILED (op:" << op << "), skipping rule.";
                break; // One condition failed, no need to check others
            } else {
                qDebug() << "    Condition" << c << "MATCHED.";
            }
        }

        // 3. If all conditions match, construct the directory path
        if (allConditionsMatch) {
            qDebug() << "  Rule" << i << "(" << ruleName << ") MATCHED!";

            QString finalSubfolder;
            if (!subfolderPattern.isEmpty()) {
                finalSubfolder = parseAndReplaceTokens(subfolderPattern, videoMetadata);
            }

            return QDir(targetFolder).filePath(finalSubfolder);
        }
    }

    // No rule matched, return default directory
    qDebug() << "  No sorting rule matched. Returning default directory.";
    QString baseDir = m_configManager->get("Paths", "completed_downloads_directory").toString();
    if (baseDir.isEmpty()) {
        baseDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    }
    qDebug() << "  Default directory:" << baseDir;
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
