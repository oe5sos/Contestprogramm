// rigctld started by the program itself when the CAT radio needs one
// (AppController::rigctldLaunchFor / superviseRigctld): only for a
// loopback host with a device named, only after the client's own dial
// failed, stopped again when the settings no longer want it, restarted
// when they change. The rotor half of this has worked since
// 2026-09-11 (tests/test_rotctld_supervision.cpp); this is the same
// arrangement for the transceiver, built because the station is
// getting a TS-590 and nobody wants to open a terminal for it.

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTcpServer>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/RigModels.h"
#include "core/RigctldProcess.h"
#include "ui/SettingsDialog.h"

#include <QComboBox>

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

// A stand-in rigctld on PATH: records its arguments and then just
// stays alive, like the real one would with a radio behind it.
bool installFakeRigctld(const QDir& dir, const QString& argsFile)
{
    QFile script(dir.filePath(QStringLiteral("rigctld")));
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

class TestRigctldSupervision : public QObject
{
    Q_OBJECT

private slots:
    void launchOnlyForALoopbackHostWithADevice();
    void argumentsAreTheDocumentedRigctldForm();
    void startsAfterTheClientFailsAndFollowsTheSettings();
    void theModelListComesFromTheInstalledHamlib();
    void theWholeChainWorksWithARealHamlib();
    void thePickerOffersEveryRadioHamlibKnows();
};

void TestRigctldSupervision::launchOnlyForALoopbackHostWithADevice()
{
    ContestSettings settings;
    settings.rigctldHost = QStringLiteral("127.0.0.1");
    settings.rigctldPort = 4532;
    settings.rigctldHamlibModel = 2037; // TS-590SG
    settings.rigctldDevice = QStringLiteral("/dev/cu.usbserial-1410");
    settings.rigctldBaud = 115200;
    const auto launch = AppController::rigctldLaunchFor(settings);
    QVERIFY(launch.has_value());
    QCOMPARE(launch->hamlibModel, 2037);
    QCOMPARE(launch->device, QStringLiteral("/dev/cu.usbserial-1410"));
    QCOMPARE(launch->baud, 115200);
    QCOMPARE(launch->port, quint16(4532));

    // "localhost" counts as ours too; another machine's rigctld never.
    settings.rigctldHost = QStringLiteral("localhost");
    QVERIFY(AppController::rigctldLaunchFor(settings).has_value());
    settings.rigctldHost = QStringLiteral("172.30.30.16");
    QVERIFY(!AppController::rigctldLaunchFor(settings).has_value());

    // No device named: the operator runs rigctld their own way. This is
    // the one deliberate way to switch the whole thing off.
    settings.rigctldHost = QStringLiteral("127.0.0.1");
    settings.rigctldDevice = QStringLiteral("   ");
    QVERIFY(!AppController::rigctldLaunchFor(settings).has_value());
}

void TestRigctldSupervision::argumentsAreTheDocumentedRigctldForm()
{
    const QStringList args = RigctldProcess::arguments(2037, QStringLiteral("/dev/cu.usbserial-1410"),
                                                       115200, 4532);
    QCOMPARE(args, QStringList({QStringLiteral("-m"), QStringLiteral("2037"),
                                QStringLiteral("-r"), QStringLiteral("/dev/cu.usbserial-1410"),
                                QStringLiteral("-s"), QStringLiteral("115200"),
                                QStringLiteral("-T"), QStringLiteral("127.0.0.1"),
                                QStringLiteral("-t"), QStringLiteral("4532")}));

    // No device, no baud: Hamlib's own defaults, and -r/-s stay away
    // rather than going out empty.
    const QStringList bare = RigctldProcess::arguments(1, QString(), 0, 4532);
    QCOMPARE(bare, QStringList({QStringLiteral("-m"), QStringLiteral("1"),
                                QStringLiteral("-T"), QStringLiteral("127.0.0.1"),
                                QStringLiteral("-t"), QStringLiteral("4532")}));

    // Loopback always: a transmitter anyone on the network can key is
    // a transmitter that will eventually be keyed by someone else.
    QVERIFY(args.contains(QStringLiteral("127.0.0.1")));
    QVERIFY(!args.contains(QStringLiteral("0.0.0.0")));
}

void TestRigctldSupervision::startsAfterTheClientFailsAndFollowsTheSettings()
{
#ifdef Q_OS_WIN
    QSKIP("Der Stellvertreter-rigctld ist ein Shell-Skript; unter Windows startet QProcess es nicht.");
#endif
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString argsFile = dir.filePath(QStringLiteral("rigctld.args"));
    QVERIFY(installFakeRigctld(QDir(dir.path()), argsFile));

    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("rig-supervision.sqlite"))));
    const quint16 port = freePort();
    ContestSettings settings = controller.settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.rigctldHost = QStringLiteral("127.0.0.1");
    settings.rigctldPort = port;
    settings.rigctldHamlibModel = 2037;
    settings.rigctldDevice = QStringLiteral("/dev/cu.fake-A");
    settings.rigctldBaud = 115200;
    controller.setSettings(settings);

    // Nothing listens on the port: the client's dial is refused, and
    // that -- not the settings save itself -- starts our rigctld.
    QTRY_VERIFY_WITH_TIMEOUT(controller.rigctldRunning(), 8000);
    // 8 s, not 3: a freshly started process has to write its argument
    // line to a file, and on a loaded machine (ctest -j4) that took
    // 3.85 s once. Measured here is "the process writes its arguments",
    // not "it does so within three seconds".
    QTRY_VERIFY_WITH_TIMEOUT(!readArgs(argsFile).isEmpty(), 8000);
    const QString args = readArgs(argsFile);
    QVERIFY2(args.contains(QStringLiteral("-m 2037")), qPrintable(args));
    QVERIFY2(args.contains(QStringLiteral("-r /dev/cu.fake-A")), qPrintable(args));
    QVERIFY2(args.contains(QStringLiteral("-s 115200")), qPrintable(args));
    QVERIFY2(args.contains(QStringLiteral("-t %1").arg(port)), qPrintable(args));
    QVERIFY2(args.contains(QStringLiteral("-T 127.0.0.1")), qPrintable(args));

    // Another device: the running one goes, the client's next failed
    // dial brings one with the new arguments.
    settings.rigctldDevice = QStringLiteral("/dev/cu.fake-B");
    controller.setSettings(settings);
    QTRY_VERIFY_WITH_TIMEOUT(readArgs(argsFile).contains(QStringLiteral("-r /dev/cu.fake-B")), 8000);
    QVERIFY(controller.rigctldRunning());

    // Pointed at another machine: not ours to run any more.
    settings.rigctldHost = QStringLiteral("172.30.30.16");
    controller.setSettings(settings);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.rigctldRunning(), 8000);

    // And back on the loopback, but with the device cleared: still not
    // ours -- that is how an operator says "I start rigctld myself".
    settings.rigctldHost = QStringLiteral("127.0.0.1");
    settings.rigctldDevice.clear();
    controller.setSettings(settings);
    QVERIFY(!controller.rigctldRunning());
}

