#include <QtTest>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/MultiplierTracker.h"
#include "data/QsoRecord.h"

using namespace Contestprogramm;

namespace {

const char* kGridDefJson = R"JSON(
{
  "id": "OE_VHF_UHF",
  "name": "ÖVSV OE VHF/UHF Contest",
  "bands": ["144", "432"],
  "dupe_scope": ["callsign", "band", "mode"],
  "exchange_fields": [
    { "key": "serial", "label": "Serial", "type": "int", "auto_increment": true },
    { "key": "grid",   "label": "Grid",   "type": "grid6" }
  ]
}
)JSON";

const char* kDxccDefJson = R"JSON(
{
  "id": "SOME_DXCC_CONTEST",
  "name": "Hypothetical DXCC contest",
  "bands": ["144"],
  "dupe_scope": ["callsign"],
  "multiplier_field": "dxcc",
  "exchange_fields": [
    { "key": "serial", "label": "Serial", "type": "int", "auto_increment": true }
  ]
}
)JSON";

QsoRecord makeRecord(const QString& callsign, const QString& band, const QString& grid, const QString& contestId)
{
    QsoRecord record;
    record.callsign = callsign;
    record.band = band;
    record.mode = QStringLiteral("SSB");
    record.timestampUtc = QStringLiteral("2026-06-13T12:00:00Z");
    record.gridSquare = grid;
    record.contestId = contestId;
    return record;
}

// ContestDatabase::insertQso() takes QsoRecord& (it writes the new row
// id back into it), so it cannot bind directly to makeRecord()'s
// temporary return value -- this gives every call site an lvalue.
bool insertMadeRecord(ContestDatabase& db, const QString& callsign, const QString& band, const QString& grid,
                       const QString& contestId)
{
    QsoRecord record = makeRecord(callsign, band, grid, contestId);
    return db.insertQso(record);
}

} // namespace

class TestMultiplierTracker : public QObject
{
    Q_OBJECT

private slots:
    void multiplierKeyTruncatesToFourCharsAndUppercases();
    void recomputeGroupsByBandAndDeduplicatesSubSquares();
    void totalMultiplierCountIsUnionAcrossBands();
    void isNeededMultiplierReflectsPerBandWorkedStatus();
    void unimplementedMultiplierBasisStaysEmpty();
};

void TestMultiplierTracker::multiplierKeyTruncatesToFourCharsAndUppercases()
{
    QCOMPARE(MultiplierTracker::multiplierKeyForGrid(QStringLiteral("jn77qt")), QStringLiteral("JN77"));
    QCOMPARE(MultiplierTracker::multiplierKeyForGrid(QStringLiteral("  JN88TC ")), QStringLiteral("JN88"));
    // Defensive: shorter than 4 chars is returned as-is (upper-cased).
    QCOMPARE(MultiplierTracker::multiplierKeyForGrid(QStringLiteral("jn7")), QStringLiteral("JN7"));
}

void TestMultiplierTracker::recomputeGroupsByBandAndDeduplicatesSubSquares()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("mult_recompute.sqlite")), QStringLiteral("mult_recompute")));

    QString error;
    const ContestDefinition def = ContestDefinition::loadFromJson(QByteArray(kGridDefJson), &error);
    QVERIFY2(def.isValid(), qPrintable(error));

    // Two QSOs in the same 4-char square on 144 -- one multiplier, not two.
    QVERIFY(insertMadeRecord(db, QStringLiteral("OE1AAA"), QStringLiteral("144"), QStringLiteral("JN77QT"), def.id()));
    QVERIFY(insertMadeRecord(db, QStringLiteral("OE1BBB"), QStringLiteral("144"), QStringLiteral("JN77XY"), def.id()));
    // A different square on 432.
    QVERIFY(insertMadeRecord(db, QStringLiteral("OE2CCC"), QStringLiteral("432"), QStringLiteral("JN88TC"), def.id()));

    MultiplierTracker tracker(db);
    tracker.recompute(def.id(), def);

    QCOMPARE(tracker.bands(), def.bands());
    QCOMPARE(tracker.workedMultipliers(QStringLiteral("144")).size(), 1);
    QVERIFY(tracker.workedMultipliers(QStringLiteral("144")).contains(QStringLiteral("JN77")));
    QCOMPARE(tracker.workedMultipliers(QStringLiteral("432")).size(), 1);
    QVERIFY(tracker.workedMultipliers(QStringLiteral("432")).contains(QStringLiteral("JN88")));
}

