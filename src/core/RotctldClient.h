#pragma once

// =================================================================
// src/core/RotctldClient.h  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original. Architecturally ported from
// Longpath/NereusSDR's src/core/RotctldClient.h/.cpp -- same idea as
// this project's own RigctldClient (which ported the identical shape
// for rigctld's CAT command set): QTcpSocket, one-outstanding-command
// queue, poll timer, reply watchdog, simple retry timer, static
// pure-function parsers. This class speaks rotctld's rotator command
// set instead of rigctld's CAT set:
//
//   p            -> "145.000000\n0.000000\n"   azimuth, elevation
//   P az el      -> "RPRT 0\n"                   set position
//
// rotctld's default TCP port is 4533 -- a sibling daemon to rigctld's
// 4532, same Hamlib suite.
//
// Azimuth-only rotors: neither mast here carries an elevation rotor
// (see the plan's Phase 3 section -- two independent 2m/70cm masts,
// az-only Yaesu GS-232-class rotors). setAzimuth() therefore always
// commands elevation 0, and a read's elevation line is parsed (so the
// two-line reply shape is still consumed correctly) but otherwise
// ignored -- not stored, not exposed via a getter, per the task's own
// framing ("ignore elevation on reads beyond parsing it").
//
// A single reusable class, not a per-band subclass: AppController
// instantiates this twice (m_rotor1, m_rotor2 -- two rotor "slots", see
// ContestSettings::rotor1Host/-Label etc.), each pointed at its own
// rotctld process via setTarget() -- same shape as having two
// independent RigctldClient-style clients, just parameterized by
// host/port at connect time rather than by band in the class itself.
//
// A negative RPRT is an error code, not a position -- the parsing here
// is careful to tell the two apart, same reasoning as RigctldClient/
// the NereusSDR original. rotctld answers one command at a time, so
// commands queue; sending a second while the first is outstanding gets
// the replies crossed.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-09 — Created in C++20/Qt6, architecturally ported from
//                 Longpath/NereusSDR's RotctldClient (which is itself
//                 NereusSDR-original, not a Thetis port -- no GPL
//                 attribution chain applies here beyond the porting
//                 note above). AI-assisted via Anthropic Claude Code,
//                 operator Ralph Martin Fischer.
// =================================================================

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QTcpSocket>

class QTimer;

namespace Contestprogramm {

class RotctldClient : public QObject {
    Q_OBJECT
public:
    enum class State { Disconnected, Connecting, Connected, Error };

    explicit RotctldClient(QObject* parent = nullptr);

    void setTarget(const QString& host, quint16 port);
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }

    // How often to ask where the rotor is. Half a second, matching the
    // NereusSDR RotctldClient template -- well under the time a rotor
    // takes to move a degree, and cheap on a loopback/LAN connection.
    void setPollIntervalMs(int ms);

    // How long an outstanding command may wait for its reply before the
    // link is declared dead and cut -- same reasoning/value as
    // RigctldClient::kReplyTimeoutMs.
    static constexpr int kReplyTimeoutMs = 4000;

    QString description() const;
    State state() const { return m_state; }
    bool isConnected() const { return m_state == State::Connected; }

    double azimuthDeg() const { return m_azimuthDeg; }

    void connectToRotor();
    void disconnectFromRotor();

    // Commands the rotor to `azimuthDeg`. Elevation is always sent as
    // 0 -- see the class comment on azimuth-only rotors.
    void setAzimuth(double azimuthDeg);

    // -- Protocol, as pure functions --------------------------------

    // "145.000000\n0.000000\n" -> azimuth 145, elevation 0. False for
    // anything that is not two numbers, including an RPRT line, which
    // must never be read as a position ("RPRT -1" would otherwise
    // become -1 degrees).
    static bool parsePosition(const QByteArray& reply, double& azDegOut, double& elDegOut);

    // "RPRT 0" -> 0. "RPRT -1" -> -1. False if not an RPRT line.
    static bool parseReport(const QByteArray& reply, int& codeOut);

    // Same Hamlib rig_errcode_e-shaped enum RigctldClient::describeReport
    // already maps -- Hamlib shares the numbering across its ROT/RIG
    // backends.
    static QString describeReport(int code);

    // "P 145.00 0.00\n" -- plain decimals (rotctld rejects a decimal
    // comma), azimuth wrapped to 0..360.
    static QByteArray setPositionCommand(double azimuthDeg, double elevationDeg = 0.0);

signals:
    void stateChanged();
    void azimuthChanged(double azimuthDeg);
    void errorOccurred(const QString& message);

private:
    enum class Pending { None, Position, Report };

    struct Command {
        QByteArray bytes;
        Pending    expect;
    };

    void send(const QByteArray& command, Pending expect);
    void pump();
    void onReadyRead();
    void setState(State s);
    void fail(const QString& why);

    QTcpSocket m_socket;
    QString    m_host;
    quint16    m_port{4533}; // rotctld's default (rigctld's is 4532)

    QQueue<Command> m_queue;
    Pending    m_awaiting{Pending::None};
    QByteArray m_buffer;

    QTimer* m_poll{nullptr};
    QTimer* m_retry{nullptr};
    QTimer* m_deadline{nullptr}; // reply watchdog, see kReplyTimeoutMs
    int     m_pollMs{500};

    State  m_state{State::Disconnected};
    double m_azimuthDeg{0.0};
};

} // namespace Contestprogramm
