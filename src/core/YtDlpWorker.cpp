#include "YtDlpWorker.h"
#include "core/ConfigManager.h"
#include "core/ProcessUtils.h"

#include <QDebug>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <cmath> // For pow

YtDlpWorker::YtDlpWorker(const QString &id, const QStringList &args, ConfigManager *configManager, QObject *parent)
    : QObject(parent), m_id(id), m_args(args), m_configManager(configManager), m_process(nullptr), m_finishEmitted(false), m_videoTitle(QString()),
      m_thumbnailPath(QString()), m_infoJsonPath(QString()), m_infoJsonRetryCount(0) {

    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &YtDlpWorker::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &YtDlpWorker::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &YtDlpWorker::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &YtDlpWorker::onReadyReadStandardError);
}

void YtDlpWorker::start() {
    qDebug() << "[YtDlpWorker] start() called for ID:" << m_id;
    
    // Clear any leftover state from previous downloads
    m_fullMetadata.clear();

    const ProcessUtils::FoundBinary ytDlpBinary = ProcessUtils::findBinary("yt-dlp", m_configManager);
    if (ytDlpBinary.source == "Not Found" || ytDlpBinary.path.isEmpty()) {
        const QString message = "Download failed.\nyt-dlp could not be found. Configure it in Advanced Settings -> External Binaries.";
        qWarning() << message;
        if (!m_finishEmitted) {
            m_finishEmitted = true;
            emit finished(m_id, false, message, QString(), QString(), QVariantMap());
        }
        return;
    }

    const QString ytDlpPath = ytDlpBinary.path;
    const QString workingDirPath = QFileInfo(ytDlpPath).absolutePath();
    m_process->setWorkingDirectory(workingDirPath);
    ProcessUtils::setProcessEnvironment(*m_process);
    
    // Force yt-dlp to emit progress to stdout, which it otherwise suppresses when
    // not attached to a TTY. A `--progress-template` argument seems to have
    // been used previously, but it is preventing any progress lines from being
    // emitted. By adding --progress and removing the template, we fall back to
    // yt-dlp's default progress line format, which the parser is designed to
    // handle.
    int pt_index = m_args.indexOf("--progress-template");
    if (pt_index != -1) {
        m_args.removeAt(pt_index); // remove flag
        if (pt_index < m_args.size()) {
            m_args.removeAt(pt_index); // remove value
        }
    }
    if (!m_args.contains("--progress")) {
        m_args.prepend("--progress");
    }

    qDebug() << "[YtDlpWorker] Binary path:" << ytDlpPath;
    qDebug() << "[YtDlpWorker] Working directory:" << workingDirPath;
    qDebug() << "[YtDlpWorker] Number of arguments:" << m_args.size();

    qDebug() << "Starting yt-dlp with path:" << ytDlpPath << "source:" << ytDlpBinary.source << "and arguments:" << m_args;
    qDebug() << "Working directory set to:" << workingDirPath;

    // Log full command for debugging
    QString fullCommand = "\"" + ytDlpPath + "\"";
    for (const QString &arg : m_args) {
        if (arg.contains(' ')) {
            fullCommand += " \"" + arg + "\"";
        } else {
            fullCommand += " " + arg;
        }
    }
    qDebug() << "Full yt-dlp command:" << fullCommand;
    
    // Connect state change signals for diagnostics
    connect(m_process, &QProcess::stateChanged, [this](QProcess::ProcessState state) {
        qDebug() << "[YtDlpWorker] Process state changed to:" << state;
    });
    connect(m_process, &QProcess::errorOccurred, [this](QProcess::ProcessError error) {
        qWarning() << "[YtDlpWorker] Process error occurred:" << error << m_process->errorString();
    });
    
    qDebug() << "[YtDlpWorker] Calling m_process->start()...";
    m_process->start(ytDlpPath, m_args);
    qDebug() << "[YtDlpWorker] start() returned. Process state:" << m_process->state() << "Process ID:" << m_process->processId();
    
    // Check if process started successfully
    if (m_process->state() == QProcess::NotRunning) {
        qWarning() << "[YtDlpWorker] ERROR: Process failed to start immediately!";
        qWarning() << "[YtDlpWorker] Process error:" << m_process->error() << m_process->errorString();
    } else if (m_process->state() == QProcess::Starting) {
        qDebug() << "[YtDlpWorker] Process is starting...";
    } else if (m_process->state() == QProcess::Running) {
        qDebug() << "[YtDlpWorker] Process is running. PID:" << m_process->processId();
    }
}