void TestRigctldSupervision::theModelListComesFromTheInstalledHamlib()
{
    const QVector<RigModel> models = rigModelsFromHamlib();
    if (models.isEmpty()) {
        QSKIP("Kein rigctl auf dieser Maschine -- dann trägt die kurze Liste allein.");
    }

    // The parser has to survive the real "rigctl -l" output, which is a
    // fixed-width table with a header line and model names that contain
    // spaces. If it ever silently matched nothing, the picker would
    // quietly shrink back to the eight curated entries.
    QVERIFY2(models.size() > 50, qPrintable(QStringLiteral("nur %1 Modelle gelesen").arg(models.size())));

    const auto named = [&models](int id) {
        for (const RigModel& model : models) {
            if (model.hamlibId == id) {
                return model.name;
            }
        }
        return QString();
    };
    // The station's own radio, both variants -- the numbers this whole
    // feature hangs on.
    QCOMPARE(named(2031), QStringLiteral("Kenwood TS-590S"));
    QCOMPARE(named(2037), QStringLiteral("Kenwood TS-590SG"));
    // A name with a space in it, to show the column was not cut short.
    QVERIFY2(named(2) == QStringLiteral("Hamlib NET rigctl"), qPrintable(named(2)));

    // Every curated entry has to NAME THE SAME RADIO the installed
    // Hamlib has under that number -- not merely resolve to something.
    // "Exists" is not enough: FT-710 was briefly written down as 1042,
    // which exists perfectly well and is the FTDX-10, so a check for
    // existence alone stayed green while the picker offered one radio's
    // name in front of another radio's number (measured 2026-10-16).
    //
    // Compared against the part before " / " for the entries that cover
    // two models in one line ("Yaesu FT-991 / FT-991A").
    for (const RigModel& curated : commonRigModels()) {
        const QString fromHamlib = named(curated.hamlibId);
        QVERIFY2(!fromHamlib.isEmpty(),
                 qPrintable(QStringLiteral("%1 (%2) kennt dieses Hamlib nicht")
                                .arg(curated.name).arg(curated.hamlibId)));
        const QString ours = curated.name.section(QStringLiteral(" / "), 0, 0).trimmed();
        QVERIFY2(ours == fromHamlib,
                 qPrintable(QStringLiteral("Nummer %1 heißt in diesem Hamlib \"%2\", in der Liste aber \"%3\"")
                                .arg(curated.hamlibId).arg(fromHamlib, curated.name)));
    }
}


