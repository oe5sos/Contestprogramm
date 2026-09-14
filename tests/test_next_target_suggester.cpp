// Tests for NextTargetSuggester::suggest() -- a pure function over
// plain (SpotCandidate, score, GeoFilter::Result) tuples, deliberately
// decoupled from ChatFeedModel/QAbstractTableModel so the ranking logic
// can be exercised with synthetic data. See src/core/assistant/
// NextTargetSuggester.h's own doc comment.

#include <QtTest>

#include "core/assistant/NextTargetSuggester.h"

using namespace Contestprogramm;

namespace {
NextTargetSuggester::Candidate makeCandidate(const QString& callsign, double score)
{
    NextTargetSuggester::Candidate entry;
    entry.candidate.callsign = callsign;
    entry.score = score;
    return entry;
}
} // namespace

class TestNextTargetSuggester : public QObject
{
    Q_OBJECT

private slots:
    void emptyCandidateListYieldsNoSuggestion();
    void highestScoredCandidateWins();
    void tieKeepsFirstEncountered();
    void suggestionCarriesGeoResultThrough();
};

void TestNextTargetSuggester::emptyCandidateListYieldsNoSuggestion()
{
    const auto result = NextTargetSuggester::suggest({});
    QVERIFY(!result.has_value());
}

void TestNextTargetSuggester::highestScoredCandidateWins()
{
    const QVector<NextTargetSuggester::Candidate> candidates = {
        makeCandidate(QStringLiteral("OE1AAA"), 0.4),
        makeCandidate(QStringLiteral("OE2BBB"), 0.9),
        makeCandidate(QStringLiteral("OE3CCC"), 0.6),
    };
    const auto result = NextTargetSuggester::suggest(candidates);
    QVERIFY(result.has_value());
    QCOMPARE(result->candidate.callsign, QStringLiteral("OE2BBB"));
    QCOMPARE(result->score, 0.9);
}

void TestNextTargetSuggester::tieKeepsFirstEncountered()
{
    const QVector<NextTargetSuggester::Candidate> candidates = {
        makeCandidate(QStringLiteral("OE1AAA"), 0.5),
        makeCandidate(QStringLiteral("OE2BBB"), 0.5),
    };
    const auto result = NextTargetSuggester::suggest(candidates);
    QVERIFY(result.has_value());
    QCOMPARE(result->candidate.callsign, QStringLiteral("OE1AAA"));
}

void TestNextTargetSuggester::suggestionCarriesGeoResultThrough()
{
    NextTargetSuggester::Candidate entry = makeCandidate(QStringLiteral("OE1AAA"), 0.7);
    entry.geo.distanceKnown = true;
    entry.geo.distanceKm = 123.4;
    entry.geo.bearingDeg = 45.0;

    const auto result = NextTargetSuggester::suggest({entry});
    QVERIFY(result.has_value());
    QVERIFY(result->geo.distanceKnown);
    QCOMPARE(result->geo.distanceKm, 123.4);
    QCOMPARE(result->geo.bearingDeg, 45.0);
}

QTEST_MAIN(TestNextTargetSuggester)
#include "test_next_target_suggester.moc"
