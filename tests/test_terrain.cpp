// Tests for the Phase 2 terrain line-of-sight module
// (src/core/terrain/*). All tile data here is synthetic, injected
// directly via SrtmTileLoader::injectTileForTest()/
// TerrainDataManager::tileLoaderForTest() -- no test in this file
// touches the real network (see SrtmTileLoader.h's own doc comment for
// where real tiles actually come from). Uses a hand-rolled main() with
// a real QCoreApplication, not QTEST_APPLESS_MAIN, since
// TerrainDataManager owns a QNetworkAccessManager internally -- same
// reasoning test_dxclusterprotocol.cpp's own hand-rolled main gives.

#include <QtTest>

#include "core/terrain/LineOfSight.h"
#include "core/terrain/PathProfile.h"
#include "core/terrain/SrtmTileLoader.h"
#include "core/terrain/TerrainDataManager.h"

using namespace Contestprogramm;

namespace {
// A full SRTM1 tile's worth of samples, every one the same elevation --
// the simplest possible synthetic tile for exercising the fetch/cache/
// lookup plumbing without needing a real terrain shape.
QVector<qint16> flatTile(qint16 elevationM)
{
    return QVector<qint16>(SrtmTileLoader::kSrtmSize * SrtmTileLoader::kSrtmSize, elevationM);
}
} // namespace

class TestTerrain : public QObject
{
    Q_OBJECT

private slots:
    void tileNamingCoversAllFourHemisphereCombinations();
    void elevationLookupIsUnknownBeforeTileIsLoaded();
    void elevationLookupReturnsFlatTileValueEverywhereInside();
    void elevationLookupBilinearlyInterpolatesAGradient();
    void elevationLookupHandlesVoidSamples();
    void pathProfileSamplesRealDistanceAndFlatElevation();
    void pathProfileSamplesAreUnknownWithoutALoadedTile();
    void lineOfSightClearWhenTerrainFarBelowLineWithFresnelClearance();
    void lineOfSightMarginalWhenFresnelZoneEncroachedButNotBlocked();
    void lineOfSightBlockedWhenTerrainExceedsTheDirectLine();
    void lineOfSightUnknownWhenAnySampleIsUnknownAndNoneAreBlocked();
    void lineOfSightBlockedTakesPriorityOverUnknownElsewhereOnThePath();
    void lineOfSightUnknownForATooShortProfile();
    void terrainDataManagerResolvesSynchronouslyWithPreloadedTiles();
    void sectorSweepReturns360UnknownWithoutOwnGrid();
    void sectorSweepResolvesBlockedInEveryDirectionWithPreloadedTiles();
};

void TestTerrain::tileNamingCoversAllFourHemisphereCombinations()
{
    // Verified against a real downloaded tile for this task (see
    // SrtmTileLoader.h's own doc comment): N47E013 genuinely covers
    // the Feuerkogel/JN67VV area.
    QCOMPARE(SrtmTileLoader::tileNameForLatLon(47.7, 13.7), QStringLiteral("N47E013"));
    QCOMPARE(SrtmTileLoader::tileNameForLatLon(-33.9, -58.4), QStringLiteral("S34W059"));
    QCOMPARE(SrtmTileLoader::tileNameForLatLon(0.5, 0.5), QStringLiteral("N00E000"));
    QCOMPARE(SrtmTileLoader::tileNameForLatLon(-0.5, -0.5), QStringLiteral("S01W001"));
}

void TestTerrain::elevationLookupIsUnknownBeforeTileIsLoaded()
{
    SrtmTileLoader loader;
    QVERIFY(!loader.isTileLoaded(QStringLiteral("N47E013")));
    QVERIFY(!loader.elevationAt(47.7, 13.7).has_value());
}

void TestTerrain::elevationLookupReturnsFlatTileValueEverywhereInside()
{
    SrtmTileLoader loader;
    loader.injectTileForTest(QStringLiteral("N47E013"), flatTile(1234));
    QVERIFY(loader.isTileLoaded(QStringLiteral("N47E013")));

    for (const auto& point : {std::pair{47.0, 13.0}, std::pair{47.999, 13.999}, std::pair{47.5, 13.5}}) {
        const auto elevation = loader.elevationAt(point.first, point.second);
        QVERIFY(elevation.has_value());
        QCOMPARE(*elevation, 1234.0);
    }
}

