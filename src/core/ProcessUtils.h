#pragma once
#include <QString>

class ConfigManager;
class QProcess;

namespace ProcessUtils {
    struct FoundBinary {
        QString path;
        QString source; // "Custom", "Bundled", "System PATH", or "Not Found"
    };
    FoundBinary findBinary(const QString& name, ConfigManager* configManager);
    void setProcessEnvironment(QProcess &process);
}