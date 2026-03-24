#include "Aria2DownloadWorker.h"
#include <QDir>
#include <QRegularExpression>
#include <QFileInfo>
#include <QPointer>
#include <QDebug>
#include "core/YtDlpArgsBuilder.h"

Aria2DownloadWorker::Aria2DownloadWorker(Aria2Daemon* globalDaemon, QObject* parent)
    : QObject(parent), m_daemon(globalDaemon)
{
    m_extractor = new YtDlpJsonExtractor(this);
    m_ffmpeg = new FfmpegPostProcessor(this);
    m_pollTimer = new QTimer(this);

    connect(m_extractor, &YtDlpJsonExtractor::extractionSuccess, this, &Aria2DownloadWorker::onExtractionSuccess);
    connect(m_extractor, &YtDlpJsonExtractor::extractionFailed, this, &Aria2DownloadWorker::onExtractionFailed);
    
    connect(m_ffmpeg, &FfmpegPostProcessor::mergeSuccess, this, &Aria2DownloadWorker::onMergeSuccess);
    connect(m_ffmpeg, &FfmpegPostProcessor::mergeFailed, this, &Aria2DownloadWorker::onMergeFailed);

    connect(m_pollTimer, &QTimer::timeout, this, &Aria2DownloadWorker::pollAria2Status);
    
    // Listen to the global daemon's signals
    connect(m_daemon, &Aria2Daemon::downloadProgress, this, &Aria2DownloadWorker::onDownloadProgress);
}

Aria2DownloadWorker::~Aria2DownloadWorker() {
    m_pollTimer->stop();
}

void Aria2DownloadWorker::start(const QString& ytDlpPath, const QString& ffmpegPath, const QString& url, const QString& saveDir, ConfigManager* configManager, const QVariantMap& options) {
    m_state = State::Extracting;
    m_ffmpegPath = ffmpegPath;
    m_saveDir = saveDir;
    m_isCancelled = false;

    emit statusTextChanged("Gathering metadata...");
    
    YtDlpArgsBuilder builder;
    QStringList extractionArgs = builder.build(configManager, url, options);
    
    // Remove conflicting args for JSON extraction
    extractionArgs.removeAll("--print");
    extractionArgs.removeAll("after_move:filepath");
    
    extractionArgs << "--dump-json";
    
    m_extractor->extract(ytDlpPath, extractionArgs);
}

void Aria2DownloadWorker::onExtractionSuccess(const QString& title, const QString& thumbnailUrl, const QList<DownloadTarget>& targets, const QString& finalFilename, const QMap<QString, QString>& httpHeaders, const QVariantMap& metadata) {
    m_state = State::Downloading;
    emit statusTextChanged("Starting download...");

    m_title = title;
    m_thumbnailUrl = thumbnailUrl;
    m_downloadedParts.clear();
    m_downloadedSubtitles.clear();
    m_finalFileName = finalFilename;

    // Store metadata for the DownloadManager to retrieve later, bypassing brittle JSON disk reads
    this->setProperty("metadata", metadata);

    // Write standard .info.json for consistency and cancellation cleanups
    QString infoFilePath = QDir(m_saveDir).filePath(QFileInfo(m_finalFileName).completeBaseName() + ".info.json");
    QFile infoFile(infoFilePath);
    if (infoFile.open(QIODevice::WriteOnly)) { infoFile.write(QJsonDocument::fromVariant(metadata).toJson()); infoFile.close(); }

    int expectedGids = targets.size();
    bool hasThumbnail = !m_thumbnailUrl.isEmpty();
    
    if (hasThumbnail) {
        expectedGids++;
        m_thumbnailPath = QDir(m_saveDir).filePath(QFileInfo(m_finalFileName).completeBaseName() + "_thumb.jpg");
    }

    // Using QPointer ensures callbacks don't crash if this worker is destroyed early
    QPointer<Aria2DownloadWorker> ptr(this);
    auto checkStartPolling = [ptr, expectedGids]() {
        if (ptr && ptr->m_activeGids.size() == expectedGids) {
            ptr->m_pollTimer->start(500); // Poll every 500ms
        }
    };
    
    for (const DownloadTarget& target : targets) {
        QString partFilePath = QDir(m_saveDir).filePath(target.filename);
        if (target.type == DownloadTarget::Type::Subtitle) {
            m_downloadedSubtitles.append({partFilePath, target.lang});
        } else {
            m_downloadedParts.append(partFilePath);
        }
        
        // Tell aria2c to start downloading this part
        m_daemon->addDownload(target.url, m_saveDir, target.filename, httpHeaders, [ptr, checkStartPolling](const QString& gid) {
            if (ptr) {
                ptr->m_activeGids.append(gid);
                ptr->m_allGids.append(gid);
                checkStartPolling();
            }
        }, [ptr](const QString& err) {
            if (ptr && ptr->m_state != State::Error) {
                ptr->m_state = State::Error;
                for (const QString& activeGid : ptr->m_activeGids) {
                    ptr->m_daemon->removeDownload(activeGid);
                }
                ptr->cleanupPartialFiles();
                emit ptr->error("Aria2 rejected download: " + err);
            }
        });
    }

    if (hasThumbnail) {
        m_daemon->addDownload(m_thumbnailUrl, m_saveDir, QFileInfo(m_thumbnailPath).fileName(), httpHeaders, [ptr, checkStartPolling](const QString& gid) {
            if (ptr) {
                ptr->m_activeGids.append(gid);
                ptr->m_allGids.append(gid);
                checkStartPolling();
            }
        }, [ptr](const QString& err) {
            if (ptr && ptr->m_state != State::Error) {
                ptr->m_state = State::Error;
                for (const QString& activeGid : ptr->m_activeGids) {
                    ptr->m_daemon->removeDownload(activeGid);
                }
                ptr->cleanupPartialFiles();
                emit ptr->error("Aria2 rejected thumbnail: " + err);
            }
        });
    }
}

