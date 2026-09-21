#include "app/SingleInstanceGuard.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>

namespace Contestprogramm {

namespace {
constexpr int kConnectTimeoutMs = 1500;
// "raise <build stamp>\n" -- the stamp is optional on the wire (an
// older build sends none), see SingleInstanceGuard::setBuildStamp().
const QByteArray kRaiseVerb = QByteArrayLiteral("raise");

QString executableStamp()
{
    const QFileInfo exe(QCoreApplication::applicationFilePath());
    return exe.exists() ? QString::number(exe.lastModified().toMSecsSinceEpoch()) : QString();
}
} // namespace

SingleInstanceGuard::SingleInstanceGuard(const QString& dataDir, QObject* parent)
    : QObject(parent)
    , m_dataDir(dataDir)
    , m_buildStamp(executableStamp())
{
}

void SingleInstanceGuard::setBuildStamp(const QString& stamp)
{
    m_buildStamp = stamp;
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
            socket.write(kRaiseVerb + ' ' + m_buildStamp.toUtf8() + '\n');
            socket.waitForBytesWritten(kConnectTimeoutMs);
            // Wait for the running instance's "ok" before hanging up:
            // on Windows a named pipe closed right after the write can
            // reach the server without the line (2026-09-21, CI), on
            // macOS it merely never mattered.
            socket.waitForReadyRead(kConnectTimeoutMs);
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
            const auto handle = [this, client] {
                const QByteArray line = client->readAll().trimmed();
                if (!line.startsWith(kRaiseVerb)) {
                    return;
                }
                client->write("ok\n");
                client->flush();
                const QString theirStamp = QString::fromUtf8(line.mid(kRaiseVerb.size()).trimmed());
                if (!theirStamp.isEmpty() && !m_buildStamp.isEmpty() && theirStamp != m_buildStamp) {
                    emit newerBuildStarted();
                    return;
                }
                emit activateRequested();
            };
            connect(client, &QLocalSocket::readyRead, this, handle);
            connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
            // The line may already be in the buffer when the connection
            // is handed to us (Windows) -- readyRead then never fires.
            if (client->bytesAvailable() > 0) {
                handle();
            }
        }
    });
    return true;
}

} // namespace Contestprogramm
