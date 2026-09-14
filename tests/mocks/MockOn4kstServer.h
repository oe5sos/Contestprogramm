#pragma once

#include <QByteArray>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

namespace Contestprogramm {

// QTcpServer-based ON4KST stand-in for tests/test_on4kstprotocol.cpp.
// Listens on an ephemeral loopback port, accepts one client, and once
// it sees a LOGIN| line plays back a fixed script: SDONE| (login
// confirmed), one DL| spot line, one CH| chat line, then a CK|
// keepalive challenge.
//
// Deliberately not a real ON4KST server -- no credential checking, no
// multi-client support, no room-switch handling. The plan's own
// "Offene Punkte" section reserves live-server verification for the
// operator, separately, later; this mock (and the test that drives it)
// never touches the real www.on4kst.org:23001.
class MockOn4kstServer : public QObject {
    Q_OBJECT

public:
    explicit MockOn4kstServer(QObject* parent = nullptr);

    bool startListening(); // picks an ephemeral loopback port
    quint16 port() const;

    // Bytes the client sent after the CK| keepalive was written -- the
    // test reads this back to confirm the required bare "\r\n" reply.
    QByteArray bytesReceivedAfterKeepalive() const { return m_afterKeepalive; }

signals:
    void keepaliveSent();

private slots:
    void onNewConnection();
    void onReadyRead();

private:
    QTcpServer m_server;
    QTcpSocket* m_client = nullptr;
    QByteArray  m_buffer;
    QByteArray  m_afterKeepalive;
    bool m_loggedIn = false;
    bool m_keepaliveSent = false;
};

} // namespace Contestprogramm
