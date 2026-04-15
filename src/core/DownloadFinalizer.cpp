#include "DownloadFinalizer.h"
#include "ConfigManager.h"
#include "SortingManager.h"
#include "ArchiveManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QCoreApplication>
#include <QDebug>

DownloadFinalizer::DownloadFinalizer(ConfigManager *configManager, SortingManager *sortingManager, ArchiveManager *archiveManager, QObject *parent)
    : QObject(parent), m_configManager(configManager), m_sortingManager(sortingManager), m_archiveManager(archiveManager) {
}

bool DownloadFinalizer::copyDirectoryRecursively(const QString &sourceDir, const QString &destDir) {
    QDir source(sourceDir);
    if (!source.exists()) {
        qWarning() << "copyDirectoryRecursively: source does not exist:" << sourceDir;
        return false;
    }
    QDir dest(destDir);
    if (!dest.exists()) {
        if (!dest.mkpath(".")) {
            qWarning() << "copyDirectoryRecursively: failed to create dest dir:" << destDir;
            return false;
        }
    }
    bool success = true;
    QFileInfoList entries = source.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
    for (const QFileInfo &entry : entries) {
        QString srcPath = entry.absoluteFilePath();
        QString dstPath = dest.absoluteFilePath(entry.fileName());
        if (entry.isDir()) {
            success &= copyDirectoryRecursively(srcPath, dstPath);
        } else {
            if (QFile::exists(dstPath)) {
                if (!QFile::remove(dstPath)) {
                    qWarning() << "copyDirectoryRecursively: failed to remove existing file:" << dstPath;
                }
            }
            if (!QFile::copy(srcPath, dstPath)) {
                qWarning() << "copyDirectoryRecursively: failed to copy" << srcPath << "to" << dstPath;
                success = false;
            }
        }
    }
    return success;
}

