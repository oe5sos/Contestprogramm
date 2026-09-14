#pragma once

// =================================================================
// src/core/RotctldProcess.h  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original -- not a Thetis port. Ported verbatim
// (behavior, argument order and constants unchanged) from
// Longpath/NereusSDR's own src/core/RotctldProcess.h/.cpp, which is
// itself NereusSDR-original -- see that file's own header for the
// full rationale. Only the namespace changed, `Longpath` ->
// `Contestprogramm`.
//
// Starts Hamlib's rotctld so the operator does not have to.
//
// Contestprogramm's rotor link (RotctldClient, core/RotctldClient.h)
// already speaks to rotctld over TCP -- that part needed no change.
// What was still missing is what NereusSDR/Longpath solved first: the
// installation instructions were "open a terminal and run rotctld -m
// 601 -r /dev/tty.usbserial-1410 -T 0.0.0.0", which is a reasonable
// thing to ask of a developer and not of someone who wants to point
// an antenna at a contest.
//
// So: pick the controller from a list, pick the port, press Connect.
// This starts rotctld on the loopback interface, and stops it again on
// the way out. An operator who already runs rotctld their own way is
// not affected — that path is still there and still preferred when it
// is already running.
//
// Contestprogramm has two independent rotor slots (rotor1/rotor2, see
// ContestSettings), unlike Longpath which has only one rotor.
// RotctldProcess itself needed no change for that: it is stateless per
// instance (one QProcess member), so AppController/MainWindow simply
// hold two RotctldProcess instances, one per slot, exactly the same
// way it already holds two RotctldClient instances.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-11 — Ported from Longpath/NereusSDR's RotctldProcess for
//                 Contestprogramm's two-rotor-slot architecture (the
//                 class itself is unchanged; only the owning slot
//                 count differs, handled by instantiating it twice).
//                 AI-assisted via Anthropic Claude Code, operator
//                 Ralph Martin Fischer.
// =================================================================

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace Contestprogramm {

class RotctldProcess : public QObject {
    Q_OBJECT
public:
    explicit RotctldProcess(QObject* parent = nullptr);
    ~RotctldProcess() override;

    // Where rotctld lives, or empty if it cannot be found.
    //
    // PATH alone is not enough: a GUI application launched from Finder
    // inherits a PATH without /opt/homebrew/bin, so Hamlib installed
    // with brew is invisible to it while being perfectly present in the
    // operator's terminal. That discrepancy would read as "the software
    // cannot find something I can see", which is worse than a plain
    // absence.
    static QString findBinary();

    // The command line, built where it can be checked. Public because
    // the dialog shows it to the operator: a person who can see the
    // exact command can run it by hand when the automatic path fails,
    // and can paste it into a bug report.
    static QStringList arguments(int hamlibModel, const QString& device,
                                 int baud, quint16 listenPort);

    bool isRunning() const;

    // Start rotctld. Returns false and fills `error` if the binary is
    // missing or the process refuses to start; a rotctld that starts
    // and then exits reports through exited() instead, because that
    // failure arrives later.
    bool start(int hamlibModel, const QString& device, int baud,
               quint16 listenPort, QString* error);

    void stop();

signals:
    // rotctld stopped on its own. Carries whatever it wrote to stderr,
    // which is where Hamlib puts the reason — a wrong model number or a
    // serial port that is not there both come out here and nowhere
    // else.
    void exited(int exitCode, const QString& stderrText);

private:
    QProcess m_proc;
};

} // namespace Contestprogramm
