// rotctld started by the program itself when a rotor slot needs one
// (AppController::rotctldLaunchFor / superviseRotctld): only for an
// enabled slot on the loopback with a device named, only after the
// slot's own client failed to reach anything, stopped again when the
// settings no longer want it, restarted when they change.

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTcpServer>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"

#include <memory>

using namespace Contestprogramm;

namespace {

quint16 freePort()
{
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    const quint16 port = probe.serverPort();
    probe.close();
    return port;
}

// A stand-in rotctld on PATH: records its arguments and then just
// stays alive, like the real one would with a rotator behind it.
bool installFakeRotctld(const QDir& dir, const QString& argsFile)
{
    QFile script(dir.filePath(QStringLiteral("rotctld")));
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    script.write(QStringLiteral("#!/bin/sh\necho \"$@\" > \"%1\"\nexec sleep 60\n").arg(argsFile).toUtf8());
    script.close();
    script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    qputenv("PATH", (dir.absolutePath() + QLatin1Char(':') + qEnvironmentVariable("PATH")).toUtf8());
    return true;
}

QString readArgs(const QString& argsFile)
{
    QFile file(argsFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

} // namespace

class TestRotctldSupervision : public QObject
{
    Q_OBJECT

private slots:
    void launchOnlyForAnEnabledLoopbackSlotWithADevice();
    void startsAfterTheClientFailsAndFollowsTheSettings();
};

void TestRotctldSupervision::launchOnlyForAnEnabledLoopbackSlotWithADevice()
{
    ContestSettings settings;
    settings.rotor1Enabled = true;
    settings.rotor1Host = QStringLiteral("127.0.0.1");
    settings.rotor1Port = 4533;
    settings.rotor1HamlibModel = 601;
    settings.rotor1Device = QStringLiteral("/dev/tty.usbserial-1410");
    settings.rotor1Baud = 9600;
    const auto launch = AppController::rotctldLaunchFor(settings, 1);
    QVERIFY(launch.has_value());
    QCOMPARE(launch->hamlibModel, 601);
    QCOMPARE(launch->device, QStringLiteral("/dev/tty.usbserial-1410"));
    QCOMPARE(launch->baud, 9600);
    QCOMPARE(launch->port, quint16(4533));

    // "localhost" counts as ours too; another machine's rotctld never.
    settings.rotor1Host = QStringLiteral("localhost");
    QVERIFY(AppController::rotctldLaunchFor(settings, 1).has_value());
    settings.rotor1Host = QStringLiteral("172.30.30.16");
    QVERIFY(!AppController::rotctldLaunchFor(settings, 1).has_value());

    // No device named: the operator runs rotctld their own way.
    settings.rotor1Host = QStringLiteral("127.0.0.1");
    settings.rotor1Device = QStringLiteral("   ");
    QVERIFY(!AppController::rotctldLaunchFor(settings, 1).has_value());

    // A disabled slot wants nothing.
    settings.rotor1Device = QStringLiteral("/dev/ttyUSB0");
    settings.rotor1Enabled = false;
    QVERIFY(!AppController::rotctldLaunchFor(settings, 1).has_value());

    // Slot 2 reads its own fields.
    settings.rotor2Enabled = true;
    settings.rotor2Host = QStringLiteral("127.0.0.1");
    settings.rotor2Port = 4534;
    settings.rotor2HamlibModel = 603;
    settings.rotor2Device = QStringLiteral("192.168.1.50:4001");
    settings.rotor2Baud = 0;
    const auto second = AppController::rotctldLaunchFor(settings, 2);
    QVERIFY(second.has_value());
    QCOMPARE(second->hamlibModel, 603);
    QCOMPARE(second->port, quint16(4534));
}

void TestRotctldSupervision::startsAfterTheClientFailsAndFollowsTheSettings()
{
#ifdef Q_OS_WIN
    QSKIP("Der Stellvertreter-rotctld ist ein Shell-Skript; unter Windows startet QProcess es nicht (2026-09-21).");
#endif
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString argsFile = dir.filePath(QStringLiteral("rotctld.args"));
    QVERIFY(installFakeRotctld(QDir(dir.path()), argsFile));

    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("supervision.sqlite"))));
    const quint16 port = freePort();
    ContestSettings settings = controller.settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.rotor1Enabled = true;
    settings.rotor1Host = QStringLiteral("127.0.0.1");
    settings.rotor1Port = port;
    settings.rotor1HamlibModel = 601;
    settings.rotor1Device = QStringLiteral("/dev/tty.fake-A");
    settings.rotor1Baud = 9600;
    settings.rotor2Enabled = false;
    settings.rigctldHost.clear();
    controller.setSettings(settings);

    // Nothing listens on the port: the client's dial is refused, and
    // that -- not the settings save itself -- starts our rotctld.
    QTRY_VERIFY_WITH_TIMEOUT(controller.rotctldRunning(1), 5000);
    QVERIFY(!controller.rotctldRunning(2));
    QTRY_VERIFY_WITH_TIMEOUT(!readArgs(argsFile).isEmpty(), 3000);
    const QString args = readArgs(argsFile);
    QVERIFY2(args.contains(QStringLiteral("-m 601")), qPrintable(args));
    QVERIFY2(args.contains(QStringLiteral("-r /dev/tty.fake-A")), qPrintable(args));
    QVERIFY2(args.contains(QStringLiteral("-t %1").arg(port)), qPrintable(args));
    QVERIFY2(args.contains(QStringLiteral("-T 127.0.0.1")), qPrintable(args));

    // Another device: the running one goes, the client's next failed
    // dial brings one with the new arguments.
    settings.rotor1Device = QStringLiteral("/dev/tty.fake-B");
    controller.setSettings(settings);
    QTRY_VERIFY_WITH_TIMEOUT(readArgs(argsFile).contains(QStringLiteral("-r /dev/tty.fake-B")), 8000);
    QVERIFY(controller.rotctldRunning(1));

    // The slot pointed at another machine: not ours to run any more.
    settings.rotor1Host = QStringLiteral("172.30.30.16");
    controller.setSettings(settings);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.rotctldRunning(1), 5000);
}

QTEST_MAIN(TestRotctldSupervision)
#include "test_rotctld_supervision.moc"
