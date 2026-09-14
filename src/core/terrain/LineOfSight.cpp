#include "core/terrain/LineOfSight.h"

#include <cmath>

namespace Contestprogramm {

namespace {

// Standard 4/3 effective-earth-radius model for radio path profiles.
// The classic d1*d2/(2*R) "bulge" formula (km in, km out) gives how
// much the earth's true curvature would hide of a straight line at a
// point d1 from one end / d2 from the other -- applied here as height
// ADDED to the terrain sample for comparison against the straight line,
// which is equivalent to (and simpler than) subtracting it from the
// line itself.
constexpr double kEffectiveEarthRadiusKm = 6371.0 * 4.0 / 3.0;

double earthBulgeM(double d1Km, double d2Km)
{
    return (d1Km * d2Km) / (2.0 * kEffectiveEarthRadiusKm) * 1000.0; // km -> m
}

// First Fresnel zone radius, metres -- 17.3*sqrt(d1*d2/(f*D)), the
// standard VHF/UHF link-planning formula (d1/d2/D in km, f in GHz).
// Genuinely frequency-dependent: 144 MHz needs a wider clear zone than
// 432 MHz over the identical physical path.
double firstFresnelRadiusM(double d1Km, double d2Km, double totalKm, double frequencyMhz)
{
    if (totalKm <= 0.0 || frequencyMhz <= 0.0) {
        return 0.0;
    }
    const double frequencyGhz = frequencyMhz / 1000.0;
    return 17.3 * std::sqrt((d1Km * d2Km) / (frequencyGhz * totalKm));
}

} // namespace

LineOfSightClass classifyLineOfSight(const QVector<ElevationSample>& profile, double ownElevationM,
                                      double ownAntennaHeightM, double otherElevationM, double otherAntennaHeightM,
                                      double frequencyMhz)
{
    if (profile.size() < 2) {
        return LineOfSightClass::Unknown;
    }

    const double totalKm = profile.last().distanceKm;
    const double startHeightM = ownElevationM + ownAntennaHeightM;
    const double endHeightM = otherElevationM + otherAntennaHeightM;

    bool anyUnknown = false;
    bool anyMarginal = false;

    for (const ElevationSample& sample : profile) {
        if (!sample.elevationM) {
            anyUnknown = true;
            continue;
        }

        const double d1 = sample.distanceKm;
        const double d2 = totalKm - d1;
        const double fraction = totalKm > 0.0 ? (d1 / totalKm) : 0.0;
        const double lineHeightM = startHeightM + (endHeightM - startHeightM) * fraction;
        const double obstructionHeightM = *sample.elevationM + earthBulgeM(d1, d2);
        const double clearanceM = lineHeightM - obstructionHeightM;

        if (clearanceM < 0.0) {
            // A real, physical obstruction -- confident regardless of
            // what else is unknown on the path (see the header's own
            // doc comment on this priority).
            return LineOfSightClass::Blocked;
        }

        const double fresnelRadiusM = firstFresnelRadiusM(d1, d2, totalKm, frequencyMhz);
        if (clearanceM < 0.6 * fresnelRadiusM) {
            anyMarginal = true;
        }
    }

    if (anyUnknown) {
        return LineOfSightClass::Unknown;
    }
    return anyMarginal ? LineOfSightClass::Marginal : LineOfSightClass::Clear;
}

} // namespace Contestprogramm
