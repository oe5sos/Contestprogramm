#include <QtTest>

#include "data/ContestStatistics.h"
#include "data/QsoRecord.h"

using namespace Contestprogramm;

namespace {

QsoRecord makeQso(const QString& call, const QString& band, const QString& time, const QString& grid, double km)
{
    QsoRecord r;
    r.callsign = call;
    r.band = band;
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QStringLiteral("2026-10-03T%1:00Z").arg(time);
    r.gridSquare = grid;
    r.distanceKm = km;
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    return r;
}

} // namespace

class TestContestStatistics : public QObject
{
    Q_OBJECT

private slots:
    void hoursCoverTheWholeSpanAndMarkTheBest();
    void longestListIsSortedCappedAndSkipsDupesAndInvalid();
    void emptyLogGivesEmptyStatistics();
    void widelySpreadLogListsOnlyHoursWithQsos();
    void onAirAndBreaksCountTheGapsInTheLog();
};

void TestContestStatistics::hoursCoverTheWholeSpanAndMarkTheBest()
{
    QVector<QsoRecord> records = {
        makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("14:05"), QStringLiteral("JN58SD"), 187.4),
        makeQso(QStringLiteral("OE3XYZ"), QStringLiteral("144"), QStringLiteral("14:50"), QStringLiteral("JN88TC"), 214.6),
        // 15:xx has nothing, 16:xx one QSO on 432
        makeQso(QStringLiteral("OE5XYZ"), QStringLiteral("432"), QStringLiteral("16:20"), QStringLiteral("JN67UT"), 0.0),
    };
    const ContestStatistics stats = computeContestStatistics(records, QStringLiteral("JN67UT"),
                                                             {QStringLiteral("144"), QStringLiteral("432")});
    QCOMPARE(stats.hours.size(), 3);
    QCOMPARE(stats.hours.at(0).hourStartUtc.toString(QStringLiteral("HH:mm")), QStringLiteral("14:00"));
    QCOMPARE(stats.hours.at(0).qsos, 2);
    QCOMPARE(stats.hours.at(0).points, qint64(188 + 215));
    QCOMPARE(stats.hours.at(1).qsos, 0);
    QCOMPARE(stats.hours.at(1).points, qint64(0));
    QCOMPARE(stats.hours.at(2).qsos, 1);
    QCOMPARE(stats.hours.at(2).points, qint64(1));
    QCOMPARE(stats.bestHourQsos, 2);
    QCOMPARE(stats.bestHourStartUtc.toString(QStringLiteral("HH:mm")), QStringLiteral("14:00"));
    QCOMPARE(stats.score.validQsos, 3);
    QCOMPARE(stats.score.points, qint64(188 + 215 + 1));
    QCOMPARE(qRound(stats.averageKm), qRound((188 + 215 + 1) / 3.0));
}

void TestContestStatistics::longestListIsSortedCappedAndSkipsDupesAndInvalid()
{
    QVector<QsoRecord> records;
    for (int i = 0; i < 14; ++i) {
        records.append(makeQso(QStringLiteral("DL%1ABC").arg(i), QStringLiteral("144"),
                               QStringLiteral("%1:%2").arg(14 + i / 6, 2, 10, QLatin1Char('0')).arg((i % 6) * 10, 2, 10, QLatin1Char('0')),
                               QStringLiteral("JN58SD"), 100.0 + i * 10));
    }
    records[13].isDupe = true;      // the farthest, but a dupe
    records[12].isInvalid = true;   // the next farthest, invalid
    const ContestStatistics stats = computeContestStatistics(records, QStringLiteral("JN67UT"), {QStringLiteral("144")});
    QCOMPARE(stats.longest.size(), 10);
    QCOMPARE(stats.longest.first().callsign, QStringLiteral("DL11ABC"));
    QCOMPARE(stats.longest.first().km, 211);
    QVERIFY(stats.longest.first().km >= stats.longest.last().km);
    for (const OdxEntry& e : stats.longest) {
        QVERIFY(e.callsign != QStringLiteral("DL13ABC"));
        QVERIFY(e.callsign != QStringLiteral("DL12ABC"));
    }
}

