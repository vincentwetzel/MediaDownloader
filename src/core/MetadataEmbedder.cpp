#include "MetadataEmbedder.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QFile>

MetadataEmbedder::MetadataEmbedder(QObject *parent) : QObject(parent) {
    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &MetadataEmbedder::onProcessFinished);
}

void MetadataEmbedder::embedTrackNumber(const QString &filePath, int trackNumber) {
    m_originalFilePath = filePath;
    QFileInfo fileInfo(filePath);

    // Skip .opus files to avoid stripping album art with ffmpeg -c copy
    if (fileInfo.suffix().toLower() == "opus") {
        qDebug() << "Skipping metadata embedding for .opus file to preserve album art.";
        emit finished(true, ""); // Report success so the file move can proceed
        return;
    }

    m_tempFilePath = fileInfo.absolutePath() + "/temp_" + fileInfo.fileName();

    QStringList args;
    args << "-i" << filePath;
    args << "-metadata" << QString("track=%1").arg(trackNumber);
    args << "-c" << "copy";
    args << "-y"; // Overwrite output
    args << m_tempFilePath;

    m_process->start("ffmpeg", args);
}

void MetadataEmbedder::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
    QString error;

    if (success) {
        // Replace original file with temp file
        if (QFile::remove(m_originalFilePath)) {
            if (!QFile::rename(m_tempFilePath, m_originalFilePath)) {
                success = false;
                error = "Failed to rename temp file to original file.";
            }
        } else {
            success = false;
            error = "Failed to remove original file.";
        }
    } else {
        error = m_process->readAllStandardError();
        QFile::remove(m_tempFilePath); // Clean up
    }

    emit finished(success, error);
}
