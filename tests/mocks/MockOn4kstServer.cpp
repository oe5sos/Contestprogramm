#include "MockOn4kstServer.h"

#include <QDebug>
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
        // Die Antwort auf CK| mitschreiben -- ABER weiterparsen: früher
        // stand hier ein return, und damit verschluckte der Prüfstand
        // alles, was der Client nach dem ersten Keepalive noch schickte.
        // Ein zweites LOGINC (Raumwechsel) kam deshalb nie an, und der
        // Prüfstand behauptete, der Wechsel funktioniere nicht.
        m_afterKeepalive += m_buffer;
    }

    while (true) {
        const int idx = m_buffer.indexOf('\n');
        if (idx < 0) {
            break;
        }
        const QByteArray line = m_buffer.left(idx).trimmed();
        m_buffer.remove(0, idx + 1);

        if (line.startsWith("LOGIN")) {
            ++m_loginAttempts;
            if (m_rejectCode != 0) {
                const int code = m_rejectCode;
                m_rejectCode = 0; // nur dieser eine Versuch
                m_client->write("LOGSTAT|" + QByteArray::number(code) + "|"
                                + m_rejectMessage.toLatin1() + "|\r\n");
                continue;
            }
            // Die vierte Spalte von LOGINC|call|pw|chat_id|version| ist
            // der Raum. Der echte Server bestätigt ihn und schickt
            // danach Zeilen aus GENAU diesem Raum -- der Prüfstand tut
            // dasselbe, sonst ließe sich ein Raumwechsel nicht prüfen.
            const QList<QByteArray> felder = line.split('|');
            m_lastChatId = felder.size() > 3 ? felder.at(3).toInt() : 2;
            m_loggedIn = true;
            const QByteArray raum = QByteArray::number(m_lastChatId);
            m_client->write("SDONE|" + raum + "|\r\n");
            m_client->write("DL|1700000000|1200Z|OE1TST|144300.0|OE3TST|FT8 -12dB|JN78|JN77|\r\n");
            m_client->write("CH|" + raum + "|1200Z|OE7TST|Hans|ALL|CQ CQ JN88TC|0|\r\n");
            if (!m_keepaliveSent) {
                m_client->write("CK|\r\n");
                m_keepaliveSent = true;
                emit keepaliveSent();
            }
        }
    }
}

void MockOn4kstServer::rejectNextLogin(int code, const QString& message)
{
    m_rejectCode = code;
    m_rejectMessage = message;
}

} // namespace Contestprogramm
