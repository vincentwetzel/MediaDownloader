#include "BrowserUtils.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>

namespace BrowserUtils {

QStringList getInstalledBrowsers() {
    QStringList browsers;

    // Common paths on Windows
    // Note: This is a basic check. A more robust check would query the registry.

    QString programFiles = qgetenv("ProgramFiles");
    QString programFilesX86 = qgetenv("ProgramFiles(x86)");
    QString localAppData = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/AppData/Local";

    // Chrome
    if (QFile::exists(programFiles + "/Google/Chrome/Application/chrome.exe") ||
        QFile::exists(programFilesX86 + "/Google/Chrome/Application/chrome.exe")) {
        browsers << "chrome";
    }

    // Firefox
    if (QFile::exists(programFiles + "/Mozilla Firefox/firefox.exe") ||
        QFile::exists(programFilesX86 + "/Mozilla Firefox/firefox.exe")) {
        browsers << "firefox";
    }

    // Edge
    if (QFile::exists(programFiles + "/Microsoft/Edge/Application/msedge.exe") ||
        QFile::exists(programFilesX86 + "/Microsoft/Edge/Application/msedge.exe")) {
        browsers << "edge";
    }

    // Opera
    if (QFile::exists(localAppData + "/Programs/Opera/launcher.exe")) {
        browsers << "opera";
    }

    // Brave
    if (QFile::exists(programFiles + "/BraveSoftware/Brave-Browser/Application/brave.exe") ||
        QFile::exists(programFilesX86 + "/BraveSoftware/Brave-Browser/Application/brave.exe")) {
        browsers << "brave";
    }

    // Vivaldi
    if (QFile::exists(localAppData + "/Vivaldi/Application/vivaldi.exe")) {
        browsers << "vivaldi";
    }

    return browsers;
}

}
