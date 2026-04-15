#include "Logger.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QMutex>
#include <iostream>

static QFile *logFile = nullptr;
static QMutex logMutex;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QMutexLocker locker(&logMutex);

    QString txt;
    switch (type) {
    case QtDebugMsg:
        txt = QString("Debug: %1").arg(msg);
        break;
    case QtWarningMsg:
        txt = QString("Warning: %1").arg(msg);
        break;
    case QtCriticalMsg:
        txt = QString("Critical: %1").arg(msg);
        break;
    case QtFatalMsg:
        txt = QString("Fatal: %1").arg(msg);
        break;
    case QtInfoMsg:
        txt = QString("Info: %1").arg(msg);
        break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString formattedMessage = QString("[%1] %2").arg(timestamp, txt);

    // Print to console
    std::cout << formattedMessage.toStdString() << std::endl;

    // Write to file
    if (logFile && logFile->isOpen()) {
        QTextStream stream(logFile);
        stream << formattedMessage << "\n";
        stream.flush(); // Ensure it's written immediately
    }
}

namespace Logger {

void init() {
    QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logPath);

    QString logFilePath = logPath + "/LzyDownloader.log";

    // Rotation logic
    QFile oldLog(logFilePath);
    if (oldLog.exists() && oldLog.size() > 10 * 1024 * 1024) { // 10 MB
        QFile::remove(logFilePath + ".old");
        oldLog.rename(logFilePath + ".old");
    }

    logFile = new QFile(logFilePath);
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler(messageHandler);
    }
}

QString getLogFilePath() {
    if (logFile) {
        return logFile->fileName();
    }
    return QString();
}

}
