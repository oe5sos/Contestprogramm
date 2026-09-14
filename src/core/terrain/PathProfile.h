#pragma once

#include <QVector>

#include <optional>

namespace Contestprogramm {

class SrtmTileLoader;

// One sampled point along a great-circle path between two stations.
struct ElevationSample {
    double distanceKm = 0.0;
    // Unknown (not a fabricated 0m) when the covering SRTM tile isn't
    // loaded yet (see SrtmTileLoader::ensureTileAvailable()) or every
    // one of its four nearest samples is a genuine SRTM void -- HAUSSTIL
    // rule 7, "Unbekannt ist ein Strich, keine Null", applies here just
    // as much as anywhere else this codebase shows a QSO/exchange value.
    std::optional<double> elevationM;
};

// Samples elevation at `sampleCount` evenly-spaced points (inclusive of
// both endpoints) along the great-circle path between two lat/lon
// points, using `loader`. Distance is real, computed once via
// Maidenhead::calculateDistanceKmBetween() and split evenly across the
// samples -- not re-derived per sample from possibly-imprecise lat/lon
// deltas. `loader` must already have every tile the path crosses loaded
// for a fully-known profile (see SrtmTileLoader::ensureTileAvailable());
// a sample whose tile isn't loaded yet comes back with
// ElevationSample::elevationM unset, which LineOfSight::classify()
// treats as Unknown rather than a fabricated flat/clear reading.
//
// 200 samples is the default -- fine resolution for the short end
// (metre-scale spacing over a few km) without excessive tile churn at
// the long end (500km / 200 = 2.5km spacing, well under typical terrain
// feature size); a caller with a specific accuracy need can override it.
QVector<ElevationSample> samplePathProfile(const SrtmTileLoader& loader, double lat1, double lon1, double lat2,
                                            double lon2, int sampleCount = 200);

} // namespace Contestprogramm