void YtDlpWorker::killProcess() {
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->kill();
    }
}

void YtDlpWorker::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (m_finishEmitted) {
        return;
    }

    // Process any remaining output. This is crucial for capturing the final file path.
    parseStandardOutput(m_process->readAllStandardOutput());
    // Also process any remaining stderr output
    parseStandardError(m_process->readAllStandardError());

    if (!m_outputBuffer.isEmpty()) {
        handleOutputLine(QString::fromUtf8(m_outputBuffer).trimmed()); // Trim any remaining buffer content
        m_outputBuffer.clear();
    }

    // New logic to find the final filename from collected output lines
    for (int i = m_allOutputLines.size() - 1; i >= 0; --i) {
        const QString& line = m_allOutputLines.at(i);
        // Check if the line looks like a file path and not a yt-dlp message
        if (!line.isEmpty() &&
            (line.contains('/') || line.contains('\\')) && // Must contain path separators
            !line.startsWith("[") && // Exclude yt-dlp messages like [debug], [info], [download]
            !line.contains("url = /s/player/") && // Exclude the incorrect URL capture
            !line.contains("command line:") && // Exclude ffmpeg/aria2c command lines
            !line.contains("Invoking") && // Exclude Invoking downloader lines
            !line.contains("Destination:") && // Exclude [download] Destination: lines
            !line.contains("Writing video metadata as JSON to:") && // Exclude info.json path line
            !line.contains("Writing video thumbnail") && // Exclude thumbnail path line
            !line.contains("Writing video subtitles") && // Exclude subtitle path line
            !line.contains("Merging formats into") && // Exclude merging line
            !line.contains("Embedding subtitles in") && // Exclude embedding subtitles line
            !line.contains("Adding metadata to") // Exclude adding metadata line
            ) {
            m_finalFilename = line;
            qDebug() << "Captured Final Path (after_move:filepath) from collected output:" << m_finalFilename;
            break; // Found the last path, stop searching
        }
    }


    m_finishEmitted = true;
    bool success = (exitStatus == QProcess::NormalExit && exitCode == 0 && !m_finalFilename.isEmpty());
    QString message = success ? "Download completed successfully." : "Download failed.";

    if (!m_finalFilename.isEmpty()) {
        qDebug() << "Final filename captured:" << m_finalFilename;
    } else {
        qWarning() << "Could not determine final filename. Download may have failed or produced no output.";
        message += "\nCould not determine final filename.";
    }

    QVariantMap metadata;
    if (success) {
        // Use the full metadata that was already parsed from info.json during readInfoJsonWithRetry
        if (!m_fullMetadata.isEmpty()) {
            metadata = m_fullMetadata;
            qDebug() << "onProcessFinished: Using cached metadata with" << metadata.size() << "keys. Keys:" << metadata.keys();
            // Ensure m_videoTitle is consistent with what's in metadata
            if (metadata.contains("title") && m_videoTitle.isEmpty()) {
                m_videoTitle = metadata["title"].toString();
            }
            if (metadata.contains("uploader")) {
                qDebug() << "onProcessFinished: uploader from metadata:" << metadata["uploader"].toString();
            } else {
                qWarning() << "onProcessFinished: uploader NOT found in cached metadata!";
            }
        } else {
            qWarning() << "onProcessFinished: No cached metadata available. Sorting rules may not work.";
        }
    }

    if (!success) {
        // Ensure stderr is also read as UTF-8
        QString errorOutput = QString::fromUtf8(m_process->readAllStandardError());
        message += "\n" + errorOutput.left(200);
    }

    // Ensure m_videoTitle is included in the metadata for the finished signal
    if (!m_videoTitle.isEmpty() && !metadata.contains("title")) {
        metadata["title"] = m_videoTitle;
    }
    if (!m_thumbnailPath.isEmpty() && !metadata.contains("thumbnail_path")) {
        metadata["thumbnail_path"] = m_thumbnailPath;
    }

    emit finished(m_id, success, message, m_finalFilename, m_originalDownloadedFilename, metadata);
}


