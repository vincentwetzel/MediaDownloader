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
#include <QTimer>
#include <QProcess>
#include <QVariantList>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>

static void calculateUnifiedProgress(QObject* worker, const QVariantMap& fullMetadata, 
                                     double& percentage, double& downloadedBytes, 
                                     double& totalBytes) {
    double overallTotal = 0;
    if (fullMetadata.contains("requested_downloads")) {
        QVariantList reqs = fullMetadata["requested_downloads"].toList();
        for (const QVariant &req : reqs) {
            QVariantMap reqMap = req.toMap();
            if (reqMap.contains("filesize") && reqMap["filesize"].toDouble() > 0) {
                overallTotal += reqMap["filesize"].toDouble();
            } else if (reqMap.contains("filesize_approx") && reqMap["filesize_approx"].toDouble() > 0) {
                overallTotal += reqMap["filesize_approx"].toDouble();
            }
        }
    }

    double lastDownloaded = worker->property("lastDownloadedBytes").toDouble();
    double lastTotal = worker->property("lastTotalBytes").toDouble();
    double accumulated = worker->property("accumulatedBytes").toDouble();

    // Detect new stream by checking if downloaded bytes dropped significantly
    if (downloadedBytes < lastDownloaded && lastDownloaded > 0 && (lastDownloaded - downloadedBytes) > lastTotal * 0.1) {
        accumulated += lastTotal > 0 ? lastTotal : lastDownloaded;
        worker->setProperty("accumulatedBytes", accumulated);
        lastTotal = totalBytes; // Reset lastTotal for the new stream
    }

    worker->setProperty("lastDownloadedBytes", downloadedBytes);
    if (totalBytes > 0) {
        worker->setProperty("lastTotalBytes", totalBytes);
    }

    double displayDownloaded = accumulated + downloadedBytes;
    double displayTotal = overallTotal > 0 ? overallTotal : (accumulated + totalBytes);

    // Fallback max size to prevent total from shrinking
    double maxTotal = worker->property("maxTotalBytes").toDouble();
    if (displayTotal > maxTotal) {
        maxTotal = displayTotal;
        worker->setProperty("maxTotalBytes", maxTotal);
    }
    if (displayTotal < maxTotal) {
        displayTotal = maxTotal;
    }

    if (displayTotal > 0) {
        percentage = (displayDownloaded / displayTotal) * 100.0;
        if (percentage > 100.0) percentage = 100.0;
        if (percentage < 0.0) percentage = 0.0;
    }

    downloadedBytes = displayDownloaded;
    totalBytes = displayTotal;
}

