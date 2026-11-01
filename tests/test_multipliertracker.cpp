#include <QtTest>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "core/CallsignPrefix.h"
#include "core/CountryPrefixIndex.h"
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
    void wpxPrefixFollowsTheContestRules();
    void prefixBasisCountsPrefixesPerBand();
    void dxccBasisNeedsTheCountryListAndCountsCountries();
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
    QVERIFY(!tracker.isNeededMultiplier(QStringLiteral("144"), QStringLiteral("JN77QT"), QStringLiteral("OE1AAA")));
    // Same grid, but never worked on 432 -- still needed there. This is
    // the genuine multi-band question the plan's worked/needed-per-band
    // table is meant to answer.
    QVERIFY(tracker.isNeededMultiplier(QStringLiteral("432"), QStringLiteral("JN77QT"), QStringLiteral("OE1AAA")));
    // A grid never seen at all is needed everywhere.
    QVERIFY(tracker.isNeededMultiplier(QStringLiteral("144"), QStringLiteral("KN05IX"), QStringLiteral("UR5XXX")));
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

// core/CallsignPrefix.h: der Präfix nach den WPX-Regeln, der einzige
// Multiplikator, der ohne fremde Tabelle auskommt.
void TestMultiplierTracker::wpxPrefixFollowsTheContestRules()
{
    // Bis einschließlich der letzten Ziffer.
    QCOMPARE(wpxPrefix(QStringLiteral("OE5SOS")), QStringLiteral("OE5"));
    QCOMPARE(wpxPrefix(QStringLiteral("DL1ABC")), QStringLiteral("DL1"));
    QCOMPARE(wpxPrefix(QStringLiteral("W1AW")), QStringLiteral("W1"));
    QCOMPARE(wpxPrefix(QStringLiteral("9A1A")), QStringLiteral("9A1"));
    QCOMPARE(wpxPrefix(QStringLiteral("4X4XXX")), QStringLiteral("4X4"));
    QCOMPARE(wpxPrefix(QStringLiteral("VP2EXX")), QStringLiteral("VP2"));
    // Klein geschrieben und mit Leerzeichen kommt es auch an.
    QCOMPARE(wpxPrefix(QStringLiteral("  oe5sos ")), QStringLiteral("OE5"));

    // Ohne Ziffer: eine 0 hinter die ersten beiden Zeichen.
    QCOMPARE(wpxPrefix(QStringLiteral("RAEM")), QStringLiteral("RA0"));

    // Betriebszusätze zählen nicht.
    QCOMPARE(wpxPrefix(QStringLiteral("OE5SOS/P")), QStringLiteral("OE5"));
    QCOMPARE(wpxPrefix(QStringLiteral("OE5SOS/MM")), QStringLiteral("OE5"));
    QCOMPARE(wpxPrefix(QStringLiteral("DL1ABC/QRP")), QStringLiteral("DL1"));

    // Ein Zusatz zählt, vor wie hinter dem Schrägstrich.
    QCOMPARE(wpxPrefix(QStringLiteral("KH6/N8BJQ")), QStringLiteral("KH6"));
    QCOMPARE(wpxPrefix(QStringLiteral("N8BJQ/KH6")), QStringLiteral("KH6"));
    // Zusatz ohne Ziffer: wieder die 0.
    QCOMPARE(wpxPrefix(QStringLiteral("PA/N8BJQ")), QStringLiteral("PA0"));
    QCOMPARE(wpxPrefix(QStringLiteral("OE5SOS/DL")), QStringLiteral("DL0"));
    // Eine einzelne Ziffer ersetzt die Ziffer.
    QCOMPARE(wpxPrefix(QStringLiteral("N8BJQ/9")), QStringLiteral("N9"));
    QCOMPARE(wpxPrefix(QStringLiteral("OE5SOS/1")), QStringLiteral("OE1"));
    // Zusatz und Betriebszusatz zusammen.
    QCOMPARE(wpxPrefix(QStringLiteral("DL/OE5SOS/P")), QStringLiteral("DL0"));

    QCOMPARE(wpxPrefix(QString()), QString());
    QCOMPARE(wpxPrefix(QStringLiteral("  ")), QString());
}

