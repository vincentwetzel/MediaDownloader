#include "GalleryDlWorker.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QFileInfo>

GalleryDlWorker::GalleryDlWorker(const QString &id, const QStringList &args, QObject *parent)
    : QObject(parent), m_id(id), m_args(args)
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

    m_process->setWorkingDirectory(QFileInfo(galleryDlPath).absolutePath());
    m_process->start(galleryDlPath, m_args);
}

void GalleryDlWorker::killProcess()
{
    if (m_process->state() == QProcess::Running) {
        m_process->kill();
    }
}

void GalleryDlWorker::onReadyReadStandardOutput()
{
    m_outputBuffer.append(m_process->readAllStandardOutput());
    const QStringList lines = QString::fromUtf8(m_outputBuffer).split(QRegularExpression("[\\r\\n]"), Qt::SkipEmptyParts);
    m_outputBuffer.clear();

    QRegularExpression re(R"(\[gallery-dl\]\[debug\] Writing to '([^']+)'.*)");
    for (const QString &line : lines) {
        emit outputReceived(m_id, line);
        QRegularExpressionMatch match = re.match(line);
        if (!match.hasMatch()) {
            continue;
        }
        m_lastFile = match.captured(1);
        emit progressUpdated(m_id, {
            {"progress", 50},
            {"status", "Downloading: " + QFileInfo(m_lastFile).fileName()}
        });
    }
}

void GalleryDlWorker::onReadyReadStandardError()
{
    m_errorBuffer.append(m_process->readAllStandardError());
    const QStringList lines = QString::fromUtf8(m_errorBuffer).split(QRegularExpression("[\\r\\n]"), Qt::SkipEmptyParts);
    m_errorBuffer.clear();

    for (const QString &line : lines) {
        emit outputReceived(m_id, line);
    }
}

void GalleryDlWorker::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::CrashExit || (exitCode != 0 && exitCode != 101)) { // 101 can mean "no images found"
        QString stderrOutput = QString::fromUtf8(m_process->readAllStandardError());
        qWarning() << "GalleryDlWorker failed. Exit code:" << exitCode << "Stderr:" << stderrOutput;
        emit finished(m_id, false, "gallery-dl failed: " + stderrOutput, QString(), QVariantMap());
        return;
    }

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
