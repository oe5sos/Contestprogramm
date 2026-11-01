#pragma once

// =================================================================
// src/core/RigctldProcess.h  (Contestprogramm)
// =================================================================
//
// Starts Hamlib's rigctld so the operator does not have to.
//
// The rotor half of this has existed since 2026-09-11
// (core/RotctldProcess.h): pick the controller from a list, pick the
// port, press Connect, and the program runs rotctld itself. The
// transceiver half did not. Its setup instruction was "open a terminal
// and run rigctld -m 2037 -r /dev/cu.usbserial-XXXX -s 115200", which
// is a reasonable thing to ask of a developer and not of someone who
// wants to work a contest at six in the morning on a mountain.
//
// Same shape as RotctldProcess on purpose -- same arguments, same
// loopback-only rule, same "an operator already running their own
// rigctld is left alone" posture (AppController::superviseRigctld only
// starts one after the client's own dial was refused). The binary
// search is shared outright, see core/HamlibBinary.h.
//
// Why a class of its own rather than a parameter on RotctldProcess:
// that file is a verbatim port from Longpath/NereusSDR and is kept
// comparable with its original. What the two genuinely share -- the
// search for the Hamlib binary -- is shared; the rest is twenty lines
// and reads better as itself.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-10-16 — Created, modelled on RotctldProcess, so the TS-590
//                 the station is getting needs no terminal.
//                 AI-assisted via Anthropic Claude Code, operator
//                 Ralph Martin Fischer.
// =================================================================

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace Contestprogramm {

class RigctldProcess : public QObject {
    Q_OBJECT
public:
    explicit RigctldProcess(QObject* parent = nullptr);
    ~RigctldProcess() override;

    // Where rigctld lives, or empty if it cannot be found.
    static QString findBinary();

    // The command line, built where it can be checked. Public because
    // the settings dialog shows it to the operator: a person who can
    // see the exact command can run it by hand when the automatic path
    // fails, and can paste it into a bug report.
    static QStringList arguments(int hamlibModel, const QString& device,
                                 int baud, quint16 listenPort);

    bool isRunning() const;

    // Start rigctld. Returns false and fills `error` if the binary is
    // missing or the process refuses to start; a rigctld that starts
    // and then exits reports through exited() instead, because that
    // failure arrives later.
    bool start(int hamlibModel, const QString& device, int baud,
               quint16 listenPort, QString* error);

    void stop();

signals:
    // rigctld stopped on its own. Carries whatever it wrote to stderr,
    // which is where Hamlib puts the reason -- a wrong model number, a
    // serial port that is not there, or a baud rate the radio's own
    // menu disagrees with all come out here and nowhere else.
    void exited(int exitCode, const QString& stderrText);

private:
    QProcess m_proc;
};

} // namespace Contestprogramm