// Mit multiplier_field "prefix" zählt der Präfix je Band -- die
// Kurzwellen-Grundlage, für die kein Locator nötig ist.
void TestMultiplierTracker::prefixBasisCountsPrefixesPerBand()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("mult_prefix.sqlite")), QStringLiteral("mult_prefix")));

    QString error;
    const ContestDefinition def = ContestDefinition::loadFromJson(QByteArrayLiteral(R"({
        "id": "PREFIX_TEST",
        "name": "Präfixprobe",
        "bands": ["14", "21"],
        "dupe_scope": ["callsign", "band"],
        "scoring": "qso_count",
        "multiplier_field": "prefix",
        "exchange_fields": [ { "key": "rst", "label": "RST", "type": "rst" } ]
    })"), &error);
    QVERIFY2(def.isValid(), qPrintable(error));

    // Zwei Rufzeichen mit demselben Präfix auf 14: ein Multiplikator.
    // Ohne jeden Locator -- auf Kurzwelle wird keiner getauscht.
    QVERIFY(insertMadeRecord(db, QStringLiteral("DL1ABC"), QStringLiteral("14"), QString(), def.id()));
    QVERIFY(insertMadeRecord(db, QStringLiteral("DL1XYZ"), QStringLiteral("14"), QString(), def.id()));
    QVERIFY(insertMadeRecord(db, QStringLiteral("DL2QQQ"), QStringLiteral("14"), QString(), def.id()));
    QVERIFY(insertMadeRecord(db, QStringLiteral("DL1ABC"), QStringLiteral("21"), QString(), def.id()));

    MultiplierTracker tracker(db);
    tracker.recompute(def.id(), def);

    QCOMPARE(tracker.workedMultipliers(QStringLiteral("14")),
             QSet<QString>({QStringLiteral("DL1"), QStringLiteral("DL2")}));
    QCOMPARE(tracker.workedMultipliers(QStringLiteral("21")), QSet<QString>{QStringLiteral("DL1")});
    QCOMPARE(tracker.totalMultiplierCount(), 2);

    // Gearbeitet auf 14, auf 21 noch offen -- dieselbe Frage wie beim
    // Locator, nur auf der anderen Grundlage. Der Locator ist hier
    // leer und darf keine Rolle spielen.
    QVERIFY(!tracker.isNeededMultiplier(QStringLiteral("14"), QString(), QStringLiteral("DL1ZZZ")));
    QVERIFY(tracker.isNeededMultiplier(QStringLiteral("21"), QString(), QStringLiteral("DL2QQQ")));
    QVERIFY(tracker.isNeededMultiplier(QStringLiteral("14"), QString(), QStringLiteral("G3ABC")));
}

// Mit multiplier_field "dxcc" zählt das Land -- und dafür braucht es
// eine geladene Länderliste. Ohne sie bleibt die Liste leer, statt ein
// Land zu erraten.
void TestMultiplierTracker::dxccBasisNeedsTheCountryListAndCountsCountries()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("mult_dxcc2.sqlite")), QStringLiteral("mult_dxcc2")));

    QString error;
    const ContestDefinition def = ContestDefinition::loadFromJson(QByteArrayLiteral(R"({
        "id": "DXCC_TEST",
        "name": "Länderprobe",
        "bands": ["14", "21"],
        "dupe_scope": ["callsign", "band"],
        "scoring": "qso_count",
        "multiplier_field": "dxcc",
        "exchange_fields": [ { "key": "rst", "label": "RST", "type": "rst" } ]
    })"), &error);
    QVERIFY2(def.isValid(), qPrintable(error));

    QVERIFY(insertMadeRecord(db, QStringLiteral("DL1ABC"), QStringLiteral("14"), QString(), def.id()));
    QVERIFY(insertMadeRecord(db, QStringLiteral("DK5XYZ"), QStringLiteral("14"), QString(), def.id()));
    QVERIFY(insertMadeRecord(db, QStringLiteral("JA1QQQ"), QStringLiteral("14"), QString(), def.id()));
    QVERIFY(insertMadeRecord(db, QStringLiteral("DL1ABC"), QStringLiteral("21"), QString(), def.id()));

    MultiplierTracker tracker(db);
    // Ohne Liste: nichts, keine erfundenen Länder.
    tracker.recompute(def.id(), def);
    QCOMPARE(tracker.totalMultiplierCount(), 0);

    CountryPrefixIndex index;
    QVERIFY(index.loadFromCty(QByteArrayLiteral(
        "Fed. Rep. of Germany: 14: 28: EU: 51.00: -10.00: -1.0: DL:\n"
        "    DA,DB,DC,DD,DK,DL,DM;\n"
        "Japan: 25: 45: AS: 36.40: -138.38: -9.0: JA:\n"
        "    JA,JE,JF,JG,JH;\n")));
    tracker.setCountryIndex(&index);
    tracker.recompute(def.id(), def);

    // DL1ABC und DK5XYZ sind dasselbe Land -- ein Multiplikator.
    QCOMPARE(tracker.workedMultipliers(QStringLiteral("14")),
             QSet<QString>({QStringLiteral("DL"), QStringLiteral("JA")}));
    QCOMPARE(tracker.workedMultipliers(QStringLiteral("21")), QSet<QString>{QStringLiteral("DL")});
    QCOMPARE(tracker.totalMultiplierCount(), 2);
    QVERIFY(!tracker.isNeededMultiplier(QStringLiteral("14"), QString(), QStringLiteral("DM9ZZZ")));
    QVERIFY(tracker.isNeededMultiplier(QStringLiteral("21"), QString(), QStringLiteral("JA9ZZZ")));
    // Ein Rufzeichen, das die Liste nicht kennt, zählt nicht mit.
    QVERIFY(!tracker.isNeededMultiplier(QStringLiteral("14"), QString(), QStringLiteral("ZZ9ZZZ")));
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
