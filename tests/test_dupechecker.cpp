#include <QtTest>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "data/ContestDatabase.h"
#include "data/DupeChecker.h"
#include "data/QsoRecord.h"

using namespace Contestprogramm;

namespace {

QsoRecord makeRecord(const QString& callsign, const QString& band, const QString& mode, const QString& contestId)
{
    QsoRecord record;
    record.callsign = callsign;
    record.band = band;
    record.mode = mode;
    record.timestampUtc = QStringLiteral("2026-06-13T12:00:00Z");
    record.contestId = contestId;
    return record;
}

} // namespace

class TestDupeChecker : public QObject
{
    Q_OBJECT

private slots:
    void exactMatchIsDupe();
    void differentBandIsNotDupe();
    void differentModeIsNotDupe();
    void differentContestIsNotDupe();
    void caseAndWhitespaceAreNormalized();
    void narrowerScopeOverrideTreatsCrossBandAsDupe();
    void invalidQsoIsNotDupe();
    void lastErrorEmptyAfterGenuineResult();
    void lastErrorSetOnQueryFailure();
    void firstMatchIdIsTheEarliestValidQso();
};

void TestDupeChecker::exactMatchIsDupe()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("exact.sqlite")), QStringLiteral("dupe_exact")));

    QsoRecord existing = makeRecord(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("OE_VHF_UHF"));
    QVERIFY(db.insertQso(existing));

    const QStringList scope = {QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")};
    DupeChecker checker(db);
    QVERIFY(checker.isDupe(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("OE_VHF_UHF"), scope));
}

void TestDupeChecker::differentBandIsNotDupe()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("band.sqlite")), QStringLiteral("dupe_band")));

    QsoRecord existing = makeRecord(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("OE_VHF_UHF"));
    QVERIFY(db.insertQso(existing));

    const QStringList scope = {QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")};
    DupeChecker checker(db);
    QVERIFY(!checker.isDupe(QStringLiteral("OE1ABC"), QStringLiteral("432"), QStringLiteral("SSB"), QStringLiteral("OE_VHF_UHF"), scope));
}

void TestDupeChecker::differentModeIsNotDupe()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("mode.sqlite")), QStringLiteral("dupe_mode")));

    QsoRecord existing = makeRecord(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("OE_VHF_UHF"));
    QVERIFY(db.insertQso(existing));

    const QStringList scope = {QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")};
    DupeChecker checker(db);
    QVERIFY(!checker.isDupe(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("FM"), QStringLiteral("OE_VHF_UHF"), scope));
}

void TestDupeChecker::differentContestIsNotDupe()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("contest.sqlite")), QStringLiteral("dupe_contest")));

    QsoRecord existing = makeRecord(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("OE_VHF_UHF"));
    QVERIFY(db.insertQso(existing));

    const QStringList scope = {QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")};
    DupeChecker checker(db);
    QVERIFY(!checker.isDupe(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("IARU_R1_VHF_UHF"), scope));
}

void TestDupeChecker::caseAndWhitespaceAreNormalized()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("norm.sqlite")), QStringLiteral("dupe_norm")));

    QsoRecord existing = makeRecord(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("OE_VHF_UHF"));
    QVERIFY(db.insertQso(existing));

    const QStringList scope = {QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")};
    DupeChecker checker(db);
    // Same as AdifLog::isSameQso's normalization: trimmed + case-folded
    // callsign/band/mode must still match.
    QVERIFY(checker.isDupe(QStringLiteral("  oe1abc "), QStringLiteral(" 144 "), QStringLiteral("ssb"), QStringLiteral("OE_VHF_UHF"), scope));
}

void TestDupeChecker::narrowerScopeOverrideTreatsCrossBandAsDupe()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("override.sqlite")), QStringLiteral("dupe_override")));

    QsoRecord existing = makeRecord(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("SOME_CONTEST"));
    QVERIFY(db.insertQso(existing));

    // A hypothetical contest whose ContestDefinition.dupe_scope omits
    // "band" and "mode" (single-band-worked-once rules) should treat a
    // same-callsign QSO on a different band/mode as a dupe too, purely
    // from that contest's own dupe_scope -- not a hardcoded rule.
    const QStringList narrowScope = {QStringLiteral("callsign")};
    DupeChecker checker(db);
    QVERIFY(checker.isDupe(QStringLiteral("OE1ABC"), QStringLiteral("432"), QStringLiteral("FM"), QStringLiteral("SOME_CONTEST"), narrowScope));
}

