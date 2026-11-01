#include "core/RigctldProcess.h"

#include "core/HamlibBinary.h"

namespace Contestprogramm {

RigctldProcess::RigctldProcess(QObject* parent) : QObject(parent)
{
    connect(&m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        emit exited(code, QString::fromLocal8Bit(m_proc.readAllStandardError()).trimmed());
    });
}

RigctldProcess::~RigctldProcess()
{
    stop();
}

QString RigctldProcess::findBinary()
{
    return findHamlibBinary(QStringLiteral("rigctld"));
}

QStringList RigctldProcess::arguments(int hamlibModel, const QString& device,
                                      int baud, quint16 listenPort)
{
    QStringList args;
    args << QStringLiteral("-m") << QString::number(hamlibModel);

    // A network model takes an address where a serial model takes a
    // device node; either way it is -r, and either way an empty one
    // means "let Hamlib use its default".
    if (!device.trimmed().isEmpty()) {
        args << QStringLiteral("-r") << device.trimmed();
    }
    if (baud > 0) {
        args << QStringLiteral("-s") << QString::number(baud);
    }

    // Loopback only, same reasoning as RotctldProcess: rigctld has no
    // authentication of any kind, and a transmitter anyone on the
    // network can key is a transmitter that will eventually be keyed by
    // someone else. An operator who wants it reachable from another
    // machine can run rigctld themselves and point Contestprogramm at
    // it -- that is a decision worth making deliberately.
    args << QStringLiteral("-T") << QStringLiteral("127.0.0.1")
         << QStringLiteral("-t") << QString::number(listenPort);
    return args;
}

bool RigctldProcess::isRunning() const
{
    return m_proc.state() != QProcess::NotRunning;
}

bool RigctldProcess::start(int hamlibModel, const QString& device, int baud,
                           quint16 listenPort, QString* error)
{
    if (isRunning()) { return true; }

    const QString binary = findBinary();
    if (binary.isEmpty()) {
        if (error) {
            *error = QStringLiteral(
                "Hamlibs rigctld wurde nicht gefunden. Zu installieren mit:\n\n"
                "    brew install hamlib\n\n"
                "Oder rigctld selbst starten und hier nur Host und Port eintragen.");
        }
        return false;
    }

    m_proc.setProgram(binary);
    m_proc.setArguments(arguments(hamlibModel, device, baud, listenPort));
    m_proc.start();

    if (!m_proc.waitForStarted(3000)) {
        if (error) {
            *error = QStringLiteral("%1 ließ sich nicht starten: %2").arg(binary, m_proc.errorString());
        }
        return false;
    }
    return true;
}

void RigctldProcess::stop()
{
    if (!isRunning()) { return; }

    // Ask first. rigctld closes the serial port on SIGTERM; killed
    // outright it can leave the port held until the device is
    // re-plugged, and the next connection attempt then fails for a
    // reason that has nothing to do with the radio.
    m_proc.terminate();
    if (!m_proc.waitForFinished(2000)) {
        m_proc.kill();
        m_proc.waitForFinished(1000);
    }
}

} // namespace Contestprogramm
