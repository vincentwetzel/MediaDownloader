#include "GalleryDlWorker.h"
#include "ProcessUtils.h"
#include "core/ConfigManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QFileInfo>
#include <QProcess>

GalleryDlWorker::GalleryDlWorker(const QString &id, const QStringList &args, ConfigManager *configManager, QObject *parent)
    : QObject(parent), m_configManager(configManager), m_id(id), m_args(args)
{
    m_process = new QProcess(this);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &GalleryDlWorker::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &GalleryDlWorker::onReadyReadStandardError);
    connect(m_process, &QProcess::finished, this, &GalleryDlWorker::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &GalleryDlWorker::onProcessError);
}

void GalleryDlWorker::start()
{
    const QString galleryDlPath = resolveExecutablePath("gallery-dl.exe");
    if (galleryDlPath.isEmpty()) {
        emit finished(m_id, false, "gallery-dl executable was not found.", QString(), QVariantMap());
        return;
    }

    // Enforce UTF-8 output to prevent Unicode path mangling in stdout
    ProcessUtils::setProcessEnvironment(*m_process);

    // Emit initial progress
    emit progressUpdated(m_id, {
        {"progress", 0},
        {"status", "Starting gallery-dl..."}
    });

    m_process->setWorkingDirectory(QFileInfo(galleryDlPath).absolutePath());
    m_process->start(galleryDlPath, m_args);
}

void GalleryDlWorker::killProcess()
{
    if (m_process->state() == QProcess::Running) {
        m_process->disconnect(); // Prevent re-entrant read operations on the dying process buffer
        ProcessUtils::terminateProcessTree(m_process);
        m_process->kill(); // Forcefully kill the QProcess instance as fallback
    }
}

void GalleryDlWorker::onReadyReadStandardOutput()
{
    m_outputBuffer.append(m_process->readAllStandardOutput());

    // Find the last complete line ending
    int lastNewline = m_outputBuffer.lastIndexOf('\n');
    int lastCarriageReturn = m_outputBuffer.lastIndexOf('\r');
    int lastDelimiter = qMax(lastNewline, lastCarriageReturn);

    if (lastDelimiter == -1) {
        return; // No complete line yet, wait for more data
    }

    // Extract all complete lines and leave any partial line in the buffer
    QByteArray completeData = m_outputBuffer.left(lastDelimiter + 1);
    m_outputBuffer.remove(0, lastDelimiter + 1);

    const QStringList lines = QString::fromUtf8(completeData).split(QRegularExpression("[\\r\\n]"), Qt::SkipEmptyParts);

    // Match file paths on Windows (C:\...), UNC (\\server\...), and Unix (/home/...)
    QRegularExpression pathRe(R"((?:(?:[A-Za-z]:[/\\])|(?:[/\\]{2}[\w.-]+[/\\])|(?:/[A-Za-z0-9_.-]+?/))[^\n]+\.(?:jpg|jpeg|png|gif|webp|webm|mp4|mkv|avi|mov|pdf|zip|txt|mp3|ogg|flac|wav|svg|bmp|tiff|gifv|mpd|m3u8|vtt|ass|srt|json|xml|html|torrent|cbz|cbr|epub))", QRegularExpression::CaseInsensitiveOption);

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) continue;

        emit outputReceived(m_id, trimmedLine);

        // Check if this line contains a downloaded file path
        QRegularExpressionMatch match = pathRe.match(trimmedLine);
        if (match.hasMatch()) {
            QString filePath = match.captured(0).trimmed();
            if (!filePath.isEmpty() && filePath != m_lastFile) {
                m_lastFile = filePath;
                QFileInfo fi(m_lastFile);
                // Emit full progress with "processing" color
                emit progressUpdated(m_id, {
                    {"progress", 100},
                    {"status", "Downloading: " + fi.fileName()},
                    {"current_file", m_lastFile}
                });
            }
        }
    }
}

void GalleryDlWorker::onReadyReadStandardError()
{
    m_errorBuffer.append(m_process->readAllStandardError());
    
    int lastNewline = m_errorBuffer.lastIndexOf('\n');
    int lastCarriageReturn = m_errorBuffer.lastIndexOf('\r');
    int lastDelimiter = qMax(lastNewline, lastCarriageReturn);

    if (lastDelimiter == -1) {
        return;
    }

    QByteArray completeData = m_errorBuffer.left(lastDelimiter + 1);
    m_errorBuffer.remove(0, lastDelimiter + 1);

    const QStringList lines = QString::fromUtf8(completeData).split(QRegularExpression("[\\r\\n]"), Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (!trimmedLine.isEmpty()) {
            emit outputReceived(m_id, trimmedLine);
        }
    }
}

void GalleryDlWorker::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Process any remaining buffered output that lacked a trailing newline
    if (!m_outputBuffer.isEmpty()) {
        QString remainder = QString::fromUtf8(m_outputBuffer).trimmed();
        if (!remainder.isEmpty()) emit outputReceived(m_id, remainder);
        m_outputBuffer.clear();
    }
    if (!m_errorBuffer.isEmpty()) {
        QString remainder = QString::fromUtf8(m_errorBuffer).trimmed();
        if (!remainder.isEmpty()) emit outputReceived(m_id, remainder);
        m_errorBuffer.clear();
    }

    if (exitStatus == QProcess::CrashExit || (exitCode != 0 && exitCode != 101)) { // 101 can mean "no images found"
        QString stderrOutput = QString::fromUtf8(m_process->readAllStandardError());
        qWarning() << "GalleryDlWorker failed. Exit code:" << exitCode << "Stderr:" << stderrOutput;
        emit finished(m_id, false, "gallery-dl failed: " + stderrOutput, QString(), QVariantMap());
        return;
    }

    // Emit final progress before finishing
    emit progressUpdated(m_id, {
        {"progress", 100},
        {"status", "Finalizing..."}
    });

    QString outputDirectory;
    for (int i = 0; i < m_args.size(); ++i) {
        if ((m_args[i] == "--directory" || m_args[i] == "-D") && i + 1 < m_args.size()) {
            outputDirectory = m_args[i + 1];
            break;
        }
    }

    emit finished(m_id, true, "Download completed successfully.", outputDirectory, QVariantMap());
}

void GalleryDlWorker::onProcessError(QProcess::ProcessError processError)
{
    if (processError == QProcess::FailedToStart) {
        qWarning() << "GalleryDlWorker failed to start process:" << m_process->errorString();
        emit finished(m_id, false, "Failed to start gallery-dl process. Please check if it's installed and in your PATH, or configure the path in settings.", QString(), QVariantMap());
    } else {
        qWarning() << "GalleryDlWorker process error:" << m_process->errorString();
        emit finished(m_id, false, "An error occurred with the gallery-dl process: " + m_process->errorString(), QString(), QVariantMap());
    }
}

QString GalleryDlWorker::resolveExecutablePath(const QString &name) const
{
    // Remove .exe suffix for ProcessUtils
    QString baseName = name;
    if (baseName.endsWith(".exe")) {
        baseName.chop(4);
    }
    
    ProcessUtils::FoundBinary found = ProcessUtils::findBinary(baseName, m_configManager);
    
    if (found.source == "Not Found" || found.source == "Invalid Custom") {
        return QString();
    }
    
    return found.path;
}
