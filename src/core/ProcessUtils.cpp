#include "ProcessUtils.h"
#include "core/ConfigManager.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>

namespace ProcessUtils {

// Static cache for resolved binary paths
static QHash<QString, FoundBinary> s_binaryCache;

// Helper: check common per-user tool install locations that may not be in PATH
// (e.g. deno at ~/.deno/bin, node at ~/.nvm, etc.)
static QString findCommonUserTool(const QString& exeName)
{
#ifdef Q_OS_WIN
    const QString home = QProcessEnvironment::systemEnvironment().value("USERPROFILE");
    if (!home.isEmpty()) {
        const QString denoPath = QDir(home).filePath(".deno/bin/" + exeName);
        if (QFileInfo::exists(denoPath)) return denoPath;
    }
#else
    const QString home = QProcessEnvironment::systemEnvironment().value("HOME");
    if (!home.isEmpty()) {
        const QString denoPath = QDir(home).filePath(".deno/bin/" + exeName);
        if (QFileInfo::exists(denoPath)) return denoPath;
    }
#endif
    return QString();
}

FoundBinary findBinary(const QString& name, ConfigManager* configManager)
{
    // Check cache first
    if (s_binaryCache.contains(name)) {
        return s_binaryCache.value(name);
    }

    FoundBinary result = resolveBinary(name, configManager);
    
    // Cache the result
    s_binaryCache.insert(name, result);
    
    return result;
}

FoundBinary resolveBinary(const QString& name, ConfigManager* configManager)
{
#ifdef Q_OS_WIN
    QString exeName = name + ".exe";
#else
    QString exeName = name;
#endif

    QString systemPath = QStandardPaths::findExecutable(exeName);
    QString bundledPathBin = QDir(QCoreApplication::applicationDirPath()).filePath("bin/" + exeName);
    QString bundledPathRoot = QDir(QCoreApplication::applicationDirPath()).filePath(exeName);

    // 1. Check config override
    QString configKey = name + "_path";
    QString customPath = configManager->get("Binaries", configKey, "").toString().trimmed();

    if (!customPath.isEmpty()) {
        if (!QFileInfo::exists(customPath)) {
            return {QDir::toNativeSeparators(customPath), "Invalid Custom"};
        }

        QString canonicalCustom = QFileInfo(customPath).canonicalFilePath();

        if (!systemPath.isEmpty() && canonicalCustom == QFileInfo(systemPath).canonicalFilePath()) {
            return {QDir::toNativeSeparators(systemPath), "System PATH"};
        }
        if (QFileInfo::exists(bundledPathBin) && canonicalCustom == QFileInfo(bundledPathBin).canonicalFilePath()) {
            return {QDir::toNativeSeparators(bundledPathBin), "Bundled"};
        }
        if (QFileInfo::exists(bundledPathRoot) && canonicalCustom == QFileInfo(bundledPathRoot).canonicalFilePath()) {
            return {QDir::toNativeSeparators(bundledPathRoot), "Bundled"};
        }

        return {QDir::toNativeSeparators(customPath), "Custom"};
    }

    // 2. Check system PATH first (allows users to provide their own unbundled binaries)
    if (!systemPath.isEmpty()) {
        return {QDir::toNativeSeparators(systemPath), "System PATH"};
    }

    // 2b. Check common per-user tool locations (deno at ~/.deno/bin, etc.)
    //     These tools often install to a user-local path that isn't in PATH.
    QString userToolPath = findCommonUserTool(exeName);
    if (!userToolPath.isEmpty()) {
        return {QDir::toNativeSeparators(userToolPath), "User Local"};
    }

    // 3. Check bundled paths (first in 'bin' subdirectory, then app root)
    if (QFileInfo::exists(bundledPathBin)) {
        return {QDir::toNativeSeparators(bundledPathBin), "Bundled"};
    }
    if (QFileInfo::exists(bundledPathRoot)) {
        return {QDir::toNativeSeparators(bundledPathRoot), "Bundled"};
    }

    return {name, "Not Found"};
}

void clearCache() {
    s_binaryCache.clear();
}

void cacheBinary(const QString& name, const FoundBinary& found) {
    s_binaryCache.insert(name, found);
}

FoundBinary getCachedBinary(const QString& name) {
    return s_binaryCache.value(name);
}

bool hasCachedBinary(const QString& name) {
    return s_binaryCache.contains(name);
}

void setProcessEnvironment(QProcess &process) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONUTF8", "1");
    env.insert("PYTHONIOENCODING", "utf-8");
    process.setProcessEnvironment(env);
}

} // namespace ProcessUtils