void Aria2DownloadWorker::onExtractionFailed(const QString& errorMsg) {
    if (m_isCancelled) return;
    
    m_state = State::Error;
    emit error(errorMsg);
}

void Aria2DownloadWorker::pollAria2Status() {
    for (const QString& gid : m_activeGids) {
        m_daemon->queryStatus(gid);
    }
}

void Aria2DownloadWorker::onDownloadProgress(const QString& gid, qint64 completedLength, qint64 totalLength, qint64 downloadSpeed, const QString& status) {
    if (!m_allGids.contains(gid)) return; // Belongs to another worker

    if (m_state == State::Error || m_isCancelled) return;

    if (status == "error") {
        m_pollTimer->stop();
        for (const QString& activeGid : m_activeGids) {
            m_daemon->removeDownload(activeGid);
        }
        cleanupPartialFiles();
        m_state = State::Error;
        emit error("Aria2c encountered an error downloading a segment.");
        return;
    }

    m_completedLengths[gid] = completedLength;
    m_totalLengths[gid] = totalLength;
    m_downloadSpeeds[gid] = (status == "complete") ? 0 : downloadSpeed;

    qint64 totalCompleted = 0;
    qint64 totalSize = 0;
    qint64 totalSpeed = 0;

    for (const QString& trackedGid : m_allGids) {
        totalCompleted += m_completedLengths.value(trackedGid, 0);
        totalSize += m_totalLengths.value(trackedGid, 0);
        totalSpeed += m_downloadSpeeds.value(trackedGid, 0);
    }

    if (totalSize > 0) {
        int percent = static_cast<int>((totalCompleted * 100) / totalSize);
        QString speedStr = totalSpeed > 1048576 ? QString::number(totalSpeed / 1048576.0, 'f', 2) + " MB/s" : QString::number(totalSpeed / 1024) + " KB/s";
        emit progressUpdated(percent, speedStr);
        emit statusTextChanged("Downloading...");
    }

    if (status == "complete" && m_activeGids.contains(gid)) {
        m_activeGids.removeOne(gid);
        
        if (m_activeGids.isEmpty()) {
            // All parts downloaded! Move to next state.
            m_pollTimer->stop();
            m_state = State::PostProcessing;
            
            emit statusTextChanged("Merging files...");
            emit progressUpdated(100, "Processing");
            
            QString finalPath = QDir(m_saveDir).filePath(m_finalFileName);
            m_ffmpeg->merge(m_ffmpegPath, m_downloadedParts, finalPath, m_title, m_thumbnailPath, m_downloadedSubtitles);
        }
    }
}

void Aria2DownloadWorker::onMergeSuccess(const QString& outputFile) {
    m_state = State::Finished;
    emit statusTextChanged("Done");
    emit finished(outputFile);
}

void Aria2DownloadWorker::onMergeFailed(const QString& errorMsg) {
    if (m_isCancelled) return;
    
    cleanupPartialFiles();
    m_state = State::Error;
    emit error(errorMsg);
}

void Aria2DownloadWorker::cancel() {
    if (m_state == State::Finished || m_state == State::Error || m_state == State::Idle) return;
    
    m_isCancelled = true;
    m_pollTimer->stop();

    if (m_state == State::Extracting) {
        m_extractor->cancel();
    } else if (m_state == State::Downloading) {
        for (const QString& gid : m_activeGids) {
            m_daemon->removeDownload(gid);
        }
    } else if (m_state == State::PostProcessing) {
        m_ffmpeg->cancel();
    }
    
    cleanupPartialFiles();

    m_state = State::Error;
    emit statusTextChanged("Cancelled");
}

void Aria2DownloadWorker::cleanupPartialFiles() {
    QStringList parts = m_downloadedParts;
    QList<SubtitleFile> subs = m_downloadedSubtitles;
    QString thumb = m_thumbnailPath;
    QString info = m_finalFileName.isEmpty() ? "" : QDir(m_saveDir).filePath(QFileInfo(m_finalFileName).completeBaseName() + ".info.json");
    
    // Delay file deletion so aria2c/ffmpeg have time to release OS file locks
    QTimer::singleShot(500, [parts, subs, thumb, info]() {
        for (const QString& partFile : parts) {
            QFile::remove(partFile);
            QFile::remove(partFile + ".aria2"); 
        }
        for (const SubtitleFile& subFile : subs) {
            QFile::remove(subFile.path);
            QFile::remove(subFile.path + ".aria2");
        }
        if (!thumb.isEmpty()) QFile::remove(thumb);
        if (!info.isEmpty()) QFile::remove(info);
    });
}