// DXLog.net deliberately has no delete function for a logged QSO
// (dxlog.net/docs/index.php/Menu_Edit, verified for this task) -- a QSO
// is marked invalid instead, and a QSO marked invalid must no longer
// count as a previous contact, regardless of dupe_scope.
void TestDupeChecker::invalidQsoIsNotDupe()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("invalid.sqlite")), QStringLiteral("dupe_invalid")));

    QsoRecord existing = makeRecord(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("OE_VHF_UHF"));
    QVERIFY(db.insertQso(existing));
    QVERIFY(db.setQsoInvalid(existing.id, true));

    const QStringList scope = {QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")};
    DupeChecker checker(db);
    QVERIFY(!checker.isDupe(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("OE_VHF_UHF"), scope));

    // Un-marking it makes it count again.
    QVERIFY(db.setQsoInvalid(existing.id, false));
    QVERIFY(checker.isDupe(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("OE_VHF_UHF"), scope));
}

// A real SQL failure must never be confused with a genuine "not a
// dupe" -- see DupeChecker::lastError()'s own doc comment. Found live,
// 2026-09-12: isDupe() used to swallow query.exec() failures as a
// silent `false`, which handleLogRequested() then logged as a fresh
// QSO with zero indication the dupe check itself never ran.
void TestDupeChecker::lastErrorEmptyAfterGenuineResult()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("err_ok.sqlite")), QStringLiteral("dupe_err_ok")));

    const QStringList scope = {QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")};
    DupeChecker checker(db);
    QVERIFY(!checker.isDupe(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("SSB"),
                             QStringLiteral("OE_VHF_UHF"), scope));
    // A genuine miss is a genuine result, not a failure.
    QVERIFY(checker.lastError().isEmpty());
}

void TestDupeChecker::lastErrorSetOnQueryFailure()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("err_fail.sqlite")), QStringLiteral("dupe_err_fail")));

    const QStringList scope = {QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")};
    DupeChecker checker(db);

    // Force a real query.exec() failure by closing the underlying
    // connection out from under it -- SELECT against a closed
    // QSqlDatabase fails cleanly rather than crashing.
    db.db().close();
    QVERIFY(!checker.isDupe(QStringLiteral("OE1ABC"), QStringLiteral("144"), QStringLiteral("SSB"),
                             QStringLiteral("OE_VHF_UHF"), scope));
    QVERIFY(!checker.lastError().isEmpty());
}

// The dupe reply names the QSO the station was FIRST logged under
// (number + time), not just "yes" -- see DupeChecker::firstMatchId().
void TestDupeChecker::firstMatchIdIsTheEarliestValidQso()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("firstmatch.sqlite")), QStringLiteral("test_firstmatch")));
    DupeChecker checker(db);
    const QStringList scope{QStringLiteral("callsign"), QStringLiteral("band")};

    QVERIFY(!checker.firstMatchId(QStringLiteral("OE5AOO"), QStringLiteral("144"), QStringLiteral("SSB"),
                                  QStringLiteral("OE_VHF_UHF"), scope).has_value());

    QsoRecord first = makeRecord(QStringLiteral("OE5AOO"), QStringLiteral("144"), QStringLiteral("SSB"),
                                 QStringLiteral("OE_VHF_UHF"));
    first.serialSent = 3;
    QVERIFY(db.insertQso(first));
    QsoRecord second = first;
    second.timestampUtc = QStringLiteral("2026-06-13T13:10:00Z");
    second.serialSent = 9;
    second.isDupe = true;
    QVERIFY(db.insertQso(second));

    const auto id = checker.firstMatchId(QStringLiteral(" oe5aoo "), QStringLiteral("144"), QStringLiteral("CW"),
                                         QStringLiteral("OE_VHF_UHF"), scope);
    QVERIFY(id.has_value());
    QCOMPARE(*id, first.id);

    // The first one invalidated: the (still valid) second is what remains.
    QVERIFY(db.setQsoInvalid(first.id, true));
    const auto after = checker.firstMatchId(QStringLiteral("OE5AOO"), QStringLiteral("144"), QStringLiteral("SSB"),
                                            QStringLiteral("OE_VHF_UHF"), scope);
    QVERIFY(after.has_value());
    QCOMPARE(*after, second.id);
}

// Not QTEST_APPLESS_MAIN: QSqlDatabase requires a live QCoreApplication
// instance (thread-affinity bookkeeping in the SQLite driver), which
// APPLESS_MAIN deliberately does not create.
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestDupeChecker tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_dupechecker.moc"
