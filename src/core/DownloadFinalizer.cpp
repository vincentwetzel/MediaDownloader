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
#include <QUrlQuery>
#include <QDebug>

namespace { // Anonymous namespace to limit scope to this file

void cleanupTempFiles(const DownloadItem &item, const QDir &tempDir, const QString &mediaInfoJsonPath)
{
    if (item.options.value("type").toString() == "gallery") {
        return;
    }

    // 1. Clean up the info.json that matches the media file name.
    QFile::remove(mediaInfoJsonPath);
    qDebug() << "Attempted to clean up media info.json:" << mediaInfoJsonPath;

    // 2. If it was a playlist download, find and remove the playlist's info.json file.
    QString playlistId;

    // Strategy 1: Get playlist_id from the original playlist URL stored in options. This is the most reliable.
    if (item.options.contains("original_playlist_url")) {
        QUrl url(item.options.value("original_playlist_url").toString());
        if (url.hasQuery()) {
            QUrlQuery query(url);
            if (query.hasQueryItem("list")) {
                playlistId = query.queryItemValue("list");
            }
        }
    }

    // Strategy 2: Fallback to metadata if the original URL wasn't available for some reason.
    if (playlistId.isEmpty() && item.metadata.contains("playlist_id")) {
        playlistId = item.metadata.value("playlist_id").toString();
        qDebug() << "Using playlist_id from metadata as fallback for cleanup:" << playlistId;
    }

    if (!playlistId.isEmpty()) {
        QStringList potentialFiles = tempDir.entryList(QStringList() << "*.info.json", QDir::Files);
        for (const QString &fileName : potentialFiles) {
            // Check if the filename contains the playlist ID, typically formatted as "[<playlist_id>]"
            // by yt-dlp's default playlist output template. This is more reliable than parsing JSON content.
            if (fileName.contains("[" + playlistId + "]")) {
                QString filePath = tempDir.filePath(fileName);
                if (QFile::remove(filePath)) {
                    qDebug() << "Cleaned up playlist info.json by filename match:" << filePath;
                } else {
                    qWarning() << "Failed to remove playlist info.json by filename match:" << filePath;
                }
                break; // Assume only one such file exists per playlist.
            }
        }
    } else {
        qDebug() << "No playlist_id found for cleanup for item:" << item.id;
    }
}

} // namespace

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
        qWarning() << "Metadata is empty in finalize for id:" << id << ", attempting to read from disk.";
        QFileInfo fi(item.tempFilePath);
        QString jsonPath = fi.absoluteDir().filePath(fi.completeBaseName() + ".info.json");

        QFile jsonFile(jsonPath);
        if (!jsonFile.open(QIODevice::ReadOnly)) {
            qWarning() << "Could not open info.json for fallback:" << jsonPath;
            emit finalizationComplete(id, false, "Downloaded file not found.");
            return;
        }

        QByteArray jsonData = jsonFile.readAll();
        jsonFile.close();
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (doc.isObject()) {
            item.metadata = doc.object().toVariantMap();
            qDebug() << "Successfully loaded metadata from fallback for id:" << id;
        } else {
            qWarning() << "Invalid info.json in fallback:" << jsonPath;
            emit finalizationComplete(id, false, "Downloaded file not found.");
            return;
        }
    }

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
    QString finalName = fileInfo.fileName();

    if (item.options.value("type").toString() == "audio" && item.playlistIndex > 0) {
        QString paddedIndex = QString("%1").arg(item.playlistIndex, 2, 10, QChar('0'));
        finalName = QString("%1 - %2").arg(paddedIndex, finalName);
    }

    QString destPath = QDir(finalDir).filePath(finalName);

    // Capture temp file info BEFORE it's moved/renamed.
    QFileInfo tempFileInfo(item.tempFilePath);
    QDir tempDir = tempFileInfo.absoluteDir();
    QString mediaInfoJsonPath = tempDir.filePath(tempFileInfo.completeBaseName() + ".info.json");

    if (item.options.value("type").toString() == "audio" && item.playlistIndex > 0) {
        // Find the generated folder image. It might not be .jpg if the user selected .png or no conversion
        QStringList filters;
        filters << id + "_folder.*";
        QStringList thumbFiles = tempDir.entryList(filters, QDir::Files);
        if (!thumbFiles.isEmpty()) {
            QString thumbTempPath = tempDir.filePath(thumbFiles.first());
            QFileInfo thumbInfo(thumbTempPath);
            QString thumbDestPath = QDir(finalDir).filePath("folder." + thumbInfo.suffix());
            if (!QFile::exists(thumbDestPath)) {
                QFile::copy(thumbTempPath, thumbDestPath);
            }
            QFile::remove(thumbTempPath);
            qDebug() << "Moved playlist folder artwork to:" << thumbDestPath;
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

    // Cleanup must happen after all signals are emitted and operations are complete.
    cleanupTempFiles(item, tempDir, mediaInfoJsonPath);
}