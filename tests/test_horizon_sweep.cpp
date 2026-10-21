// Tests for the site-scouting horizon-sweep module (core/terrain/
// HorizonSweep.h/.cpp) -- Contestprogramm-original (2026-09-14), built
// for ui/SiteComparisonDialog.h's own standalone "compare two candidate
// sites' real terrain openness" feature. All tile data here is
// synthetic, injected directly via SrtmTileLoader::
// injectTileForTest() -- no test in this file touches the real network
// (see test_terrain.cpp's own header comment for the same posture, and
// SrtmTileLoader.h's own doc comment for where real tiles actually come
// from). Uses a hand-rolled main() with a real QCoreApplication, not
// QTEST_APPLESS_MAIN, since SrtmTileLoader owns a QNetworkAccessManager
// internally -- same reasoning test_terrain.cpp's own hand-rolled main
// already gives for the identical situation.
//
// Every test below picks a bearing (sectorCount=1, so
// computeHorizonSweep() only ever evaluates that ONE bearing) and a
// small, explicit sampleDistancesKm override rather than the real
// 14-entry/36-bearing default -- letting a test cover the full default
// sweep would call SrtmTileLoader::ensureTileAvailable() for dozens of
// directions this file has no injected tile for at all, which is
// unnecessary noise for what each test actually wants to assert (this
// file only ever wants to prove ONE bearing's own classification logic
// is correct, not exercise the full-circle default). Every distance an
// injected test DOES use is chosen so its own destination point lands
// in a different SRTM tile than any other distance in the same test --
// verified by an explicit QVERIFY() rather than assumed, since a flat
// synthetic tile (the same helper test_terrain.cpp's own flatTile()
// uses) cannot hold two different elevations at once, so two same-test
// distances accidentally landing in the same tile would silently
// corrupt the very comparison the test is trying to make.

#include <QtTest>

#include "core/Maidenhead.h"
#include "core/terrain/HorizonSweep.h"
#include "core/terrain/LineOfSight.h"
#include "core/terrain/SrtmTileLoader.h"

using namespace Contestprogramm;

namespace {

// Same "one elevation everywhere in this tile" synthetic tile
// test_terrain.cpp's own flatTile() helper builds -- duplicated here
// rather than shared through a new test-only header, for the same
// "one ten-line helper is not worth a new header" reasoning
// HorizonCompassWidget.cpp's own pointOnCircle() comment already gives
// for an analogous small duplication.
QVector<qint16> flatTile(qint16 elevationM)
{
    return QVector<qint16>(SrtmTileLoader::kSrtmSize * SrtmTileLoader::kSrtmSize, elevationM);
}

// The real SRTM tile name covering the point `distanceKm` away from
// (originLat, originLon) along `bearingDeg` -- computed via the exact
// same destinationPoint()/tileNameForLatLon() functions
// computeHorizonSweep() itself uses internally, so a test never has to
// hand-derive tile boundary arithmetic (and risk getting it subtly
// wrong the way a manual degree/km conversion could) to know which
// tile to inject.
QString tileForBearingAndDistance(double originLat, double originLon, double bearingDeg, double distanceKm)
{
    double lat = 0.0;
    double lon = 0.0;
    destinationPoint(originLat, originLon, bearingDeg, distanceKm, lat, lon);
    return SrtmTileLoader::tileNameForLatLon(lat, lon);
}

// A mid-tile origin (not on a whole-degree boundary, unlike the
// hemisphere-edge cases test_terrain.cpp's own tileNamingCovers...
// test specifically targets), so every destination point's own lat/lon
// lands comfortably away from a tile edge itself rather than risking a
// hairline floating-point nudge across a boundary. kBearingDeg is 0
// (due north) deliberately, NOT an arbitrary angle -- every test below
// passes sectorCount=1 to computeHorizonSweep(), and that always
// evaluates bearingDeg = 0 for its one sector (index 0 * (360/1) = 0,
// see that function's own bearingDeg formula), so the tiles this file
// injects have to be computed for bearing 0 to actually be the ones
// the sweep queries. A due-north bearing has no accuracy downside here
// either: sin(0.0) is exactly 0.0 in IEEE doubles, so the destination-
// point formula's own longitude term comes out exactly unchanged
// (10.5), not merely close to it -- no hairline floating-point tile-
// boundary risk the way a bearing landing near a whole-degree longitude
// by coincidence could have.
constexpr double kOriginLat = 60.5;
constexpr double kOriginLon = 10.5;
constexpr double kBearingDeg = 0.0;
constexpr double kEyeElevM = 1000.0; // originGroundElevationM=1000, eyeHeightAboveGroundM=0 in every test below

} // namespace

