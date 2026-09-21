#include "app/SingleInstanceGuard.h"

#include <QCryptographicHash>
#include <QDir>
#include <QLocalSocket>

namespace Contestprogramm {

namespace {
constexpr int kConnectTimeoutMs = 1500;
const QByteArray kRaiseMessage = QByteArrayLiteral("raise\n");
} // namespace

SingleInstanceGuard::SingleInstanceGuard(const QString& dataDir, QObject* parent)
    : QObject(parent)
    , m_dataDir(dataDir)
{
}

SingleInstanceGuard::~SingleInstanceGuard()
{
    release();
}

void SingleInstanceGuard::release()
{
    if (m_server) {
        m_server->close();
        QLocalServer::removeServer(m_server->serverName());
        m_server.reset();
    }
    if (m_lock) {
        m_lock->unlock();
        m_lock.reset();
    }
}

QString SingleInstanceGuard::serverNameFor(const QString& dataDir)
{
    // A short, path-safe name: the data directory's hash, so a test
    // instance on CONTESTPROGRAMM_DATA_DIR and the real one never
    // answer each other.
    const QByteArray digest = QCryptographicHash::hash(QDir(dataDir).absolutePath().toUtf8(), QCryptographicHash::Sha1);
    return QStringLiteral("contestprogramm-") + QString::fromLatin1(digest.toHex().left(16));
}

bool SingleInstanceGuard::tryAcquire()
{
    const QString serverName = serverNameFor(m_dataDir);
    m_lock = std::make_unique<QLockFile>(QDir(m_dataDir).filePath(QStringLiteral("contestprogramm.lock")));
    m_lock->setStaleLockTime(0); // stale means "its process is gone", nothing time-based
    if (!m_lock->tryLock(200)) {
        // Somebody holds it: ask them to come forward.
        QLocalSocket socket;
        socket.connectToServer(serverName);
        if (socket.waitForConnected(kConnectTimeoutMs)) {
            socket.write(kRaiseMessage);
            socket.waitForBytesWritten(kConnectTimeoutMs);
            socket.disconnectFromServer();
            m_lock.reset();
            return false;
        }
        // Lock held, nobody answering: a stale lock QLockFile could not
        // tell apart (or a hung process). The operator wins.
        m_lock->removeStaleLockFile();
        if (!m_lock->tryLock(200)) {
            m_lock.reset();
            return true;
        }
    }
    m_server = std::make_unique<QLocalServer>(this);
    QLocalServer::removeServer(serverName); // leftover socket file from a crash
    m_server->listen(serverName);
    connect(m_server.get(), &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket* client = m_server->nextPendingConnection()) {
            connect(client, &QLocalSocket::readyRead, this, [this, client] {
                if (client->readAll().contains("raise")) {
                    emit activateRequested();
                }
            });
            connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        }
    });
    return true;
}

} // namespace Contestprogramm
