// =================================================================
// src/core/On4kstClient.cpp  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original -- see On4kstClient.h.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-09 — Created in C++20/Qt6, architecture ported from
//                 Longpath/NereusSDR's DxClusterClient.cpp. AI-assisted
//                 via Anthropic Claude Code, operator Ralph Martin
//                 Fischer.
// =================================================================

#include "On4kstClient.h"

#include "core/SpotParser.h"

#include <algorithm>

namespace Contestprogramm {

namespace {
const QString kClientVersion = QStringLiteral("Contestprogramm-1.0");
} // namespace

On4kstClient::On4kstClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
    , m_connectTimer(new QTimer(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &On4kstClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &On4kstClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &On4kstClient::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &On4kstClient::onSocketError);

    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &On4kstClient::onReconnectTimer);

    // A connect attempt that neither succeeds nor fails within
    // kConnectTimeoutMs is aborted and retried with the usual backoff.
    // A member timer, restarted per attempt: the earlier fire-and-forget
    // QTimer::singleShot could outlive its attempt and abort the next
    // one mid-connect.
    m_connectTimer->setSingleShot(true);
    m_connectTimer->setInterval(kConnectTimeoutMs);
    connect(m_connectTimer, &QTimer::timeout, this, [this] {
        if (!m_connected && m_socket->state() != QAbstractSocket::ConnectedState) {
            m_socket->abort();
            emit connectionError(QStringLiteral("Connection timeout"));
            scheduleReconnect();
        }
    });
}

On4kstClient::~On4kstClient()
{
    m_intentionalDisconnect = true;
    m_reconnectTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
}

void On4kstClient::connectAndLogin(const QString& host, quint16 port,
                                    const QString& callsign, const QString& password,
                                    int chatId)
{
    if (m_connected) {
        return;
    }

    m_host = host;
    m_port = port;
    m_callsign = callsign;
    m_password = password;
    m_chatId = chatId;
    m_loggedIn = false;
    m_intentionalDisconnect = false;
    m_readBuffer.clear();

    m_socket->connectToHost(host, port);
    m_connectTimer->start();
}

void On4kstClient::disconnectFromServer()
{
    m_intentionalDisconnect = true;
    m_reconnectTimer->stop();
    m_connectTimer->stop();
    if (m_connected) {
        sendRaw(QStringLiteral("/QUIT"));
    }
    m_socket->disconnectFromHost();
}

void On4kstClient::sendRaw(const QString& line)
{
    if (!m_connected) {
        return;
    }
    m_socket->write((line + QStringLiteral("\r\n")).toLatin1());
}

void On4kstClient::switchRoom(const QString& value)
{
    sendRaw(QStringLiteral("/CHAT %1").arg(value));
}

void On4kstClient::sendChatMessage(const QString& text)
{
    sendRaw(text);
}

void On4kstClient::sendCqCall(const QString& callsign, const QString& message)
{
    sendRaw(QStringLiteral("/CQ %1 %2").arg(callsign, message));
}

void On4kstClient::sendAway()
{
    sendRaw(QStringLiteral("/AWAY"));
}

void On4kstClient::sendBack()
{
    sendRaw(QStringLiteral("/BACK"));
}

// -- Socket slots --------------------------------------------------------

void On4kstClient::onConnected()
{
    m_connectTimer->stop();
    m_connected = true;
    m_reconnectAttempts = 0;
    emit connected();

    // LOGINC|callsign|password|chat_id|client_version| -- the plan's own
    // wtKST-doc-derived LOGIN| form was live-verified against the real
    // server on 2026-09-13 and turned out to be the WRONG command for a
    // third-party client: Alain (ON4KST sysop), replying directly to
    // Martin's own login-failure report, confirmed the server always
    // treats LOGIN|...|'s password field as a *temporary* password
    // (explaining the earlier "Wrong password!" failures against a real,
    // permanent ON4KST account password) and said to use LOGINC instead
    // ("I propose to use the LOGINC method. You can set parameters in
    // real time." -- 73s Alain, ON4KST). Same field order/count as
    // LOGIN's own -- his message only named the command to swap, not a
    // different field shape -- kept pending a live re-test to confirm.
    sendRaw(QStringLiteral("LOGINC|%1|%2|%3|%4|")
                .arg(m_callsign, m_password, QString::number(m_chatId), kClientVersion));
}