void TestTerrain::elevationLookupBilinearlyInterpolatesAGradient()
{
    // A tile where every sample's elevation equals its own row index
    // (0 at the NORTH edge, kSrtmSize-1 at the SOUTH edge, constant
    // across each row) -- exactly predictable bilinear output: at the
    // tile's vertical midpoint, the interpolated elevation must be the
    // grid's own middle row index, regardless of longitude.
    SrtmTileLoader loader;
    QVector<qint16> samples(SrtmTileLoader::kSrtmSize * SrtmTileLoader::kSrtmSize);
    for (int row = 0; row < SrtmTileLoader::kSrtmSize; ++row) {
        for (int col = 0; col < SrtmTileLoader::kSrtmSize; ++col) {
            samples[row * SrtmTileLoader::kSrtmSize + col] = static_cast<qint16>(row);
        }
    }
    loader.injectTileForTest(QStringLiteral("N47E013"), samples);

    // Tile spans lat 47..48 (north edge = 48, row 0), lon 13..14. The
    // vertical midpoint (lat 47.5) should read back the grid's own
    // middle row index (kSrtmSize-1)/2 = 1800.
    const auto midElevation = loader.elevationAt(47.5, 13.5);
    QVERIFY(midElevation.has_value());
    QVERIFY(qAbs(*midElevation - 1800.0) < 1.0);

    // The north edge (lat -> 48, row -> 0) should read back close to 0;
    // the south edge (lat -> 47, row -> kSrtmSize-1) close to 3600.
    const auto northElevation = loader.elevationAt(47.9999, 13.5);
    QVERIFY(northElevation.has_value());
    QVERIFY(*northElevation < 50.0);
    const auto southElevation = loader.elevationAt(47.0001, 13.5);
    QVERIFY(southElevation.has_value());
    QVERIFY(*southElevation > 3550.0);
}

void TestTerrain::elevationLookupHandlesVoidSamples()
{
    SrtmTileLoader loader;
    QVector<qint16> samples = flatTile(500);
    // Void out the entire tile -- every corner any query could land on
    // is void, so every lookup must come back unknown, not a fabricated
    // elevation (HAUSSTIL rule 7).
    std::fill(samples.begin(), samples.end(), SrtmTileLoader::kVoidValue);
    loader.injectTileForTest(QStringLiteral("N47E013"), samples);
    QVERIFY(!loader.elevationAt(47.5, 13.5).has_value());
}

void TestTerrain::pathProfileSamplesRealDistanceAndFlatElevation()
{
    SrtmTileLoader loader;
    loader.injectTileForTest(QStringLiteral("N47E013"), flatTile(800));

    // Two nearby points, both well inside the same injected tile --
    // JN67VV and JN67VW are adjacent 6-character sub-squares, a few km
    // apart.
    const double lat1 = 47.7;
    const double lon1 = 13.6;
    const double lat2 = 47.71;
    const double lon2 = 13.62;

    const QVector<ElevationSample> profile = samplePathProfile(loader, lat1, lon1, lat2, lon2, 10);
    QCOMPARE(profile.size(), 10);
    QCOMPARE(profile.first().distanceKm, 0.0);
    QVERIFY(profile.last().distanceKm > 0.0);
    // Monotonically non-decreasing distance along the path.
    for (int i = 1; i < profile.size(); ++i) {
        QVERIFY(profile.at(i).distanceKm >= profile.at(i - 1).distanceKm);
    }
    for (const ElevationSample& sample : profile) {
        QVERIFY(sample.elevationM.has_value());
        QCOMPARE(*sample.elevationM, 800.0);
    }
}

void TestTerrain::pathProfileSamplesAreUnknownWithoutALoadedTile()
{
    SrtmTileLoader loader; // nothing injected
    const QVector<ElevationSample> profile = samplePathProfile(loader, 47.7, 13.6, 47.71, 13.62, 5);
    QCOMPARE(profile.size(), 5);
    for (const ElevationSample& sample : profile) {
        QVERIFY(!sample.elevationM.has_value());
    }
}

void TestTerrain::lineOfSightClearWhenTerrainFarBelowLineWithFresnelClearance()
{
    // Both stations at 1000m elevation + 10m antenna (line height
    // 1010m throughout, flat line since both ends match); terrain a
    // flat 500m the whole 10km path -- clearance (~510m minus a
    // negligible earth-bulge term) vastly exceeds 60% of the first
    // Fresnel zone at 144 MHz over this short a path.
    const QVector<ElevationSample> profile = {
        {0.0, 500.0},
        {5.0, 500.0},
        {10.0, 500.0},
    };
    QCOMPARE(classifyLineOfSight(profile, 1000.0, 10.0, 1000.0, 10.0, 144.0), LineOfSightClass::Clear);
}

void TestTerrain::lineOfSightMarginalWhenFresnelZoneEncroachedButNotBlocked()
{
    // Same geometry as the Clear case, but the midpoint terrain is
    // raised to 970m -- clearance drops to ~38.5m against a first
    // Fresnel radius of ~72m at 144 MHz/10km (0.6*72 = ~43m), so the
    // line still physically clears the terrain (positive clearance,
    // not Blocked) but no longer has 60% Fresnel clearance.
    const QVector<ElevationSample> profile = {
        {0.0, 1000.0},
        {5.0, 970.0},
        {10.0, 1000.0},
    };
    QCOMPARE(classifyLineOfSight(profile, 1000.0, 10.0, 1000.0, 10.0, 144.0), LineOfSightClass::Marginal);
}

void TestTerrain::lineOfSightBlockedWhenTerrainExceedsTheDirectLine()
{
    // Midpoint terrain (1200m) is above the 1010m direct line -- a
    // real, physical obstruction.
    const QVector<ElevationSample> profile = {
        {0.0, 1000.0},
        {5.0, 1200.0},
        {10.0, 1000.0},
    };
    QCOMPARE(classifyLineOfSight(profile, 1000.0, 10.0, 1000.0, 10.0, 144.0), LineOfSightClass::Blocked);
}

