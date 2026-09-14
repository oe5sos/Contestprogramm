#include <QtTest>

#include "core/GeoFilter.h"
#include "core/SpotCandidate.h"
#include "core/terrain/SrtmTileLoader.h"
#include "core/terrain/TerrainDataManager.h"

using namespace Contestprogramm;

namespace {

SpotCandidate makeCandidate(const QString& callsign, const QString& grid)
{
    SpotCandidate candidate;
    candidate.callsign = callsign;
    candidate.grid = grid;
    candidate.source = QStringLiteral("on4kst");
    return candidate;
}

} // namespace

class TestGeoFilter : public QObject
{
    Q_OBJECT

private slots:
    void inRangeCandidateClassifiedInRange();
    void outOfRangeCandidateClassifiedOutOfRange();
    void missingCandidateGridDoesNotHideIt();
    void missingOwnGridDoesNotHideCandidate();
    void bearingIsComputedWhenDistanceIsKnown();
    void terrainBlockedHardExcludesEvenWithinRadius();
    void noTerrainDataManagerLeavesInRangePurelyRadiusBased();
};

void TestGeoFilter::inRangeCandidateClassifiedInRange()
{
    GeoFilter filter;
    filter.setOwnGrid(QStringLiteral("JN77QT"));
    filter.setRadiusKm(300.0);

    // JN77QT/JN88TC are ~170 km apart (see test_maidenhead.cpp's own
    // cross-checked reference pair).
    const GeoFilter::Result result = filter.classify(makeCandidate(QStringLiteral("OE1ABC"), QStringLiteral("JN88TC")));
    QVERIFY(result.distanceKnown);
    QVERIFY(result.inRange);
    QVERIFY(result.distanceKm > 100.0 && result.distanceKm < 250.0);
}

void TestGeoFilter::outOfRangeCandidateClassifiedOutOfRange()
{
    GeoFilter filter;
    filter.setOwnGrid(QStringLiteral("JN77QT"));
    filter.setRadiusKm(300.0);

    // FN31PR (New England) is thousands of km from JN77QT -- well
    // outside any VHF/UHF contest radius.
    const GeoFilter::Result result = filter.classify(makeCandidate(QStringLiteral("W1TST"), QStringLiteral("FN31PR")));
    QVERIFY(result.distanceKnown);
    QVERIFY(!result.inRange);
}

void TestGeoFilter::missingCandidateGridDoesNotHideIt()
{
    GeoFilter filter;
    filter.setOwnGrid(QStringLiteral("JN77QT"));
    filter.setRadiusKm(300.0);

    const GeoFilter::Result result = filter.classify(makeCandidate(QStringLiteral("OE9XYZ"), QString()));
    QVERIFY(!result.distanceKnown);
    // Per the plan's Phase 2 LineOfSight::Unknown precedent: an
    // unclassifiable candidate is not the same as an out-of-range one,
    // and must not be silently suppressed.
    QVERIFY(result.inRange);
}

void TestGeoFilter::missingOwnGridDoesNotHideCandidate()
{
    GeoFilter filter;
    filter.setOwnGrid(QString());
    filter.setRadiusKm(300.0);

    const GeoFilter::Result result = filter.classify(makeCandidate(QStringLiteral("OE9XYZ"), QStringLiteral("JN88TC")));
    QVERIFY(!result.distanceKnown);
    QVERIFY(result.inRange);
}

void TestGeoFilter::bearingIsComputedWhenDistanceIsKnown()
{
    GeoFilter filter;
    filter.setOwnGrid(QStringLiteral("JN77QT"));
    filter.setRadiusKm(300.0);

    const GeoFilter::Result result = filter.classify(makeCandidate(QStringLiteral("OE1ABC"), QStringLiteral("JN88TC")));
    QVERIFY(result.distanceKnown);
    // JN77QT -> JN88TC is mostly eastward (see test_maidenhead.cpp).
    QVERIFY(result.bearingDeg > 70.0 && result.bearingDeg < 85.0);
}

// Phase 2 terrain hookup (2026-09-12) -- see GeoFilter::classify()'s own
// doc comment on the exact rule: only a CONFIRMED Blocked classification
// hard-excludes, and only when a TerrainDataManager has actually been
// wired up via setTerrainDataManager().
void TestGeoFilter::terrainBlockedHardExcludesEvenWithinRadius()
{
    TerrainDataManager terrainManager;
    // A single flat, deliberately obstructive tile covering both ends
    // of a short in-radius path -- injected directly via
    // TerrainDataManager::tileLoaderForTest(), no real network touched.
    // 2500m elevation everywhere versus 1000m ground + 10m antenna on
    // both ends guarantees a genuine Blocked classification (terrain
    // far above the direct line), not just Marginal.
    terrainManager.tileLoaderForTest().injectTileForTest(
        QStringLiteral("N47E013"), QVector<qint16>(SrtmTileLoader::kSrtmSize * SrtmTileLoader::kSrtmSize, 2500));
    terrainManager.setOwnStation(QStringLiteral("JN67VV"), 1000.0, 10.0);

    GeoFilter filter;
    filter.setOwnGrid(QStringLiteral("JN67VV"));
    filter.setRadiusKm(300.0);
    filter.setTerrainDataManager(&terrainManager, 144.0);

    // JN67VW is a few km from JN67VV -- well within any sane contest
    // radius on its own.
    const GeoFilter::Result result =
        filter.classify(makeCandidate(QStringLiteral("OE5XYZ"), QStringLiteral("JN67VW")));
    QVERIFY(result.distanceKnown);
    QVERIFY(result.distanceKm < 300.0);
    QCOMPARE(result.terrain, LineOfSightClass::Blocked);
    // ...but hard-excluded anyway, per the terrain hook.
    QVERIFY(!result.inRange);
}

// The overwhelming common case in this codebase today (no
// setTerrainDataManager() call at all, matching every other test in
// this file) -- Result::terrain must stay Unknown and `inRange` must
// stay exactly what the radius check alone decides, unaffected by
// terrain ever being consulted.
void TestGeoFilter::noTerrainDataManagerLeavesInRangePurelyRadiusBased()
{
    GeoFilter filter;
    filter.setOwnGrid(QStringLiteral("JN77QT"));
    filter.setRadiusKm(300.0);

    const GeoFilter::Result result =
        filter.classify(makeCandidate(QStringLiteral("OE1ABC"), QStringLiteral("JN88TC")));
    QCOMPARE(result.terrain, LineOfSightClass::Unknown);
    QVERIFY(result.inRange);
}

// Not QTEST_APPLESS_MAIN: terrainBlockedHardExcludesEvenWithinRadius()
// constructs a TerrainDataManager, which owns a QNetworkAccessManager
// internally -- same reasoning test_terrain.cpp's/
// test_dxclusterprotocol.cpp's own hand-rolled main already gives.
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestGeoFilter tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_geofilter.moc"
