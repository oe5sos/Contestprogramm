// Datei > Startcheck (bereit?): data/ReadinessCheck.h over a snapshot,
// and the window MainWindow fills from the live program.

#include <QtTest>

#include <QApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimeZone>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ReadinessCheck.h"
#include "ui/MainWindow.h"
#include "ui/ReadinessWindow.h"

#include <memory>

using namespace Contestprogramm;

namespace {

const ReadinessItem* itemWithCode(const ReadinessResult& result, const QString& code)
{
    for (const ReadinessItem& item : result.items) {
        if (item.code == code) {
            return &item;
        }
    }
    return nullptr;
}

// Everything in order: a station on the Feuerkogel, the UHF contest
// twelve days away, an empty log, every link up, data on disk.
ReadinessContext readyContext()
{
    ReadinessContext ctx;
    ctx.nowUtc = QDateTime(QDate(2026, 9, 21), QTime(6, 0), QTimeZone::utc());
    ctx.ownCallsign = QStringLiteral("OE5SOS");
    ctx.ownGrid = QStringLiteral("JN67UT");
    ctx.useExactOwnLocation = true;
    ctx.ownExactLatitude = 47.815814;
    ctx.ownExactLongitude = 13.721506;
    ctx.ownElevationM = 1587.0;
    ctx.antennaHeightM = 10.0;
    ctx.clockChecked = true;
    ctx.clockReachable = true;
    ctx.clockOffsetSecs = 0;
    ctx.clockSource = QStringLiteral("www.google.com");
    ctx.contestFound = true;
    ctx.contestName = QStringLiteral("IARU Region 1 UHF/Microwave Contest");
    ctx.contestBands = {QStringLiteral("432"), QStringLiteral("1296")};
    ctx.window.startUtc = QDateTime(QDate(2026, 10, 3), QTime(14, 0), QTimeZone::utc());
    ctx.window.endUtc = QDateTime(QDate(2026, 10, 4), QTime(14, 0), QTimeZone::utc());
    ctx.qsoCount = 0;
    ctx.catTarget = QStringLiteral("127.0.0.1:4532");
    ctx.cat = LinkState::Connected;
    ctx.rotor1 = {true, QStringLiteral("2m"), QStringLiteral("127.0.0.1:4533"), LinkState::Connected, QString()};
    ctx.rotor2 = {true, QStringLiteral("70cm"), QStringLiteral("127.0.0.1:4534"), LinkState::Connected, QString()};
    ctx.on4kstConfigured = true;
    ctx.on4kst = LinkState::Connected;
    ctx.clusterTarget = QStringLiteral("dxc.example:7300");
    ctx.cluster = LinkState::Connected;
    ctx.backupDirectory = QStringLiteral("/tmp/backups");
    ctx.backupDirectoryWritable = true;
    ctx.lastBackupUtc = ctx.nowUtc.addSecs(-120);
    ctx.terrainLoadedForOwnLocation = true;
    ctx.importedLocators = 1234;
    ctx.mirrorDirectory = QStringLiteral("/Volumes/STICK/Contestprogramm");
    ctx.mirrorWritable = true;
    return ctx;
}

} // namespace

class TestReadinessCheck : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void everythingInOrderIsReadyWithNoFindings();
    void missingStationDataIsAnError();
    void exactLocationOutsideTheLocatorIsAWarning();
    void contestWindowBeforeDuringAndAfter();
    void qsosBeforeTheStartAreAWarning();
    void linksDownAreWarningsAFailedRotctldAnError();
    void unconfiguredLinksAreHints();
    void dataOnDisk();
    void spansReadNaturally();
    void windowShowsTheLiveSnapshot();
    void secondBackupFolderIsWiredThrough();
};

void TestReadinessCheck::everythingInOrderIsReadyWithNoFindings()
{
    const ReadinessResult result = checkReadiness(readyContext());
    QVERIFY2(result.ready(), qPrintable(result.summaryText()));
    QCOMPARE(result.errors, 0);
    QCOMPARE(result.warnings, 0);
    QCOMPARE(result.hints, 0);
    QCOMPARE(result.summaryText(), QStringLiteral("Bereit — alles in Ordnung."));
    QCOMPARE(itemWithCode(result, QStringLiteral("window"))->detail,
             QStringLiteral("Start in 12 T 8 h — Sa 03.10. 14:00 – So 04.10. 14:00 UTC"));
    QCOMPARE(itemWithCode(result, QStringLiteral("log"))->detail, QStringLiteral("Leer, nächste Nummer 001"));
    QCOMPARE(itemWithCode(result, QStringLiteral("contest"))->detail,
             QStringLiteral("IARU Region 1 UHF/Microwave Contest — 432 / 1296"));
    QCOMPARE(itemWithCode(result, QStringLiteral("own_elevation"))->detail, QStringLiteral("1587 m, Antenne 10 m darüber"));
}

