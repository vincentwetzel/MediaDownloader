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

// Helper: check common per-user tool install locations that may not be PATH
static QString findCommonUserTool(const QString& exeName)
{
#ifdef Q_OS_WIN
    const QString home = QProcessEnvironment::systemEnvironment().value("USERPROFILE");
    const QString localAppData = QProcessEnvironment::systemEnvironment().value("LOCALAPPDATA");
    const QString programData = QProcessEnvironment::systemEnvironment().value("ProgramData");

    if (!home.isEmpty()) {
        // deno (~/.deno/bin)
        const QString denoPath = QDir(home).filePath(".deno/bin/" + exeName);
        if (QFileInfo::exists(denoPath)) return denoPath;

        // scoop shims (~\scoop\shims)
        const QString scoopPath = QDir(home).filePath("scoop/shims/" + exeName);
        if (QFileInfo::exists(scoopPath)) return scoopPath;
    }

    if (!localAppData.isEmpty()) {
        // pip-installed Python scripts (%LOCALAPPDATA%\Programs\Python\Python*\Scripts\)
        const QString pythonScriptsDir = QDir(localAppData).filePath("Programs/Python");
        if (QFileInfo::exists(pythonScriptsDir)) {
            QDir pyDir(pythonScriptsDir);
            const QFileInfoList entries = pyDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo &entry : entries) {
                const QString candidate = entry.filePath() + "/Scripts/" + exeName;
                if (QFileInfo::exists(candidate)) return candidate;
            }
        }

        // WindowsApps execution aliases (winget-installed tools that may not be in PATH)
        // These are 0-byte stubs that only work through shell alias resolution.
        // We still return the path here — the caller (BinariesPage) detects 0-byte
        // stubs and handles them by prepending WindowsApps to PATH instead of
        // invoking the stub directly.
        const QString windowsAppsDir = QDir(localAppData).filePath("Microsoft/WindowsApps");
        if (QFileInfo::exists(windowsAppsDir)) {
            const QString aliasPath = QDir(windowsAppsDir).filePath(exeName);
            if (QFileInfo::exists(aliasPath)) return aliasPath;
        }
    }

    if (!programData.isEmpty()) {
        // Chocolatey (C:\ProgramData\chocolatey\bin)
        const QString chocoPath = QDir(programData).filePath("chocolatey/bin/" + exeName);
        if (QFileInfo::exists(chocoPath)) return chocoPath;
    }
#else
    const QString home = QProcessEnvironment::systemEnvironment().value("HOME");
    if (!home.isEmpty()) {
        // deno (~/.deno/bin)
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

    // Test QStandardPaths::findExecutable
    QString systemPath = QStandardPaths::findExecutable(exeName);

    qDebug() << "[ProcessUtils] resolveBinary:" << name << "- systemPath:" << systemPath;

    // 1. Check config override
    QString configKey = name + "_path";
    QString customPath = configManager->get("Binaries", configKey, "").toString().trimmed();

    if (!customPath.isEmpty()) {
        qDebug() << "[ProcessUtils] Custom path configured for" << name << ":" << customPath;
        if (!QFileInfo::exists(customPath)) {
            return {QDir::toNativeSeparators(customPath), "Invalid Custom"};
        }

        QString canonicalCustom = QFileInfo(customPath).canonicalFilePath();

        if (!systemPath.isEmpty() && canonicalCustom == QFileInfo(systemPath).canonicalFilePath()) {
            return {QDir::toNativeSeparators(systemPath), "System PATH"};
        }

        return {QDir::toNativeSeparators(customPath), "Custom"};
    }

    // 2. Check system PATH first (allows users to provide their own unbundled binaries)
    if (!systemPath.isEmpty()) {
        qDebug() << "[ProcessUtils] Found" << name << "in System PATH:" << systemPath;
        return {QDir::toNativeSeparators(systemPath), "System PATH"};
    }

    // 2b. Check common per-user tool locations (deno at ~/.deno/bin, etc.)
    //     These tools often install to a user-local path that isn't in PATH.
    QString userToolPath = findCommonUserTool(exeName);
    if (!userToolPath.isEmpty()) {
        qDebug() << "[ProcessUtils] Found" << name << "in User Local:" << userToolPath;
        return {QDir::toNativeSeparators(userToolPath), "User Local"};
    }

    qDebug() << "[ProcessUtils]" << name << "NOT FOUND - searching all PATH directories for" << exeName;
    
    // Final diagnostic: manually search PATH
    QString pathEnv = QProcessEnvironment::systemEnvironment().value("PATH");
    QStringList pathDirs = pathEnv.split(QDir::listSeparator(), Qt::SkipEmptyParts);
    qDebug() << "[ProcessUtils] PATH contains" << pathDirs.size() << "directories";
    for (const QString& dir : pathDirs) {
        QString candidate = QDir(dir).filePath(exeName);
        if (QFileInfo::exists(candidate)) {
            qDebug() << "[ProcessUtils] FOUND" << name << "at" << candidate << "(in PATH dir:" << dir << ")";
            return {QDir::toNativeSeparators(candidate), "System PATH"};
        }
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

void terminateProcessTree(QProcess *process, int gracefulTimeoutMs) {
    if (!process || process->state() == QProcess::NotRunning) {
        return;
    }

    const qint64 pid = process->processId();
    qInfo() << "[ProcessUtils] Terminating process tree for PID" << pid;

#ifdef Q_OS_WIN
    if (pid > 0) {
        QProcess taskkill;
        taskkill.setProcessChannelMode(QProcess::MergedChannels);
        taskkill.start("taskkill", {"/PID", QString::number(pid), "/T", "/F"});
        if (!taskkill.waitForFinished(5000)) {
            qWarning() << "[ProcessUtils] taskkill timed out for PID" << pid;
        } else if (taskkill.exitCode() != 0) {
            qWarning() << "[ProcessUtils] taskkill failed for PID" << pid << taskkill.readAllStandardOutput();
        }
    }
    process->waitForFinished(gracefulTimeoutMs);
#else
    process->terminate();
    if (!process->waitForFinished(gracefulTimeoutMs)) {
        process->kill();
        process->waitForFinished(5000);
    }
#endif
}

} // namespace ProcessUtils