void TestContestStatistics::widelySpreadLogListsOnlyHoursWithQsos()
{
    QVector<QsoRecord> records = {
        makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("14:05"), QStringLiteral("JN58SD"), 187.4),
        makeQso(QStringLiteral("OE3XYZ"), QStringLiteral("144"), QStringLiteral("16:50"), QStringLiteral("JN88TC"), 214.6),
    };
    // A test QSO from three weeks earlier in the same contest id.
    QsoRecord old = makeQso(QStringLiteral("OE5TEST"), QStringLiteral("144"), QStringLiteral("10:00"), QStringLiteral("JN67UT"), 0.0);
    old.timestampUtc = QStringLiteral("2026-09-11T10:00:00Z");
    records.append(old);
    const ContestStatistics stats = computeContestStatistics(records, QStringLiteral("JN67UT"), {QStringLiteral("144")});
    QCOMPARE(stats.hours.size(), 3); // no 500 empty rows in between
    QCOMPARE(stats.hours.at(0).hourStartUtc.toString(QStringLiteral("dd HH:mm")), QStringLiteral("11 10:00"));
    QCOMPARE(stats.hours.at(1).hourStartUtc.toString(QStringLiteral("dd HH:mm")), QStringLiteral("03 14:00"));
    QCOMPARE(stats.hours.at(2).hourStartUtc.toString(QStringLiteral("dd HH:mm")), QStringLiteral("03 16:00"));
}

void TestContestStatistics::emptyLogGivesEmptyStatistics()
{
    const ContestStatistics stats = computeContestStatistics({}, QStringLiteral("JN67UT"), {QStringLiteral("144")});
    QVERIFY(stats.hours.isEmpty());
    QVERIFY(stats.longest.isEmpty());
    QCOMPARE(stats.bestHourQsos, 0);
    QCOMPARE(stats.averageKm, 0.0);
    QCOMPARE(stats.score.bands.size(), 1); // the definition's band still listed, at 0
}

// Am Gerät und in Pause -- N1MMs "on time"/"off time". Eine Lücke ab
// einer halben Stunde ist eine Pause, alles darunter ist Betrieb (auch
// wenn zehn Minuten nichts kam). Vor dem ersten und nach dem letzten
// QSO wird nichts gezählt: davon weiß das Log nichts.
void TestContestStatistics::onAirAndBreaksCountTheGapsInTheLog()
{
    // 14:00, 14:20, 14:35 -- durchgehend. Dann zwei Stunden nichts.
    // Danach 16:35, 16:50.
    const QVector<QsoRecord> records{
        makeQso(QStringLiteral("A"), QStringLiteral("144"), QStringLiteral("14:00"), QStringLiteral("JN88TC"), 200.0),
        makeQso(QStringLiteral("B"), QStringLiteral("144"), QStringLiteral("14:20"), QStringLiteral("JN88TC"), 200.0),
        makeQso(QStringLiteral("C"), QStringLiteral("144"), QStringLiteral("14:35"), QStringLiteral("JN88TC"), 200.0),
        makeQso(QStringLiteral("D"), QStringLiteral("144"), QStringLiteral("16:35"), QStringLiteral("JN88TC"), 200.0),
        makeQso(QStringLiteral("E"), QStringLiteral("144"), QStringLiteral("16:50"), QStringLiteral("JN88TC"), 200.0),
    };
    const ContestStatistics stats = computeContestStatistics(records, QStringLiteral("JN67UT"),
                                                             {QStringLiteral("144")});
    // 20 + 15 + 15 Minuten Betrieb.
    QCOMPARE(stats.onAirSecs, qint64(50 * 60));
    QCOMPARE(stats.breaks, 1);
    QCOMPARE(stats.offAirSecs, qint64(120 * 60));
    QCOMPARE(stats.longestBreakSecs, qint64(120 * 60));
    QCOMPARE(stats.longestBreakStartUtc.toString(QStringLiteral("HH:mm")), QStringLiteral("14:35"));

    // Ein Log ohne Lücke hat keine Pause.
    const QVector<QsoRecord> tight{
        makeQso(QStringLiteral("A"), QStringLiteral("144"), QStringLiteral("14:00"), QStringLiteral("JN88TC"), 200.0),
        makeQso(QStringLiteral("B"), QStringLiteral("144"), QStringLiteral("14:10"), QStringLiteral("JN88TC"), 200.0),
    };
    const ContestStatistics none = computeContestStatistics(tight, QStringLiteral("JN67UT"),
                                                            {QStringLiteral("144")});
    QCOMPARE(none.breaks, 0);
    QCOMPARE(none.offAirSecs, qint64(0));
    QCOMPARE(none.onAirSecs, qint64(10 * 60));

    // Ein einziges QSO ergibt keine Zeitspanne.
    const ContestStatistics single = computeContestStatistics({tight.first()}, QStringLiteral("JN67UT"),
                                                              {QStringLiteral("144")});
    QCOMPARE(single.onAirSecs, qint64(0));
    QCOMPARE(single.breaks, 0);
}

QTEST_APPLESS_MAIN(TestContestStatistics)
#include "test_contest_statistics.moc"
