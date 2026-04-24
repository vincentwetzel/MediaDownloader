#include "FfmpegPostProcessor.h"
#include "core/ConfigManager.h"
#include "core/ProcessUtils.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QFile>

FfmpegPostProcessor::FfmpegPostProcessor(ConfigManager *configManager, QObject *parent)
    : QObject(parent), m_configManager(configManager)
{
    m_process = new QProcess(this);
    ProcessUtils::setProcessEnvironment(*m_process);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        appendProcessOutput(m_process->readAllStandardOutput());
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        appendProcessOutput(m_process->readAllStandardError());
    });
    connect(m_process, &QProcess::finished, this, &FfmpegPostProcessor::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &FfmpegPostProcessor::onProcessError);
}

void FfmpegPostProcessor::embedTrackNumber(const QString &filePath, int trackNumber, int totalTracks)
{
    if (m_process->state() != QProcess::NotRunning) {
        emit error("FFmpeg Post-processor is already running.");
        return;
    }

    m_originalFile = filePath;
    m_processOutputTail.clear();
    QFileInfo fileInfo(filePath);
    m_tempFile = fileInfo.path() + "/" + fileInfo.completeBaseName() + "_tagged." + fileInfo.suffix();

    QStringList args;
    args << "-nostdin";
    args << "-i" << m_originalFile;
    args << "-c" << "copy"; // Copy all streams
    args << "-metadata" << QString("track=%1/%2").arg(trackNumber).arg(totalTracks);
    args << "-y" << m_tempFile;

    QString program = ProcessUtils::findBinary("ffmpeg", m_configManager).path;
    m_process->start(program, args);
}

void FfmpegPostProcessor::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    appendProcessOutput(m_process->readAllStandardOutput());
    appendProcessOutput(m_process->readAllStandardError());

    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        QString stderrOutput = m_processOutputTail;
        qWarning() << "FfmpegPostProcessor failed. Exit code:" << exitCode << "Stderr:" << stderrOutput;
        QFile::remove(m_tempFile); // Clean up temp file
        emit error("FFmpeg post-processing failed: " + stderrOutput);
        return;
    }

    // Replace original file with the tagged one
    if (!QFile::remove(m_originalFile)) {
        qWarning() << "Could not remove original file:" << m_originalFile;
        emit error("Could not replace original file with tagged version.");
        return;
    }
    if (!QFile::rename(m_tempFile, m_originalFile)) {
        qWarning() << "Could not rename temp file" << m_tempFile << "to" << m_originalFile;
        emit error("Could not rename temp file to original file.");
        return;
    }

    emit finished();
}

void FfmpegPostProcessor::onProcessError(QProcess::ProcessError processError)
{
    if (processError == QProcess::FailedToStart) {
        qWarning() << "FfmpegPostProcessor failed to start process:" << m_process->errorString();
        emit error("Failed to start ffmpeg process. Please check if it's installed and in your PATH, or configure the path in settings.");
    } else {
        qWarning() << "FfmpegPostProcessor process error:" << m_process->errorString();
        emit error("An error occurred with the ffmpeg process: " + m_process->errorString());
    }
}

void FfmpegPostProcessor::appendProcessOutput(const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }

    m_processOutputTail += QString::fromUtf8(data);
    constexpr qsizetype maxTailLength = 12000;
    if (m_processOutputTail.size() > maxTailLength) {
        m_processOutputTail = m_processOutputTail.right(maxTailLength);
    }
}
