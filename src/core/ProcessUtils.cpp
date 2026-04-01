#include "ProcessUtils.h"
#include "core/ConfigManager.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>

namespace ProcessUtils {

FoundBinary findBinary(const QString& name, ConfigManager* configManager)
{
    // 1. Check config override
    QString configKey = name + "_path";
    QString customPath = configManager->get("Binaries", configKey, "").toString();
    if (!customPath.isEmpty() && QFileInfo::exists(customPath)) {
        return {QDir::toNativeSeparators(customPath), "Custom"};
    }

#ifdef Q_OS_WIN
    QString exeName = name + ".exe";
#else
    QString exeName = name;
#endif

    // 2. Check system PATH first (allows users to provide their own unbundled binaries)
    QString systemPath = QStandardPaths::findExecutable(exeName);
    if (!systemPath.isEmpty()) {
        return {QDir::toNativeSeparators(systemPath), "System PATH"};
    }

    // 3. Check bundled paths (first in 'bin' subdirectory, then app root)
    QString bundledPathBin = QDir(QCoreApplication::applicationDirPath()).filePath("bin/" + exeName);
    if (QFileInfo::exists(bundledPathBin)) {
        return {QDir::toNativeSeparators(bundledPathBin), "Bundled"};
    }
    QString bundledPathRoot = QDir(QCoreApplication::applicationDirPath()).filePath(exeName);
    if (QFileInfo::exists(bundledPathRoot)) {
        return {QDir::toNativeSeparators(bundledPathRoot), "Bundled"};
    }

    return {name, "Not Found"};
}

void setProcessEnvironment(QProcess &process) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONUTF8", "1");
    env.insert("PYTHONIOENCODING", "utf-8");
    process.setProcessEnvironment(env);
}

} // namespace ProcessUtils