void On4kstClient::onDisconnected()
{
    const bool wasConnected = m_connected;
    m_connected = false;
    m_loggedIn = false;

    if (wasConnected) {
        emit disconnected();
    }

    scheduleReconnect();
}

void On4kstClient::onSocketError(QAbstractSocket::SocketError /*err*/)
{
    emit connectionError(m_socket->errorString());
    // A refused or failed connect never produces disconnected() -- the
    // socket drops straight back to Unconnected -- so the retry has to
    // be armed here too, or a server that is not reachable when this
    // client first dials (no internet yet at the contest site) is never
    // dialed again (found 2026-09-21, the same gap as in RotctldClient).
    if (!m_connected && m_socket->state() == QAbstractSocket::UnconnectedState) {
        scheduleReconnect();
    }
}

void On4kstClient::scheduleReconnect()
{
    if (m_intentionalDisconnect || m_reconnectTimer->isActive()) {
        return;
    }
    // Exponential backoff, shift count clamped to avoid signed-int UB;
    // saturates at kMaxReconnectDelayMs well before the clamp matters.
    const int shiftBits = std::min(m_reconnectAttempts, 30);
    const int delay = std::min(kInitialReconnectDelayMs * (1 << shiftBits), kMaxReconnectDelayMs);
    m_reconnectTimer->start(delay);
    m_reconnectAttempts++;
}

void On4kstClient::onReconnectTimer()
{
    if (m_intentionalDisconnect) {
        return;
    }
    connectAndLogin(m_host, m_port, m_callsign, m_password, m_chatId);
}

// -- Line-buffered read ---------------------------------------------------

void On4kstClient::stripTelnetIACBuffer(QByteArray& buf)
{
    // Same algorithm as DxClusterClient::stripTelnetIACBuffer (skip
    // 0xFF + 2 command bytes per IAC sequence). ON4KST's port 23001 is
    // its own application protocol rather than raw telnet, so this is
    // mostly a defensive no-op here -- kept for architectural parity
    // with the template and in case a stray 0xFF ever appears on the
    // wire.
    int i = 0;
    while (i < buf.size()) {
        if (static_cast<unsigned char>(buf[i]) == 0xFF && i + 2 < buf.size()) {
            buf.remove(i, 3);
        } else {
            i++;
        }
    }
}

void On4kstClient::stripTelnetIAC()
{
    stripTelnetIACBuffer(m_readBuffer);
}

void On4kstClient::onReadyRead()
{
    m_readBuffer.append(m_socket->readAll());
    stripTelnetIAC();

    while (true) {
        const int idx = m_readBuffer.indexOf('\n');
        if (idx < 0) {
            break;
        }

        const QString line = QString::fromLatin1(m_readBuffer.left(idx)).trimmed();
        m_readBuffer.remove(0, idx + 1);

        if (line.isEmpty()) {
            continue;
        }

        emit rawLineReceived(line);
        handleLine(line);
    }
}