void TestReadinessCheck::missingStationDataIsAnError()
{
    ReadinessContext ctx = readyContext();
    ctx.ownCallsign.clear();
    ctx.ownGrid = QStringLiteral("JN6");
    ctx.ownElevationM = 0.0;
    const ReadinessResult result = checkReadiness(ctx);
    QVERIFY(!result.ready());
    QCOMPARE(result.errors, 2);
    QCOMPARE(itemWithCode(result, QStringLiteral("own_call"))->level, ReadinessItem::Level::Error);
    QCOMPARE(itemWithCode(result, QStringLiteral("own_grid"))->level, ReadinessItem::Level::Error);
    QVERIFY(itemWithCode(result, QStringLiteral("own_grid"))->detail.contains(QStringLiteral("JN6")));
    QCOMPARE(itemWithCode(result, QStringLiteral("own_elevation"))->level, ReadinessItem::Level::Hint);
    QVERIFY(result.summaryText().startsWith(QStringLiteral("Nicht bereit — 2 Fehler")));
}

void TestReadinessCheck::exactLocationOutsideTheLocatorIsAWarning()
{
    ReadinessContext ctx = readyContext();
    // The home locator (Gmunden, JN67VV) left in the settings, but the
    // exact position is the Feuerkogel (JN67UT): the log would claim
    // the wrong square.
    ctx.ownGrid = QStringLiteral("JN67VV");
    ReadinessResult result = checkReadiness(ctx);
    QVERIFY(result.ready()); // a warning, not a stop
    const ReadinessItem* grid = itemWithCode(result, QStringLiteral("own_grid"));
    QCOMPARE(grid->level, ReadinessItem::Level::Warning);
    QVERIFY2(grid->detail.contains(QStringLiteral("liegt in JN67UT")), qPrintable(grid->detail));

    ctx.ownExactLatitude = 0.0;
    ctx.ownExactLongitude = 0.0;
    result = checkReadiness(ctx);
    QCOMPARE(itemWithCode(result, QStringLiteral("own_grid"))->level, ReadinessItem::Level::Warning);
    QVERIFY(itemWithCode(result, QStringLiteral("own_grid"))->detail.contains(QStringLiteral("ohne Koordinaten")));

    ctx.useExactOwnLocation = false;
    result = checkReadiness(ctx);
    QCOMPARE(itemWithCode(result, QStringLiteral("own_grid"))->level, ReadinessItem::Level::Ok);
    QCOMPARE(itemWithCode(result, QStringLiteral("own_grid"))->detail, QStringLiteral("JN67VV"));
}

void TestReadinessCheck::contestWindowBeforeDuringAndAfter()
{
    ReadinessContext ctx = readyContext();
    ctx.nowUtc = QDateTime(QDate(2026, 10, 3), QTime(16, 30), QTimeZone::utc());
    ReadinessResult result = checkReadiness(ctx);
    QCOMPARE(itemWithCode(result, QStringLiteral("window"))->level, ReadinessItem::Level::Ok);
    QCOMPARE(itemWithCode(result, QStringLiteral("window"))->detail,
             QStringLiteral("Läuft, noch 21 h 30 min — bis So 04.10. 14:00 UTC"));

    ctx.nowUtc = QDateTime(QDate(2026, 10, 6), QTime(8, 0), QTimeZone::utc());
    result = checkReadiness(ctx);
    QCOMPARE(itemWithCode(result, QStringLiteral("window"))->level, ReadinessItem::Level::Warning);
    QVERIFY(itemWithCode(result, QStringLiteral("window"))->detail.startsWith(QStringLiteral("Vorbei seit 1 T 18 h")));

    ctx.window = ContestWindow();
    result = checkReadiness(ctx);
    QCOMPARE(itemWithCode(result, QStringLiteral("window"))->level, ReadinessItem::Level::Hint);

    ctx.contestFound = false;
    result = checkReadiness(ctx);
    QCOMPARE(itemWithCode(result, QStringLiteral("contest"))->level, ReadinessItem::Level::Error);
    QVERIFY(!itemWithCode(result, QStringLiteral("window")));
    QVERIFY(!itemWithCode(result, QStringLiteral("log")));
}

