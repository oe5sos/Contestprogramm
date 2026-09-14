#include <QtTest>

#include <QDateTime>
#include <QVector>

#include "core/ChatImportanceScorer.h"
#include "core/ChatVisibilityPolicy.h"

using namespace Contestprogramm;

class TestAdaptiveChatFilter : public QObject
{
    Q_OBJECT

private slots:
    void tempoForRateBoundaries();
    void quietTempoShowsAlmostEverythingAboveLowBar();
    void moderateTempoUsesAHigherBar();
    void busyTempoKeepsOnlyTopScoredFraction();
    void busyTempoAlwaysKeepsAtLeastOneRow();
    void emptyScoresYieldEmptyMask();

    void scorerTreatsDupeAsLowestImportance();
    void scorerBoostsNeededMultiplier();
    void scorerBoostsRecentPropagationNearby();
    void scorerDecaysWithAge();
};

void TestAdaptiveChatFilter::tempoForRateBoundaries()
{
    using ChatVisibilityPolicy::tempoForRate;
    using ChatVisibilityPolicy::Tempo;

    // Plain QVERIFY (not QCOMPARE): Tempo is an unregistered enum class
    // with no QTest::toString overload.
    QVERIFY(tempoForRate(0.0) == Tempo::Quiet);
    QVERIFY(tempoForRate(1.9) == Tempo::Quiet);
    QVERIFY(tempoForRate(2.0) == Tempo::Moderate); // "below 2 = quiet" -- 2.0 itself is not below
    QVERIFY(tempoForRate(5.9) == Tempo::Moderate);
    QVERIFY(tempoForRate(6.0) == Tempo::Busy);      // "above 6 = busy" -- read as >= for a clean boundary
    QVERIFY(tempoForRate(20.0) == Tempo::Busy);
}

void TestAdaptiveChatFilter::quietTempoShowsAlmostEverythingAboveLowBar()
{
    const QVector<double> scores = {0.5, 0.4, 0.01};
    const QVector<bool> mask = ChatVisibilityPolicy::visibilityMask(scores, ChatVisibilityPolicy::Tempo::Quiet);
    QCOMPARE(mask.size(), scores.size());
    QVERIFY(mask.at(0));
    QVERIFY(mask.at(1));
    QVERIFY(!mask.at(2)); // 0.01 < kQuietMinScore (0.05)
}

void TestAdaptiveChatFilter::moderateTempoUsesAHigherBar()
{
    // Same scores as the quiet test, but Moderate's higher bar (0.30)
    // now also drops the 0.01 entry AND a mid-range one that Quiet let
    // through.
    const QVector<double> scores = {0.5, 0.2, 0.01};
    const QVector<bool> mask = ChatVisibilityPolicy::visibilityMask(scores, ChatVisibilityPolicy::Tempo::Moderate);
    QVERIFY(mask.at(0));
    QVERIFY(!mask.at(1));
    QVERIFY(!mask.at(2));
}

void TestAdaptiveChatFilter::busyTempoKeepsOnlyTopScoredFraction()
{
    // 10 candidates, strictly descending scores 1.0 down to 0.1.
    // kBusyTopFraction (0.30) -> keep the top 3.
    QVector<double> scores;
    for (int i = 0; i < 10; ++i) {
        scores.append(1.0 - i * 0.1);
    }
    const QVector<bool> mask = ChatVisibilityPolicy::visibilityMask(scores, ChatVisibilityPolicy::Tempo::Busy);

    int visibleCount = 0;
    for (int i = 0; i < mask.size(); ++i) {
        if (mask.at(i)) {
            ++visibleCount;
            QVERIFY(i < 3); // only the three highest-scored indices survive
        }
    }
    QCOMPARE(visibleCount, 3);
}

void TestAdaptiveChatFilter::busyTempoAlwaysKeepsAtLeastOneRow()
{
    // A single, low-scoring survivor -- must not be filtered down to
    // zero rows just because the busy fraction of 1 rounds to
    // "less than one".
    const QVector<double> scores = {0.05};
    const QVector<bool> mask = ChatVisibilityPolicy::visibilityMask(scores, ChatVisibilityPolicy::Tempo::Busy);
    QCOMPARE(mask.size(), 1);
    QVERIFY(mask.at(0));
}

void TestAdaptiveChatFilter::emptyScoresYieldEmptyMask()
{
    const QVector<bool> mask = ChatVisibilityPolicy::visibilityMask({}, ChatVisibilityPolicy::Tempo::Busy);
    QVERIFY(mask.isEmpty());
}

void TestAdaptiveChatFilter::scorerTreatsDupeAsLowestImportance()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const double dupeScore = ChatImportanceScorer::score(/*isDupe=*/true, /*isNeededMultiplier=*/true,
                                                           /*recentPropagationNearby=*/true, now, now);
    const double freshScore = ChatImportanceScorer::score(/*isDupe=*/false, /*isNeededMultiplier=*/false,
                                                            /*recentPropagationNearby=*/false, now, now);
    QCOMPARE(dupeScore, ChatImportanceScorer::kDupeScore);
    QVERIFY(dupeScore < freshScore);
}

void TestAdaptiveChatFilter::scorerBoostsNeededMultiplier()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const double unweighted = ChatImportanceScorer::score(false, false, false, now, now);
    const double needed = ChatImportanceScorer::score(false, true, false, now, now);
    QVERIFY(needed > unweighted);
    QCOMPARE(needed - unweighted, ChatImportanceScorer::kMultiplierBoost);
}

void TestAdaptiveChatFilter::scorerBoostsRecentPropagationNearby()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const double unweighted = ChatImportanceScorer::score(false, false, false, now, now);
    const double boosted = ChatImportanceScorer::score(false, false, true, now, now);
    QVERIFY(boosted > unweighted);
    QCOMPARE(boosted - unweighted, ChatImportanceScorer::kPropagationBoost);
    // Smaller than a needed-multiplier boost -- the plan's own "hebt...
    // leicht an" (raises... slightly) wording, deliberately a softer
    // nudge than a confirmed still-needed multiplier.
    QVERIFY(ChatImportanceScorer::kPropagationBoost < ChatImportanceScorer::kMultiplierBoost);
}

void TestAdaptiveChatFilter::scorerDecaysWithAge()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const double brandNew = ChatImportanceScorer::score(false, false, false, now, now);
    const double old =
        ChatImportanceScorer::score(false, false, false, now.addSecs(-ChatImportanceScorer::kRecencyHorizonSeconds), now);
    const double veryOld = ChatImportanceScorer::score(
        false, false, false, now.addSecs(-10 * ChatImportanceScorer::kRecencyHorizonSeconds), now);
    QVERIFY(brandNew > old);
    // Beyond the recency horizon, the bonus is fully decayed (clamped
    // at zero, not negative) -- same score whether it is one horizon or
    // ten horizons old.
    QCOMPARE(old, veryOld);
}

QTEST_APPLESS_MAIN(TestAdaptiveChatFilter)
#include "test_adaptive_chat_filter.moc"
