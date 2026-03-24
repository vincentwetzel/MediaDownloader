#include "GalleryDlWorker.h"
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

GalleryDlWorker::GalleryDlWorker(const QString &id, const QStringList &args, QObject *parent)
    : QObject(parent), m_id(id), m_args(args) {

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::finished, this, &GalleryDlWorker::onProcessFinished);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &GalleryDlWorker::onReadyReadStandardOutput);
}

void GalleryDlWorker::start() {
    QString galleryDlPath = resolveExecutablePath("gallery-dl.exe");
    if (galleryDlPath.isEmpty()) {
        emit finished(m_id, false, "gallery-dl.exe not found in application directory or bin/ subdirectory.", "", QVariantMap());
        return;
    }

    m_process->start(galleryDlPath, m_args);
}

void GalleryDlWorker::killProcess() {
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->kill();
    }
}

QString GalleryDlWorker::resolveExecutablePath(const QString &name) const {
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

void GalleryDlWorker::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
    QString message = success ? "Gallery download completed." : "Gallery download failed.";

    if (!success) {
        message += "\n" + m_processOutput;
    }

    QString tempPath;
    for(int i = 0; i < m_args.size(); ++i) {
        if (m_args[i] == "--directory" && i + 1 < m_args.size()) {
            tempPath = m_args[i+1];
            break;
        }
    }

    emit finished(m_id, success, message, tempPath, QVariantMap());
}

void GalleryDlWorker::onReadyReadStandardOutput() {
    QString output = m_process->readAllStandardOutput();
    m_processOutput.append(output);
    emit outputReceived(m_id, output);
}
