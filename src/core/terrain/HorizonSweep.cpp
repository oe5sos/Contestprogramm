#include "core/terrain/HorizonSweep.h"

#include "core/Maidenhead.h"
#include "core/terrain/SrtmTileLoader.h"

#include <QtGlobal>

#include <cmath>
#include <limits>

namespace Contestprogramm {

namespace {

// 4/3-effective-earth-radius approximation for standard atmospheric
// radio refraction -- metres, per this file's own task brief (kept in
// metres throughout, unlike LineOfSight.cpp's earthBulgeM(), which
// works in km because its two-point path profile is expressed in km
// distances; this sweep's samples are described in km too, but the
// angle formula itself reads more directly in metres, so the
// conversion happens once at the horizonElevationAngleDeg() call site
// below rather than threading km through the trig).
constexpr double kEffectiveEarthRadiusM = 6371000.0 * 4.0 / 3.0;

// A near-graze cutoff, not a standards-body constant -- see
// computeHorizonSweep()'s own doc comment in the header for the full
// reasoning on why -0.3deg specifically.
constexpr double kMarginalThresholdDeg = -0.3;

} // namespace

const QVector<double>& defaultHorizonSweepDistancesKm()
{
    static const QVector<double> distances
        = {2.0, 5.0, 10.0, 15.0, 20.0, 30.0, 40.0, 50.0, 65.0, 80.0, 100.0, 120.0, 150.0, 180.0};
    return distances;
}

double horizonElevationAngleDeg(double eyeElevM, double terrainElevM, double distanceM)
{
    // dropM: how much the effective-earth-radius bulge lowers the
    // visible horizon at this distance -- see LineOfSight.cpp's own
    // earthBulgeM() for the km-based sibling of this same physical
    // effect. Subtracted from the terrain's own height-above-eye
    // difference before taking atan2, exactly as this feature's task
    // brief specifies: a real, distant mountain must clear BOTH the
    // origin's own eye height AND this curvature drop to register as
    // rising above the horizontal.
    const double dropM = (distanceM * distanceM) / (2.0 * kEffectiveEarthRadiusM);
    const double angleRad = std::atan2((terrainElevM - eyeElevM) - dropM, distanceM);
    return qRadiansToDegrees(angleRad);
}

QVector<HorizonSector> computeHorizonSweep(SrtmTileLoader& loader, double originLat, double originLon,
                                            double originGroundElevationM, double eyeHeightAboveGroundM,
                                            int sectorCount, const QVector<double>& sampleDistancesKm)
{
    if (sectorCount < 1) {
        sectorCount = 36;
    }
    const double eyeElevM = originGroundElevationM + eyeHeightAboveGroundM;
    const double stepDeg = 360.0 / static_cast<double>(sectorCount);

    QVector<HorizonSector> sectors;
    sectors.reserve(sectorCount);

    for (int index = 0; index < sectorCount; ++index) {
        HorizonSector sector;
        sector.bearingDeg = static_cast<int>(std::lround(index * stepDeg)) % 360;

        bool everySampleResolved = true;
        double maxAngleDeg = -std::numeric_limits<double>::infinity();
        double obstructionDistanceKm = 0.0;
        double obstructionElevationM = 0.0;

        for (double distanceKm : sampleDistancesKm) {
            double lat = 0.0;
            double lon = 0.0;
            destinationPoint(originLat, originLon, static_cast<double>(sector.bearingDeg), distanceKm, lat, lon);

            // Requested unconditionally on every call, whether or not
            // this bearing ultimately resolves this time -- idempotent
            // (SrtmTileLoader::ensureTileAvailable()'s own doc comment)
            // and this is exactly how a still-Unknown bearing's tiles
            // ever end up loaded for a LATER call to find. See this
            // function's own header comment on the per-bearing (not
            // per-sweep) async gate.
            loader.ensureTileAvailable(lat, lon);
            const std::optional<double> elevationM = loader.elevationAt(lat, lon);
            if (!elevationM) {
                everySampleResolved = false;
                continue; // still request every remaining sample's tile below
            }

            const double angleDeg = horizonElevationAngleDeg(eyeElevM, *elevationM, distanceKm * 1000.0);
            if (angleDeg > maxAngleDeg) {
                maxAngleDeg = angleDeg;
                obstructionDistanceKm = distanceKm;
                obstructionElevationM = *elevationM;
            }
        }

        if (everySampleResolved) {
            sector.maxElevationAngleDeg = maxAngleDeg;
            if (maxAngleDeg >= 0.0) {
                sector.classification = LineOfSightClass::Blocked;
                sector.obstructionDistanceKm = obstructionDistanceKm;
                sector.obstructionElevationM = obstructionElevationM;
            } else if (maxAngleDeg > kMarginalThresholdDeg) {
                sector.classification = LineOfSightClass::Marginal;
                sector.obstructionDistanceKm = obstructionDistanceKm;
                sector.obstructionElevationM = obstructionElevationM;
            } else {
                sector.classification = LineOfSightClass::Clear;
                // obstructionDistanceKm/obstructionElevationM stay at
                // their struct defaults -- not meaningful for Clear,
                // see HorizonSector's own doc comment.
            }
        }
        // else: sector.classification stays LineOfSightClass::Unknown
        // (the struct default), maxElevationAngleDeg/obstruction* stay
        // at 0.0 -- a genuinely unresolved bearing, not a fabricated
        // "flat/open" reading (HAUSSTIL rule 7, same posture
        // LineOfSight.h's own Unknown value documents).

        sectors.append(sector);
    }
    return sectors;
}

} // namespace Contestprogramm
