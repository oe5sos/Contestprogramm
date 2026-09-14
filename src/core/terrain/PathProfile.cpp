#include "core/terrain/PathProfile.h"

#include "core/Maidenhead.h"
#include "core/terrain/SrtmTileLoader.h"

namespace Contestprogramm {

QVector<ElevationSample> samplePathProfile(const SrtmTileLoader& loader, double lat1, double lon1, double lat2,
                                            double lon2, int sampleCount)
{
    if (sampleCount < 2) {
        sampleCount = 2;
    }
    const double totalDistanceKm = calculateDistanceKmBetween(lat1, lon1, lat2, lon2);

    QVector<ElevationSample> result;
    result.reserve(sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        const double fraction = static_cast<double>(i) / static_cast<double>(sampleCount - 1);

        // Endpoints use the caller's own exact coordinates rather than
        // a fraction=0/1 round trip through
        // intermediatePointOnGreatCircle() -- avoids that function's
        // own "coincident points" edge case ever mattering for the one
        // pair of samples that must always be exact.
        double lat = lat1;
        double lon = lon1;
        if (i == sampleCount - 1) {
            lat = lat2;
            lon = lon2;
        } else if (i > 0) {
            intermediatePointOnGreatCircle(lat1, lon1, lat2, lon2, fraction, lat, lon);
        }

        ElevationSample sample;
        sample.distanceKm = totalDistanceKm * fraction;
        sample.elevationM = loader.elevationAt(lat, lon);
        result.append(sample);
    }
    return result;
}

} // namespace Contestprogramm
