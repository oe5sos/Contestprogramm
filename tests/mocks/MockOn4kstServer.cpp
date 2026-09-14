#include "MockOn4kstServer.h"

#include <QHostAddress>

namespace Contestprogramm {

MockOn4kstServer::MockOn4kstServer(QObject* parent) : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &MockOn4kstServer::onNewConnection);
}

bool MockOn4kstServer::startListening()
{
    return m_server.listen(QHostAddress::LocalHost, 0);
}

quint16 MockOn4kstServer::port() const
{
    return m_server.serverPort();
}

void MockOn4kstServer::onNewConnection()
{
    m_client = m_server.nextPendingConnection();
    if (!m_client) {
        return;
    }
    connect(m_client, &QTcpSocket::readyRead, this, &MockOn4kstServer::onReadyRead);
}

void MockOn4kstServer::onReadyRead()
{
    if (!m_client) {
        return;
    }
    m_buffer += m_client->readAll();

    if (m_keepaliveSent) {
        // Anything the client sends after the keepalive challenge is
        // the reply under test -- capture it verbatim rather than
        // trying to parse it (a bare "\r\n" has nothing to parse).
        m_afterKeepalive += m_buffer;
        m_buffer.clear();
        return;
    }

    while (true) {
        const int idx = m_buffer.indexOf('\n');
        if (idx < 0) {
            break;
        }
        const QByteArray line = m_buffer.left(idx).trimmed();
        m_buffer.remove(0, idx + 1);

        if (!m_loggedIn && line.startsWith("LOGIN")) {
            m_loggedIn = true;
            m_client->write("SDONE|2|\r\n");
            m_client->write("DL|1700000000|1200Z|OE1TST|144300.0|OE3TST|FT8 -12dB|JN78|JN77|\r\n");
            m_client->write("CH|2|1200Z|OE7TST|Hans|ALL|CQ CQ JN88TC|0|\r\n");
            m_client->write("CK|\r\n");
            m_keepaliveSent = true;
            emit keepaliveSent();
        }
    }
}

} // namespace Contestprogramm