void TestReadinessCheck::qsosBeforeTheStartAreAWarning()
{
    ReadinessContext ctx = readyContext();
    ctx.qsoCount = 7;
    ReadinessResult result = checkReadiness(ctx);
    const ReadinessItem* log = itemWithCode(result, QStringLiteral("log"));
    QCOMPARE(log->level, ReadinessItem::Level::Warning);
    QVERIFY2(log->detail.startsWith(QStringLiteral("7 QSOs im Log vor dem Start")), qPrintable(log->detail));

    // The same seven while the contest runs: just the next number --
    // per band when the definition numbers per band.
    ctx.nowUtc = ctx.window.startUtc.addSecs(3600);
    result = checkReadiness(ctx);
    QCOMPARE(itemWithCode(result, QStringLiteral("log"))->level, ReadinessItem::Level::Ok);
    QCOMPARE(itemWithCode(result, QStringLiteral("log"))->detail, QStringLiteral("7 QSOs, nächste Nummer 008"));
    ctx.nextSerials = {{QStringLiteral("432"), 6}, {QStringLiteral("1296"), 3}};
    result = checkReadiness(ctx);
    QCOMPARE(itemWithCode(result, QStringLiteral("log"))->detail, QStringLiteral("7 QSOs, nächste Nummer 432: 006 · 1296: 003"));
}

void TestReadinessCheck::linksDownAreWarningsAFailedRotctldAnError()
{
    ReadinessContext ctx = readyContext();
    ctx.cat = LinkState::Connecting;
    ctx.rotor1.link = LinkState::Disconnected;
    ctx.rotor2.link = LinkState::Disconnected;
    ctx.rotor2.rotctldError = QStringLiteral("rotctld: error = IO error\nCannot open /dev/tty.usbserial-B");
    ctx.on4kst = LinkState::Disconnected;
    ctx.cluster = LinkState::Disconnected;
    const ReadinessResult result = checkReadiness(ctx);
    QVERIFY(!result.ready());
    QCOMPARE(result.errors, 1);
    QCOMPARE(result.warnings, 4);
    QCOMPARE(itemWithCode(result, QStringLiteral("cat"))->level, ReadinessItem::Level::Warning);
    QVERIFY(itemWithCode(result, QStringLiteral("cat"))->detail.contains(QStringLiteral("verbindet…")));
    QCOMPARE(itemWithCode(result, QStringLiteral("rotor1"))->level, ReadinessItem::Level::Warning);
    QCOMPARE(itemWithCode(result, QStringLiteral("rotor1"))->title, QStringLiteral("Rotor 1 (2m)"));
    QCOMPARE(itemWithCode(result, QStringLiteral("rotor2"))->level, ReadinessItem::Level::Error);
    QCOMPARE(itemWithCode(result, QStringLiteral("rotor2"))->detail,
             QStringLiteral("rotctld 127.0.0.1:4534 konnte nicht gestartet werden: rotctld: error = IO error"));
    QCOMPARE(itemWithCode(result, QStringLiteral("on4kst"))->level, ReadinessItem::Level::Warning);
    QCOMPARE(itemWithCode(result, QStringLiteral("cluster"))->level, ReadinessItem::Level::Warning);
}

void TestReadinessCheck::unconfiguredLinksAreHints()
{
    ReadinessContext ctx = readyContext();
    ctx.catTarget.clear();
    ctx.rotor2.enabled = false;
    ctx.on4kstConfigured = false;
    ctx.clusterTarget.clear();
    const ReadinessResult result = checkReadiness(ctx);
    QVERIFY(result.ready());
    QCOMPARE(result.warnings, 0);
    QCOMPARE(result.hints, 4);
    QCOMPARE(itemWithCode(result, QStringLiteral("rotor2"))->detail, QStringLiteral("Abgeschaltet."));
}

void TestReadinessCheck::dataOnDisk()
{
    ReadinessContext ctx = readyContext();
    ctx.backupDirectoryWritable = false;
    ctx.terrainLoadedForOwnLocation = false;
    ctx.importedLocators = 0;
    ReadinessResult result = checkReadiness(ctx);
    QCOMPARE(itemWithCode(result, QStringLiteral("backup"))->level, ReadinessItem::Level::Error);
    QCOMPARE(itemWithCode(result, QStringLiteral("terrain"))->level, ReadinessItem::Level::Warning);
    QCOMPARE(itemWithCode(result, QStringLiteral("locators"))->level, ReadinessItem::Level::Hint);

    ctx.backupDirectoryWritable = true;
    ctx.lastBackupUtc = QDateTime();
    result = checkReadiness(ctx);
    QCOMPARE(itemWithCode(result, QStringLiteral("backup"))->level, ReadinessItem::Level::Hint);

    // The second copy: fine, unplugged, none.
    QCOMPARE(itemWithCode(result, QStringLiteral("mirror"))->level, ReadinessItem::Level::Ok);
    ctx.mirrorWritable = false;
    result = checkReadiness(ctx);
    QCOMPARE(itemWithCode(result, QStringLiteral("mirror"))->level, ReadinessItem::Level::Warning);
    QVERIFY(itemWithCode(result, QStringLiteral("mirror"))->detail.contains(QStringLiteral("Stick eingesteckt?")));
    ctx.mirrorDirectory.clear();
    result = checkReadiness(ctx);
    QCOMPARE(itemWithCode(result, QStringLiteral("mirror"))->level, ReadinessItem::Level::Hint);
    ctx.lastBackupUtc = ctx.nowUtc.addSecs(-60);
    result = checkReadiness(ctx);
    QCOMPARE(itemWithCode(result, QStringLiteral("backup"))->detail,
             QStringLiteral("Zuletzt Mo 21.09. 05:59 UTC, nach /tmp/backups"));
}