YtDlpWorker::YtDlpWorker(const QString &id, const QStringList &args, ConfigManager *configManager, QObject *parent)
    : QObject(parent), m_id(id), m_args(args), m_configManager(configManager), m_process(nullptr), m_finishEmitted(false), m_errorEmitted(false), m_videoTitle(QString()),
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


    m_finishEmitted = true;
    bool success = (exitStatus == QProcess::NormalExit && exitCode == 0 && !m_finalFilename.isEmpty());
    QString message = success ? "Download completed successfully." : "Download failed.";

    if (!m_finalFilename.isEmpty()) {
        qDebug() << "Final filename captured:" << m_finalFilename;
    } else {
        qWarning() << "Could not determine final filename. Download may have failed or produced no output.";
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            message += "\nCould not determine final filename.";
        }
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

        // CRITICAL FIX: Emit 100% progress update before finished signal to ensure
        // the UI progress bar reaches 100% and turns green, not stuck at <100%
        QVariantMap finalProgressData;
        finalProgressData["progress"] = 100;
        finalProgressData["status"] = "Complete";
        if (!m_videoTitle.isEmpty()) {
            finalProgressData["title"] = m_videoTitle;
        }
        if (!m_thumbnailPath.isEmpty()) {
            finalProgressData["thumbnail_path"] = m_thumbnailPath;
        }
        emit progressUpdated(m_id, finalProgressData);
    }

    if (!success) {
        if (!m_errorLines.isEmpty()) {
            message += "\n" + m_errorLines.join("\n").left(200);
        } else {
            // Fallback: Ensure stderr is also read as UTF-8
            QString errorOutput = QString::fromUtf8(m_process->readAllStandardError());
            if (!errorOutput.isEmpty()) {
                message += "\n" + errorOutput.left(200);
            }
        }
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
    
    // Extract thumbnail path if available from the info.json
    if (m_thumbnailPath.isEmpty() && obj.contains("thumbnails") && obj["thumbnails"].isArray()) {
        QJsonArray thumbnails = obj["thumbnails"].toArray();
        // yt-dlp adds a "filepath" key to the thumbnail entry it downloaded.
        for (const QJsonValue &thumbValue : thumbnails) {
            QJsonObject thumbObj = thumbValue.toObject();
            if (thumbObj.contains("filepath") && thumbObj["filepath"].isString()) {
                m_thumbnailPath = QDir::toNativeSeparators(thumbObj["filepath"].toString());
                updateData["thumbnail_path"] = m_thumbnailPath;
                qDebug() << "Extracted thumbnail path from info.json:" << m_thumbnailPath;
                break; // Found it
            }
        }
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
    // qDebug().noquote() << "yt-dlp (processed line):" << normalizedLine;

    if (normalizedLine.startsWith("LZY_FINAL_PATH:")) {
        m_finalFilename = normalizedLine.mid(15).trimmed(); // 15 is length of "LZY_FINAL_PATH:"
        qDebug() << "Captured precise Final Path:" << m_finalFilename;
        return; // No need to process this line further
    }

    // Parse ERROR: lines from stderr for specific error types
    if (normalizedLine.startsWith("ERROR:")) {
        m_errorLines.append(normalizedLine);
        
        // Check for private video error
        if (normalizedLine.contains("private", Qt::CaseInsensitive) || 
            normalizedLine.contains("This video is private", Qt::CaseInsensitive)) {
            if (!m_errorEmitted) {
                m_errorEmitted = true;
                emit ytDlpErrorDetected(m_id, "private", 
                    "This video is private and cannot be downloaded.", 
                    normalizedLine);
            }
        }
        // Check for unavailable video error
        else if (normalizedLine.contains("unavailable", Qt::CaseInsensitive) ||
                 normalizedLine.contains("Video is unavailable", Qt::CaseInsensitive) ||
                 normalizedLine.contains("This video is no longer available", Qt::CaseInsensitive) ||
                 normalizedLine.contains("does not exist", Qt::CaseInsensitive)) {
            if (!m_errorEmitted) {
                m_errorEmitted = true;
                emit ytDlpErrorDetected(m_id, "unavailable",
                    "This video is unavailable or has been removed.",
                    normalizedLine);
            }
        }
        // Check for geo-restriction error
        else if (normalizedLine.contains("geo", Qt::CaseInsensitive) && 
                 (normalizedLine.contains("restrict", Qt::CaseInsensitive) ||
                  normalizedLine.contains("unavailable in your country", Qt::CaseInsensitive))) {
            if (!m_errorEmitted) {
                m_errorEmitted = true;
                emit ytDlpErrorDetected(m_id, "geo_restricted",
                    "This video is not available in your region.",
                    normalizedLine);
            }
        }
        // Check for members-only error
        else if (normalizedLine.contains("members", Qt::CaseInsensitive) &&
                 normalizedLine.contains("only", Qt::CaseInsensitive)) {
            if (!m_errorEmitted) {
                m_errorEmitted = true;
                emit ytDlpErrorDetected(m_id, "members_only",
                    "This video is exclusive to channel members.",
                    normalizedLine);
            }
        }
        // Check for age-restriction error
        else if (normalizedLine.contains("age", Qt::CaseInsensitive) &&
                 (normalizedLine.contains("restrict", Qt::CaseInsensitive) ||
                  normalizedLine.contains("verify your age", Qt::CaseInsensitive) ||
                  normalizedLine.contains("confirm your age", Qt::CaseInsensitive))) {
            if (!m_errorEmitted) {
                m_errorEmitted = true;
                emit ytDlpErrorDetected(m_id, "age_restricted",
                    "This video requires age verification. Try enabling cookies from your browser.",
                    normalizedLine);
            }
        }
        // Check for content removed/unavailable (e.g., deleted tweet)
        else if (normalizedLine.contains("Requested tweet is unavailable", Qt::CaseInsensitive) ||
                 normalizedLine.contains("This content is no longer available", Qt::CaseInsensitive) ||
                 normalizedLine.contains("The requested content was removed", Qt::CaseInsensitive) ||
                 normalizedLine.contains("Suspended", Qt::CaseInsensitive)) {
            if (!m_errorEmitted) {
                m_errorEmitted = true;
                emit ytDlpErrorDetected(m_id, "content_removed",
                    "The requested content is unavailable or has been removed by the uploader.",
                    normalizedLine);
            }
        }
        // Check for scheduled livestream/premiere
        else if (normalizedLine.contains("Premieres in", Qt::CaseInsensitive) ||
                 normalizedLine.contains("live event will begin", Qt::CaseInsensitive) ||
                 normalizedLine.contains("is upcoming", Qt::CaseInsensitive)) {
            if (!m_errorEmitted) {
                m_errorEmitted = true;
                emit ytDlpErrorDetected(m_id, "scheduled_livestream",
                    "This video is a scheduled livestream or premiere that has not started yet.\n\nWould you like to wait for the video to begin and download it automatically?",
                    normalizedLine);
            }
        }
    }

    // Parse post-processing operations (so the UI doesn't look "stuck" after the final small stream)
    if (normalizedLine.startsWith("[Merger]")) {
        emit progressUpdated(m_id, {{"progress", 100.0}, {"status", "Merging segments with ffmpeg..."}});
    } else if (normalizedLine.startsWith("[ExtractAudio]")) {
        emit progressUpdated(m_id, {{"progress", 100.0}, {"status", "Extracting audio..."}});
    } else if (normalizedLine.startsWith("[VideoConvertor]")) {
        emit progressUpdated(m_id, {{"progress", 100.0}, {"status", "Converting video format..."}});
    } else if (normalizedLine.startsWith("[Metadata]")) {
        emit progressUpdated(m_id, {{"progress", 100.0}, {"status", "Applying metadata..."}});
    } else if (normalizedLine.startsWith("[FixupM3u8]")) {
        emit progressUpdated(m_id, {{"progress", 100.0}, {"status", "Fixing stream timestamps..."}});
    }

    // If we are waiting for a scheduled livestream, fetch metadata in the background so the UI 
    // can show the title and thumbnail instead of just the URL during the long wait.
    if (normalizedLine.startsWith("[wait]", Qt::CaseInsensitive) || normalizedLine.startsWith("[download] Waiting for video", Qt::CaseInsensitive)) {
        if (m_videoTitle.isEmpty() && !property("fetchingPreWaitMetadata").toBool()) {
            setProperty("fetchingPreWaitMetadata", true);
            
            QString url;
            for (const QString &arg : m_args) {
                if (arg.startsWith("http")) { url = arg; break; }
            }
            if (url.isEmpty()) {
                for (const QString &arg : m_args) {
                    // Grab the first non-flag argument as a fallback URL
                    if (!arg.startsWith("-")) { url = arg; break; }
                }
            }
            
            // Fast-path: YouTube oEmbed API avoids spawning a yt-dlp process and bypassing 
            // the ExtractorError completely for upcoming livestreams.
            if (url.contains("youtube.com") || url.contains("youtu.be")) {
                qDebug() << "[YtDlpWorker] Detected [wait] state. Using YouTube oEmbed API for pre-wait metadata...";
                QNetworkAccessManager *manager = new QNetworkAccessManager(this);
                QUrl oembedUrl("https://www.youtube.com/oembed?url=" + url + "&format=json");
                QNetworkRequest request(oembedUrl);
                QNetworkReply *reply = manager->get(request);
                connect(reply, &QNetworkReply::finished, this, [this, reply, manager]() {
                    if (reply->error() == QNetworkReply::NoError) {
                        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                        if (doc.isObject()) {
                            QJsonObject obj = doc.object();
                            m_videoTitle = obj.value("title").toString();
                            QString thumbUrl = obj.value("thumbnail_url").toString();
                            
                            qDebug() << "[YtDlpWorker] oEmbed title:" << m_videoTitle << "thumb:" << thumbUrl;
                            
                            QVariantMap progressData;
                            progressData["progress"] = -1;
                            progressData["status"] = "Waiting for livestream to start...";
                            progressData["title"] = m_videoTitle;
                            if (!m_thumbnailPath.isEmpty()) {
                                progressData["thumbnail_path"] = m_thumbnailPath;
                            }
                            emit progressUpdated(m_id, progressData);

                            if (!thumbUrl.isEmpty() && m_thumbnailPath.isEmpty()) {
                                QNetworkRequest thumbReq((QUrl(thumbUrl)));
                                QNetworkReply *thumbReply = manager->get(thumbReq);
                                connect(thumbReply, &QNetworkReply::finished, this, [this, thumbReply, manager]() {
                                    if (thumbReply->error() == QNetworkReply::NoError) {
                                        QString tempDir = m_configManager->get("Paths", "temporary_downloads_directory").toString();
                                        QString newThumbPath = QDir(tempDir).filePath(m_id + "_wait_thumbnail.jpg");
                                        QFile file(newThumbPath);
                                        if (file.open(QIODevice::WriteOnly)) {
                                            file.write(thumbReply->readAll());
                                            file.close();
                                            m_thumbnailPath = newThumbPath;
                                            qDebug() << "[YtDlpWorker] oEmbed thumbnail downloaded to:" << m_thumbnailPath;
                                            
                                            QVariantMap pd;
                                            pd["progress"] = -1;
                                            pd["status"] = "Waiting for livestream to start...";
                                            pd["title"] = m_videoTitle;
                                            pd["thumbnail_path"] = m_thumbnailPath;
                                            emit progressUpdated(m_id, pd);
                                        }
                                    }
                                    thumbReply->deleteLater();
                                    manager->deleteLater();
                                });
                            } else {
                                manager->deleteLater();
                            }
                        } else {
                            manager->deleteLater();
                        }
                    } else {
                        qWarning() << "[YtDlpWorker] oEmbed API failed:" << reply->errorString();
                        manager->deleteLater();
                    }
                    reply->deleteLater();
                });
                return;
            }

            // Fallback for non-YouTube sites
            qDebug() << "[YtDlpWorker] Detected [wait] state. Fetching pre-wait metadata via yt-dlp in background...";
            QProcess *fetchProcess = new QProcess(this);
            ProcessUtils::setProcessEnvironment(*fetchProcess);
            
            QString ytDlpPath = ProcessUtils::findBinary("yt-dlp", m_configManager).path;
            QStringList fetchArgs;
            
            fetchArgs << "--dump-single-json" << "--flat-playlist" << "--ignore-errors" << url;
            int cookieIdx = m_args.indexOf("--cookies-from-browser");
            if (cookieIdx != -1 && cookieIdx + 1 < m_args.size()) {
                fetchArgs << "--cookies-from-browser" << m_args[cookieIdx + 1];
            }

            qDebug() << "[YtDlpWorker] Pre-wait fetch command:" << ytDlpPath << fetchArgs;
            connect(fetchProcess, &QProcess::finished, this, [this, fetchProcess](int exitCode, QProcess::ExitStatus) {
                    QByteArray jsonData = fetchProcess->readAllStandardOutput();
                    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
                    if (doc.isObject()) {
                        QJsonObject obj = doc.object();
                        m_fullMetadata = obj.toVariantMap();
                        if (obj.contains("title") && obj["title"].isString()) {
                            m_videoTitle = obj["title"].toString();
                            qDebug() << "[YtDlpWorker] Pre-wait title fetched:" << m_videoTitle;
                            
                            // Immediately update the UI with the title before we wait for the thumbnail
                            QVariantMap progressData;
                            progressData["progress"] = -1;
                            progressData["status"] = "Waiting for livestream to start...";
                            progressData["title"] = m_videoTitle;
                            if (!m_thumbnailPath.isEmpty()) {
                                progressData["thumbnail_path"] = m_thumbnailPath;
                            }
                            emit progressUpdated(m_id, progressData);
                        }
                        
                        QString thumbUrl;
                        if (obj.contains("thumbnails") && obj["thumbnails"].isArray()) {
                            QJsonArray thumbs = obj["thumbnails"].toArray();
                            if (!thumbs.isEmpty()) {
                                thumbUrl = thumbs.last().toObject().value("url").toString();
                            }
                        } else if (obj.contains("thumbnail")) {
                            thumbUrl = obj.value("thumbnail").toString();
                        }

                        if (!thumbUrl.isEmpty()) {
                            qDebug() << "[YtDlpWorker] Pre-wait thumbnail URL found:" << thumbUrl;
                            QNetworkAccessManager *manager = new QNetworkAccessManager(this);
                            QNetworkRequest request((QUrl(thumbUrl)));
                            QNetworkReply *reply = manager->get(request);
                            connect(reply, &QNetworkReply::finished, this, [this, reply, manager]() {
                                if (reply->error() == QNetworkReply::NoError) {
                                    QString tempDir = m_configManager->get("Paths", "temporary_downloads_directory").toString();
                                    QString newThumbPath = QDir(tempDir).filePath(m_id + "_wait_thumbnail.jpg");
                                    QFile file(newThumbPath);
                                    if (file.open(QIODevice::WriteOnly)) {
                                        file.write(reply->readAll());
                                        file.close();
                                        m_thumbnailPath = newThumbPath;
                                        qDebug() << "[YtDlpWorker] Pre-wait thumbnail downloaded to:" << m_thumbnailPath;
                                        
                                        // Update the UI again now that we have the image
                                        QVariantMap progressData;
                                        progressData["progress"] = -1;
                                        progressData["status"] = "Waiting for livestream to start...";
                                        progressData["title"] = m_videoTitle;
                                        progressData["thumbnail_path"] = m_thumbnailPath;
                                        emit progressUpdated(m_id, progressData);
                                    }
                                } else {
                                    qWarning() << "[YtDlpWorker] Failed to download pre-wait thumbnail:" << reply->errorString();
                                }
                                reply->deleteLater();
                                manager->deleteLater();
                            });
                        }
                    } else {
                        qWarning() << "[YtDlpWorker] Pre-wait metadata fetch failed or returned invalid JSON. Exit code:" << exitCode;
                        qWarning() << "[YtDlpWorker] Stderr:" << fetchProcess->readAllStandardError();
                    }
                fetchProcess->deleteLater();
            });
            
            fetchProcess->start(ytDlpPath, fetchArgs);
        }
    }

    if (normalizedLine.startsWith("[wait] Remaining time until next attempt:")) {
        const QString prefix = "[wait] Remaining time until next attempt: ";
        QString time = normalizedLine.mid(prefix.length()).trimmed();
        QVariantMap progressData;
        progressData["progress"] = -1; // Indeterminate state
        progressData["status"] = QString("Next check in %1").arg(time);
        if (!m_videoTitle.isEmpty()) {
            progressData["title"] = m_videoTitle;
        }
        if (!m_thumbnailPath.isEmpty()) {
            progressData["thumbnail_path"] = m_thumbnailPath;
        }
        emit progressUpdated(m_id, progressData);
        return; // This line is handled, no further processing needed.
    }

    if (normalizedLine.startsWith("[download] Waiting for video") || normalizedLine.startsWith("[Wait]")) {
        QVariantMap progressData;
        progressData["progress"] = -1; // Indeterminate state
        progressData["status"] = "Waiting for livestream to start...";
        if (!m_videoTitle.isEmpty()) {
            progressData["title"] = m_videoTitle;
        }
        if (!m_thumbnailPath.isEmpty()) {
            progressData["thumbnail_path"] = m_thumbnailPath;
        }
        emit progressUpdated(m_id, progressData);
        return;
    }

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
        // qDebug() << "parseYtDlpProgressLine: Line does not start with [download]:" << line;
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

    double totalBytes = parseSizeStringToBytes(totalString);
    double downloadedBytes = totalBytes > 0.0 ? (totalBytes * (percentage / 100.0)) : 0.0;
    const double speedBytes = parseSizeStringToBytes(speedString);

    calculateUnifiedProgress(this, m_fullMetadata, percentage, downloadedBytes, totalBytes);

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
    double downloadedBytes = parseSizeStringToBytes(match.captured(1));
    double totalBytes = parseSizeStringToBytes(match.captured(2));
    const double speedBytes = parseSizeStringToBytes(match.captured(4));
    double percentage = match.captured(3).toDouble();

    calculateUnifiedProgress(this, m_fullMetadata, percentage, downloadedBytes, totalBytes);

    progressData["progress"] = percentage;
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
