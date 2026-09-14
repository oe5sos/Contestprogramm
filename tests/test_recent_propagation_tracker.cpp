// Tests for RecentPropagationTracker -- the plan's "Rate-Potenzial aus
// einem frischen eigenen QSO" refinement (see the class's own doc
// comment). A pure, self-contained class (no DB/network), directly
// unit-testable.

#include <QtTest>

#include "core/RecentPropagationTracker.h"

using namespace Contestprogramm;

class TestRecentPropagationTracker : public QObject
{
    Q_OBJECT

private slots:
    void noEntriesMeansNoOpeningAnywhere();
    void exactBearingMatchOnSameBandIsAnOpening();
    void bearingJustInsideToleranceCounts();
    void bearingJustOutsideToleranceDoesNotCount();
    void wrapsAroundTheCompassCorrectly();
    void differentBandNeverCounts();
    void entryExpiresAfterTheWindow();
    void angularDifferenceIsCircularAndSymmetric();
};

void TestRecentPropagationTracker::noEntriesMeansNoOpeningAnywhere()
{
    RecentPropagationTracker tracker;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVERIFY(!tracker.hasRecentOpeningNear(QStringLiteral("144"), 90.0, now));
}

void TestRecentPropagationTracker::exactBearingMatchOnSameBandIsAnOpening()
{
    RecentPropagationTracker tracker;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    tracker.recordQso(QStringLiteral("144"), 90.0, now);
    QVERIFY(tracker.hasRecentOpeningNear(QStringLiteral("144"), 90.0, now));
}

void TestRecentPropagationTracker::bearingJustInsideToleranceCounts()
{
    RecentPropagationTracker tracker;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    tracker.recordQso(QStringLiteral("144"), 90.0, now);
    const double justInside = 90.0 + RecentPropagationTracker::kBearingToleranceDeg - 0.1;
    QVERIFY(tracker.hasRecentOpeningNear(QStringLiteral("144"), justInside, now));
}

void TestRecentPropagationTracker::bearingJustOutsideToleranceDoesNotCount()
{
    RecentPropagationTracker tracker;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    tracker.recordQso(QStringLiteral("144"), 90.0, now);
    const double justOutside = 90.0 + RecentPropagationTracker::kBearingToleranceDeg + 0.1;
    QVERIFY(!tracker.hasRecentOpeningNear(QStringLiteral("144"), justOutside, now));
}

void TestRecentPropagationTracker::wrapsAroundTheCompassCorrectly()
{
    RecentPropagationTracker tracker;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    // 350 degrees and 10 degrees are only 20 degrees apart across the
    // 0/360 seam, not 340 -- a naive |a - b| comparison would miss this.
    tracker.recordQso(QStringLiteral("144"), 350.0, now);
    QVERIFY(tracker.hasRecentOpeningNear(QStringLiteral("144"), 10.0, now));
}

void TestRecentPropagationTracker::differentBandNeverCounts()
{
    RecentPropagationTracker tracker;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    tracker.recordQso(QStringLiteral("144"), 90.0, now);
    // Same bearing, different band -- a 144 MHz opening says nothing
    // about 432 MHz.
    QVERIFY(!tracker.hasRecentOpeningNear(QStringLiteral("432"), 90.0, now));
}

void TestRecentPropagationTracker::entryExpiresAfterTheWindow()
{
    RecentPropagationTracker tracker;
    const QDateTime loggedAt = QDateTime::currentDateTimeUtc();
    tracker.recordQso(QStringLiteral("144"), 90.0, loggedAt);

    const QDateTime stillWithin = loggedAt.addSecs(RecentPropagationTracker::kWindowSeconds - 1);
    QVERIFY(tracker.hasRecentOpeningNear(QStringLiteral("144"), 90.0, stillWithin));

    const QDateTime justAfter = loggedAt.addSecs(RecentPropagationTracker::kWindowSeconds + 1);
    QVERIFY(!tracker.hasRecentOpeningNear(QStringLiteral("144"), 90.0, justAfter));
}

void TestRecentPropagationTracker::angularDifferenceIsCircularAndSymmetric()
{
    QCOMPARE(RecentPropagationTracker::angularDifferenceDeg(350.0, 10.0), 20.0);
    QCOMPARE(RecentPropagationTracker::angularDifferenceDeg(10.0, 350.0), 20.0);
    QCOMPARE(RecentPropagationTracker::angularDifferenceDeg(0.0, 180.0), 180.0);
    QCOMPARE(RecentPropagationTracker::angularDifferenceDeg(45.0, 45.0), 0.0);
}

QTEST_APPLESS_MAIN(TestRecentPropagationTracker)
#include "test_recent_propagation_tracker.moc"