void YtDlpWorker::onProcessError(QProcess::ProcessError error) {
    if (m_finishEmitted) {
        return;
    }

    if (error == QProcess::FailedToStart || error == QProcess::Crashed ||
        error == QProcess::ReadError || error == QProcess::WriteError) {
        m_finishEmitted = true;
        const QString message = QString("Download failed.\nFailed to start yt-dlp process (%1): %2")
                                    .arg(static_cast<int>(error))
                                    .arg(m_process->errorString());
        qWarning() << message;
        emit finished(m_id, false, message, QString(), QString(), QVariantMap());
    }
}

void YtDlpWorker::onReadyReadStandardOutput() {
    QByteArray data = m_process->readAllStandardOutput();
    qDebug() << "[STDOUT] Received" << data.size() << "bytes:" << QString::fromUtf8(data).left(300);
    parseStandardOutput(data);
}

void YtDlpWorker::onReadyReadStandardError() {
    QByteArray data = m_process->readAllStandardError();
    qDebug() << "[STDERR] Received" << data.size() << "bytes:" << QString::fromUtf8(data).left(300);
    parseStandardError(data);
}

void YtDlpWorker::parseStandardOutput(const QByteArray &output) {
    m_outputBuffer.append(output);

    // Find the last complete line ending
    int lastNewline = m_outputBuffer.lastIndexOf('\n');
    int lastCarriageReturn = m_outputBuffer.lastIndexOf('\r');
    int lastDelimiter = qMax(lastNewline, lastCarriageReturn);

    if (lastDelimiter == -1) {
        // No complete line yet, wait for more data
        return;
    }

    // Extract all complete lines
    QByteArray completeData = m_outputBuffer.left(lastDelimiter + 1);
    m_outputBuffer.remove(0, lastDelimiter + 1);

    // Split and process lines, skipping empty parts
    QStringList lines = QString::fromUtf8(completeData).split(QRegularExpression("[\\r\\n]"), Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        handleOutputLine(line.trimmed()); // Ensure each line is trimmed
    }
}

void YtDlpWorker::parseStandardError(const QByteArray &output) {
    m_errorBuffer.append(output);
    qDebug() << "parseStandardError called. Current buffer size:" << m_errorBuffer.size();

    int lastNewline = m_errorBuffer.lastIndexOf('\n');
    int lastCarriageReturn = m_errorBuffer.lastIndexOf('\r');
    int lastDelimiter = qMax(lastNewline, lastCarriageReturn);

    if (lastDelimiter == -1) {
        return;
    }

    QByteArray completeData = m_errorBuffer.left(lastDelimiter + 1);
    m_errorBuffer.remove(0, lastDelimiter + 1);

    QStringList lines = QString::fromUtf8(completeData).split(QRegularExpression("[\\r\\n]"), Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        qDebug() << "Processing stderr line:" << line.trimmed();
        handleOutputLine(line.trimmed());
    }
}

