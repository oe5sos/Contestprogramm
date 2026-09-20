#pragma once

#include <QVector>

namespace Contestprogramm {

class SrtmTileLoader;

// The terrain horizon seen from the antenna: for every whole degree of
// bearing, the highest elevation angle any terrain sample along that
// bearing subtends (0 = flat to the effective horizon, 8 = a mountain
// wall close by). MapWidget draws it as the radar's dark rim and as the
// unrolled skyline under the map -- "which stations sit behind the
// Traunstein" at a glance. Computed from the SRTM tiles the app already
// caches for TerrainDataManager's line-of-sight work; a bearing whose
// samples all fall on missing tiles reports 0 (flat), never a guess.
//
// Sampling is dense close in (where a nearby ridge makes the largest
// angle) and coarser far out; 4/3 effective earth radius, the same
// convention LineOfSight.cpp uses for its bulge.
constexpr double kHorizonProfileMaxKm = 160.0;

double terrainHorizonAngleDeg(double eyeElevationM, double terrainElevationM, double distanceKm);

QVector<double> computeHorizonProfile(SrtmTileLoader& loader, double lat, double lon, double eyeElevationM,
                                      double maxDistanceKm = kHorizonProfileMaxKm);

} // namespace Contestprogramm
