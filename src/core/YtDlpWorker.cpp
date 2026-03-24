#include "YtDlpWorker.h"
#include <QDebug>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QProcessEnvironment>
#include <cmath> // For pow

YtDlpWorker::YtDlpWorker(const QString &id, const QStringList &args, QObject *parent)
    : QObject(parent), m_id(id), m_args(args), m_process(nullptr), m_finishEmitted(false), m_videoTitle(QString()),
      m_thumbnailPath(QString()), m_infoJsonPath(QString()), m_infoJsonRetryCount(0) {

    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &YtDlpWorker::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &YtDlpWorker::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &YtDlpWorker::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &YtDlpWorker::onReadyReadStandardError);
}

void YtDlpWorker::start() {
    const QString ytDlpPath = resolveExecutablePath("yt-dlp.exe");
    if (ytDlpPath.isEmpty()) {
        const QString message = QString("Download failed.\nBundled yt-dlp.exe was not found in '%1' or '%1/bin'.")
                                    .arg(QCoreApplication::applicationDirPath());
        qWarning() << message;
        if (!m_finishEmitted) {
            m_finishEmitted = true;
            emit finished(m_id, false, message, QString(), QString(), QVariantMap());
        }
        return;
    }

    const QString workingDirPath = QFileInfo(ytDlpPath).absolutePath();
    m_process->setWorkingDirectory(workingDirPath);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONUTF8", "1");
    env.insert("PYTHONIOENCODING", "utf-8");
    m_process->setProcessEnvironment(env);

    qDebug() << "Starting yt-dlp with path:" << ytDlpPath << "and arguments:" << m_args;
    qDebug() << "Working directory set to:" << workingDirPath;
    m_process->start(ytDlpPath, m_args);
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
        // Final attempt to extract title from info.json if not already found
        if ((m_videoTitle.isEmpty()) && !m_infoJsonPath.isEmpty()) {
            qDebug() << "onProcessFinished: Attempting final title/thumbnail extraction.";
            readInfoJsonWithRetry(); // This will try to read it one last time
        }

        // The metadata map is populated from the info.json file if available
        // This block is for populating metadata for the finished signal, not for UI updates during download
        if (!m_infoJsonPath.isEmpty()) {
            QFile jsonFile(m_infoJsonPath);
            if (jsonFile.exists() && jsonFile.open(QIODevice::ReadOnly)) {
                QByteArray jsonData = jsonFile.readAll();
                QJsonDocument doc = QJsonDocument::fromJson(jsonData);
                if (doc.isObject()) {
                    metadata = doc.object().toVariantMap();
                    // Ensure m_videoTitle is consistent with what's in metadata
                    if (metadata.contains("title")) {
                        m_videoTitle = metadata["title"].toString();
                    }
                }
                jsonFile.close();
            }
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
    parseStandardOutput(m_process->readAllStandardOutput());
}

void YtDlpWorker::onReadyReadStandardError() {
    parseStandardError(m_process->readAllStandardError());
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

    if (m_videoTitle.isEmpty() && obj.contains("title") && obj["title"].isString()) {
        m_videoTitle = obj["title"].toString();
        updateData["title"] = m_videoTitle;
        qDebug() << "Extracted title from info.json:" << m_videoTitle;
    }

    if (!updateData.isEmpty()) {
        emit progressUpdated(m_id, updateData);
    }
}

double YtDlpWorker::parseSizeStringToBytes(const QString &sizeString) {
    QRegularExpression re(R"(^([\d\.]+)([KMGT]?iB)(?:/s)?$)");
    QRegularExpressionMatch match = re.match(sizeString);

    if (match.hasMatch()) {
        double value = match.captured(1).toDouble();
        QString unit = match.captured(2);

        double ret = 0.0;
        if (unit == "KiB")
            ret = value * 1024;
        if (unit == "MiB")
            ret = value * 1024 * 1024;
        if (unit == "GiB")
         ret = value * 1024 * 1024 * 1024;
        if (unit == "TiB")
            ret = value * 1024 * 1024 * 1024 * 1024;
        qDebug() << "Converted " << value << unit << " to " << ret << " bytes";
        return ret;
    }
    return 0.0; // Return 0 for unparseable strings
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
    if (line.isEmpty()) {
        return;
    }

    m_allOutputLines.append(line); // Store all output lines

    emit outputReceived(m_id, line);
    qDebug().noquote() << "yt-dlp (processed line):" << line;

    static QRegularExpression thumbnailRegex("\\[ThumbnailsConvertor\\] Converting thumbnail \"([^\"]+)\" to jpg");
    QRegularExpressionMatch thumbnailMatch = thumbnailRegex.match(line);
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
    QRegularExpressionMatch infoJsonMatch = infoJsonRegex.match(line);
    if (infoJsonMatch.hasMatch()) {
        m_infoJsonPath = infoJsonMatch.captured(1).trimmed();
        // Normalize path separators if necessary, although QFile should handle it
        m_infoJsonPath = QDir::toNativeSeparators(m_infoJsonPath);
        qDebug() << "Detected info.json path and initiating retry mechanism:" << m_infoJsonPath;
        m_infoJsonRetryCount = 0; // Reset retry count for a new file
        readInfoJsonWithRetry(); // Start the retry mechanism
    }

    // Try to parse yt-dlp's --progress-template output first
    if (line.startsWith("download:")) {
        static QRegularExpression re(
            R"(download:\[download\]\s+([\d\.]+%)\s+of\s+([\d\.]+[a-zA-Z]+)\s+at\s+([\d\.]+[a-zA-Z/s]+)\s+ETA\s+([\d:]+))"
        );
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
            QVariantMap progressData;
            QString percentStr = match.captured(1);
            percentStr.remove('%');
            double percentage = percentStr.toDouble();
            double totalBytes = parseSizeStringToBytes(match.captured(2));

            progressData["progress"] = percentage;
            progressData["downloaded_size"] = formatBytes(totalBytes * (percentage / 100.0));
            progressData["total_size"] = formatBytes(totalBytes);
            progressData["speed"] = formatBytes(parseSizeStringToBytes(match.captured(3))) + "/s";
            progressData["eta"] = match.captured(4);
            if (!m_videoTitle.isEmpty()) {
                progressData["title"] = m_videoTitle;
            }
            if (!m_thumbnailPath.isEmpty()) {
                progressData["thumbnail_path"] = m_thumbnailPath;
            }
            emit progressUpdated(m_id, progressData);
            qDebug() << "yt-dlp: Progress match found (yt-dlp template)!";
        } else {
            qDebug() << "yt-dlp: Progress line did not match yt-dlp template regex:" << line;
        }
    }
    // If not yt-dlp's template, try to parse aria2c's raw output
    else if (line.startsWith("[#") && (line.contains("MiB/") || line.contains("KiB/"))) { // Heuristic to quickly identify aria2c progress lines
        static QRegularExpression ariaRe(
            R"(\[#\w+\s+([\d\.]+[KMGT]?iB)/([\d\.]+[KMGT]?iB)\(([\d\.]+)%\)(?:\s+CN:\d+)?(?:\s+DL:([\d\.]+[KMGT]?iB(?:/s)?))?(?:\s+ETA:([\d\w:]+))?\])"
        );
        QRegularExpressionMatch ariaMatch = ariaRe.match(line);
        if (ariaMatch.hasMatch()) {
            QVariantMap progressData;
            double downloadedBytes = parseSizeStringToBytes(ariaMatch.captured(1));
            double totalBytes = parseSizeStringToBytes(ariaMatch.captured(2));

            progressData["progress"] = ariaMatch.captured(3).toDouble(); // Percentage
            progressData["downloaded_size"] = formatBytes(downloadedBytes); // Formatted downloaded size
            progressData["total_size"] = formatBytes(totalBytes);         // Formatted total size

            QString speedString = ariaMatch.captured(4);
            double speedBytes = parseSizeStringToBytes(speedString);
            progressData["speed"] = formatBytes(speedBytes) + "/s"; // Formatted speed

            QString eta = ariaMatch.captured(5);
            progressData["eta"] = eta.isEmpty() ? "N/A" : eta;                // ETA (can be empty)
            if (!m_videoTitle.isEmpty()) {
                progressData["title"] = m_videoTitle;
            }
            if (!m_thumbnailPath.isEmpty()) {
                progressData["thumbnail_path"] = m_thumbnailPath;
            }
            emit progressUpdated(m_id, progressData);
            qDebug() << "yt-dlp: Progress match found (aria2c raw)!";
        } else {
            qDebug() << "yt-dlp: Progress line did not match aria2c raw regex:" << line;
        }
    }


    if (line.startsWith("[download] Destination: ")) {
        QString filename = line.mid(24).trimmed();
        if (m_originalDownloadedFilename.isEmpty()) {
            m_originalDownloadedFilename = filename;
        }
    }
}


QString YtDlpWorker::resolveExecutablePath(const QString &name) const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(name),
        QDir(QDir(appDir).filePath("bin")).filePath(name)
    };

    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }

    return QString();
}
