#pragma once

#include "core/terrain/PathProfile.h"

namespace Contestprogramm {

// Terrain line-of-sight classification for one path between two
// stations -- Phase 2 of the plan's GeoFilter design (radius-only in
// Phase 1). See GeoFilter.h's own doc comment for where this plugs in.
enum class LineOfSightClass {
    // Full first-Fresnel-zone clearance (>=60%, the standard VHF/UHF
    // link-planning rule of thumb) at every sampled point along the
    // path -- a genuinely good path, not just "no obstruction found".
    Clear,
    // The direct line clears the terrain everywhere, but dips below
    // 60% first-Fresnel-zone clearance somewhere along the path --
    // workable, especially with tropo enhancement, but not a sure
    // thing the way Clear is.
    Marginal,
    // Terrain (plus the standard 4/3-effective-earth-radius bulge)
    // actually exceeds the direct line height somewhere along the
    // path -- a real, physical obstruction, not a Fresnel-clearance
    // nicety.
    Blocked,
    // Not the same as Blocked -- deliberately. At least one sampled
    // point's elevation is unknown (its SRTM tile hasn't been fetched
    // yet, or is a genuine void), so Clear/Marginal/Blocked cannot be
    // confidently claimed. A candidate this recent for a Phase-1
    // radius-only build was never hidden just because its exact
    // grid was unknown (see GeoFilter's own existing behaviour) --
    // this preserves that same "don't punish missing data" posture
    // for terrain, and callers (GeoFilter, RotorWidget) must treat it
    // as "show, don't hard-exclude", the same as they already do for
    // an out-of-radius-but-unconfirmed candidate.
    Unknown,
};

// Classifies one path from `profile` (see samplePathProfile()) plus
// each end's own ground elevation and antenna height above it (both
// metres -- matches ContestSettings::ownElevationM/antennaHeightM's own
// units) and the operating frequency in MHz (first-Fresnel-zone radius
// is frequency-dependent -- lower bands need more clearance for the
// same path, so 144 MHz and 432 MHz genuinely classify differently over
// the same terrain).
//
// Priority when samples disagree: a confirmed physical obstruction
// (Blocked) always wins, even over missing data elsewhere on the path --
// one certain blockage is still certain regardless of what else is
// unknown. Only once no Blocked point is found does a missing sample
// anywhere downgrade the result to Unknown; Marginal/Clear are only
// ever returned when EVERY sample on the path is actually known.
LineOfSightClass classifyLineOfSight(const QVector<ElevationSample>& profile, double ownElevationM,
                                      double ownAntennaHeightM, double otherElevationM, double otherAntennaHeightM,
                                      double frequencyMhz);

} // namespace Contestprogramm
