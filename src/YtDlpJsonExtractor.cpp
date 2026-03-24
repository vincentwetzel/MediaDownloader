#include "YtDlpJsonExtractor.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

YtDlpJsonExtractor::YtDlpJsonExtractor(QObject *parent)
    : QObject(parent), m_process(new QProcess(this))
{
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            emit extractionFailed("Failed to start yt-dlp. Is the executable missing?");
        }
    });

    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus == QProcess::CrashExit || exitCode != 0) {
            emit extractionFailed("yt-dlp process failed. Error: " + m_process->errorString() + "\n" + m_process->readAllStandardError());
            return;
        }

        QByteArray jsonData = m_process->readAllStandardOutput();
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);

        if (!doc.isObject()) {
            emit extractionFailed("Failed to parse yt-dlp JSON output.");
            return;
        }

        QJsonObject root = doc.object();
        QString title = root.value("title").toString();
        QString thumbnailUrl = root.value("thumbnail").toString();
        QString finalFilename = root.value("_filename").toString();
        QList<DownloadTarget> targets;
        QMap<QString, QString> httpHeaders;

        if (root.contains("http_headers")) {
            QJsonObject headersObj = root.value("http_headers").toObject();
            for (auto it = headersObj.constBegin(); it != headersObj.constEnd(); ++it) {
                httpHeaders.insert(it.key(), it.value().toString());
            }
        }

        if (root.contains("requested_downloads")) {
            // Case for separate video and audio files that need merging
            QJsonArray downloads = root.value("requested_downloads").toArray();
            for (const QJsonValue &val : downloads) {
                QJsonObject obj = val.toObject();
                if (obj.contains("url")) {
                    DownloadTarget target;
                    target.url = obj.value("url").toString();
                    target.filename = QFileInfo(obj.value("filename").toString()).fileName();
                    if (obj.contains("vcodec") && obj.value("vcodec") != "none") target.type = DownloadTarget::Type::Video;
                    if (obj.contains("acodec") && obj.value("acodec") != "none") target.type = DownloadTarget::Type::Audio;
                    targets.append(target);
                }
            }
        } else if (root.contains("url")) {
            // Case for a single file download (e.g., audio-only format)
            DownloadTarget target;
            target.url = root.value("url").toString();
            target.filename = root.value("_filename").toString();
            if (root.contains("vcodec") && root.value("vcodec") != "none") target.type = DownloadTarget::Type::Video;
            if (root.contains("acodec") && root.value("acodec") != "none") target.type = DownloadTarget::Type::Audio;
            targets.append(target);
        }

        if (root.contains("requested_subtitles")) {
            QJsonObject subs = root.value("requested_subtitles").toObject();
            QString baseName = QFileInfo(root.value("_filename").toString()).completeBaseName();
            for (auto it = subs.constBegin(); it != subs.constEnd(); ++it) {
                DownloadTarget target;
                target.type = DownloadTarget::Type::Subtitle;
                target.url = it.value().toObject().value("url").toString();
                target.ext = it.value().toObject().value("ext").toString();
                target.lang = it.key();
                target.filename = QString("%1.%2.%3").arg(baseName, target.lang, target.ext);
                targets.append(target);
            }
        }

        if (targets.isEmpty()) {
            emit extractionFailed("No downloadable URLs found in yt-dlp JSON output.");
            return;
        }

        emit extractionSuccess(title, thumbnailUrl, targets, finalFilename, httpHeaders, root.toVariantMap());
    });
}

void YtDlpJsonExtractor::extract(const QString &ytDlpPath, const QStringList &args)
{
    if (m_process->state() != QProcess::NotRunning) {
        emit extractionFailed("Extractor is already running.");
        return;
    }

    m_process->start(ytDlpPath, args);
}

void YtDlpJsonExtractor::cancel() {
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
}