#include "StartupWorker.h"
#include "core/ProcessUtils.h"

#include <QDebug>
#include <QMetaObject>

StartupWorker::StartupWorker(ConfigManager *configManager, ExtractorJsonParser *extractorJsonParser, QObject *parent)
    : QObject(parent),
      m_configManager(configManager),
      m_extractorJsonParser(extractorJsonParser),
      m_ytDlpUpdater(std::make_unique<YtDlpUpdater>(m_configManager)),
      m_galleryDlUpdater(std::make_unique<GalleryDlUpdater>(m_configManager)),
      m_ytDlpCheckDone(false),
      m_galleryDlCheckDone(false),
      m_extractorsCheckDone(false)
{
    // Move updaters to the current thread context (which will be the worker thread)
    m_ytDlpUpdater->moveToThread(this->thread());
    m_galleryDlUpdater->moveToThread(this->thread());

    connect(m_ytDlpUpdater.get(), &YtDlpUpdater::updateFinished, this, &StartupWorker::onYtDlpUpdateFinished);
    connect(m_ytDlpUpdater.get(), &YtDlpUpdater::versionFetched, this, &StartupWorker::ytDlpVersionFetched);

    connect(m_galleryDlUpdater.get(), &GalleryDlUpdater::updateFinished, this, &StartupWorker::onGalleryDlUpdateFinished);
    connect(m_galleryDlUpdater.get(), &GalleryDlUpdater::versionFetched, this, &StartupWorker::galleryDlVersionFetched);

    connect(m_extractorJsonParser, &ExtractorJsonParser::extractorsReady, this, &StartupWorker::onExtractorsReady);
}

StartupWorker::~StartupWorker()
{
    // The unique_ptrs will handle the deletion of the updaters.
    // The updaters' destructors will be called, which will handle their own cleanup.
}

void StartupWorker::start() {
    logBinaryPaths();
    checkBinaries();
    checkYtDlpUpdate();
    checkGalleryDlUpdate();
}

void StartupWorker::logBinaryPaths() {
    const QStringList binaries = {"yt-dlp", "ffmpeg", "ffprobe", "gallery-dl", "aria2c", "deno"};
    for (const QString &binary : binaries) {
        const ProcessUtils::FoundBinary foundBinary = ProcessUtils::findBinary(binary, m_configManager);
        if (foundBinary.source == "Not Found") {
            qWarning() << "Binary not found:" << binary;
        } else {
            qInfo() << "Binary resolved:" << binary << "source:" << foundBinary.source << "path:" << foundBinary.path;
        }
    }
}

void StartupWorker::checkBinaries() {
    QStringList missing;
    const QStringList requiredBinaries = {"yt-dlp", "ffmpeg", "ffprobe", "deno"};
    for (const QString &binary : requiredBinaries) {
        QString source = ProcessUtils::findBinary(binary, m_configManager).source;
        if (source == "Not Found" || source == "Invalid Custom") {
            missing << binary;
        }
    }
    emit binariesChecked(missing);
}

void StartupWorker::checkYtDlpUpdate() {
    qInfo() << "Checking for yt-dlp updates.";
    QMetaObject::invokeMethod(m_ytDlpUpdater.get(), "checkForUpdates", Qt::QueuedConnection);
}

void StartupWorker::checkGalleryDlUpdate() {
    qInfo() << "Checking for gallery-dl updates.";
    QMetaObject::invokeMethod(m_galleryDlUpdater.get(), "checkForUpdates", Qt::QueuedConnection);
}

void StartupWorker::onYtDlpUpdateFinished(Updater::UpdateStatus status, const QString &message) {
    if (status == Updater::UpdateStatus::UpdateAvailable || status == Updater::UpdateStatus::UpToDate) {
        qInfo() << message;
    } else { // Error
        qWarning() << "yt-dlp auto-update failed: " + message;
    }
    m_ytDlpCheckDone = true;

    // Now that the update is done, we can safely generate the extractor list.
    qInfo() << "Starting extractor list generation.";
    m_extractorJsonParser->startGeneration();

    this->checkAllFinished();
}

void StartupWorker::onGalleryDlUpdateFinished(Updater::UpdateStatus status, const QString &message) {
    if (status == Updater::UpdateStatus::UpdateAvailable || status == Updater::UpdateStatus::UpToDate) {
        qInfo() << message;
    } else { // Error
        qWarning() << "gallery-dl auto-update failed: " + message;
    }
    m_galleryDlCheckDone = true;
    this->checkAllFinished();
}

void StartupWorker::onExtractorsReady() {
    qInfo() << "Extractor list generation finished.";
    m_extractorsCheckDone = true;
    this->checkAllFinished();
}

void StartupWorker::checkAllFinished() {
    if (m_ytDlpCheckDone && m_galleryDlCheckDone && m_extractorsCheckDone) {
        qInfo() << "Startup checks finished.";
        emit finished();
    }
}
