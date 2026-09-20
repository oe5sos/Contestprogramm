#include "core/terrain/HorizonProfile.h"

#include "core/Maidenhead.h"
#include "core/terrain/SrtmTileLoader.h"

#include <QtMath>

#include <algorithm>

namespace Contestprogramm {

namespace {

// 4/3 effective earth radius, in metres.
constexpr double kEffectiveEarthRadiusM = 4.0 / 3.0 * 6371000.0;

// Sample distances: 250 m steps to 10 km, 500 m to 40 km, 1 km to
// 100 km, 2 km beyond -- a ridge 3 km away is what makes the angle,
// a range 120 km out only ever adds a fraction of a degree.
QVector<double> sampleDistancesKm(double maxKm)
{
    QVector<double> out;
    for (double km = 0.25; km <= maxKm; ) {
        out.append(km);
        km += km < 10.0 ? 0.25 : km < 40.0 ? 0.5 : km < 100.0 ? 1.0 : 2.0;
    }
    return out;
}

} // namespace

double terrainHorizonAngleDeg(double eyeElevationM, double terrainElevationM, double distanceKm)
{
    const double distanceM = distanceKm * 1000.0;
    if (distanceM <= 0.0) {
        return 0.0;
    }
    const double bulgeM = distanceM * distanceM / (2.0 * kEffectiveEarthRadiusM);
    return qRadiansToDegrees(std::atan2(terrainElevationM - eyeElevationM - bulgeM, distanceM));
}

QVector<double> computeHorizonProfile(SrtmTileLoader& loader, double lat, double lon, double eyeElevationM,
                                      double maxDistanceKm)
{
    QVector<double> profile(360, 0.0);
    const QVector<double> distances = sampleDistancesKm(std::max(1.0, maxDistanceKm));
    for (int bearing = 0; bearing < 360; ++bearing) {
        double best = 0.0;
        for (double km : distances) {
            double sampleLat = 0.0;
            double sampleLon = 0.0;
            destinationPoint(lat, lon, static_cast<double>(bearing), km, sampleLat, sampleLon);
            loader.ensureTileAvailable(sampleLat, sampleLon);
            const auto elevation = loader.elevationAt(sampleLat, sampleLon);
            if (!elevation) {
                continue;
            }
            best = std::max(best, terrainHorizonAngleDeg(eyeElevationM, *elevation, km));
        }
        profile[bearing] = best;
    }
    return profile;
}

} // namespace Contestprogramm