void DownloadFinalizer::finalize(const QString &id, DownloadItem item) {
    qDebug() << "Starting finalize for id:" << id;

    if (item.options.value("type").toString() != "gallery" && item.metadata.isEmpty()) {
        QString tempDirPath = m_configManager->get("Paths", "temporary_downloads_directory").toString();
        QDir tempDir(tempDirPath);
        QStringList jsonFiles = tempDir.entryList(QStringList() << "*.info.json", QDir::Files, QDir::Time);
        if (jsonFiles.isEmpty()) {
            qWarning() << "No info.json found in temp dir";
            emit finalizationComplete(id, false, "Downloaded file not found.");
            return;
        }
        QString jsonPath = tempDir.filePath(jsonFiles.first());
        QFile jsonFile(jsonPath);
        if (!jsonFile.open(QIODevice::ReadOnly)) {
            qWarning() << "Could not open info.json:" << jsonPath;
            emit finalizationComplete(id, false, "Downloaded file not found.");
            return;
        }
        QByteArray jsonData = jsonFile.readAll();
        jsonFile.close();
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (!doc.isObject()) {
            qWarning() << "Invalid info.json";
            emit finalizationComplete(id, false, "Downloaded file not found.");
            return;
        }
        item.metadata = doc.object().toVariantMap();
    }

    QString finalName = QFileInfo(item.tempFilePath).fileName();
    if (finalName.isEmpty()) {
        qWarning() << "Could not resolve final media filename from yt-dlp after_move path output.";
        emit finalizationComplete(id, false, "Download completed, but could not resolve output filename from yt-dlp.");
        return;
    }

    item.tempFilePath = QDir(m_configManager->get("Paths", "temporary_downloads_directory").toString()).filePath(finalName);
    QFileInfo fileInfo(item.tempFilePath);
    qint64 lastSize = -1;
    int stableCount = 0;
    int maxRetries = 20;

    emit progressUpdated(id, {{"status", "Verifying download completeness..."}});

    if (fileInfo.isFile()) {
        for (int i = 0; i < maxRetries; ++i) {
            fileInfo.refresh();
            qint64 currentSize = fileInfo.size();
            if (currentSize == lastSize && currentSize > 0) {
                stableCount++;
            } else {
                stableCount = 0;
            }
            lastSize = currentSize;
            if (stableCount >= 3) break;
            QThread::msleep(100);
            QCoreApplication::processEvents();
        }
    }
    
    emit progressUpdated(id, {{"status", "Applying sorting rules..."}});

    QString finalDir = m_sortingManager->getSortedDirectory(item.metadata, item.options);
    QDir().mkpath(finalDir);
    finalDir = QDir(finalDir).absolutePath();

    if (item.options.value("type").toString() == "audio" && item.playlistIndex > 0) {
        QString paddedIndex = QString("%1").arg(item.playlistIndex, 2, 10, QChar('0'));
        finalName = QString("%1 - %2").arg(paddedIndex, finalName);
    }

    QString destPath = QDir(finalDir).filePath(finalName);

    if (item.options.value("type").toString() == "audio" && item.playlistIndex > 0) {
        QString thumbTempPath = QDir(m_configManager->get("Paths", "temporary_downloads_directory").toString()).filePath(id + "_folder.jpg");
        if (QFile::exists(thumbTempPath)) {
            QString thumbDestPath = QDir(finalDir).filePath("folder.jpg");
            if (!QFile::exists(thumbDestPath)) {
                QFile::copy(thumbTempPath, thumbDestPath);
            }
            QFile::remove(thumbTempPath);
        }
    }

    if (item.options.value("type").toString() == "gallery") {
        QString tempPath = QDir::fromNativeSeparators(item.tempFilePath);
        QDir tempDir(tempPath);
        if (!tempDir.exists()) {
            emit finalizationComplete(id, false, "Gallery download failed: temp directory missing.");
            return;
        }

        QFileInfoList entries = tempDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
        if (entries.size() == 1) {
            QFileInfo entry = entries.first();
            QString newDestPath = QDir(finalDir).filePath(entry.fileName());
            
            if (entry.isFile()) {
                if (QFile::rename(entry.absoluteFilePath(), newDestPath) || (QFile::copy(entry.absoluteFilePath(), newDestPath) && QFile::remove(entry.absoluteFilePath()))) {
                    m_archiveManager->addToArchive(item.url);
                    emit finalPathReady(id, newDestPath);
                    emit finalizationComplete(id, true, QString("Gallery download completed → %1").arg(QDir::toNativeSeparators(finalDir)));
                } else {
                    emit finalizationComplete(id, false, "Gallery download completed, but failed to move file to final destination.");
                }
            } else {
                if (QDir().rename(entry.absoluteFilePath(), newDestPath) || (copyDirectoryRecursively(entry.absoluteFilePath(), newDestPath) && QDir(entry.absoluteFilePath()).removeRecursively())) {
                    m_archiveManager->addToArchive(item.url);
                    emit finalPathReady(id, newDestPath);
                    emit finalizationComplete(id, true, QString("Gallery download completed → %1").arg(QDir::toNativeSeparators(finalDir)));
                } else {
                    emit finalizationComplete(id, false, "Gallery download completed, but failed to move directory.");
                }
            }
        } else {
            if (QDir().rename(item.tempFilePath, destPath) || (copyDirectoryRecursively(item.tempFilePath, destPath) && QDir(item.tempFilePath).removeRecursively())) {
                m_archiveManager->addToArchive(item.url);
                emit finalPathReady(id, destPath);
                emit finalizationComplete(id, true, QString("Gallery download completed → %1").arg(QDir::toNativeSeparators(finalDir)));
            } else {
                emit finalizationComplete(id, false, "Gallery download completed, but failed to move directory.");
            }
        }
    } else {
        emit progressUpdated(id, {{"status", "Moving to final destination..."}});

        if (QFile::exists(destPath) && !QFile::remove(destPath)) {
            emit finalizationComplete(id, false, "Download completed, but failed to replace existing file.");
            return;
        }

        bool moved = QFile::rename(item.tempFilePath, destPath);
        if (!moved) {
            emit progressUpdated(id, {{"status", "Copying file to destination..."}});
            if ((moved = QFile::copy(item.tempFilePath, destPath))) {
                QFile::remove(item.tempFilePath);
            }
        }

        if (moved) {
            m_archiveManager->addToArchive(item.url);
            emit finalPathReady(id, destPath);
            emit finalizationComplete(id, true, QString("Download completed → %1").arg(QDir::toNativeSeparators(finalDir)));
            if (!item.originalDownloadedFilePath.isEmpty() && item.originalDownloadedFilePath != item.tempFilePath) {
                QFile::remove(item.originalDownloadedFilePath);
            }
        } else {
            emit finalizationComplete(id, false, "Download completed, but failed to move file.");
        }
    }

    if (item.options.value("type").toString() != "gallery") {
        QFileInfo fi(item.tempFilePath);
        QFile::remove(fi.absoluteDir().filePath(fi.completeBaseName() + ".info.json"));
    }
}