#pragma once

#include "core/terrain/LineOfSight.h"

#include <QVector>

namespace Contestprogramm {

class SrtmTileLoader;

// Contestprogramm-original (2026-09-14), for the standalone site-
// scouting/comparison feature (ui/SiteComparisonDialog.h) -- see that
// class's own doc comment for the full motivating story. Structurally
// different from PathProfile.h's samplePathProfile(): that one samples
// a great-circle line between TWO known stations; this one samples N
// rays radiating OUTWARD from a single origin with nothing at the far
// end, because the whole point here is "what does the terrain around
// this one candidate site actually look like", not "is the path to
// this other specific station clear". TerrainDataManager::
// sectorSweep() looks superficially similar (also a 360-degree sweep
// around one origin) but is deliberately capped at a fixed 50km
// reference distance for a fast near-field contest-time safety check
// (see its own kSectorSweepDistanceKm comment) -- this sweep exists
// specifically to see FURTHER than that (the Alps from Jauerling, at
// 65-100km, is the concrete case that prompted this file), so it reuses
// none of that sweep's fixed-distance machinery.

// One bearing sector's real-terrain horizon result -- the "how open is
// the view in this direction" answer for a single origin, as opposed to
// LineOfSight.h's classification of one PATH between two stations.
// classification reuses LineOfSightClass rather than a parallel enum:
// its four values (Clear/Marginal/Blocked/Unknown) map cleanly onto
// this sweep's own three-way split-plus-Unknown (see
// computeHorizonSweep()'s own doc comment for the exact thresholds) and
// every existing caller (RotorWidget's dormant terrain-wash painter,
// MapWidget's rim) already knows how to colour these four values --
// reusing it means ui/HorizonCompassWidget.cpp can paint with the same
// two named colours (Style::kRedBorder for Blocked, Style::kAmberWarn
// for Marginal) those callers already established, rather than
// inventing a second, competing colour mapping for what is
// conceptually the same three-way judgement call.
struct HorizonSector {
    int bearingDeg = 0;
    LineOfSightClass classification = LineOfSightClass::Unknown;
    // The distance/elevation of whichever sampled point along this
    // bearing produced maxElevationAngleDeg -- i.e. the actual, real
    // limiting terrain feature in this direction, not just a yes/no
    // verdict. Only meaningful once classification is Marginal or
    // Blocked (a Clear or Unknown sector's "worst" sample is not a
    // limiting obstruction, and an Unknown sector has no fully-resolved
    // maximum at all) -- both fields stay at their default 0.0
    // otherwise.
    double obstructionDistanceKm = 0.0;
    double obstructionElevationM = 0.0;
    // The highest elevation angle (degrees, positive = above the local
    // horizontal, matching classifyElevationAngleDeg()'s own sign
    // convention below) found across every sampled distance along this
    // bearing -- meaningful whenever classification is not Unknown
    // (every sample was resolved), left at 0.0 while still Unknown.
    double maxElevationAngleDeg = 0.0;
};

// The distance-sample list computeHorizonSweep() uses when the caller
// doesn't supply its own: closer-spaced near the origin (a nearby ridge
// a few km away needs fine-grained sampling to catch at all) and wider-
// spaced far out (terrain shape does not need km-scale precision at
// 150km to tell "the Alps are there" from "they are not"). Reaches
// 180km specifically so a real long-range obstruction like the Alps
// seen from Jauerling (65-100km away) sits comfortably inside this
// list's own far end rather than right at its edge -- deliberately NOT
// capped anywhere near TerrainDataManager::kSectorSweepDistanceKm's own
// 50km, which is that class's own fast near-field check, not a limit
// this sweep inherits.
const QVector<double>& defaultHorizonSweepDistancesKm();

// Elevation angle (degrees, positive = above the local horizontal at
// the origin) from an origin at `eyeElevM` to a terrain point at
// `terrainElevM`, `distanceM` away, accounting for standard atmospheric
// radio refraction via the 4/3-effective-earth-radius approximation
// (the same "how much does the earth's curvature hide" family of
// formula LineOfSight.cpp's own earthBulgeM() uses for a two-ended
// path, applied here to a single ray from one origin instead). Public
// (not file-local like LineOfSight.cpp's earthBulgeM()) so
// test_horizon_sweep.cpp can assert this formula's own output directly,
// independent of computeHorizonSweep()'s tile-loading plumbing --
// see that test file's own comment on why the curvature term needs a
// dedicated regression case.
double horizonElevationAngleDeg(double eyeElevM, double terrainElevM, double distanceM);

// For ONE origin (lat/lon, ground elevation, and eye/antenna height
// above that ground -- all metres), computes a per-bearing horizon
// profile using real SRTM terrain via `loader`. Sweeps `sectorCount`
// evenly-spaced bearings (default 36, i.e. every 10 degrees, covering
// the full circle -- bearingDeg = index * (360/sectorCount) for each
// result entry, index 0 = 0deg/North exactly like TerrainDataManager::
// sectorSweep()'s own degree indexing) and, for each bearing, samples
// elevation at every distance in `sampleDistancesKm` (defaultHorizon
// SweepDistancesKm() unless the caller overrides it) via
// SrtmTileLoader::elevationAt(), requesting each point's tile first via
// ensureTileAvailable() exactly like PathProfile/TerrainDataManager
// already do for their own two-point paths.
//
// Async-tile-availability contract, per bearing rather than for the
// whole sweep at once (deliberately finer-grained than
// TerrainDataManager::recompute()'s own all-tiles-or-nothing gate,
// since a 180km ray can cross a dozen or more SRTM tiles and there is
// no reason a nearby bearing whose tiles are already cached should sit
// at Unknown just because a distant bearing's tiles are still
// downloading): every sample's tile is requested unconditionally on
// every call (ensureTileAvailable() is itself idempotent -- see its own
// doc comment), but a bearing's classification/obstruction/angle fields
// only become real once EVERY one of its own samples resolves to a
// known elevation; if even one is still unset (its tile hasn't loaded,
// or is a genuine SRTM void), that bearing's classification stays
// Unknown and its other fields stay at their struct defaults for THIS
// call -- calling again once more tiles have arrived (see
// SiteHorizonManager, which does exactly that) can then resolve it.
//
// Classification thresholds, chosen to mirror the Clear/Marginal/
// Blocked split this session already established conversationally for
// this exact feature: the bearing's own maximum elevation angle across
// all its samples (see horizonElevationAngleDeg() above) >= 0deg means
// real terrain rises above the origin's own eye-height horizontal
// somewhere along the ray -- a genuine physical obstruction, Blocked.
// Between 0deg and -0.3deg is treated as Marginal -- a near-graze, e.g.
// a nearby ridge or plateau close enough in height to the origin that
// it isn't a clean, comfortably-open view even though it doesn't
// technically rise above eye level. Below -0.3deg is Clear -- a real
// margin of open sky below the horizontal in every sampled direction.
// -0.3deg is not a standards-body constant; it is this feature's own
// deliberately-chosen "how much margin counts as a real graze" cutoff,
// documented here so a future reader can retune it without having to
// reverse-engineer why 0.3 specifically.
QVector<HorizonSector> computeHorizonSweep(SrtmTileLoader& loader, double originLat, double originLon,
                                            double originGroundElevationM, double eyeHeightAboveGroundM,
                                            int sectorCount = 36,
                                            const QVector<double>& sampleDistancesKm
                                            = defaultHorizonSweepDistancesKm());

} // namespace Contestprogramm