class TestHorizonSweep : public QObject
{
    Q_OBJECT

private slots:
    void bearingTowardATallRidgeClassifiesBlockedAtTheRidgesOwnDistance();
    void bearingTowardFlatLowTerrainClassifiesClear();
    void bearingWithNoInjectedTilesStaysUnknown();
    void curvatureTermPreventsAFalseBlockedAtLongRange();
};

void TestHorizonSweep::bearingTowardATallRidgeClassifiesBlockedAtTheRidgesOwnDistance()
{
    const QVector<double> distances = {10.0, 150.0};
    const QString nearTile = tileForBearingAndDistance(kOriginLat, kOriginLon, kBearingDeg, 10.0);
    const QString farTile = tileForBearingAndDistance(kOriginLat, kOriginLon, kBearingDeg, 150.0);
    QVERIFY(nearTile != farTile); // see this file's own header comment on why this must hold

    SrtmTileLoader loader;
    loader.injectTileForTest(nearTile, flatTile(500));   // well below eye level -- must not win "max angle"
    loader.injectTileForTest(farTile, flatTile(5000));   // a real ridge, far above eye level

    const QVector<HorizonSector> sectors
        = computeHorizonSweep(loader, kOriginLat, kOriginLon, 1000.0, 0.0, 1, distances);
    QCOMPARE(sectors.size(), 1);
    const HorizonSector& sector = sectors.first();
    QCOMPARE(sector.bearingDeg, 0); // sectorCount=1 -> the one bearing is always index 0 -> 0deg
    QCOMPARE(sector.classification, LineOfSightClass::Blocked);
    QCOMPARE(sector.obstructionDistanceKm, 150.0);
    QCOMPARE(sector.obstructionElevationM, 5000.0);
    // Cross-checked against horizonElevationAngleDeg() itself (the same
    // formula computeHorizonSweep() uses internally) rather than a
    // hand-computed constant -- this test's job is to verify the ridge
    // at 150km actually wins over the near, low sample and correctly
    // classifies Blocked, not to re-derive the trig by hand.
    QCOMPARE(sector.maxElevationAngleDeg, horizonElevationAngleDeg(kEyeElevM, 5000.0, 150000.0));
    QVERIFY(sector.maxElevationAngleDeg >= 0.0); // Blocked's own defining threshold
}

void TestHorizonSweep::bearingTowardFlatLowTerrainClassifiesClear()
{
    const QVector<double> distances = {10.0, 150.0};
    const QString nearTile = tileForBearingAndDistance(kOriginLat, kOriginLon, kBearingDeg, 10.0);
    const QString farTile = tileForBearingAndDistance(kOriginLat, kOriginLon, kBearingDeg, 150.0);
    QVERIFY(nearTile != farTile);

    SrtmTileLoader loader;
    // Both samples well below eye level -- open terrain the whole ray,
    // no ridge anywhere near either sampled distance.
    loader.injectTileForTest(nearTile, flatTile(500));
    loader.injectTileForTest(farTile, flatTile(500));

    const QVector<HorizonSector> sectors
        = computeHorizonSweep(loader, kOriginLat, kOriginLon, 1000.0, 0.0, 1, distances);
    QCOMPARE(sectors.size(), 1);
    const HorizonSector& sector = sectors.first();
    QCOMPARE(sector.classification, LineOfSightClass::Clear);
    // Not meaningful for Clear (see HorizonSector's own doc comment) --
    // left at their struct defaults rather than the winning sample's
    // real distance/elevation.
    QCOMPARE(sector.obstructionDistanceKm, 0.0);
    QCOMPARE(sector.obstructionElevationM, 0.0);
}

