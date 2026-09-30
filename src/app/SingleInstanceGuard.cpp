#include "app/SingleInstanceGuard.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QLocalSocket>
#include <QTimer>

namespace Contestprogramm {

namespace {
constexpr int kConnectTimeoutMs = 1500;
// "raise <build stamp> <program path>\n", one line. Stamp and path are
// optional on the wire: builds before 2026-09-21 send a bare "raise",
// builds before 2026-09-30 "raise <stamp>". The path is percent-encoded
// (it may hold spaces) and only sent together with a stamp.
const QByteArray kRaiseVerb = QByteArrayLiteral("raise");

QString executableStamp()
{
    const QFileInfo exe(QCoreApplication::applicationFilePath());
    return exe.exists() ? QString::number(exe.lastModified().toMSecsSinceEpoch()) : QString();
}

// The program a second start named, if it is one: an absolute path to
// an executable file. Anything else is dropped, and the restart falls
// back to the running program's own path.
QString handedProgram(const QByteArray& field)
{
    if (field.isEmpty()) {
        return {};
    }
    const QString path = QString::fromUtf8(QByteArray::fromPercentEncoding(field));
    const QFileInfo program(path);
    return program.isAbsolute() && program.isFile() && program.isExecutable() ? path : QString();
}
} // namespace

SingleInstanceGuard::SingleInstanceGuard(const QString& dataDir, QObject* parent)
    : QObject(parent)
    , m_dataDir(dataDir)
    , m_buildStamp(executableStamp())
    , m_programPath(QCoreApplication::applicationFilePath())
{
}

void SingleInstanceGuard::setBuildStamp(const QString& stamp)
{
    m_buildStamp = stamp;
}

void SingleInstanceGuard::setProgramPath(const QString& path)
{
    m_programPath = path;
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
        // Not deleted here: release() is called from newerBuildStarted
        // (main.cpp's restart), i.e. from inside a client socket's
        // readyRead -- and the client is the server's child. Deleting
        // the server took the socket with it while Qt was still in its
        // read notification, which then emitted channelReadyRead() on
        // freed memory (crash 2026-09-28 11:01:33, second build started
        // beside the running one). close() already stops listening and
        // frees the name, the object itself can go once the stack is
        // unwound.
        m_server.release()->deleteLater();
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
            QByteArray line = kRaiseVerb + ' ' + m_buildStamp.toUtf8();
            if (!m_buildStamp.isEmpty() && !m_programPath.isEmpty()) {
                line += ' ' + m_programPath.toUtf8().toPercentEncoding("/");
            }
            socket.write(line + '\n');
            socket.flush();
            // Wait for the running instance's "ok" before hanging up --
            // with the event loop running, not waitForReadyRead(): that
            // one blocks the thread, and when the running instance is
            // the same process (the tests) it can never answer, so the
            // line was hung up on unacknowledged. On Windows a named
            // pipe closed that way reached the server without the line
            // (2026-09-21, CI); a two-process hand-over is unchanged.
            QEventLoop wait;
            connect(&socket, &QLocalSocket::readyRead, &wait, &QEventLoop::quit);
            connect(&socket, &QLocalSocket::disconnected, &wait, &QEventLoop::quit);
            connect(&socket, &QLocalSocket::errorOccurred, &wait, &QEventLoop::quit);
            QTimer::singleShot(kConnectTimeoutMs, &wait, &QEventLoop::quit);
            wait.exec();
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
    // The server by pointer, not m_server: handle() below may run
    // release() right here, which empties m_server while this loop is
    // still asking for the next connection (none, once closed).
    connect(m_server.get(), &QLocalServer::newConnection, this, [this, server = m_server.get()] {
        while (QLocalSocket* client = server->nextPendingConnection()) {
            const auto handle = [this, client] {
                // Whole lines only: with a program path the line is long
                // enough to arrive in two reads, and every build so far
                // ends it with '\n'.
                if (!client->canReadLine()) {
                    return;
                }
                const QByteArray line = client->readLine().trimmed();
                if (!line.startsWith(kRaiseVerb)) {
                    return;
                }
                client->write("ok\n");
                client->flush();
                const QList<QByteArray> fields = line.mid(kRaiseVerb.size()).simplified().split(' ');
                const QString theirStamp = QString::fromUtf8(fields.value(0));
                if (!theirStamp.isEmpty() && !m_buildStamp.isEmpty() && theirStamp != m_buildStamp) {
                    emit newerBuildStarted(handedProgram(fields.value(1)));
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