void TestMultiplierTracker::totalMultiplierCountIsUnionAcrossBands()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("mult_total.sqlite")), QStringLiteral("mult_total")));

    QString error;
    const ContestDefinition def = ContestDefinition::loadFromJson(QByteArray(kGridDefJson), &error);
    QVERIFY2(def.isValid(), qPrintable(error));

    // JN77 worked on both bands -- counts once in the union, not twice.
    QVERIFY(insertMadeRecord(db, QStringLiteral("OE1AAA"), QStringLiteral("144"), QStringLiteral("JN77QT"), def.id()));
    QVERIFY(insertMadeRecord(db, QStringLiteral("OE1AAA"), QStringLiteral("432"), QStringLiteral("JN77QT"), def.id()));
    QVERIFY(insertMadeRecord(db, QStringLiteral("OE2CCC"), QStringLiteral("432"), QStringLiteral("JN88TC"), def.id()));

    MultiplierTracker tracker(db);
    tracker.recompute(def.id(), def);

    QCOMPARE(tracker.totalMultiplierCount(), 2); // JN77, JN88
}

void TestMultiplierTracker::isNeededMultiplierReflectsPerBandWorkedStatus()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("mult_needed.sqlite")), QStringLiteral("mult_needed")));

    QString error;
    const ContestDefinition def = ContestDefinition::loadFromJson(QByteArray(kGridDefJson), &error);
    QVERIFY2(def.isValid(), qPrintable(error));

    QVERIFY(insertMadeRecord(db, QStringLiteral("OE1AAA"), QStringLiteral("144"), QStringLiteral("JN77QT"), def.id()));

    MultiplierTracker tracker(db);
    tracker.recompute(def.id(), def);

    // Worked on 144 -- not needed there.
    QVERIFY(!tracker.isNeededMultiplier(QStringLiteral("144"), QStringLiteral("JN77QT")));
    // Same grid, but never worked on 432 -- still needed there. This is
    // the genuine multi-band question the plan's worked/needed-per-band
    // table is meant to answer.
    QVERIFY(tracker.isNeededMultiplier(QStringLiteral("432"), QStringLiteral("JN77QT")));
    // A grid never seen at all is needed everywhere.
    QVERIFY(tracker.isNeededMultiplier(QStringLiteral("144"), QStringLiteral("KN05IX")));
}

void TestMultiplierTracker::unimplementedMultiplierBasisStaysEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("mult_dxcc.sqlite")), QStringLiteral("mult_dxcc")));

    QString error;
    const ContestDefinition def = ContestDefinition::loadFromJson(QByteArray(kDxccDefJson), &error);
    QVERIFY2(def.isValid(), qPrintable(error));
    QCOMPARE(def.multiplierField(), QStringLiteral("dxcc"));

    QVERIFY(insertMadeRecord(db, QStringLiteral("OE1AAA"), QStringLiteral("144"), QStringLiteral("JN77QT"), def.id()));

    MultiplierTracker tracker(db);
    tracker.recompute(def.id(), def);

    // Not implemented (see MultiplierTracker.h) -- stays empty rather
    // than fabricating a DXCC-based multiplier set from grid data.
    QCOMPARE(tracker.totalMultiplierCount(), 0);
    QVERIFY(tracker.workedMultipliers(QStringLiteral("144")).isEmpty());
}

// Not QTEST_APPLESS_MAIN: QSqlDatabase requires a live QCoreApplication
// instance (thread-affinity bookkeeping in the SQLite driver), which
// APPLESS_MAIN deliberately does not create.
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestMultiplierTracker tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_multipliertracker.moc"