void TestHorizonSweep::bearingWithNoInjectedTilesStaysUnknown()
{
    // Nothing injected at all -- SrtmTileLoader::ensureTileAvailable()
    // gets called (computeHorizonSweep() always requests every sample's
    // tile, see that function's own doc comment on the per-bearing
    // async gate) but the tile can never actually be "loaded" within
    // this synchronous test slot (no event loop runs here to let a real
    // fetch complete, and this exact coordinate has nothing pre-seeded
    // either way) -- elevationAt() correctly comes back unset
    // immediately regardless, which is the behaviour under test: a
    // still-pending tile must never be silently treated as flat/open
    // terrain (HAUSSTIL rule 7, same posture LineOfSight.h's own
    // Unknown value documents).
    SrtmTileLoader loader;
    const QVector<double> distances = {10.0};

    const QVector<HorizonSector> sectors
        = computeHorizonSweep(loader, kOriginLat, kOriginLon, 1000.0, 0.0, 1, distances);
    QCOMPARE(sectors.size(), 1);
    const HorizonSector& sector = sectors.first();
    QCOMPARE(sector.classification, LineOfSightClass::Unknown);
    QCOMPARE(sector.obstructionDistanceKm, 0.0);
    QCOMPARE(sector.obstructionElevationM, 0.0);
    QCOMPARE(sector.maxElevationAngleDeg, 0.0);
}

void TestHorizonSweep::curvatureTermPreventsAFalseBlockedAtLongRange()
{
    // The regression case for the 4/3-effective-earth-radius drop term
    // (horizonElevationAngleDeg()'s own dropM): a far sample whose
    // terrain sits at EXACTLY the origin's own eye elevation. Under an
    // (incorrect) flat-earth calculation ignoring earth curvature, that
    // sample's elevation angle is exactly atan2(0, distance) = 0deg --
    // right at Blocked's own ">=0" threshold, so a flat-earth bug would
    // wrongly classify this bearing Blocked. The real, curvature-aware
    // formula subtracts a real ~1.32km-equivalent drop at 100km (see
    // horizonElevationAngleDeg()'s own doc comment), pulling the true
    // angle down to roughly -0.34deg -- past the -0.3deg Marginal cutoff
    // too, so the CORRECT classification here is Clear. A
    // computeHorizonSweep() that forgot the earth-curvature term
    // entirely would fail this test by returning Blocked instead.
    const QVector<double> distances = {10.0, 100.0};
    const QString nearTile = tileForBearingAndDistance(kOriginLat, kOriginLon, kBearingDeg, 10.0);
    const QString farTile = tileForBearingAndDistance(kOriginLat, kOriginLon, kBearingDeg, 100.0);
    QVERIFY(nearTile != farTile);

    SrtmTileLoader loader;
    // Comfortably below eye level -- this sample must not be the
    // bearing's "max angle" sample; the far sample below is the one
    // this test is actually about.
    loader.injectTileForTest(nearTile, flatTile(200));
    // Exactly the origin's own eye elevation (1000m ground + 0m eye
    // height) -- see this test's own header comment on why this exact
    // value is the whole point.
    loader.injectTileForTest(farTile, flatTile(static_cast<qint16>(kEyeElevM)));

    const QVector<HorizonSector> sectors
        = computeHorizonSweep(loader, kOriginLat, kOriginLon, 1000.0, 0.0, 1, distances);
    QCOMPARE(sectors.size(), 1);
    const HorizonSector& sector = sectors.first();

    const double expectedAngleDeg = horizonElevationAngleDeg(kEyeElevM, kEyeElevM, 100000.0);
    QVERIFY(expectedAngleDeg < -0.3); // sanity-check the formula's own real output before trusting the classification below
    QCOMPARE(sector.maxElevationAngleDeg, expectedAngleDeg);
    QCOMPARE(sector.classification, LineOfSightClass::Clear);
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestHorizonSweep tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_horizon_sweep.moc"
