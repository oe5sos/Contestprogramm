#include <QtTest>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"

using namespace Contestprogramm;

// Covers the operator's "rotor 1/2 optional + freely relabelable"
// request: ContestSettings' defaults/persistence for the two rotor
// slots, and AppController::activeRotorForBand()'s band -> slot
// routing, including the disabled-slot no-op case. RotorWidget's own
// setBandLabel()/second-antenna behavior is covered separately in
// test_rotorwidget.cpp.
class TestRotorSettings : public QObject
{
    Q_OBJECT

private slots:
    void defaultSettingsKeepTwoRotorBehaviorUnchanged();
    void settingsRoundTripSingleRotorMode();
    void settingsRoundTripRenamedRotorLabel();
    void appControllerRoutesBandsForTwoRotorDefault();
    void appControllerNoOpsForBandMappedToDisabledRotor();
    void hamlibModelDeviceAndBaudDefaultToYaesuAndRoundTripPerSlot();
};

void TestRotorSettings::defaultSettingsKeepTwoRotorBehaviorUnchanged()
{
    // A freshly default-constructed ContestSettings must still describe
    // exactly the original fixed two-mast VHF/UHF station -- both slots
    // enabled, labelled "2m"/"70cm", same band routing as the old
    // hardcoded 144->2m/432->70cm/1296-shares-2m wiring.
    ContestSettings settings;
    QVERIFY(settings.rotor1Enabled);
    QCOMPARE(settings.rotor1Label, QStringLiteral("2m"));
    QVERIFY(settings.rotor2Enabled);
    QCOMPARE(settings.rotor2Label, QStringLiteral("70cm"));
    QCOMPARE(settings.band144RotorSlot, ContestSettings::RotorSlot::Slot1);
    QCOMPARE(settings.band432RotorSlot, ContestSettings::RotorSlot::Slot2);
    QCOMPARE(settings.band1296RotorSlot, ContestSettings::RotorSlot::Slot1);
}

void TestRotorSettings::settingsRoundTripSingleRotorMode()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("single_rotor.sqlite")), QStringLiteral("rotor_single_mode")));

    // Single-rotor mode: the operator turned slot 2 off entirely and
    // left 432 MHz unassigned rather than routing it through the
    // (now nonexistent) second mast.
    ContestSettings settings;
    settings.rotor2Enabled = false;
    settings.band432RotorSlot = ContestSettings::RotorSlot::None;
    settings.saveTo(db);

    ContestSettings reloaded;
    reloaded.loadFrom(db);
    QVERIFY(reloaded.rotor1Enabled);
    QCOMPARE(reloaded.rotor1Label, QStringLiteral("2m"));
    QVERIFY(!reloaded.rotor2Enabled);
    QCOMPARE(reloaded.band144RotorSlot, ContestSettings::RotorSlot::Slot1);
    QCOMPARE(reloaded.band432RotorSlot, ContestSettings::RotorSlot::None);
    QCOMPARE(reloaded.band1296RotorSlot, ContestSettings::RotorSlot::Slot1);
}

void TestRotorSettings::settingsRoundTripRenamedRotorLabel()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("renamed_rotor.sqlite")), QStringLiteral("rotor_renamed_label")));

    // The operator's own example: repurposing both slots for a
    // shortwave/HF setup instead of the original 2m/70cm masts.
    ContestSettings settings;
    settings.rotor1Label = QStringLiteral("Kurzwelle");
    settings.rotor2Label = QStringLiteral("HF");
    settings.saveTo(db);

    ContestSettings reloaded;
    reloaded.loadFrom(db);
    QCOMPARE(reloaded.rotor1Label, QStringLiteral("Kurzwelle"));
    QCOMPARE(reloaded.rotor2Label, QStringLiteral("HF"));
    // Renaming must not silently disable or re-route anything else.
    QVERIFY(reloaded.rotor1Enabled);
    QVERIFY(reloaded.rotor2Enabled);
    QCOMPARE(reloaded.band144RotorSlot, ContestSettings::RotorSlot::Slot1);
    QCOMPARE(reloaded.band432RotorSlot, ContestSettings::RotorSlot::Slot2);
}

