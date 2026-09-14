// =================================================================
// src/core/RotctldClient.cpp  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original -- see RotctldClient.h.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-09 — Created in C++20/Qt6, architecturally ported from
//                 Longpath/NereusSDR's RotctldClient.cpp. AI-assisted
//                 via Anthropic Claude Code, operator Ralph Martin
//                 Fischer.
// =================================================================

#include "RotctldClient.h"

#include <QTimer>

#include <cmath>

namespace Contestprogramm {

namespace {

double norm360(double d)
{
    d = std::fmod(d, 360.0);
    return d < 0.0 ? d + 360.0 : d;
}

} // namespace

RotctldClient::RotctldClient(QObject* parent) : QObject(parent)
{
    m_poll = new QTimer(this);
    m_poll->setInterval(m_pollMs);
    connect(m_poll, &QTimer::timeout, this, [this]() {
        // One outstanding command at a time -- rotctld answers in
        // order, and a queue of stale position requests only delays a
        // set-position command sent from the UI behind them.
        if (m_awaiting == Pending::None && m_queue.isEmpty()) {
            send(QByteArrayLiteral("p\n"), Pending::Position);
        }
    });

    m_retry = new QTimer(this);
    m_retry->setSingleShot(true);
    m_retry->setInterval(3000);
    connect(m_retry, &QTimer::timeout, this, [this]() {
        if (m_state == State::Disconnected && !m_host.isEmpty()) {
            connectToRotor();
        }
    });

    // Reply watchdog -- see RotctldClient.h / kReplyTimeoutMs. Without
    // it, a rotctld that freezes while its TCP side stays open leaves
    // m_awaiting set forever and the client is silently dead until
    // someone reconnects by hand.
    m_deadline = new QTimer(this);
    m_deadline->setSingleShot(true);
    m_deadline->setInterval(kReplyTimeoutMs);
    connect(m_deadline, &QTimer::timeout, this, [this]() {
        if (m_awaiting == Pending::None) { return; }
        emit errorOccurred(QStringLiteral("rotctld stopped answering — reconnecting"));
        m_socket.abort();
        m_poll->stop();
        m_queue.clear();
        m_buffer.clear();
        m_awaiting = Pending::None;
        setState(State::Disconnected);
        if (!m_host.isEmpty()) { m_retry->start(); }
    });

    connect(&m_socket, &QTcpSocket::connected, this, [this]() {
        m_buffer.clear();
        m_queue.clear();
        m_awaiting = Pending::None;
        setState(State::Connected);
        m_poll->start();
        send(QByteArrayLiteral("p\n"), Pending::Position);
    });

    connect(&m_socket, &QTcpSocket::readyRead, this, &RotctldClient::onReadyRead);

    connect(&m_socket, &QTcpSocket::disconnected, this, [this]() {
        m_poll->stop();
        m_deadline->stop();
        setState(State::Disconnected);
        // Keep trying -- a rotor controller that is power-cycled mid
        // contest should come back on its own rather than needing the
        // operator to notice and reconnect by hand.
        if (!m_host.isEmpty()) { m_retry->start(); }
    });

    connect(&m_socket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        fail(m_socket.errorString());
    });
}

// -- Configuration ---------------------------------------------------

void RotctldClient::setTarget(const QString& host, quint16 port)
{
    m_host = host.trimmed();
    m_port = port;
}

void RotctldClient::setPollIntervalMs(int ms)
{
    m_pollMs = qBound(100, ms, 10000);
    m_poll->setInterval(m_pollMs);
}

QString RotctldClient::description() const
{
    if (m_host.isEmpty()) { return QStringLiteral("rotctld (not set up)"); }
    return QStringLiteral("rotctld %1:%2").arg(m_host).arg(m_port);
}

// -- Connection --------------------------------------------------------

void RotctldClient::connectToRotor()
{
    if (m_host.isEmpty()) {
        emit errorOccurred(QStringLiteral("No rotor address set"));
        return;
    }
    if (m_socket.state() != QAbstractSocket::UnconnectedState) { return; }

    m_retry->stop();
    setState(State::Connecting);
    m_socket.connectToHost(m_host, m_port);
}

void RotctldClient::disconnectFromRotor()
{
    // Explicit: stop retrying too, otherwise "disconnect" reconnects a
    // few seconds later and looks like the button does nothing.
    m_retry->stop();
    m_poll->stop();
    m_deadline->stop();
    m_queue.clear();
    m_awaiting = Pending::None;
    m_socket.abort();
    setState(State::Disconnected);
}

// -- Commands ----------------------------------------------------------

QByteArray RotctldClient::setPositionCommand(double azimuthDeg, double elevationDeg)
{
    // Plain decimals -- rotctld wants "145.00"; a decimal comma is
    // rejected and the antenna then simply does not move. toLatin1()
    // (not a locale-aware formatter) is deliberate for the same reason
    // documented on the NereusSDR original.
    return QStringLiteral("P %1 %2\n")
        .arg(norm360(azimuthDeg), 0, 'f', 2)
        .arg(elevationDeg, 0, 'f', 2)
        .toLatin1();
}

