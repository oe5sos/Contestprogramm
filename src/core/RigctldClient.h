#pragma once

// =================================================================
// src/core/RigctldClient.h  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original. Architecturally ported from
// Longpath/NereusSDR's src/core/RotctldClient.h/.cpp -- same Hamlib
// line-based-text-protocol client shape (QTcpSocket, one-outstanding-
// command queue, poll timer, reply watchdog, exponential-backoff-free
// simple retry timer, static pure-function parsers) -- adapted from
// rotctld's rotator command set (`p`/`P az el`/`S`) to rigctld's CAT
// command set for the contest station's classic transceiver (Kenwood
// TS-590 or Elecraft K3, decision open -- irrelevant to this client,
// which only ever speaks rigctld's TCP protocol, never the radio
// directly):
//
//   f            -> "145200000\n"            frequency, Hz, plain text
//   F <hz>       -> "RPRT 0\n"                set frequency
//   m            -> "USB\n2400\n"             mode name, then passband Hz
//   t            -> "0\n" / "1\n"             PTT state
//   T <0|1>      -> "RPRT 0\n"                set PTT
//
// rigctld's default TCP port is 4532 -- a sibling daemon to rotctld's
// 4533, same Hamlib suite, run side by side for CAT and rotor control
// respectively (Longpath already manages the rotctld side; this class
// is Contestprogramm's own, independent client to the CAT side).
//
// A negative RPRT is an error code, not a value -- the parsing here is
// careful to tell the two apart, same reasoning as RotctldClient.
// rigctld answers one command at a time, so commands queue; sending a
// second while the first is outstanding gets the replies crossed.
//
// The `m` (mode) reply is two lines; like RotctldClient's own position
// read, the read loop below unconditionally waits for two newlines
// once a mode query is outstanding. If rigctld ever answered a mode
// query with a single-line RPRT error instead of the documented
// two-line reply, that would stall until the reply watchdog
// (kReplyTimeoutMs) cuts the connection -- the same latent shape
// RotctldClient itself has for its two-line position query, carried
// over deliberately rather than special-cased away, per "mirror the
// template's shape."
//
// CW keying (`sendMorse`/`b <text>`): rigctld's documented command set
// includes `b: send_morse (Morse)` -- verified against this machine's
// actually-installed Hamlib 4.7.2 (`rigctld --help` and `man rigctld`,
// not assumed/guessed): the command letter is `b`, followed by the text
// to send as the rest of the line (e.g. "b CQ CQ DE ME"), and the reply
// is a standard one-line RPRT report, same as `F`/`T`. Whether the
// operator's actual rig backend (Kenwood TS-590 or Elecraft K3, per the
// plan -- decision still open) implements RIG_FUNC_SEND_MORSE behind
// that generic rigctld command is a separate, rig-specific question
// that cannot be checked without the real hardware; this class only
// speaks the generic rigctld protocol, which is confirmed correct.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-09 — Created in C++20/Qt6, architecturally ported from
//                 Longpath/NereusSDR's RotctldClient. AI-assisted via
//                 Anthropic Claude Code, operator Ralph Martin Fischer.
//   2026-09-09 — Added sendMorse()/sendMorseCommand() (CW keying via
//                 rigctld's `b` command, Kern-Welle 2). AI-assisted via
//                 Anthropic Claude Code, operator Ralph Martin Fischer.
// =================================================================

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QTcpSocket>

class QTimer;

namespace Contestprogramm {

class RigctldClient : public QObject {
    Q_OBJECT
public:
    enum class State { Disconnected, Connecting, Connected, Error };

    explicit RigctldClient(QObject* parent = nullptr);

    void setTarget(const QString& host, quint16 port);
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }

    // How often to ask for frequency/mode/PTT. One second is generous
    // for a manual-logging contest station (not a rotor that needs
    // sub-second tracking) and cheap on a loopback/LAN connection.
    void setPollIntervalMs(int ms);

    // How long an outstanding command may wait for its reply before the
    // link is declared dead and cut -- same reasoning and same value as
    // RotctldClient::kReplyTimeoutMs: generous next to the poll
    // interval, a last resort for a daemon that freezes without closing
    // the socket, not a latency budget.
    static constexpr int kReplyTimeoutMs = 4000;

