// Tests for computeRateBreakdown() -- a pure function over
// QVector<QsoRecord>, deliberately factored out of RateMeterWidget so
// the rolling-window counts, trend direction, and band/mode tallies
// can be exercised without a live QSqlDatabase fixture. See
// src/ui/RateMeterWidget.h's own doc comment.

#include <QtTest>
#include <QTimeZone>

#include "data/QsoRecord.h"
#include "ui/RateMeterWidget.h"

using namespace Contestprogramm;

namespace {
QsoRecord makeRecord(const QString& band, const QString& mode, const QDateTime& timestampUtc)
{
    QsoRecord record;
    record.callsign = QStringLiteral("OE1TEST");
    record.band = band;
    record.mode = mode;
    record.timestampUtc = timestampUtc.toString(Qt::ISODate);
    record.contestId = QStringLiteral("TEST");
    return record;
}
} // namespace

class TestRateBreakdown : public QObject
{
    Q_OBJECT

private slots:
    void emptyLogIsAllZeroNotDash();
    void windowCountsSplitCorrectlyByAge();
    void trendComparesCurrentAgainstPrecedingTenMinuteWindow();
    void bandAndModeTalliesSortedByCountDescendingThenNameAscending();
};

// A fresh contest with zero logged QSOs is a real, measured zero --
// not the "no source wired up yet" dash case RateMeterWidget::refresh()
// handles separately when m_database is null. computeRateBreakdown()
// itself only ever sees real records (possibly none), so it always
// returns real zeros here, never a sentinel.
void TestRateBreakdown::emptyLogIsAllZeroNotDash()
{
    const RateBreakdown breakdown = computeRateBreakdown({}, QDateTime::currentDateTimeUtc());
    QCOMPARE(breakdown.last10Min, 0);
    QCOMPARE(breakdown.lastHour, 0);
    QCOMPARE(breakdown.total, 0);
    QCOMPARE(breakdown.trend, RateBreakdown::Trend::Flat);
    QVERIFY(breakdown.byBand.isEmpty());
    QVERIFY(breakdown.byMode.isEmpty());
}

void TestRateBreakdown::windowCountsSplitCorrectlyByAge()
{
    const QDateTime now = QDateTime(QDate(2026, 9, 12), QTime(20, 0, 0), QTimeZone::utc());
    const QVector<QsoRecord> records = {
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-60)),   // last 10 min
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-540)),  // last 10 min (just inside)
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-900)),  // last hour, not last 10 min
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-3000)), // last hour, not last 10 min
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-7200)), // total only
    };
    const RateBreakdown breakdown = computeRateBreakdown(records, now);
    QCOMPARE(breakdown.last10Min, 2);
    QCOMPARE(breakdown.lastHour, 4); // the two 10-min ones are also within the last hour
    QCOMPARE(breakdown.total, 5);
}

void TestRateBreakdown::trendComparesCurrentAgainstPrecedingTenMinuteWindow()
{
    const QDateTime now = QDateTime(QDate(2026, 9, 12), QTime(20, 0, 0), QTimeZone::utc());

    // Speeding up: 3 QSOs in the last 10 min, only 1 in the 10 min before that.
    const QVector<QsoRecord> speedingUp = {
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-60)),
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-120)),
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-300)),
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-660)),
    };
    QCOMPARE(computeRateBreakdown(speedingUp, now).trend, RateBreakdown::Trend::Up);

    // Slowing down: 1 QSO in the last 10 min, 3 in the 10 min before that.
    const QVector<QsoRecord> slowingDown = {
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-60)),
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-660)),
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-720)),
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-1100)),
    };
    QCOMPARE(computeRateBreakdown(slowingDown, now).trend, RateBreakdown::Trend::Down);

    // Steady: equal counts in both 10-minute windows.
    const QVector<QsoRecord> steady = {
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-60)),
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-660)),
    };
    QCOMPARE(computeRateBreakdown(steady, now).trend, RateBreakdown::Trend::Flat);

    // A window older than 20 minutes contributes to neither 10-minute
    // bucket, so it must not skew the trend either way.
    const QVector<QsoRecord> onlyOld = {
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-3000)),
    };
    QCOMPARE(computeRateBreakdown(onlyOld, now).trend, RateBreakdown::Trend::Flat);
}

void TestRateBreakdown::bandAndModeTalliesSortedByCountDescendingThenNameAscending()
{
    const QDateTime now = QDateTime(QDate(2026, 9, 12), QTime(20, 0, 0), QTimeZone::utc());
    const QVector<QsoRecord> records = {
        makeRecord(QStringLiteral("432"), QStringLiteral("SSB"), now.addSecs(-60)),
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-120)),
        makeRecord(QStringLiteral("144"), QStringLiteral("SSB"), now.addSecs(-180)),
        makeRecord(QStringLiteral("144"), QStringLiteral("CW"), now.addSecs(-240)),
        makeRecord(QStringLiteral("1296"), QStringLiteral("SSB"), now.addSecs(-300)),
    };
    const RateBreakdown breakdown = computeRateBreakdown(records, now);

    // 144 (3) first, then 432/1296 tied at 1 -- name ascending breaks
    // the tie ("1296" < "432" lexicographically).
    QCOMPARE(breakdown.byBand.size(), 3);
    QCOMPARE(breakdown.byBand.at(0), qMakePair(QStringLiteral("144"), 3));
    QCOMPARE(breakdown.byBand.at(1), qMakePair(QStringLiteral("1296"), 1));
    QCOMPARE(breakdown.byBand.at(2), qMakePair(QStringLiteral("432"), 1));

    // SSB (4) first, then CW (1).
    QCOMPARE(breakdown.byMode.size(), 2);
    QCOMPARE(breakdown.byMode.at(0), qMakePair(QStringLiteral("SSB"), 4));
    QCOMPARE(breakdown.byMode.at(1), qMakePair(QStringLiteral("CW"), 1));
}

QTEST_MAIN(TestRateBreakdown)
#include "test_rate_breakdown.moc"
