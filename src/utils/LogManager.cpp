#include "LogManager.h"
#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <iostream>
#include <QDir>
#include <QCoreApplication>

// Define a static file pointer for the log file
static QFile *logFile = nullptr;

// Custom message handler
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QString formattedMsg;
    QTextStream stream(&formattedMsg);
    stream.setEncoding(QStringConverter::Utf8);

    stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " ";

    switch (type) {
    case QtDebugMsg:
        stream << "Debug: ";
        break;
    case QtInfoMsg:
        stream << "Info: ";
        break;
    case QtWarningMsg:
        stream << "Warning: ";
        break;
    case QtCriticalMsg:
        stream << "Critical: ";
        break;
    case QtFatalMsg:
        stream << "Fatal: ";
        break;
    }

    stream << msg;
    // The context information is often too verbose for a standard log file, so it's omitted.
    stream << "\n";

    // Write to stderr for console view
    QTextStream errStream(stderr);
    errStream.setEncoding(QStringConverter::Utf8);
    errStream << formattedMsg;
    errStream.flush();

    // Write to the log file if it's open
    if (logFile) {
        QTextStream logStream(logFile);
        logStream.setEncoding(QStringConverter::Utf8);
        logStream << formattedMsg;
        logStream.flush(); // Ensure the message is written immediately
    }

    if (type == QtFatalMsg) {
        abort();
    }
}

void LogManager::installHandler() {
    const QString logDir = QCoreApplication::applicationDirPath();
    const QString logPath = QDir(logDir).filePath("debug.log");
    const QString backupLogPath = logPath + ".1";
    const qint64 maxLogSizeBytes = 5 * 1024 * 1024; // 5 MB

    QFile currentLog(logPath);

    // Rotate logs if the current log is too large
    if (currentLog.exists() && currentLog.size() > maxLogSizeBytes) {
        // If a backup already exists, remove it
        if (QFile::exists(backupLogPath)) {
            QFile::remove(backupLogPath);
        }
        // Rename the current log to the backup path
        currentLog.rename(backupLogPath);
    }

    // Open the new log file
    logFile = new QFile(logPath);
    if (!logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        std::cerr << "Failed to open log file: " << logPath.toStdString() << std::endl;
        delete logFile;
        logFile = nullptr;
        // We don't return here, so console logging will still work
    }

    qInstallMessageHandler(customMessageHandler);

    // Print the log file path to the console (and log it) on startup
    qDebug() << "Log file path:" << logPath;
}
