// =================================================================
// src/core/DxClusterClient.cpp  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original -- see DxClusterClient.h for the full
// attribution note (ported nearly verbatim from Longpath/NereusSDR's
// DxClusterClient, itself GPL-3.0-or-later ported from AetherSDR).
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-09 — Created in C++20/Qt6, ported nearly verbatim from
//                 Longpath/NereusSDR's DxClusterClient.cpp.
//                 AI-assisted via Anthropic Claude Code, operator
//                 Ralph Martin Fischer.
// =================================================================

#include "DxClusterClient.h"

#include <QDateTime>
#include <QRegularExpression>

#include <algorithm>

namespace Contestprogramm {

DxClusterClient::DxClusterClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
    , m_connectTimer(new QTimer(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &DxClusterClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &DxClusterClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &DxClusterClient::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &DxClusterClient::onSocketError);

    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &DxClusterClient::onReconnectTimer);

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

DxClusterClient::~DxClusterClient()
{
    m_intentionalDisconnect = true;
    m_reconnectTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
}

void DxClusterClient::connectToCluster(const QString& host, quint16 port, const QString& callsign)
{
    if (m_connected) {
        return;
    }

    m_host = host;
    m_port = port;
    m_callsign = callsign;
    m_loggedIn = false;
    m_intentionalDisconnect = false;
    m_readBuffer.clear();

    m_socket->connectToHost(host, port);
    m_connectTimer->start();
}

void DxClusterClient::disconnectFromCluster()
{
    m_intentionalDisconnect = true;
    m_reconnectTimer->stop();
    m_connectTimer->stop();
    if (m_connected) {
        m_socket->write("bye\r\n");
        m_socket->flush();
    }
    m_socket->disconnectFromHost();
}

void DxClusterClient::sendCommand(const QString& cmd)
{
    if (!m_connected) {
        return;
    }
    m_socket->write((cmd + QStringLiteral("\r\n")).toLatin1());
}

// -- Socket slots --------------------------------------------------------

void DxClusterClient::onConnected()
{
    m_connectTimer->stop();
    m_connected = true;
    m_reconnectAttempts = 0;
    emit connected();
}

void DxClusterClient::onDisconnected()
{
    const bool wasConnected = m_connected;
    m_connected = false;
    m_loggedIn = false;

    if (wasConnected) {
        emit disconnected();
    }

    scheduleReconnect();
}

void DxClusterClient::onSocketError(QAbstractSocket::SocketError /*err*/)
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

void DxClusterClient::scheduleReconnect()
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

void DxClusterClient::onReconnectTimer()
{
    if (m_intentionalDisconnect) {
        return;
    }
    connectToCluster(m_host, m_port, m_callsign);
}

// -- Line-buffered read ---------------------------------------------------

void DxClusterClient::stripTelnetIACBuffer(QByteArray& buf)
{
    // Skip 0xFF + 2 command bytes per IAC sequence -- same algorithm as
    // On4kstClient::stripTelnetIACBuffer / the NereusSDR original.
    int i = 0;
    while (i < buf.size()) {
        if (static_cast<unsigned char>(buf[i]) == 0xFF && i + 2 < buf.size()) {
            buf.remove(i, 3);
        } else {
            i++;
        }
    }
}

void DxClusterClient::stripTelnetIAC()
{
    stripTelnetIACBuffer(m_readBuffer);
}

void DxClusterClient::onReadyRead()
{
    m_readBuffer.append(m_socket->readAll());
    stripTelnetIAC();

    while (true) {
        const int idx = m_readBuffer.indexOf('\n');
        if (idx < 0) {
            // No newline yet -- a login prompt may not end with one
            // ("login: " with no trailing newline is common).
            if (!m_loggedIn) {
                const QString partial = QString::fromLatin1(m_readBuffer).trimmed();
                if (isLoginPrompt(partial)) {
                    m_readBuffer.clear();
                    m_socket->write((m_callsign + QStringLiteral("\r\n")).toLatin1());
                    m_loggedIn = true;
                }
            }
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

void DxClusterClient::handleLine(const QString& line)
{
    if (!m_loggedIn && isLoginPrompt(line)) {
        m_socket->write((m_callsign + QStringLiteral("\r\n")).toLatin1());
        m_loggedIn = true;
        return;
    }

    SpotCandidate candidate;
    if (parseDxSpotLine(line, candidate)) {
        emit spotReceived(candidate);
    }
}

// -- Login prompt detection ------------------------------------------------

bool DxClusterClient::isLoginPrompt(const QString& line)
{
    // DX cluster servers vary: "login:", "call:", "callsign:", "Please
    // enter your call", "your call>", "Enter your callsign".
    const QString lower = line.toLower();
    if (lower.endsWith(QStringLiteral("login:"))
        || lower.endsWith(QStringLiteral("call:"))
        || lower.endsWith(QStringLiteral("callsign:"))) {
        return true;
    }
    if (lower.contains(QStringLiteral("enter your call")) || lower.contains(QStringLiteral("your call"))) {
        return true;
    }
    return false;
}

// -- DX spot line parser ----------------------------------------------------

bool DxClusterClient::parseDxSpotLine(const QString& line, SpotCandidate& candidateOut)
{
    // Format A -- classic "DX de" header (AR-Cluster, some CC-Cluster):
    //   "DX de W3LPL:     14025.0  JA1ABC       CW big signal       1824Z"
    static const QRegularExpression rxClassic(
        R"(^DX\s+de\s+(\S+?):\s+(\d+\.?\d*)\s+(\S+)\s+(.*?)\s+(\d{4})Z)",
        QRegularExpression::CaseInsensitiveOption);

    // Format B -- DXSpider standard output:
    //   "14310.0 VK2IO/P     12-May-2026 0449Z WWFF VKFF-5514     <OH0M>"
    // Tokens: FREQ_KHZ CALL DD-MMM-YYYY HHMMZ COMMENT... <SPOTTER>
    static const QRegularExpression rxDxspider(
        R"(^(\d+\.?\d*)\s+(\S+)\s+\d{1,2}-\w+-\d{4}\s+(\d{4})Z\s+(.*?)\s+<(\S+)>)",
        QRegularExpression::CaseInsensitiveOption);

    QString freqStr;
    QString callStr;
    QString timeStr;

    QRegularExpressionMatch match = rxClassic.match(line);
    if (match.hasMatch()) {
        freqStr = match.captured(2);
        callStr = match.captured(3);
        timeStr = match.captured(5);
    } else {
        match = rxDxspider.match(line);
        if (!match.hasMatch()) {
            return false;
        }
        freqStr = match.captured(1);
        callStr = match.captured(2);
        timeStr = match.captured(3);
    }

    const double freqKhz = freqStr.toDouble();
    if (!(freqKhz > 0.0) || callStr.isEmpty()) {
        return false;
    }

    // "Z is the terminator" -- ignore any trailing chars some nodes
    // append (BEL/NUL); parsed defensively, falling back to "now" (UTC)
    // rather than rejecting the whole spot over an odd time field.
    QDateTime timestamp = QDateTime::currentDateTimeUtc();
    if (timeStr.size() >= 4) {
        const int hh = timeStr.left(2).toInt();
        const int mm = timeStr.mid(2, 2).toInt();
        if (hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59) {
            timestamp.setTime(QTime(hh, mm));
        }
    }

    SpotCandidate candidate;
    candidate.callsign = callStr.toUpper();
    // Classic cluster spots carry no grid locator -- GeoFilter treats
    // an empty grid as "distance unknown", not "out of range" (see
    // GeoFilter.h), so this candidate stays visible rather than being
    // silently dropped for lacking location data DX clusters never had
    // in the first place.
    candidate.rawLine = line;
    candidate.timestampUtc = timestamp;
    candidate.source = QStringLiteral("cluster");
    candidate.freqHz = static_cast<qint64>(freqKhz * 1000.0);

    candidateOut = candidate;
    return true;
}

bool DxClusterClient::parseDxSpotLineForTest(const QString& line, SpotCandidate& candidateOut)
{
    return parseDxSpotLine(line, candidateOut);
}

} // namespace Contestprogramm
