#include "Aria2Daemon.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>
#include <QDebug>

Aria2Daemon::Aria2Daemon(QObject* parent)
    : QObject(parent), 
      m_process(new QProcess(this)), 
      m_netManager(new QNetworkAccessManager(this)),
      m_statTimer(new QTimer(this))
{
    // Default local RPC URL for aria2
    m_rpcUrl = QUrl("http://127.0.0.1:6800/jsonrpc");

    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        emit daemonError("Aria2 process error: " + QString::number(error));
    });

    connect(m_statTimer, &QTimer::timeout, this, [this]() {
        sendRpcRequest("aria2.getGlobalStat", QJsonArray(), [this](const QJsonObject& response) {
            if (response.contains("result")) {
                qint64 speed = response["result"].toObject()["downloadSpeed"].toString().toLongLong();
                emit globalStatUpdated(speed);
            }
        });
    });
}

Aria2Daemon::~Aria2Daemon() {
    stopDaemon();
}

bool Aria2Daemon::startDaemon(const QString& aria2ExecutablePath, const QString& maxOverallLimit) {
    if (m_process->state() != QProcess::NotRunning) {
        return false; // Already running
    }

    QStringList args;
    args << "--enable-rpc=true"
         << "--rpc-listen-all=false"          // Keep it local for security
         << "--rpc-listen-port=6800"
         << "--rpc-allow-origin-all=true"
         << QString("--max-overall-download-limit=%1").arg(maxOverallLimit)
         << "--daemon=false";                 // Keep attached to QProcess for lifecycle management

    m_process->start(aria2ExecutablePath, args);
    if (!m_process->waitForStarted()) {
        return false;
    }

    emit daemonStarted();
    m_statTimer->start(1000); // Poll global stats every second
    return true;
}

void Aria2Daemon::stopDaemon() {
    m_statTimer->stop();
    if (m_process->state() == QProcess::Running) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }
}

void Aria2Daemon::sendRpcRequest(const QString& method, const QJsonArray& params, std::function<void(const QJsonObject&)> callback, std::function<void(const QString&)> errorCallback) {
    QJsonObject requestObj;
    requestObj["jsonrpc"] = "2.0";
    requestObj["id"] = QUuid::createUuid().toString();
    requestObj["method"] = method;
    if (!params.isEmpty()) {
        requestObj["params"] = params;
    }

    QJsonDocument doc(requestObj);
    QByteArray data = doc.toJson();

    QNetworkRequest request(m_rpcUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_netManager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, errorCallback] {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument responseDoc = QJsonDocument::fromJson(reply->readAll());
            if (responseDoc.isObject()) {
                QJsonObject responseObj = responseDoc.object();
                if (responseObj.contains("error")) {
                    QString errMsg = responseObj["error"].toObject()["message"].toString();
                    if (errorCallback) errorCallback(errMsg);
                    emit rpcError("RPC Error: " + errMsg);
                } else if (callback) {
                    callback(responseObj);
                }
            }
        } else {
            if (errorCallback) errorCallback(reply->errorString());
            emit rpcError("Network Error: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

void Aria2Daemon::setGlobalLimit(const QString& maxOverallLimit) {
    QJsonArray params;
    QJsonObject options;
    options["max-overall-download-limit"] = maxOverallLimit;
    params.append(options);

    sendRpcRequest("aria2.changeGlobalOption", params, [](const QJsonObject&) {
        qDebug() << "Global limit updated successfully.";
    });
}

void Aria2Daemon::addDownload(const QString& url, const QString& saveDir, const QString& fileName, const QMap<QString, QString>& headers, std::function<void(const QString&)> callback, std::function<void(const QString&)> errorCallback) {
    QJsonArray params;
    QJsonArray urls;
    urls.append(url);
    params.append(urls);

    QJsonObject options;
    options["dir"] = saveDir;
    options["out"] = fileName;
    
    if (!headers.isEmpty()) {
        QJsonArray headerArray;
        for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
            headerArray.append(QString("%1: %2").arg(it.key(), it.value()));
        }
        options["header"] = headerArray;
    }
    
    params.append(options);

    sendRpcRequest("aria2.addUri", params, [this, callback](const QJsonObject& response) {
        QString gid = response["result"].toString();
        emit downloadAdded(gid);
        if (callback) {
            callback(gid);
        }
    }, errorCallback);
}

void Aria2Daemon::queryStatus(const QString& gid) {
    QJsonArray params;
    params.append(gid);

    sendRpcRequest("aria2.tellStatus", params, [this, gid](const QJsonObject& response) {
        if (response.contains("result")) {
            QJsonObject result = response["result"].toObject();
            qint64 completedLength = result["completedLength"].toString().toLongLong();
            qint64 totalLength = result["totalLength"].toString().toLongLong();
            qint64 downloadSpeed = result["downloadSpeed"].toString().toLongLong();
            QString status = result["status"].toString(); // "active", "waiting", "paused", "error", "complete", "removed"
            
            emit downloadProgress(gid, completedLength, totalLength, downloadSpeed, status);
        }
    });
}

void Aria2Daemon::removeDownload(const QString& gid) {
    QJsonArray params;
    params.append(gid);
    sendRpcRequest("aria2.remove", params, nullptr);
}