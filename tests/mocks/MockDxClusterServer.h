#pragma once

#include <QByteArray>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

namespace Contestprogramm {

// QTcpServer-based classic-DX-cluster stand-in for
// tests/test_dxclusterprotocol.cpp, mirroring MockOn4kstServer's
// pattern. Listens on an ephemeral loopback port, accepts one client,
// writes a banner ending in a login prompt WITHOUT a trailing newline
// (deliberately -- exercises DxClusterClient::onReadyRead's
// no-newline-yet login-prompt branch, the one real DX cluster nodes
// commonly trigger), then once it sees the callsign reply plays back
// one spot line in each supported format (classic "DX de" and DXSpider).
//
// Deliberately not a real cluster node -- no PC-protocol commands, no
// multi-client support, no filter handling. Cluster-protocol
// live-verification against a real node is out of scope for this pass
// (same reasoning as the plan's ON4KST "Offene Punkte" deferral).
class MockDxClusterServer : public QObject {
    Q_OBJECT

public:
    explicit MockDxClusterServer(QObject* parent = nullptr);

    bool startListening(); // picks an ephemeral loopback port
    quint16 port() const;

    // The line the client sent in reply to the login prompt (its
    // callsign, per DxClusterClient::handleLine's login-prompt branch).
    QByteArray callsignReceived() const { return m_callsignReceived; }

signals:
    void spotsSent();

private slots:
    void onNewConnection();
    void onReadyRead();

private:
    QTcpServer m_server;
    QTcpSocket* m_client = nullptr;
    QByteArray  m_buffer;
    QByteArray  m_callsignReceived;
    bool m_spotsSent = false;
};

} // namespace Contestprogramm
