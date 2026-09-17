// =================================================================
// src/core/RigctldClient.cpp  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original -- see RigctldClient.h.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-09 — Created in C++20/Qt6, architecturally ported from
//                 Longpath/NereusSDR's RotctldClient.cpp. AI-assisted
//                 via Anthropic Claude Code, operator Ralph Martin
//                 Fischer.
// =================================================================

#include "RigctldClient.h"

#include <QTimer>

namespace Contestprogramm {

RigctldClient::RigctldClient(QObject* parent) : QObject(parent)
{
    m_poll = new QTimer(this);
    m_poll->setInterval(m_pollMs);
    connect(m_poll, &QTimer::timeout, this, [this]() {
        // One outstanding command at a time -- if the last poll round
        // has not fully drained yet, do not stack another; rigctld
        // answers in order, and a queue of stale queries only delays
        // a set-frequency/set-PTT command sent from the UI behind them.
        if (m_awaiting == Pending::None && m_queue.isEmpty()) {
            send(QByteArrayLiteral("f\n"), Pending::Frequency);
            send(QByteArrayLiteral("m\n"), Pending::Mode);
            send(QByteArrayLiteral("t\n"), Pending::Ptt);
        }
    });

    m_retry = new QTimer(this);
    m_retry->setSingleShot(true);
    m_retry->setInterval(3000);
    connect(m_retry, &QTimer::timeout, this, [this]() {
        if (m_state == State::Disconnected && !m_host.isEmpty()) {
            connectToRig();
        }
    });

    // Reply watchdog -- see RigctldClient.h / kReplyTimeoutMs. Without
    // it, a rigctld that freezes while its TCP side stays open leaves
    // m_awaiting set forever and the client is silently dead until
    // someone reconnects by hand.
    m_deadline = new QTimer(this);
    m_deadline->setSingleShot(true);
    m_deadline->setInterval(kReplyTimeoutMs);
    connect(m_deadline, &QTimer::timeout, this, [this]() {
        if (m_awaiting == Pending::None) { return; }
        emit errorOccurred(QStringLiteral("rigctld stopped answering — reconnecting"));
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
        send(QByteArrayLiteral("f\n"), Pending::Frequency);
        send(QByteArrayLiteral("m\n"), Pending::Mode);
        send(QByteArrayLiteral("t\n"), Pending::Ptt);
    });

    connect(&m_socket, &QTcpSocket::readyRead, this, &RigctldClient::onReadyRead);

    connect(&m_socket, &QTcpSocket::disconnected, this, [this]() {
        m_poll->stop();
        m_deadline->stop();
        setState(State::Disconnected);
        // Keep trying -- a rig/rigctld that is power-cycled mid session
        // should come back on its own rather than needing the operator
        // to notice and reconnect by hand.
        if (!m_host.isEmpty()) { m_retry->start(); }
    });

    connect(&m_socket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        fail(m_socket.errorString());
    });
}

// -- Configuration ---------------------------------------------------

void RigctldClient::setTarget(const QString& host, quint16 port)
{
    m_host = host.trimmed();
    m_port = port;
}

void RigctldClient::setPollIntervalMs(int ms)
{
    m_pollMs = qBound(200, ms, 10000);
    m_poll->setInterval(m_pollMs);
}

QString RigctldClient::description() const
{
    if (m_host.isEmpty()) { return QStringLiteral("rigctld (not set up)"); }
    return QStringLiteral("rigctld %1:%2").arg(m_host).arg(m_port);
}

// -- Connection --------------------------------------------------------

void RigctldClient::connectToRig()
{
    if (m_host.isEmpty()) {
        emit errorOccurred(QStringLiteral("No CAT rig address set"));
        return;
    }
    if (m_socket.state() != QAbstractSocket::UnconnectedState) { return; }

    m_retry->stop();
    setState(State::Connecting);
    m_socket.connectToHost(m_host, m_port);
}

