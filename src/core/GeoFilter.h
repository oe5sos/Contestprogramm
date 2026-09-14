#pragma once

#include "core/SpotCandidate.h"
#include "core/terrain/LineOfSight.h"

#include <QString>

namespace Contestprogramm {

class TerrainDataManager;

// Phase 1: radius-based reachability. Phase 2 (2026-09-12): optionally
// ALSO consults a TerrainDataManager for a real terrain line-of-sight
// classification -- see setTerrainDataManager(). Given the operator's
// own grid and a radius_km setting (plus, once wired up, terrain data),
// decides whether a SpotCandidate is "in range", and separately
// computes bearing for display.
class GeoFilter {
public:
    struct Result {
        bool inRange = true;      // see classify() -- a missing grid does not hide a candidate
        bool distanceKnown = false;
        double distanceKm = 0.0;
        double bearingDeg = 0.0;
        // Stays Unknown (the harmless default) unless
        // setTerrainDataManager() has been called AND the candidate's
        // own grid is valid -- see classify()'s own comment for exactly
        // how this feeds into `inRange`.
        LineOfSightClass terrain = LineOfSightClass::Unknown;
    };

    void setOwnGrid(const QString& grid);
    void setRadiusKm(double radiusKm);

    // Optional -- not set (the default, and Phase 1's own untouched
    // behaviour) means terrain is never consulted at all:
    // Result::terrain stays Unknown for every candidate and `inRange`
    // stays purely radius-based, exactly as before this Phase 2
    // addition. `manager` is non-owning; the caller (AppController)
    // keeps it alive for as long as this GeoFilter exists.
    // `frequencyMhz` is the operating band to classify against (first
    // Fresnel zone width is frequency-dependent, see LineOfSight.h) --
    // callers with per-band GeoFilter instances pass each one's own
    // band; a single shared GeoFilter would need re-calling this on
    // every band change instead.
    void setTerrainDataManager(TerrainDataManager* manager, double frequencyMhz);

    QString ownGrid() const { return m_ownGrid; }
    double radiusKm() const { return m_radiusKm; }

    // A missing/invalid grid -- either the candidate's or the
    // configured own grid -- yields distanceKnown == false and
    // inRange == true: a candidate Contestprogramm cannot place should
    // not be silently suppressed, mirroring this same class's own
    // terrain precedent below ("not clear, not blocked --
    // unclassifiable, but still shown, not hidden").
    //
    // Terrain (once setTerrainDataManager() is wired up): only a
    // CONFIRMED LineOfSightClass::Blocked candidate is hard-excluded
    // (inRange forced false, even if within radius) -- Marginal, Clear,
    // and Unknown all leave `inRange` exactly as the radius check alone
    // decided it. Per the plan's own adaptive-filtering rule: "eine
    // Station... mit Terrain-Klassifikation 'Verdeckt' wird im Chat-Feed
    // gar nicht angezeigt... Der bereits bestehende Grundsatz
    // 'unbekanntes Grid wird nicht versteckt' bleibt davon unberührt".
    Result classify(const SpotCandidate& candidate) const;

private:
    QString m_ownGrid;
    double m_radiusKm = 300.0;
    TerrainDataManager* m_terrainDataManager = nullptr;
    double m_terrainFrequencyMhz = 0.0;
};

} // namespace Contestprogramm
