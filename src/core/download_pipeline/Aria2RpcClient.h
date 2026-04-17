#pragma once

#include <QObject>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
#include <QMap>
#include <QTimer>

class Aria2RpcClient : public QObject {
    Q_OBJECT
public:
    explicit Aria2RpcClient(QObject* parent = nullptr);
    ~Aria2RpcClient();

    // Starts the background aria2c process
    bool startDaemon(const QString& aria2ExecutablePath, const QString& maxOverallLimit = "0");
    void stopDaemon();

    // Change the global bandwidth limit dynamically (e.g., "1M", "500K", "0" for unlimited)
    void setGlobalLimit(const QString& maxOverallLimit);

    // Add a new download task to the daemon
    void addDownload(const QString& url, const QString& saveDir, const QString& fileName, const QMap<QString, QString>& headers = {}, std::function<void(const QString&)> callback = nullptr, std::function<void(const QString&)> errorCallback = nullptr);

    // Query the status of an active download by GID
    void queryStatus(const QString& gid);

    // Remove/abort an active download task
    void removeDownload(const QString& gid);

signals:
    void daemonStarted();
    void daemonError(const QString& error);
    void downloadAdded(const QString& gid);
    void downloadProgress(const QString& gid, qint64 completedLength, qint64 totalLength, qint64 downloadSpeed, const QString& status);
    void globalStatUpdated(qint64 downloadSpeed);
    void rpcError(const QString& error);

private:
    void sendRpcRequest(const QString& method, const QJsonArray& params, std::function<void(const QJsonObject&)> callback, std::function<void(const QString&)> errorCallback = nullptr);

    QProcess* m_process;
    QNetworkAccessManager* m_netManager;
    QUrl m_rpcUrl;
    QTimer* m_statTimer;
};