void TestRotorSettings::appControllerRoutesBandsForTwoRotorDefault()
{
    // No openDatabase() call needed: AppController's default-constructed
    // m_settings already carries ContestSettings' own defaults, which is
    // exactly what activeRotorForBand() reads.
    AppController controller;
    QCOMPARE(controller.activeRotorForBand(QStringLiteral("144")), &controller.rotor1Client());
    QCOMPARE(controller.activeRotorForBand(QStringLiteral("432")), &controller.rotor2Client());
    // 1296 shares slot 1 by default, same as the old band1296RotorRef
    // default of "2m".
    QCOMPARE(controller.activeRotorForBand(QStringLiteral("1296")), &controller.rotor1Client());
    QVERIFY(controller.activeRotorForBand(QStringLiteral("60")) == nullptr);
}

void TestRotorSettings::appControllerNoOpsForBandMappedToDisabledRotor()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("disabled_rotor.sqlite"))));

    ContestSettings settings = controller.settings();
    settings.rotor2Enabled = false;
    controller.setSettings(settings);

    // 432 MHz is still assigned to slot 2 (band432RotorSlot keeps its
    // default), but slot 2 is now disabled -- must gracefully resolve to
    // "no rotor", not crash and not silently fall back to slot 1.
    QVERIFY(controller.activeRotorForBand(QStringLiteral("432")) == nullptr);
    // 144 MHz stays mapped to slot 1, which is still enabled.
    QCOMPARE(controller.activeRotorForBand(QStringLiteral("144")), &controller.rotor1Client());
}

void TestRotorSettings::hamlibModelDeviceAndBaudDefaultToYaesuAndRoundTripPerSlot()
{
    // Operator confirmed 2026-09-11 the station's rotor will be Yaesu --
    // both slots must default to 601 (GS-232A, core/RotorModels.h's own
    // first entry) and 9600 baud, with no invented default serial
    // device (see ContestSettings::rotor1Device's own comment).
    ContestSettings defaults;
    QCOMPARE(defaults.rotor1HamlibModel, 601);
    QVERIFY(defaults.rotor1Device.isEmpty());
    QCOMPARE(defaults.rotor1Baud, 9600);
    QCOMPARE(defaults.rotor2HamlibModel, 601);
    QVERIFY(defaults.rotor2Device.isEmpty());
    QCOMPARE(defaults.rotor2Baud, 9600);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("rotor_hamlib.sqlite")), QStringLiteral("rotor_hamlib")));

    // The two slots must round-trip independently -- picking an ERC on
    // slot 1 must not affect slot 2's own Yaesu default, matching every
    // other rotor1*/rotor2* pair's independence.
    ContestSettings settings;
    settings.rotor1HamlibModel = 404; // ERC
    settings.rotor1Device = QStringLiteral("/dev/tty.usbserial-1410");
    settings.rotor1Baud = 19200;
    settings.rotor2Device = QStringLiteral("/dev/tty.usbserial-70CM");
    settings.saveTo(db);

    ContestSettings reloaded;
    reloaded.loadFrom(db);
    QCOMPARE(reloaded.rotor1HamlibModel, 404);
    QCOMPARE(reloaded.rotor1Device, QStringLiteral("/dev/tty.usbserial-1410"));
    QCOMPARE(reloaded.rotor1Baud, 19200);
    QCOMPARE(reloaded.rotor2HamlibModel, 601);
    QCOMPARE(reloaded.rotor2Device, QStringLiteral("/dev/tty.usbserial-70CM"));
    QCOMPARE(reloaded.rotor2Baud, 9600);
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestRotorSettings tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_rotor_settings.moc"
