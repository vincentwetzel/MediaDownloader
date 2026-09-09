#include "RuntimeCoordinator.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QDebug>
#include <QTimer>
#include <QVariant>

#include <QRegularExpression>

namespace {
constexpr auto kShowUi = "show-ui";
constexpr auto kEnsureApi = "ensure-api";
constexpr qsizetype kMaxCommandBytes = 16384;

bool isAllowedCommand(const QString &command)
{
    if (command == QLatin1String(kShowUi) || command == QLatin1String(kEnsureApi)) {
        return true;
    }
    static const QRegularExpression enqueueCommand(
        QStringLiteral("^enqueue:(?:video|audio|gallery):[A-Za-z0-9_-]{1,16000}$"));
    return enqueueCommand.match(command).hasMatch();
}
}

RuntimeCoordinator::RuntimeCoordinator(const QString &serverName, QObject *parent)
    : QObject(parent), m_serverName(serverName), m_server(new QLocalServer(this))
{
#ifndef Q_OS_WIN
    // Unix sockets need explicit owner-only permissions. Windows named pipes
    // inherit the process DACL; forcing UserAccessOption can fail under some
    // valid user-token configurations with "Access is denied".
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
#endif
    connect(m_server, &QLocalServer::newConnection, this, &RuntimeCoordinator::acceptConnection);
}

RuntimeCoordinator::~RuntimeCoordinator()
{
    if (m_isOwner) {
        m_server->close();
        QLocalServer::removeServer(m_serverName);
    }
}

RuntimeCoordinator::StartResult RuntimeCoordinator::startOrNotify(const QString &command)
{
    if (!isAllowedCommand(command)) {
        return StartResult::Unavailable;
    }
    if (listen()) {
        return StartResult::Owner;
    }
    const NotifyResult notifyResult = notifyOwner(command);
    if (notifyResult == NotifyResult::Notified) {
        return StartResult::ClientNotified;
    }
    if (notifyResult != NotifyResult::NoServer) {
        return StartResult::Unavailable;
    }

    // Recover only when Qt reports that no server exists. A busy coordinator
    // must never be replaced merely because it did not answer quickly.
    QLocalServer::removeServer(m_serverName);
    return listen() ? StartResult::Owner : StartResult::Unavailable;
}

void RuntimeCoordinator::dispatchPendingCommands()
{
    const QStringList commands = m_pendingCommands;
    m_pendingCommands.clear();
    for (const QString &command : commands) {
        emit commandReceived(command);
    }
}

bool RuntimeCoordinator::listen()
{
    m_isOwner = m_server->listen(m_serverName);
    if (!m_isOwner) {
        qWarning() << "Runtime coordinator listen failed:" << m_server->errorString();
    }
    return m_isOwner;
}

RuntimeCoordinator::NotifyResult RuntimeCoordinator::notifyOwner(const QString &command) const
{
    QLocalSocket socket;
    socket.connectToServer(m_serverName, QIODevice::WriteOnly);
    if (!socket.waitForConnected(750)) {
        return socket.error() == QLocalSocket::ServerNotFoundError
            ? NotifyResult::NoServer : NotifyResult::Failed;
    }
    const QByteArray payload = command.toUtf8() + '\n';
    if (socket.write(payload) != payload.size() || !socket.waitForBytesWritten(750)) {
        return NotifyResult::Failed;
    }
    socket.disconnectFromServer();
    return NotifyResult::Notified;
}

void RuntimeCoordinator::acceptConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            QByteArray bytes = socket->property("runtimeCoordinatorBuffer").toByteArray();
            bytes += socket->readAll();
            if (bytes.size() > kMaxCommandBytes) {
                socket->disconnectFromServer();
                return;
            }
            const int newline = bytes.indexOf('\n');
            if (newline < 0) {
                socket->setProperty("runtimeCoordinatorBuffer", bytes);
                return;
            }
            handleCommand(QString::fromUtf8(bytes.left(newline)).trimmed());
            socket->disconnectFromServer();
        });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void RuntimeCoordinator::handleCommand(const QString &command)
{
    if (!isAllowedCommand(command)) {
        return;
    }
    m_pendingCommands.append(command);
    QTimer::singleShot(0, this, &RuntimeCoordinator::dispatchPendingCommands);
}