void On4kstClient::handleLine(const QString& line)
{
    if (line.startsWith(QStringLiteral("LOGSTAT|"))) {
        const QStringList fields = line.split(QLatin1Char('|'));
        const int code = fields.size() > 1 ? fields.at(1).toInt() : 0;
        // Real bug, found live 2026-09-13 against the actual server with
        // a real account: LOGSTAT is not exclusively an error report --
        // code 100 is the SUCCESS acknowledgement, with a much longer,
        // differently-shaped line than the two-field error case ("LOGSTAT|
        // 100|<chat_id>|<session_id>|<hash>|<flag>|<firstname>|<lastname>|
        // <grid>|<email>|", confirmed via a direct nc test and this
        // client's own live diagnostic). Treating every LOGSTAT line as a
        // failure (the original code here) meant this client disconnected
        // itself the instant the server confirmed a SUCCESSFUL login --
        // silently breaking every ON4KST connection attempt regardless of
        // how correct the credentials were. That self-disconnect is fixed
        // below by treating code 100 as the actual login success itself.
        //
        // Second bug, found the same day from the sysop's own protocol
        // doc (dl8aau/wtkst-derived text, forwarded by the operator):
        // SDONE|chat_id| is a CLIENT-TO-SERVER command, not something the
        // server ever sends back -- "End of the settings frames... This
        // frame is needed to [...] allow the sending of all frames from
        // the server." LOGINC specifically "is waiting [for] the end
        // settings frame (SDONE)" before it starts streaming DL/CH/etc.
        // A 20s live diagnostic confirmed this directly: LOGSTAT|100|
        // arrives and then nothing else ever does, because the server is
        // sitting there waiting for US to send SDONE. So logging in via
        // LOGINC is a two-step handshake: receive LOGSTAT|100|, then send
        // SDONE|<chat_id>| ourselves to unblock the server. Skipping the
        // optional SDXQ/SMAQ/RDXQ/RMAQ frames the doc allows between
        // LOGINC and SDONE -- this client doesn't restrict QRG ranges
        // server-side, GeoFilter/DupeChecker already do that client-side.
        if (code == 100) {
            const int chatId = fields.size() > 2 ? fields.at(2).toInt() : m_chatId;
            if (!m_loggedIn) {
                m_loggedIn = true;
                m_reconnectAttempts = 0;
            }
            sendRaw(QStringLiteral("SDONE|%1|").arg(chatId));
            emit loggedIn(chatId);
            return;
        }
        const QString message = fields.size() > 2 ? fields.at(2) : QString();
        emit loginFailed(code, message);
        if (!m_loggedIn) {
            // A login-time failure (wrong password, unknown user) will
            // fail identically on every retry -- stop hammering it and
            // leave reconnecting to a deliberate action (e.g. the
            // operator fixing credentials in Settings) instead of the
            // usual auto-reconnect.
            m_intentionalDisconnect = true;
            m_socket->disconnectFromHost();
        }
        return;
    }

    if (line.startsWith(QStringLiteral("SDONE|"))) {
        // Per the sysop's protocol doc, SDONE is a client-to-server
        // command (see the LOGSTAT|100| branch above) -- the real server
        // never sends this back. Kept as a defensive, idempotent handler
        // in case some server path does echo or send it; not relied on
        // as the login-success trigger anymore.
        const QStringList fields = line.split(QLatin1Char('|'));
        const int chatId = fields.size() > 1 ? fields.at(1).toInt() : m_chatId;
        if (!m_loggedIn) {
            m_loggedIn = true;
            m_reconnectAttempts = 0;
        }
        emit loggedIn(chatId);
        return;
    }

    if (line.startsWith(QStringLiteral("CK|"))) {
        // Server keepalive challenge -- reply with a bare CRLF, per the
        // plan's protocol section.
        m_socket->write("\r\n");
        return;
    }

    SpotCandidate candidate;
    if (SpotParser::parseDxSpotLine(line, candidate)) {
        emit spotReceived(candidate);
        return;
    }
    if (SpotParser::parseChatLine(line, candidate)) {
        emit chatLineReceived(candidate);
    }
}

bool On4kstClient::parseDxSpotLineForTest(const QString& line, SpotCandidate& candidateOut)
{
    return SpotParser::parseDxSpotLine(line, candidateOut);
}

bool On4kstClient::parseChatLineForTest(const QString& line, SpotCandidate& candidateOut)
{
    return SpotParser::parseChatLine(line, candidateOut);
}

} // namespace Contestprogramm