void YtDlpWorker::readInfoJsonWithRetry() {
    qDebug() << "readInfoJsonWithRetry: Attempting to read info.json. Path:" << m_infoJsonPath << "Retry:" << m_infoJsonRetryCount;

    if (m_infoJsonPath.isEmpty()) {
        qDebug() << "readInfoJsonWithRetry: No info.json path set.";
        return;
    }

    QFile jsonFile(m_infoJsonPath);
    if (!jsonFile.exists()) {
        qDebug() << "readInfoJsonWithRetry: info.json file does not exist yet at:" << m_infoJsonPath;
        if (m_infoJsonRetryCount < 5) { // Retry up to 5 times
            m_infoJsonRetryCount++;
            QTimer::singleShot(500, this, &YtDlpWorker::readInfoJsonWithRetry); // Retry after 500ms
            qDebug() << "readInfoJsonWithRetry: Retrying in 500ms. Attempt:" << m_infoJsonRetryCount;
        } else {
            qWarning() << "readInfoJsonWithRetry: Max retries reached for info.json. File not found at:" << m_infoJsonPath;
            m_infoJsonPath.clear(); // Give up
        }
        return;
    }

    if (!jsonFile.open(QIODevice::ReadOnly)) {
        qWarning() << "readInfoJsonWithRetry: Could not open info.json file at:" << m_infoJsonPath << "Error:" << jsonFile.errorString();
        if (m_infoJsonRetryCount < 5) { // Retry up to 5 times
            m_infoJsonRetryCount++;
            QTimer::singleShot(500, this, &YtDlpWorker::readInfoJsonWithRetry); // Retry after 500ms
            qDebug() << "readInfoJsonWithRetry: Retrying in 500ms. Attempt:" << m_infoJsonRetryCount;
        } else {
            qWarning() << "readInfoJsonWithRetry: Max retries reached for info.json. Could not open file at:" << m_infoJsonPath;
            m_infoJsonPath.clear(); // Give up
        }
        return;
    }

    QByteArray jsonData = jsonFile.readAll();
    qDebug() << "readInfoJsonWithRetry: Successfully opened and read info.json. Data size:" << jsonData.size();
    jsonFile.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "readInfoJsonWithRetry: Failed to parse info.json as JSON or it's not an object. Error:" << parseError.errorString();
        if (m_infoJsonRetryCount < 5) { // Retry up to 5 times
            m_infoJsonRetryCount++;
            QTimer::singleShot(500, this, &YtDlpWorker::readInfoJsonWithRetry); // Retry after 500ms
            qDebug() << "readInfoJsonWithRetry: Retrying in 500ms. Attempt:" << m_infoJsonRetryCount;
        } else {
            qWarning() << "readInfoJsonWithRetry: Max retries reached for info.json. Invalid JSON at:" << m_infoJsonPath;
            m_infoJsonPath.clear(); // Give up
        }
        return;
    }

    // If we successfully parsed the file, we don't need to retry anymore.
    m_infoJsonPath.clear();
    m_infoJsonRetryCount = 0;

    QJsonObject obj = doc.object();
    QVariantMap updateData;

    // Store the full metadata for use in onProcessFinished
    m_fullMetadata = obj.toVariantMap();

    if (m_videoTitle.isEmpty() && obj.contains("title") && obj["title"].isString()) {
        m_videoTitle = obj["title"].toString();
        updateData["title"] = m_videoTitle;
        qDebug() << "Extracted title from info.json:" << m_videoTitle;
    }
    
    // Extract thumbnail path if available
    if (obj.contains("thumbnail") && obj["thumbnail"].isString()) {
        // Thumbnail will be downloaded by yt-dlp and we'll get the path
        // from the command line arguments
    }

    if (!updateData.isEmpty()) {
        emit progressUpdated(m_id, updateData);
    }
}