void RigctldClient::disconnectFromRig()
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

QByteArray RigctldClient::setFrequencyCommand(qint64 hz)
{
    return QStringLiteral("F %1\n").arg(hz).toLatin1();
}

QByteArray RigctldClient::setPttCommand(bool active)
{
    return QStringLiteral("T %1\n").arg(active ? 1 : 0).toLatin1();
}

void RigctldClient::setFrequency(qint64 hz)
{
    if (!isConnected()) {
        emit errorOccurred(QStringLiteral("CAT rig is not connected"));
        return;
    }
    send(setFrequencyCommand(hz), Pending::Report);
}

void RigctldClient::setPtt(bool active)
{
    if (!isConnected()) {
        emit errorOccurred(QStringLiteral("CAT rig is not connected"));
        return;
    }
    send(setPttCommand(active), Pending::Report);
}

QByteArray RigctldClient::sendMorseCommand(const QString& text)
{
    QString sanitized = text;
    sanitized.replace(QLatin1Char('\n'), QLatin1Char(' '));
    sanitized.replace(QLatin1Char('\r'), QLatin1Char(' '));
    return QStringLiteral("b %1\n").arg(sanitized).toLatin1();
}

void RigctldClient::sendMorse(const QString& text)
{
    if (!isConnected()) {
        emit errorOccurred(QStringLiteral("CAT rig is not connected"));
        return;
    }
    if (text.trimmed().isEmpty()) {
        return;
    }
    send(sendMorseCommand(text), Pending::Report);
}

QByteArray RigctldClient::stopMorseCommand()
{
    return QByteArrayLiteral("\\stop_morse\n");
}

void RigctldClient::stopMorse()
{
    if (!isConnected()) {
        return;
    }
    send(stopMorseCommand(), Pending::Report);
}

void RigctldClient::send(const QByteArray& command, Pending expect)
{
    m_queue.enqueue(Command{command, expect});
    pump();
}

void RigctldClient::pump()
{
    if (m_awaiting != Pending::None) { return; }
    if (m_queue.isEmpty()) { return; }
    if (m_socket.state() != QAbstractSocket::ConnectedState) { return; }

    const Command c = m_queue.dequeue();
    m_awaiting = c.expect;
    m_socket.write(c.bytes);
    // A queued send_morse ("b <text>\n") gets its own, much longer
    // watchdog -- see kMorseReplyTimeoutMs's own doc comment. Every
    // other command explicitly re-arms at kReplyTimeoutMs -- QTimer::
    // start(int) permanently overwrites the timer's own interval()
    // property, not just a one-shot override, so a bare start() here
    // would silently keep reusing kMorseReplyTimeoutMs for every
    // ordinary poll after the first Morse send.
    m_deadline->start(c.bytes.startsWith("b ") ? kMorseReplyTimeoutMs : kReplyTimeoutMs);
}

// -- Replies -------------------------------------------------------------

bool RigctldClient::parseReport(const QByteArray& reply, int& codeOut)
{
    const QByteArray t = reply.trimmed();
    if (!t.startsWith("RPRT")) { return false; }
    bool ok = false;
    const int v = t.mid(4).trimmed().toInt(&ok);
    if (!ok) { return false; }
    codeOut = v;
    return true;
}

bool RigctldClient::parseFrequency(const QByteArray& reply, qint64& hzOut)
{
    // An RPRT line is never a frequency -- without this check, an error
    // reply like "RPRT -6" would parse its trailing "-6" as -6 Hz.
    int ignored = 0;
    if (parseReport(reply, ignored)) { return false; }

    const QByteArray t = reply.trimmed();
    bool ok = false;
    const qint64 v = t.toLongLong(&ok);
    if (!ok) { return false; }
    hzOut = v;
    return true;
}