void TestTerrain::lineOfSightUnknownWhenAnySampleIsUnknownAndNoneAreBlocked()
{
    const QVector<ElevationSample> profile = {
        {0.0, 500.0},
        {5.0, std::nullopt},
        {10.0, 500.0},
    };
    QCOMPARE(classifyLineOfSight(profile, 1000.0, 10.0, 1000.0, 10.0, 144.0), LineOfSightClass::Unknown);
}

void TestTerrain::lineOfSightBlockedTakesPriorityOverUnknownElsewhereOnThePath()
{
    // One sample is unknown, but a DIFFERENT sample is a confirmed
    // obstruction -- the confirmed Blocked must win, per
    // classifyLineOfSight()'s own doc comment on this priority (one
    // certain blockage stays certain regardless of what else is
    // unknown on the same path).
    const QVector<ElevationSample> profile = {
        {0.0, 1000.0},
        {3.0, 1200.0}, // blocks
        {7.0, std::nullopt},
        {10.0, 1000.0},
    };
    QCOMPARE(classifyLineOfSight(profile, 1000.0, 10.0, 1000.0, 10.0, 144.0), LineOfSightClass::Blocked);
}

void TestTerrain::lineOfSightUnknownForATooShortProfile()
{
    QCOMPARE(classifyLineOfSight({}, 1000.0, 10.0, 1000.0, 10.0, 144.0), LineOfSightClass::Unknown);
    QCOMPARE(classifyLineOfSight({{0.0, 500.0}}, 1000.0, 10.0, 1000.0, 10.0, 144.0), LineOfSightClass::Unknown);
}

void TestTerrain::terrainDataManagerResolvesSynchronouslyWithPreloadedTiles()
{
    TerrainDataManager manager;
    // JN67VV and JN67VW both fall inside N47E013 -- pre-inject it via
    // the test-only accessor so classification() resolves synchronously
    // below without ever touching the real network (see this file's
    // own header comment).
    manager.tileLoaderForTest().injectTileForTest(QStringLiteral("N47E013"), flatTile(500));

    manager.setOwnStation(QStringLiteral("JN67VV"), 1000.0, 10.0);
    const LineOfSightClass result = manager.classification(QStringLiteral("JN67VW"), 144.0, 1000.0, 10.0);
    // Flat 500m terrain well below a ~1010m line over a few-km path --
    // must resolve to a real answer immediately, not stay Unknown
    // waiting on a tile that is, in fact, already loaded.
    QCOMPARE(result, LineOfSightClass::Clear);

    // A second call for the SAME pair must return the cached result
    // without re-deriving it (nothing to assert on directly here beyond
    // "it still answers the same way and doesn't crash/hang").
    QCOMPARE(manager.classification(QStringLiteral("JN67VW"), 144.0, 1000.0, 10.0), LineOfSightClass::Clear);
}

void TestTerrain::sectorSweepReturns360UnknownWithoutOwnGrid()
{
    TerrainDataManager manager; // setOwnStation() never called
    const QVector<LineOfSightClass> sectors = manager.sectorSweep(144.0);
    QCOMPARE(sectors.size(), 360);
    for (LineOfSightClass cls : sectors) {
        QCOMPARE(cls, LineOfSightClass::Unknown);
    }
}

void TestTerrain::sectorSweepResolvesBlockedInEveryDirectionWithPreloadedTiles()
{
    TerrainDataManager manager;
    // The 4-character grid "JN67" centres at EXACTLY (47.5 deg N, 13.0
    // deg E) -- calculateLatLonFromGridSquare()'s own formula for a
    // 4-char square: lat = -90 + fieldLat*10 + squareLat + 0.5 (field
    // 'N'=13, square '7' -> -90+130+7+0.5 = 47.5); lon = -180 +
    // fieldLon*20 + squareLon*2 + 1 (field 'J'=9, square '6' ->
    // -180+180+12+1 = 13.0). 47.5 is 0.5deg (~55.5km) from either
    // latitude tile boundary -- outside a 50km sweep's max north/south
    // reach (50km < 55.5km), so N46/N48 are never needed. 13.0 sits
    // exactly on the E012/E013 boundary, so both of those ARE needed
    // (E014 isn't, strictly, but is injected too for a comfortable
    // margin -- one extra tile costs nothing here).
    for (const QString& tile : {QStringLiteral("N47E012"), QStringLiteral("N47E013"), QStringLiteral("N47E014")}) {
        manager.tileLoaderForTest().injectTileForTest(tile, flatTile(2500));
    }
    manager.setOwnStation(QStringLiteral("JN67"), 1000.0, 10.0);

    const QVector<LineOfSightClass> sectors = manager.sectorSweep(144.0);
    QCOMPARE(sectors.size(), 360);
    for (int degree = 0; degree < 360; ++degree) {
        QCOMPARE(sectors.at(degree), LineOfSightClass::Blocked);
    }
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestTerrain tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_terrain.moc"
