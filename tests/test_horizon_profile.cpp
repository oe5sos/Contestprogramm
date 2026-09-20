#include <QtTest>

#include "core/terrain/HorizonProfile.h"
#include "core/terrain/SrtmTileLoader.h"

using namespace Contestprogramm;

namespace {

// A synthetic tile: flat at `base` metres, with a ridge of `ridge`
// metres (a north-south wall about 600 m thick -- wider than the
// profile's sample step, as any real ridge is) east of the origin.
QVector<qint16> tileWithRidge(qint16 base, qint16 ridge, int ridgeCol)
{
    QVector<qint16> samples(SrtmTileLoader::kSrtmSize * SrtmTileLoader::kSrtmSize, base);
    for (int row = 0; row < SrtmTileLoader::kSrtmSize; ++row) {
        for (int col = ridgeCol; col < ridgeCol + 30 && col < SrtmTileLoader::kSrtmSize; ++col) {
            samples[row * SrtmTileLoader::kSrtmSize + col] = ridge;
        }
    }
    return samples;
}

} // namespace

// core/terrain/HorizonProfile.h: elevation angle of the terrain horizon
// per bearing, from SRTM samples along each bearing.
class TestHorizonProfile : public QObject
{
    Q_OBJECT

private slots:
    void angleGeometryWithEarthBulge();
    void ridgeToTheEastShowsUpOnlyEastward();
    void missingTilesGiveAFlatHorizon();
};

void TestHorizonProfile::angleGeometryWithEarthBulge()
{
    // 100 m higher at 1 km: about 5.7°, less a hair of bulge.
    QVERIFY(std::fabs(terrainHorizonAngleDeg(500.0, 600.0, 1.0) - 5.71) < 0.05);
    // Level terrain sinks below the horizon with distance.
    QVERIFY(terrainHorizonAngleDeg(500.0, 500.0, 50.0) < 0.0);
    // At 50 km the 4/3-earth bulge is ~147 m: a 150 m step is just above flat.
    QVERIFY(terrainHorizonAngleDeg(500.0, 650.0, 50.0) > 0.0);
    QVERIFY(terrainHorizonAngleDeg(500.0, 640.0, 50.0) < 0.0);
    QCOMPARE(terrainHorizonAngleDeg(500.0, 900.0, 0.0), 0.0);
}

void TestHorizonProfile::ridgeToTheEastShowsUpOnlyEastward()
{
    SrtmTileLoader loader;
    // Origin at 47.5N 13.5E inside tile N47E013; the ridge column sits
    // ~0.1° (about 7.5 km) east of it.
    const int originCol = static_cast<int>(0.5 * (SrtmTileLoader::kSrtmSize - 1));
    const int ridgeCol = originCol + static_cast<int>(0.1 * (SrtmTileLoader::kSrtmSize - 1));
    loader.injectTileForTest(QStringLiteral("N47E013"), tileWithRidge(400, 1400, ridgeCol));
    const QVector<double> profile = computeHorizonProfile(loader, 47.5, 13.5, 410.0, 40.0);
    QCOMPARE(profile.size(), 360);
    // Due east: a 1000 m wall 7.5 km away is about 7.5°.
    QVERIFY2(profile[90] > 6.0 && profile[90] < 9.0, qPrintable(QString::number(profile[90])));
    // Due west and north: flat.
    QCOMPARE(profile[270], 0.0);
    QCOMPARE(profile[0], 0.0);
    // The wall is seen obliquely at 60° too, lower than head-on at 90°.
    QVERIFY(profile[60] > 0.0 && profile[60] < profile[90]);
}

void TestHorizonProfile::missingTilesGiveAFlatHorizon()
{
    SrtmTileLoader loader;
    // Nothing injected; the loader would try to download -- the
    // profile still comes back, all zero, never a guess.
    const QVector<double> profile = computeHorizonProfile(loader, 0.5, 0.5, 100.0, 5.0);
    QCOMPARE(profile.size(), 360);
    for (double angle : profile) {
        QCOMPARE(angle, 0.0);
    }
}

QTEST_GUILESS_MAIN(TestHorizonProfile)
#include "test_horizon_profile.moc"
