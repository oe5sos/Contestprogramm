// Ported from Longpath/NereusSDR's tests/tst_rotctld_process.cpp,
// adapted to this project's own test style (camelCase slot names,
// QTEST_APPLESS_MAIN) -- see core/RotctldProcess.h. Only the
// argument-building slots are ported here; the model-list slots live
// in test_rotormodels.cpp instead, matching this project's existing
// one-header-one-test convention.

#include <QtTest>

#include "core/RotctldProcess.h"

using namespace Contestprogramm;

class TestRotctldProcess : public QObject
{
    Q_OBJECT

private slots:
    void aSerialRotatorGetsModelDeviceAndSpeed();
    void anEmptyDeviceIsLeftOutEntirely();
    void itListensOnLoopbackOnly();
    void twoIndependentSlotsCanUseDifferentPorts();
};

void TestRotctldProcess::aSerialRotatorGetsModelDeviceAndSpeed()
{
    // 601 = Yaesu GS-232A, the station's own rotor and this project's
    // curated-list default (core/RotorModels.h) -- rotor1's default
    // port 4533 is ContestSettings::rotor1Port's own default too.
    const QStringList a = RotctldProcess::arguments(601, QStringLiteral("/dev/tty.usbserial-1410"), 9600, 4533);

    QCOMPARE(a, QStringList({QStringLiteral("-m"), QStringLiteral("601"), QStringLiteral("-r"),
                             QStringLiteral("/dev/tty.usbserial-1410"), QStringLiteral("-s"), QStringLiteral("9600"),
                             QStringLiteral("-T"), QStringLiteral("127.0.0.1"), QStringLiteral("-t"),
                             QStringLiteral("4533")}));
}

void TestRotctldProcess::anEmptyDeviceIsLeftOutEntirely()
{
    // A bare "-r" with nothing after it makes rotctld take the next
    // flag as the device name, and it then fails complaining about a
    // serial port called "-T". Leaving the pair out lets Hamlib use its
    // own default, which is what a network model wants anyway.
    const QStringList a = RotctldProcess::arguments(1501, QString(), 0, 4533);
    QVERIFY(!a.contains(QStringLiteral("-r")));
    QVERIFY(!a.contains(QStringLiteral("-s")));
    QVERIFY(a.contains(QStringLiteral("-m")));

    const QStringList blank = RotctldProcess::arguments(601, QStringLiteral("   "), 9600, 4533);
    QVERIFY(!blank.contains(QStringLiteral("-r")));
}

void TestRotctldProcess::itListensOnLoopbackOnly()
{
    // rotctld has no authentication of any kind. Bound to 0.0.0.0 --
    // which is the example in most instructions -- anyone on the
    // network can turn the mast. This is the one argument that must
    // not drift.
    for (int model : {601, 603, 404, 403, 901}) {
        const QStringList a = RotctldProcess::arguments(model, QStringLiteral("/dev/ttyUSB0"), 9600, 4533);
        const int at = a.indexOf(QStringLiteral("-T"));
        QVERIFY2(at >= 0 && at + 1 < a.size(), "no listen address given");
        QCOMPARE(a.at(at + 1), QStringLiteral("127.0.0.1"));
        QVERIFY(!a.contains(QStringLiteral("0.0.0.0")));
    }
}

void TestRotctldProcess::twoIndependentSlotsCanUseDifferentPorts()
{
    // Contestprogramm-specific: unlike Longpath (one rotor), this
    // project has two independent rotor slots (ContestSettings::
    // rotor1Port=4533/rotor2Port=4534 defaults) that must be able to
    // run rotctld side by side on the same machine without a port
    // clash. RotctldProcess itself is stateless per instance, so this
    // just confirms arguments() keeps the listen port distinct per
    // call -- the actual "two instances, two QProcesses" wiring lives
    // one layer up (AppController/MainWindow), matching how
    // RotctldClient is already held twice there.
    const QStringList slot1 = RotctldProcess::arguments(601, QStringLiteral("/dev/tty.usbserial-A"), 9600, 4533);
    const QStringList slot2 = RotctldProcess::arguments(601, QStringLiteral("/dev/tty.usbserial-B"), 9600, 4534);

    QVERIFY(slot1.contains(QStringLiteral("4533")));
    QVERIFY(slot2.contains(QStringLiteral("4534")));
    QVERIFY(!slot1.contains(QStringLiteral("4534")));
    QVERIFY(!slot2.contains(QStringLiteral("4533")));
}

QTEST_APPLESS_MAIN(TestRotctldProcess)
#include "test_rotctldprocess.moc"
