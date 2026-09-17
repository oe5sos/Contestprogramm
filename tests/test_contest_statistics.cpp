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
    QCOMPARE(stats.hours.at(0).points, qint64(187 + 215));
    QCOMPARE(stats.hours.at(1).qsos, 0);
    QCOMPARE(stats.hours.at(1).points, qint64(0));
    QCOMPARE(stats.hours.at(2).qsos, 1);
    QCOMPARE(stats.hours.at(2).points, qint64(1));
    QCOMPARE(stats.bestHourQsos, 2);
    QCOMPARE(stats.bestHourStartUtc.toString(QStringLiteral("HH:mm")), QStringLiteral("14:00"));
    QCOMPARE(stats.score.validQsos, 3);
    QCOMPARE(stats.score.points, qint64(187 + 215 + 1));
    QCOMPARE(qRound(stats.averageKm), qRound((187 + 215 + 1) / 3.0));
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
    QCOMPARE(stats.longest.first().km, 210);
    QVERIFY(stats.longest.first().km >= stats.longest.last().km);
    for (const OdxEntry& e : stats.longest) {
        QVERIFY(e.callsign != QStringLiteral("DL13ABC"));
        QVERIFY(e.callsign != QStringLiteral("DL12ABC"));
    }
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

QTEST_APPLESS_MAIN(TestContestStatistics)
#include "test_contest_statistics.moc"
