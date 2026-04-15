#include "BinaryFinder.h"
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QProcessEnvironment>

QStringList BinaryFinder::getExtendedSearchPaths() {
    QStringList paths;

    // 1. Application directory and its 'bin' subdirectory (fallback for bundled/portable use cases)
    paths << QCoreApplication::applicationDirPath();
    paths << QDir(QCoreApplication::applicationDirPath()).filePath("bin");

    // 2. System PATH
    QString systemPath = QProcessEnvironment::systemEnvironment().value("PATH");
    if (!systemPath.isEmpty()) {
        paths << systemPath.split(QDir::listSeparator(), Qt::SkipEmptyParts);
    }

    // 3. Common Package Manager & Standard Installation Paths
#ifdef Q_OS_WIN
    QString localData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    
    // winget
    if (!localData.isEmpty()) {
        paths << QDir(localData).filePath("Microsoft/WindowsApps");
    }
    // scoop
    paths << QDir::homePath() + "/scoop/shims";
    // chocolatey
    paths << "C:/ProgramData/chocolatey/bin";
    
    // Standard Program Files fallbacks
    paths << "C:/Program Files/ffmpeg/bin";
    paths << "C:/Program Files/aria2";
    paths << "C:/Program Files/yt-dlp";
#else
    // macOS / Linux package managers
    paths << "/usr/local/bin";                 // Homebrew (Intel) / standard source installs
    paths << "/opt/homebrew/bin";              // Homebrew (Apple Silicon)
    paths << "/opt/local/bin";                 // MacPorts
    paths << QDir::homePath() + "/.local/bin"; // pip user installs / pipx
#endif

    return paths;
}

QString BinaryFinder::findBinary(const QString& binaryName) {
    QString executableName = binaryName;
#ifdef Q_OS_WIN
    if (!executableName.endsWith(".exe", Qt::CaseInsensitive)) {
        executableName += ".exe";
    }
#endif

    QStringList searchPaths = getExtendedSearchPaths();
    QString foundPath = QStandardPaths::findExecutable(executableName, searchPaths);

    return QDir::toNativeSeparators(foundPath);
}

QMap<QString, QString> BinaryFinder::findAllBinaries() {
    QMap<QString, QString> results;
    QStringList requiredBinaries = {"yt-dlp", "ffmpeg", "ffprobe", "gallery-dl", "deno", "aria2c"};

    for (const QString& bin : requiredBinaries) {
        results[bin] = findBinary(bin);
    }

    return results;
}