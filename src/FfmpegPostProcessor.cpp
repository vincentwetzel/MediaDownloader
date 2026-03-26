#include "FfmpegPostProcessor.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

FfmpegPostProcessor::FfmpegPostProcessor(QObject *parent)
    : QObject(parent), m_process(new QProcess(this))
{
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            emit mergeFailed("Failed to start ffmpeg. Is the executable missing?");
        }
    });

    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus == QProcess::CrashExit || exitCode != 0) {
            QString errorMsg = "FFmpeg process failed. Error: " + m_process->errorString() + "\n" + m_process->readAllStandardError();
            emit mergeFailed(errorMsg);
            return;
        }

        // Cleanup the original unmerged parts now that the merge is successful
        for (const QString &inputFile : m_currentInputFiles) {
            if (QFile::exists(inputFile)) {
                QFile::remove(inputFile);
            }
        }
        // Also clean up subtitle parts
        for (const SubtitleFile &subFile : m_currentSubtitleFiles) {
            if (QFile::exists(subFile.path)) {
                QFile::remove(subFile.path);
            }
        }

        emit mergeSuccess(m_currentOutputFile);
    });
}

void FfmpegPostProcessor::merge(const QString &ffmpegPath, const QStringList &inputFiles, const QString &outputFile, const QString &title, const QString &artworkPath, const QList<SubtitleFile> &subtitleFiles)
{
    if (m_process->state() != QProcess::NotRunning) {
        emit mergeFailed("FFmpeg is already running another job.");
        return;
    }

    if (inputFiles.isEmpty()) {
        emit mergeFailed("No input files provided for merging.");
        return;
    }

    m_currentInputFiles = inputFiles;
    m_currentSubtitleFiles = subtitleFiles;
    m_currentOutputFile = outputFile;
    
    bool hasArtwork = !artworkPath.isEmpty() && QFile::exists(artworkPath);
    if (hasArtwork) {
        m_currentInputFiles.append(artworkPath); // Append to list so it gets auto-deleted on cleanup!
    }
    bool hasSubtitles = !subtitleFiles.isEmpty();
    
    QString ext = QFileInfo(outputFile).suffix().toLower();
    bool supportsSubtitles = (ext == "mp4" || ext == "mkv" || ext == "webm" || ext == "mov" || ext == "m4v" || ext == "m4a");
    if (hasSubtitles && !supportsSubtitles) {
        hasSubtitles = false;
        qDebug() << "Output container does not support embedded subtitles. Ignoring.";
    }

    // If there is only one file, and no metadata/artwork/subtitles, no merging is needed
    if (inputFiles.size() == 1 && title.isEmpty() && !hasArtwork && !hasSubtitles) {
        if (QFile::exists(outputFile)) QFile::remove(outputFile);
        if (QFile::rename(inputFiles.first(), outputFile)) {
            for (const SubtitleFile &subFile : m_currentSubtitleFiles) {
                if (QFile::exists(subFile.path)) {
                    QFile::remove(subFile.path);
                }
            }
            emit mergeSuccess(outputFile);
        } else {
            emit mergeFailed("Failed to rename single downloaded file to final output.");
        }
        return;
    }

    QStringList args;
    
    for (const QString &inputFile : inputFiles) {
        args << "-i" << inputFile;
    }

    if (hasSubtitles) {
        for (const SubtitleFile &subFile : subtitleFiles) {
            args << "-i" << subFile.path;
        }
    }

    if (hasArtwork) {
        args << "-i" << artworkPath;
    }

    // Map all media inputs (Video/Audio)
    int streamIndex = 0;
    for (int i = 0; i < inputFiles.size(); ++i, ++streamIndex) {
        args << "-map" << QString::number(i);
    }

    // Map subtitle inputs
    if (hasSubtitles) {
        for (int i = 0; i < subtitleFiles.size(); ++i, ++streamIndex) {
            args << "-map" << QString::number(streamIndex);
        }
    }

    // Map the artwork as an attached picture stream
    if (hasArtwork) {
        args << "-map" << QString::number(streamIndex);
    }

    // Copy media streams without re-encoding
    args << "-c:v" << "copy" << "-c:a" << "copy";
    
    if (hasSubtitles) {
        // MP4 requires mov_text, MKV/WebM handles SRT best
        if (outputFile.endsWith(".mp4", Qt::CaseInsensitive) || outputFile.endsWith(".m4a", Qt::CaseInsensitive)) {
            args << "-c:s" << "mov_text";
        } else if (outputFile.endsWith(".webm", Qt::CaseInsensitive)) {
            args << "-c:s" << "webvtt";
        } else {
            args << "-c:s" << "srt"; 
        }

        for (int i = 0; i < subtitleFiles.size(); ++i) {
            args << QString("-metadata:s:s:%1").arg(i) << QString("language=%1").arg(subtitleFiles[i].language);
        }
    }

    if (hasArtwork) {
        // If audio-only, artwork is the first video stream (v:0). If video+audio, it's the second (v:1).
        bool isAudioOnly = (ext == "mp3" || ext == "m4a" || ext == "wav" || ext == "flac" || ext == "opus" || ext == "ogg" || ext == "aac");
        args << QString("-disposition:v:%1").arg(isAudioOnly ? 0 : 1) << "attached_pic";
    }

    if (!title.isEmpty()) {
        args << "-metadata" << QString("title=%1").arg(title);
    }

    args << "-y" << outputFile;

    m_process->start(ffmpegPath, args);
}

void FfmpegPostProcessor::cancel() {
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
}