void TestReadinessCheck::spansReadNaturally()
{
    QCOMPARE(describeSpan(12 * 86400 + 8 * 3600 + 5 * 60), QStringLiteral("12 T 8 h"));
    QCOMPARE(describeSpan(2 * 3600 + 10 * 60), QStringLiteral("2 h 10 min"));
    QCOMPARE(describeSpan(7 * 60 + 30), QStringLiteral("7 min"));
    QCOMPARE(describeSpan(-3600), QStringLiteral("1 h 0 min"));
}

void TestReadinessCheck::windowShowsTheLiveSnapshot()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = std::make_unique<AppController>();
    QVERIFY(controller->openDatabase(dir.filePath(QStringLiteral("ready.sqlite"))));
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Host = QStringLiteral("127.0.0.1");
    settings.rotor1Port = 1; // nothing there
    settings.rotor2Enabled = false;
    settings.on4kstUsername.clear();
    controller->setSettings(settings);

    MainWindow window(*controller);
    QMetaObject::invokeMethod(&window, "openReadinessWindow");
    auto* check = window.findChild<ReadinessWindow*>();
    QVERIFY(check);
    QVERIFY(check->rowCount() >= 12);
    QStringList titles;
    for (int row = 0; row < check->rowCount(); ++row) {
        titles << check->rowText(row, 2);
    }
    QVERIFY2(titles.contains(QStringLiteral("Rufzeichen")), qPrintable(titles.join(QStringLiteral(" | "))));
    QVERIFY(titles.contains(QStringLiteral("Rotor 1 (2m)")));
    QVERIFY(titles.contains(QStringLiteral("Sicherung")));
    // The UHF contest with its October window; the empty log.
    bool contestSeen = false;
    for (int row = 0; row < check->rowCount(); ++row) {
        if (check->rowText(row, 2) == QStringLiteral("Contest")) {
            contestSeen = check->rowText(row, 3).contains(QStringLiteral("UHF"));
        }
        if (check->rowText(row, 2) == QStringLiteral("Log")) {
            QCOMPARE(check->rowText(row, 3), QStringLiteral("Leer, nächste Nummer 001"));
        }
    }
    QVERIFY(contestSeen);
    QVERIFY(check->summaryText().contains(QStringLiteral("ereit")));
}

void TestReadinessCheck::secondBackupFolderIsWiredThrough()
{
    // The settings-table key reaches LogBackup on the next start, the
    // Startcheck reads it back, and "entfernen" clears both.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString stick = dir.filePath(QStringLiteral("stick"));
    {
        AppController first;
        QVERIFY(first.openDatabase(dir.filePath(QStringLiteral("mirror.sqlite"))));
        QVERIFY(first.logBackup());
        QVERIFY(first.logBackup()->mirrorDirectory().isEmpty());
        first.database().setSettingValue(QStringLiteral("backup_mirror_dir"), stick);
    }
    auto controller = std::make_unique<AppController>();
    QVERIFY(controller->openDatabase(dir.filePath(QStringLiteral("mirror.sqlite"))));
    QCOMPARE(controller->logBackup()->mirrorDirectory(), stick);
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    controller->setSettings(settings);

    MainWindow window(*controller);
    QMetaObject::invokeMethod(&window, "openReadinessWindow");
    auto* check = window.findChild<ReadinessWindow*>();
    QVERIFY(check);
    bool seen = false;
    for (int row = 0; row < check->rowCount(); ++row) {
        if (check->rowText(row, 2) == QStringLiteral("Zweite Sicherung")) {
            seen = true;
            QCOMPARE(check->rowText(row, 0), QStringLiteral("OK"));
            QVERIFY(check->rowText(row, 3).contains(stick));
        }
    }
    QVERIFY(seen);

    QMetaObject::invokeMethod(&window, "clearBackupMirror");
    QVERIFY(controller->logBackup()->mirrorDirectory().isEmpty());
    QVERIFY(controller->database().settingValue(QStringLiteral("backup_mirror_dir")).isEmpty());
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestReadinessCheck tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_readiness_check.moc"
