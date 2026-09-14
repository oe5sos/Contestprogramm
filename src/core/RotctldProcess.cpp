// =================================================================
// src/core/RotctldProcess.cpp  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original -- ported verbatim from Longpath/NereusSDR's
// RotctldProcess.cpp. See RotctldProcess.h for the full rationale.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-11 — Ported from Longpath/NereusSDR's RotctldProcess for
//                 Contestprogramm. AI-assisted via Anthropic Claude
//                 Code, operator Ralph Martin Fischer.
// =================================================================

#include "core/RotctldProcess.h"

#include <QFileInfo>
#include <QStandardPaths>

namespace Contestprogramm {

RotctldProcess::RotctldProcess(QObject* parent) : QObject(parent)
{
    connect(&m_proc, &QProcess::finished, this,
            [this](int code, QProcess::ExitStatus) {
        emit exited(code,
                    QString::fromLocal8Bit(m_proc.readAllStandardError())
                        .trimmed());
    });
}

RotctldProcess::~RotctldProcess()
{
    stop();
}

QString RotctldProcess::findBinary()
{
    // PATH first: an operator who installed Hamlib somewhere unusual
    // and put it on PATH has already answered this question.
    const QString onPath = QStandardPaths::findExecutable(
        QStringLiteral("rotctld"));
    if (!onPath.isEmpty()) { return onPath; }

    // Then the places package managers actually put it. A GUI launched
    // from Finder does not inherit the shell's PATH, so Homebrew's
    // directories have to be named explicitly or Hamlib is invisible to
    // this program while being plainly present in the terminal.
    static const QStringList kDirs = {
        QStringLiteral("/opt/homebrew/bin"),   // Apple silicon Homebrew
        QStringLiteral("/usr/local/bin"),      // Intel Homebrew, and most else
        QStringLiteral("/opt/local/bin"),      // MacPorts
        QStringLiteral("/usr/bin"),
    };
    for (const QString& dir : kDirs) {
        const QString candidate = dir + QStringLiteral("/rotctld");
        if (QFileInfo(candidate).isExecutable()) { return candidate; }
    }
    return {};
}

QStringList RotctldProcess::arguments(int hamlibModel, const QString& device,
                                      int baud, quint16 listenPort)
{
    QStringList args;
    args << QStringLiteral("-m") << QString::number(hamlibModel);

    // A network model such as Ether6 takes an address where a serial
    // model takes a device node; either way it is -r, and either way
    // an empty one means "let Hamlib use its default".
    if (!device.trimmed().isEmpty()) {
        args << QStringLiteral("-r") << device.trimmed();
    }
    if (baud > 0) {
        args << QStringLiteral("-s") << QString::number(baud);
    }

    // Loopback only. rotctld has no authentication of any kind, and a
    // rotator that anyone on the network can turn is a rotator that
    // will eventually be turned by someone else. An operator who wants
    // it reachable from another machine can run rotctld themselves and
    // point Contestprogramm at it — that is a decision worth making
    // deliberately.
    args << QStringLiteral("-T") << QStringLiteral("127.0.0.1")
         << QStringLiteral("-t") << QString::number(listenPort);
    return args;
}

bool RotctldProcess::isRunning() const
{
    return m_proc.state() != QProcess::NotRunning;
}

bool RotctldProcess::start(int hamlibModel, const QString& device, int baud,
                           quint16 listenPort, QString* error)
{
    if (isRunning()) { return true; }

    const QString binary = findBinary();
    if (binary.isEmpty()) {
        if (error) {
            *error = QStringLiteral(
                "Hamlib's rotctld was not found. Install it with:\n\n"
                "    brew install hamlib\n\n"
                "Or run rotctld yourself and use the network option "
                "instead.");
        }
        return false;
    }

    m_proc.setProgram(binary);
    m_proc.setArguments(arguments(hamlibModel, device, baud, listenPort));
    m_proc.start();

    if (!m_proc.waitForStarted(3000)) {
        if (error) {
            *error = QStringLiteral("Couldn't start %1: %2")
                         .arg(binary, m_proc.errorString());
        }
        return false;
    }
    return true;
}

void RotctldProcess::stop()
{
    if (!isRunning()) { return; }

    // Ask first. rotctld closes the serial port on SIGTERM; killed
    // outright it can leave the port held until the device is
    // re-plugged, and the next connection attempt then fails for a
    // reason that has nothing to do with the rotator.
    m_proc.terminate();
    if (!m_proc.waitForFinished(2000)) {
        m_proc.kill();
        m_proc.waitForFinished(1000);
    }
}

} // namespace Contestprogramm
