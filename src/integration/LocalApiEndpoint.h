#pragma once

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QtGlobal>

namespace LocalApiEndpoint {

inline constexpr quint16 DefaultPort = 8765;
inline constexpr int MinimumPort = 1024;
inline constexpr int MaximumPort = 65535;

inline QString portFilePath(const QString &dataDirectory)
{
    if (dataDirectory.isEmpty()) {
        return {};
    }
    return QDir(dataDirectory).filePath(QStringLiteral("api_port.txt"));
}

inline quint16 readPort(const QString &dataDirectory)
{
    const QString path = portFilePath(dataDirectory);
    if (path.isEmpty()) {
        return DefaultPort;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return DefaultPort;
    }

    bool ok = false;
    const int port = QString::fromUtf8(file.readAll()).trimmed().toInt(&ok);
    if (!ok || port < MinimumPort || port > MaximumPort) {
        return DefaultPort;
    }
    return static_cast<quint16>(port);
}

inline quint16 readDefaultPort()
{
    return readPort(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
}

inline bool writePort(const QString &dataDirectory, quint16 port)
{
    if (dataDirectory.isEmpty() || port < MinimumPort || port > MaximumPort) {
        return false;
    }

    QDir().mkpath(dataDirectory);
    QSaveFile file(portFilePath(dataDirectory));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(QByteArray::number(port));
    file.write("\n");
    if (!file.commit()) {
        return false;
    }
    QFile::setPermissions(file.fileName(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

} // namespace LocalApiEndpoint