    // A `b <text>` (send_morse) command's own watchdog, used instead of
    // kReplyTimeoutMs -- see pump()'s own comment. Some Hamlib rig
    // backends implement send_morse synchronously (the reply only comes
    // back once the CW has actually finished keying out), not as a
    // fire-and-forget queue-and-return like F/T/m; a full contest
    // exchange at typical CW contest speed can easily run past
    // kReplyTimeoutMs's 4s. Generous enough for any realistic single
    // macro send (even a slow 15 WPM "CQ CONTEST DE OE5SOS OE5SOS" is
    // well under this), short enough to still recover from a genuinely
    // dead daemon in reasonable time.
    static constexpr int kMorseReplyTimeoutMs = 20000;

    QString description() const;
    State state() const { return m_state; }
    bool isConnected() const { return m_state == State::Connected; }

    qint64 frequencyHz() const { return m_frequencyHz; }
    QString mode() const { return m_mode; }
    int passbandHz() const { return m_passbandHz; }
    bool pttActive() const { return m_ptt; }

    void connectToRig();
    void disconnectFromRig();
    void setFrequency(qint64 hz);
    void setPtt(bool active);

    // CW keying, per the plan's "CW-TX via Hamlib rigctld + F-key
    // macros" section -- see the header comment above for the command
    // verification. `text` is sent as-is (any embedded newline is
    // replaced with a space so it cannot be mistaken for a second
    // command line); CwMacroPanel is the caller in practice, after
    // substituting its {call}/{exchange} placeholders.
    void sendMorse(const QString& text);

    // -- Protocol, as pure functions --------------------------------

    // "145200000\n" -> 145200000. False for anything that is not a
    // plain integer, including an RPRT line (never read as a
    // frequency, same reasoning as RotctldClient::parsePosition).
    static bool parseFrequency(const QByteArray& reply, qint64& hzOut);

    // "USB\n2400\n" -> mode "USB", passband 2400 Hz.
    static bool parseMode(const QByteArray& reply, QString& modeOut, int& passbandHzOut);

    // "0\n" / "1\n" -> false / true.
    static bool parsePtt(const QByteArray& reply, bool& activeOut);

    // "RPRT 0" -> 0. "RPRT -6" -> -6. False if not an RPRT line.
    static bool parseReport(const QByteArray& reply, int& codeOut);

    // Same Hamlib rig_errcode_e enum RotctldClient::describeReport
    // already maps for rotctld -- Hamlib defines this error enum once
    // and shares it across its RIG/ROT/AMP backends, so the same
    // numbers mean the same things here.
    static QString describeReport(int code);

    static QByteArray setFrequencyCommand(qint64 hz);
    static QByteArray setPttCommand(bool active);

    // "b <text>\n" -- see the header comment for the verification of
    // this command against the installed Hamlib 4.7.2 documentation.
    // Embedded newlines/carriage-returns in `text` are replaced with a
    // space, since either would otherwise be read as ending the
    // command line early.
    static QByteArray sendMorseCommand(const QString& text);

signals:
    void stateChanged();
    void frequencyChanged(qint64 hz);
    void modeChanged(const QString& mode, int passbandHz);
    void pttChanged(bool active);
    void errorOccurred(const QString& message);

private:
    enum class Pending { None, Frequency, Mode, Ptt, Report };

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
    quint16    m_port{4532}; // rigctld's default (rotctld's is 4533)

    QQueue<Command> m_queue;
    Pending    m_awaiting{Pending::None};
    QByteArray m_buffer;

    QTimer* m_poll{nullptr};
    QTimer* m_retry{nullptr};
    QTimer* m_deadline{nullptr}; // reply watchdog, see kReplyTimeoutMs
    int     m_pollMs{1000};

    State   m_state{State::Disconnected};
    qint64  m_frequencyHz{0};
    QString m_mode;
    int     m_passbandHz{0};
    bool    m_ptt{false};
};

} // namespace Contestprogramm
