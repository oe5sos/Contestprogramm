#pragma once

#include "core/terrain/LineOfSight.h"

#include <QMap>
#include <QObject>
#include <QString>

namespace Contestprogramm {

class SrtmTileLoader;

// Orchestrates the full terrain line-of-sight pipeline (SrtmTileLoader
// -> samplePathProfile() -> classifyLineOfSight()) as one async, cached
// operation per (other-grid, frequency) pair -- see the plan's own
// "Cache pro Grid-Paar" note. Owns one SrtmTileLoader; a request whose
// path crosses tiles not yet loaded triggers their fetch (network, or
// the local disk cache -- see SrtmTileLoader's own doc comment) and
// re-evaluates once they arrive, without the caller needing to manage
// tile state itself. Runs on the GUI thread -- SRTM tile parsing/
// bilinear lookups and a 200-sample path profile are cheap enough
// (microseconds to low milliseconds) that a QThreadPool worker was not
// worth the added complexity for a request volume of "however many
// spot/chat candidates are on screen", not "every degree of a live
// waterfall".
class TerrainDataManager : public QObject {
    Q_OBJECT

public:
    explicit TerrainDataManager(QObject* parent = nullptr);

    // Own station: grid square + elevation/antenna height (metres,
    // matching ContestSettings::ownElevationM/antennaHeightM). Re-runs
    // every already-requested pair's classification against the new
    // own-station values (an operator correcting their own elevation
    // mid-contest should not leave stale classifications behind) --
    // does NOT re-fetch any already-loaded SRTM tiles, only re-runs the
    // cheap profile+classification step over them.
    void setOwnStation(const QString& ownGrid, double ownElevationM, double ownAntennaHeightM);

    // Requests (or returns the cached) classification for the path to
    // `otherGrid` at `frequencyMhz`, assuming `otherElevationM`/
    // `otherAntennaHeightM` for the far end (both default to 0 -- a
    // reasonable, deliberately conservative assumption when the other
    // station's real elevation/mast height is unknown: ground level, no
    // mast, which can only make Blocked/Marginal MORE likely, never
    // hide a real obstruction by assuming a generous height nobody
    // actually confirmed). The (grid, frequency) pair is the cache key
    // -- a second call for the same pair with different elevation/
    // antenna values returns the FIRST call's cached result rather than
    // recomputing; this manager does not support asking the same
    // candidate about two different assumed antenna setups at once.
    //
    // Returns the immediately-known result -- LineOfSightClass::Unknown
    // the first time a given pair is asked about (unless its SRTM tiles
    // already happen to be in the local disk cache from a previous
    // session, in which case it can resolve synchronously), updating
    // asynchronously via classificationChanged() once the needed tiles
    // finish loading and the real classification can be computed.
    LineOfSightClass classification(const QString& otherGrid, double frequencyMhz, double otherElevationM = 0.0,
                                     double otherAntennaHeightM = 0.0);

    // A full 360-degree sweep around the own station at a fixed
    // reference distance (kSectorSweepDistanceKm, 50km) -- one
    // classification per integer degree (index 0 = 0deg/North, ...,
    // index 359 = 359deg), for painting a rotor compass ring's terrain
    // sectors (the plan's own "farbiges Segment auf dem Kompassring").
    // There is no real "target" grid for a bare bearing, so each sector
    // synthesizes one via Maidenhead::destinationPoint() at the fixed
    // reference distance -- this answers "is there already-known-
    // blocking terrain nearby in this general direction", NOT "the path
    // to a station at any particular distance in this direction is
    // guaranteed clear". Each sector shares classification()'s own
    // (grid, frequency) request cache, so repeated sweeps (e.g. on
    // every classificationChanged() as tiles arrive) are cheap once the
    // relevant tiles are loaded. Empty own grid returns 360 Unknown
    // entries (same "nothing sensible to compute" posture as
    // classify()/recompute() elsewhere in this class).
    QVector<LineOfSightClass> sectorSweep(double frequencyMhz);

    // Reference distance for sectorSweep() -- public so a caller (e.g.
    // RotorWidget's own tooltip/label) can state it alongside the
    // sectors themselves ("kein garantiert freier Pfad über
    // 50km hinaus").
    static constexpr double kSectorSweepDistanceKm = 50.0;

    // Test-only access to the owned SrtmTileLoader, so a test can
    // inject synthetic tile data (SrtmTileLoader::injectTileForTest())
    // before calling classification(), exercising the real tile-
    // discovery/caching/classification pipeline with no network access
    // at all. Same "*ForTest" naming convention On4kstClient::
    // parseDxSpotLineForTest() already established in this codebase.
    SrtmTileLoader& tileLoaderForTest() { return *m_loader; }

signals:
    // Emitted whenever a previously-Unknown (or since-changed, e.g.
    // after a setOwnStation() call) pair's classification is
    // (re)computed to something different.
    void classificationChanged(const QString& otherGrid, double frequencyMhz, LineOfSightClass result);

private:
    struct Request {
        QString otherGrid;
        double frequencyMhz = 0.0;
        double otherElevationM = 0.0;
        double otherAntennaHeightM = 0.0;
        LineOfSightClass result = LineOfSightClass::Unknown;
    };

    void recomputeAll();
    // Attempts to resolve `request` right now -- ensures every SRTM
    // tile the great-circle path to `otherGrid` crosses is at least
    // requested (see SrtmTileLoader::ensureTileAvailable()), and only
    // actually computes+stores a real classification once ALL of them
    // are loaded; otherwise leaves `request.result` exactly as it was
    // (Unknown on a first call, or a still-valid previous real result
    // on a setOwnStation()-triggered re-check that happens to still be
    // waiting on a newly-needed tile).
    void recompute(const QString& otherGrid, double frequencyMhz, Request& request);
    static QString requestKey(const QString& otherGrid, double frequencyMhz);

    SrtmTileLoader* m_loader;
    QString m_ownGrid;
    double m_ownElevationM = 0.0;
    double m_ownAntennaHeightM = 0.0;
    QMap<QString, Request> m_requests; // keyed by requestKey()
};

} // namespace Contestprogramm