// The whole thing against the REAL Hamlib, end to end: the program
// starts rigctld itself, the CAT client dials it, and a frequency
// comes back. The radio is Hamlib's own dummy (model 1), because the
// station's TS-590 does not exist yet -- everything else on the path
// is the real article, including rigctld.
//
// What this cannot say: whether the TS-590 answers at 115200 with its
// menu set the way it has to be. That is for the day the radio
// arrives.
void TestRigctldSupervision::theWholeChainWorksWithARealHamlib()
{
    if (RigctldProcess::findBinary().isEmpty()) {
        QSKIP("Kein rigctld auf dieser Maschine.");
    }
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("rig-echt.sqlite"))));
    const quint16 port = freePort();
    ContestSettings settings = controller.settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.rigctldHost = QStringLiteral("127.0.0.1");
    settings.rigctldPort = port;
    settings.rigctldHamlibModel = 1;  // Hamlib dummy -- no radio needed
    settings.rigctldDevice = QStringLiteral("/dev/null");
    settings.rigctldBaud = 9600;
    controller.setSettings(settings);

    QTRY_VERIFY_WITH_TIMEOUT(controller.rigctldRunning(), 10000);
    // The client reaches it and reads from it: the dummy answers
    // 145.000 MHz, which is a frequency arriving through a rigctld this
    // program started by itself.
    QTRY_VERIFY_WITH_TIMEOUT(controller.rigctldClient().isConnected(), 15000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.rigctldClient().frequencyHz() > 0, 15000);
    QCOMPARE(controller.rigctldClient().frequencyHz(), static_cast<qint64>(145000000));

    // And it goes away with the settings, not only with the program.
    settings.rigctldDevice.clear();
    controller.setSettings(settings);
    QVERIFY(!controller.rigctldRunning());
}


// The picker itself: the curated handful on top, then every radio the
// installed Hamlib supports. Without the second part an operator with
// a radio outside the short list has no way in at all -- there is no
// free number field, by design, because a number typed from memory is
// how "FT-710 is 1042" happens.
void TestRigctldSupervision::thePickerOffersEveryRadioHamlibKnows()
{
    if (rigModelsFromHamlib().isEmpty()) {
        QSKIP("Kein rigctl auf dieser Maschine.");
    }
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("picker.sqlite"))));

    SettingsDialog dialog(controller.settings(), controller.availableContestDefinitions(),
                          &controller.callsignLocatorLookup(), nullptr);
    QComboBox* picker = dialog.findChild<QComboBox*>();
    QVERIFY(picker);
    // findChild gives the first combo in the dialog, which is not
    // necessarily the rig one -- find it by what it contains.
    const auto rigPicker = [&dialog]() -> QComboBox* {
        for (QComboBox* combo : dialog.findChildren<QComboBox*>()) {
            if (combo->findData(2031) >= 0) { // Kenwood TS-590S
                return combo;
            }
        }
        return nullptr;
    }();
    QVERIFY2(rigPicker, "kein Funkgerät-Auswahlfeld gefunden");
    QVERIFY2(rigPicker->count() > 50,
             qPrintable(QStringLiteral("nur %1 Einträge im Funkgerät-Feld").arg(rigPicker->count())));

    // The curated ones come first, before the separator.
    QCOMPARE(rigPicker->itemData(0).toInt(), commonRigModels().first().hamlibId);

    // And a radio far outside the short list is reachable.
    QVERIFY2(rigPicker->findData(3073) >= 0, "Icom IC-7300 fehlt");
    QVERIFY2(rigPicker->findData(1042) >= 0, "Yaesu FTDX-10 fehlt");
}

QTEST_MAIN(TestRigctldSupervision)
#include "test_rigctld_supervision.moc"