double YtDlpWorker::parseSizeStringToBytes(const QString &sizeString) {
    const QString normalized = sizeString.trimmed().remove('~');
    if (normalized.isEmpty() || normalized.startsWith("Unknown", Qt::CaseInsensitive)) {
        return 0.0;
    }

    static const QRegularExpression re(R"(^([\d\.]+)\s*([KMGTPE]?i?B)(?:/s)?$)", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(normalized);
    if (!match.hasMatch()) {
        return 0.0;
    }

    const double value = match.captured(1).toDouble();
    const QString unit = match.captured(2).toUpper();

    static const QMap<QString, double> multipliers = {
        {"B", 1.0},
        {"KB", 1000.0},
        {"MB", 1000.0 * 1000.0},
        {"GB", 1000.0 * 1000.0 * 1000.0},
        {"TB", 1000.0 * 1000.0 * 1000.0 * 1000.0},
        {"PB", 1000.0 * 1000.0 * 1000.0 * 1000.0 * 1000.0},
        {"KIB", 1024.0},
        {"MIB", 1024.0 * 1024.0},
        {"GIB", 1024.0 * 1024.0 * 1024.0},
        {"TIB", 1024.0 * 1024.0 * 1024.0 * 1024.0},
        {"PIB", 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0}
    };

    const double multiplier = multipliers.value(unit, 0.0);
    const double bytes = value * multiplier;
    qDebug() << "Converted" << value << unit << "to" << bytes << "bytes";
    return bytes;
}

QString YtDlpWorker::formatBytes(double bytes) {
    if (bytes < 0) return "N/A";
    if (bytes == 0) return "0 B";

    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double d_bytes = bytes;

    while (d_bytes >= 1000 && i < units.size() - 1) {
        d_bytes /= 1000;
        i++;
    }

    return QString::number(d_bytes, 'f', (d_bytes < 10 && i > 0) ? 2 : 1) + " " + units[i];
}

void YtDlpWorker::handleOutputLine(const QString &line) {
    const QString normalizedLine = normalizeConsoleLine(line);
    if (normalizedLine.isEmpty()) {
        return;
    }

    m_allOutputLines.append(normalizedLine); // Store all output lines

    emit outputReceived(m_id, normalizedLine);
    qDebug().noquote() << "yt-dlp (processed line):" << normalizedLine;

    static QRegularExpression thumbnailRegex("\\[ThumbnailsConvertor\\] Converting thumbnail \"([^\"]+)\" to jpg");
    QRegularExpressionMatch thumbnailMatch = thumbnailRegex.match(normalizedLine);
    if (thumbnailMatch.hasMatch()) {
        QString webpPath = thumbnailMatch.captured(1);
        m_thumbnailPath = QDir::toNativeSeparators(webpPath.replace(".webp", ".jpg"));
        QVariantMap updateData;
        updateData["thumbnail_path"] = m_thumbnailPath;
        qDebug() << "[LOG] YtDlpWorker: Found converted thumbnail path for" << m_id << ":" << m_thumbnailPath;
        emit progressUpdated(m_id, updateData);
    }

    // 1. Capture the info.json file path and store it, then initiate retry mechanism
    static QRegularExpression infoJsonRegex(R"(\[info\] Writing video metadata as JSON to:\s*(.*\.info\.json))");
    QRegularExpressionMatch infoJsonMatch = infoJsonRegex.match(normalizedLine);
    if (infoJsonMatch.hasMatch()) {
        m_infoJsonPath = infoJsonMatch.captured(1).trimmed();
        // Normalize path separators if necessary, although QFile should handle it
        m_infoJsonPath = QDir::toNativeSeparators(m_infoJsonPath);
        qDebug() << "Detected info.json path and initiating retry mechanism:" << m_infoJsonPath;
        m_infoJsonRetryCount = 0; // Reset retry count for a new file
        readInfoJsonWithRetry(); // Start the retry mechanism
    }

    if (!parseYtDlpProgressLine(normalizedLine)) {
        parseAria2ProgressLine(normalizedLine);
    }


    if (normalizedLine.startsWith("[download] Destination: ")) {
        QString filename = normalizedLine.mid(24).trimmed();
        if (m_originalDownloadedFilename.isEmpty()) {
            m_originalDownloadedFilename = filename;
        }
    }
}

bool YtDlpWorker::parseYtDlpProgressLine(const QString &line) {
    QString normalized = line.trimmed();
    
    if (!normalized.startsWith("[download]")) {
        qDebug() << "parseYtDlpProgressLine: Line does not start with [download]:" << line;
        return false;
    }

    static const QRegularExpression progressRegex(
        R"(^\[download\]\s+([\d\.]+)%\s+of\s+(?:~\s*)?((?:Unknown total size)|(?:[\d\.]+\s*[KMGTPE]?i?B))(?:\s+at\s+((?:Unknown B/s)|(?:Unknown)|(?:[\d\.]+\s*[KMGTPE]?i?B/s)|(?:[\d\.]+\s*[KMGTPE]?B/s)))?(?:\s+ETA\s+([^\s]+))?.*$)");
    static const QRegularExpression completedRegex(
        R"(^\[download\]\s+100(?:\.0+)?%\s+of\s+(?:~\s*)?((?:Unknown total size)|(?:[\d\.]+\s*[KMGTPE]?i?B))(?:\s+in\s+([^\s]+)(?:\s+at\s+((?:Unknown B/s)|(?:Unknown)|(?:[\d\.]+\s*[KMGTPE]?i?B/s)|(?:[\d\.]+\s*[KMGTPE]?B/s)))?)?.*$)");

    qDebug() << "parseYtDlpProgressLine: Trying to match progress line:" << normalized;

    QRegularExpressionMatch match = progressRegex.match(normalized);
    bool matchedCompletedFormat = false;
    if (!match.hasMatch()) {
        match = completedRegex.match(normalized);
        matchedCompletedFormat = match.hasMatch();
    }
    if (!match.hasMatch()) {
        qDebug() << "parseYtDlpProgressLine: Failed to match progress regex. Line:" << normalized;
        return false;
    }
    
    qDebug() << "parseYtDlpProgressLine: Successfully parsed progress. Percentage:" << match.captured(1);

    QVariantMap progressData;
    double percentage = matchedCompletedFormat ? 100.0 : match.captured(1).toDouble();
    const QString totalString = matchedCompletedFormat ? match.captured(1) : match.captured(2);
    const QString speedString = matchedCompletedFormat ? match.captured(3) : match.captured(3);
    const QString etaString = matchedCompletedFormat ? QStringLiteral("0:00") : match.captured(4);

    const double totalBytes = parseSizeStringToBytes(totalString);
    const double downloadedBytes = totalBytes > 0.0 ? (totalBytes * (percentage / 100.0)) : 0.0;
    const double speedBytes = parseSizeStringToBytes(speedString);

    progressData["progress"] = percentage;
    progressData["downloaded_size"] = downloadedBytes > 0.0 ? formatBytes(downloadedBytes) : QString("N/A");
    progressData["total_size"] = totalBytes > 0.0 ? formatBytes(totalBytes) : totalString;
    progressData["speed"] = speedBytes > 0.0 ? formatBytes(speedBytes) + "/s" : (speedString.isEmpty() ? QString("Unknown") : speedString);
    progressData["speed_bytes"] = speedBytes;
    progressData["eta"] = etaString.isEmpty() ? QString("Unknown") : etaString;
    if (!m_videoTitle.isEmpty()) {
        progressData["title"] = m_videoTitle;
    }
    if (!m_thumbnailPath.isEmpty()) {
        progressData["thumbnail_path"] = m_thumbnailPath;
    }

    emit progressUpdated(m_id, progressData);
    qDebug() << "yt-dlp: Progress match found (native/template).";
    return true;
}

bool YtDlpWorker::parseAria2ProgressLine(const QString &line) {
    if (!line.startsWith("[#")) {
        return false;
    }

    static const QRegularExpression ariaRegex(
        R"(\[#\w+\s+([\d\.]+\s*[KMGTPE]?i?B)/([\d\.]+\s*[KMGTPE]?i?B)\(([\d\.]+)%\)(?:\s+CN:\d+)?(?:\s+DL:((?:[\d\.]+\s*[KMGTPE]?i?B(?:/s)?)|(?:0B/s)))?(?:\s+ETA:([\d\w:]+))?\])");
    const QRegularExpressionMatch match = ariaRegex.match(line);
    if (!match.hasMatch()) {
        return false;
    }

    QVariantMap progressData;
    const double downloadedBytes = parseSizeStringToBytes(match.captured(1));
    const double totalBytes = parseSizeStringToBytes(match.captured(2));
    const double speedBytes = parseSizeStringToBytes(match.captured(4));

    progressData["progress"] = match.captured(3).toDouble();
    progressData["downloaded_size"] = formatBytes(downloadedBytes);
    progressData["total_size"] = formatBytes(totalBytes);
    progressData["speed"] = speedBytes > 0.0 ? formatBytes(speedBytes) + "/s" : QString("0 B/s");
    progressData["speed_bytes"] = speedBytes;
    progressData["eta"] = match.captured(5).isEmpty() ? QString("N/A") : match.captured(5);
    if (!m_videoTitle.isEmpty()) {
        progressData["title"] = m_videoTitle;
    }
    if (!m_thumbnailPath.isEmpty()) {
        progressData["thumbnail_path"] = m_thumbnailPath;
    }

    emit progressUpdated(m_id, progressData);
    qDebug() << "yt-dlp: Progress match found (aria2c raw).";
    return true;
}

QString YtDlpWorker::normalizeConsoleLine(const QString &line) const {
    QString normalized = line;
    static const QRegularExpression ansiRegex(R"(\x1B\[[0-9;]*[A-Za-z])");
    normalized.remove(ansiRegex);
    return normalized.trimmed();
}
