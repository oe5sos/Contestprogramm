#include <QtTest>

#include <QTemporaryDir>

#include "data/ContestDatabase.h"
#include "data/DupeRescore.h"
#include "data/QsoRecord.h"

using namespace Contestprogramm;

namespace {

QsoRecord qso(int id, const QString& call, const QString& band, const QString& time, bool dupe = false,
              const QString& mode = QStringLiteral("SSB"))
{
    QsoRecord r;
    r.id = id;
    r.callsign = call;
    r.band = band;
    r.mode = mode;
    r.timestampUtc = time;
    r.isDupe = dupe;
    r.contestId = QStringLiteral("IARU_R1_UHF");
    return r;
}

const QStringList kCallBand = {QStringLiteral("callsign"), QStringLiteral("band")};

} // namespace

// data/DupeRescore.h: the dupe flags a corrected log should carry,
// reported as the difference to the flags it does carry.
class TestDupeRescore : public QObject
{
    Q_OBJECT

private slots:
    void consistentLogNeedsNoChange();
    void correctedCallsignBecomesADupeOrStopsBeingOne();
    void invalidQsoFreesTheCallsignForTheNextOne();
    void timeOrderDecidesWhichIsFirst();
    void scopeWithModeAndNormalisation();
    void databaseStoresTheFlag();
};

void TestDupeRescore::consistentLogNeedsNoChange()
{
    QVector<QsoRecord> log;
    log << qso(1, QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("2026-10-03T14:01:00Z"))
        << qso(2, QStringLiteral("DL1ABC"), QStringLiteral("432"), QStringLiteral("2026-10-03T14:02:00Z"))
        << qso(3, QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("2026-10-03T15:00:00Z"), true);
    QVERIFY(recomputeDupeFlags(log, kCallBand).isEmpty());
    QVERIFY(recomputeDupeFlags({}, kCallBand).isEmpty());
}

void TestDupeRescore::correctedCallsignBecomesADupeOrStopsBeingOne()
{
    // "DL1ABX" (a typo) corrected to DL1ABC: now the second DL1ABC on 144.
    QVector<QsoRecord> log;
    log << qso(1, QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("2026-10-03T14:01:00Z"))
        << qso(2, QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("2026-10-03T14:30:00Z"));
    QVector<DupeFlagChange> changes = recomputeDupeFlags(log, kCallBand);
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.first().qsoId, 2);
    QVERIFY(changes.first().isDupe);

    // A QSO marked dupe of DL1ABC that was really DL1ABD: no dupe any more.
    log[1].callsign = QStringLiteral("DL1ABD");
    log[1].isDupe = true;
    changes = recomputeDupeFlags(log, kCallBand);
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.first().qsoId, 2);
    QVERIFY(!changes.first().isDupe);
}

void TestDupeRescore::invalidQsoFreesTheCallsignForTheNextOne()
{
    QVector<QsoRecord> log;
    log << qso(1, QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("2026-10-03T14:01:00Z"))
        << qso(2, QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("2026-10-03T14:30:00Z"), true);
    log[0].isInvalid = true;
    QVector<DupeFlagChange> changes = recomputeDupeFlags(log, kCallBand);
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.first().qsoId, 2);
    QVERIFY(!changes.first().isDupe);
    // The invalid QSO's own flag is never touched, whatever it says.
    log[0].isDupe = true;
    changes = recomputeDupeFlags(log, kCallBand);
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.first().qsoId, 2);
}

void TestDupeRescore::timeOrderDecidesWhichIsFirst()
{
    // The one logged first by id but stamped later (a time edit) is the dupe.
    QVector<QsoRecord> log;
    log << qso(1, QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("2026-10-03T16:00:00Z"))
        << qso(2, QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("2026-10-03T15:00:00Z"), true);
    const QVector<DupeFlagChange> changes = recomputeDupeFlags(log, kCallBand);
    QCOMPARE(changes.size(), 2);
    QCOMPARE(changes.at(0).qsoId, 2);
    QVERIFY(!changes.at(0).isDupe);
    QCOMPARE(changes.at(1).qsoId, 1);
    QVERIFY(changes.at(1).isDupe);
}

void TestDupeRescore::scopeWithModeAndNormalisation()
{
    QVector<QsoRecord> log;
    log << qso(1, QStringLiteral("dl1abc "), QStringLiteral("144"), QStringLiteral("2026-10-03T14:01:00Z"), false, QStringLiteral("SSB"))
        << qso(2, QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("2026-10-03T14:05:00Z"), false, QStringLiteral("cw"));
    // Per band: the CW contact is a dupe of the SSB one.
    QVector<DupeFlagChange> changes = recomputeDupeFlags(log, kCallBand);
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.first().qsoId, 2);
    QVERIFY(changes.first().isDupe);
    // With mode in the scope both count.
    changes = recomputeDupeFlags(log, {QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")});
    QVERIFY(changes.isEmpty());
}

void TestDupeRescore::databaseStoresTheFlag()
{
    QTemporaryDir dir;
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("rescore.sqlite")), QStringLiteral("dupe_rescore")));
    QsoRecord record = qso(-1, QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("2026-10-03T14:01:00Z"));
    QVERIFY(db.insertQso(record));
    QVERIFY(record.id > 0);
    const int writesBefore = db.qsoWriteCounter();
    QVERIFY(db.setQsoDupe(record.id, true));
    QVERIFY(db.qsoById(record.id)->isDupe);
    QVERIFY(db.setQsoDupe(record.id, false));
    QVERIFY(!db.qsoById(record.id)->isDupe);
    // Counted as a log write, so the next backup picks it up.
    QCOMPARE(db.qsoWriteCounter(), writesBefore + 2);
}

QTEST_GUILESS_MAIN(TestDupeRescore)
#include "test_dupe_rescore.moc"
