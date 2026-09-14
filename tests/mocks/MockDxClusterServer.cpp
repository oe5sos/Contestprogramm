#include "MockDxClusterServer.h"

#include <QHostAddress>

namespace Contestprogramm {

MockDxClusterServer::MockDxClusterServer(QObject* parent) : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &MockDxClusterServer::onNewConnection);
}

bool MockDxClusterServer::startListening()
{
    return m_server.listen(QHostAddress::LocalHost, 0);
}

quint16 MockDxClusterServer::port() const
{
    return m_server.serverPort();
}

void MockDxClusterServer::onNewConnection()
{
    m_client = m_server.nextPendingConnection();
    if (!m_client) {
        return;
    }
    connect(m_client, &QTcpSocket::readyRead, this, &MockDxClusterServer::onReadyRead);

    // Banner + login prompt, no trailing newline -- see the header
    // comment for why that is deliberate.
    m_client->write("Mock DX Cluster node\r\nlogin: ");
}

void MockDxClusterServer::onReadyRead()
{
    if (!m_client) {
        return;
    }
    m_buffer += m_client->readAll();

    if (!m_callsignReceived.isEmpty()) {
        return; // already logged in; nothing else expected from the client in this mock
    }

    const int idx = m_buffer.indexOf('\n');
    if (idx < 0) {
        return;
    }
    m_callsignReceived = m_buffer.left(idx).trimmed();
    m_buffer.remove(0, idx + 1);

    m_client->write("DX de W3LPL:     14025.0  JA1ABC       CW big signal       1824Z\r\n");
    m_client->write("14310.0 VK2IO/P     12-May-2026 0449Z WWFF VKFF-5514     <OH0M>\r\n");
    m_spotsSent = true;
    emit spotsSent();
}

} // namespace Contestprogramm