void RotctldClient::setAzimuth(double azimuthDeg)
{
    if (!isConnected()) {
        emit errorOccurred(QStringLiteral("Rotor is not connected"));
        return;
    }
    // Elevation always 0 -- azimuth-only rotors, see the class comment.
    send(setPositionCommand(azimuthDeg, 0.0), Pending::Report);
}

void RotctldClient::send(const QByteArray& command, Pending expect)
{
    m_queue.enqueue(Command{command, expect});
    pump();
}

void RotctldClient::pump()
{
    if (m_awaiting != Pending::None) { return; }
    if (m_queue.isEmpty()) { return; }
    if (m_socket.state() != QAbstractSocket::ConnectedState) { return; }

    const Command c = m_queue.dequeue();
    m_awaiting = c.expect;
    m_socket.write(c.bytes);
    m_deadline->start();
}

// -- Replies -------------------------------------------------------------

bool RotctldClient::parseReport(const QByteArray& reply, int& codeOut)
{
    const QByteArray t = reply.trimmed();
    if (!t.startsWith("RPRT")) { return false; }
    bool ok = false;
    const int v = t.mid(4).trimmed().toInt(&ok);
    if (!ok) { return false; }
    codeOut = v;
    return true;
}

bool RotctldClient::parsePosition(const QByteArray& reply, double& azDegOut, double& elDegOut)
{
    // An RPRT line is never a position -- without this check, "RPRT -1"
    // would split into a token that parses as the number -1, and the
    // needle would swing to 359 degrees on every error.
    int ignored = 0;
    if (parseReport(reply, ignored)) { return false; }

    const QList<QByteArray> lines = reply.trimmed().split('\n');
    if (lines.size() < 2) { return false; }

    bool okAz = false, okEl = false;
    const double az = lines.at(0).trimmed().toDouble(&okAz);
    const double el = lines.at(1).trimmed().toDouble(&okEl);
    if (!okAz || !okEl) { return false; }

    azDegOut = az;
    elDegOut = el;
    return true;
}

QString RotctldClient::describeReport(int code)
{
    switch (code) {
    case 0:   return QStringLiteral("accepted");
    case -1:  return QStringLiteral("the rotor does not support that");
    case -2:  return QStringLiteral("invalid parameter");
    case -3:  return QStringLiteral("invalid configuration");
    case -4:  return QStringLiteral("out of memory");
    case -5:  return QStringLiteral("not implemented");
    case -6:  return QStringLiteral("timed out");
    case -8:  return QStringLiteral("input/output error");
    case -9:  return QStringLiteral("internal Hamlib error");
    case -11: return QStringLiteral("target is outside the rotor's range");
    default:  return QStringLiteral("rotctld error %1").arg(code);
    }
}

void RotctldClient::onReadyRead()
{
    m_buffer += m_socket.readAll();

    while (true) {
        if (m_awaiting == Pending::None) { break; }

        // A report is one line; a position is two. Wait for as many as
        // the outstanding command will produce, so half a reply is
        // never parsed as a whole one -- same shape as RigctldClient's
        // own read loop.
        const int want = (m_awaiting == Pending::Position) ? 2 : 1;
        if (m_buffer.count('\n') < want) { break; }

        int cut = -1;
        for (int i = 0, seen = 0; i < m_buffer.size(); ++i) {
            if (m_buffer.at(i) == '\n' && ++seen == want) { cut = i; break; }
        }
        if (cut < 0) { break; }

        const QByteArray reply = m_buffer.left(cut + 1);
        m_buffer.remove(0, cut + 1);
        const Pending was = m_awaiting;
        m_awaiting = Pending::None;
        m_deadline->stop();

        if (was == Pending::Position) {
            double az = 0.0, el = 0.0;
            if (parsePosition(reply, az, el)) {
                // Elevation is parsed (so the two-line reply shape is
                // consumed correctly) and then deliberately dropped --
                // see the class comment on azimuth-only rotors.
                Q_UNUSED(el);
                m_azimuthDeg = norm360(az);
                emit azimuthChanged(m_azimuthDeg);
            } else {
                int code = 0;
                if (parseReport(reply, code) && code != 0) {
                    emit errorOccurred(describeReport(code));
                    setState(State::Error);
                }
            }
        } else {
            int code = 0;
            if (parseReport(reply, code) && code != 0) {
                emit errorOccurred(describeReport(code));
                setState(State::Error);
            } else if (m_state == State::Error) {
                setState(State::Connected);
            }
        }
    }
    pump();
}

// -- State ---------------------------------------------------------------

void RotctldClient::setState(State s)
{
    if (m_state == s) { return; }
    m_state = s;
    emit stateChanged();
}

void RotctldClient::fail(const QString& why)
{
    // Socket errors arrive alongside disconnected(); do not announce
    // the same failure twice.
    if (m_state != State::Disconnected) {
        emit errorOccurred(why);
    }
}

} // namespace Contestprogramm