bool RigctldClient::parseMode(const QByteArray& reply, QString& modeOut, int& passbandHzOut)
{
    int ignored = 0;
    if (parseReport(reply, ignored)) { return false; }

    const QList<QByteArray> lines = reply.trimmed().split('\n');
    if (lines.size() < 2) { return false; }

    const QByteArray modeLine = lines.at(0).trimmed();
    if (modeLine.isEmpty()) { return false; }

    bool ok = false;
    const int pb = lines.at(1).trimmed().toInt(&ok);
    if (!ok) { return false; }

    modeOut = QString::fromLatin1(modeLine);
    passbandHzOut = pb;
    return true;
}

bool RigctldClient::parsePtt(const QByteArray& reply, bool& activeOut)
{
    int ignored = 0;
    if (parseReport(reply, ignored)) { return false; }

    const QByteArray t = reply.trimmed();
    if (t == "0") { activeOut = false; return true; }
    if (t == "1") { activeOut = true; return true; }
    return false;
}

QString RigctldClient::describeReport(int code)
{
    switch (code) {
    case 0:   return QStringLiteral("accepted");
    case -1:  return QStringLiteral("the rig does not support that");
    case -2:  return QStringLiteral("invalid parameter");
    case -3:  return QStringLiteral("invalid configuration");
    case -4:  return QStringLiteral("out of memory");
    case -5:  return QStringLiteral("not implemented");
    case -6:  return QStringLiteral("timed out");
    case -8:  return QStringLiteral("input/output error");
    case -9:  return QStringLiteral("internal Hamlib error");
    case -11: return QStringLiteral("target is outside the rig's range");
    default:  return QStringLiteral("rigctld error %1").arg(code);
    }
}

void RigctldClient::onReadyRead()
{
    m_buffer += m_socket.readAll();

    while (true) {
        if (m_awaiting == Pending::None) { break; }

        // A report/frequency/PTT reply is one line; a mode reply is
        // two. Wait for as many as the outstanding command will
        // produce, so half a reply is never parsed as a whole one --
        // same shape as RotctldClient::onReadyRead (see the header
        // comment for the one known latent gap this carries over).
        const int want = (m_awaiting == Pending::Mode) ? 2 : 1;
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

        switch (was) {
        case Pending::Frequency: {
            qint64 hz = 0;
            if (parseFrequency(reply, hz)) {
                m_frequencyHz = hz;
                emit frequencyChanged(m_frequencyHz);
            } else {
                int code = 0;
                if (parseReport(reply, code) && code != 0) {
                    emit errorOccurred(describeReport(code));
                }
            }
            break;
        }
        case Pending::Mode: {
            QString mode;
            int passbandHz = 0;
            if (parseMode(reply, mode, passbandHz)) {
                m_mode = mode;
                m_passbandHz = passbandHz;
                emit modeChanged(m_mode, m_passbandHz);
            } else {
                int code = 0;
                if (parseReport(reply, code) && code != 0) {
                    emit errorOccurred(describeReport(code));
                }
            }
            break;
        }
        case Pending::Ptt: {
            bool active = false;
            if (parsePtt(reply, active)) {
                if (active != m_ptt) {
                    m_ptt = active;
                    emit pttChanged(m_ptt);
                }
            } else {
                int code = 0;
                if (parseReport(reply, code) && code != 0) {
                    emit errorOccurred(describeReport(code));
                }
            }
            break;
        }
        case Pending::Report: {
            int code = 0;
            if (parseReport(reply, code) && code != 0) {
                emit errorOccurred(describeReport(code));
            }
            break;
        }
        case Pending::None:
            break;
        }
    }
    pump();
}

// -- State ---------------------------------------------------------------

void RigctldClient::setState(State s)
{
    if (m_state == s) { return; }
    m_state = s;
    emit stateChanged();
}

void RigctldClient::fail(const QString& why)
{
    // Socket errors arrive alongside disconnected(); do not announce
    // the same failure twice.
    if (m_state != State::Disconnected) {
        emit errorOccurred(why);
    }
}

} // namespace Contestprogramm
