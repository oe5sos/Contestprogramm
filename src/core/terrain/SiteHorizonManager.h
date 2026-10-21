#pragma once

#include "core/terrain/HorizonSweep.h"

#include <QMap>
#include <QObject>
#include <QString>

namespace Contestprogramm {

class SrtmTileLoader;

// Orchestrates computeHorizonSweep() as one async, cached operation per
// origin -- Contestprogramm-original (2026-09-14), the same "own an
// SrtmTileLoader, cache one request per key, re-run every still-
// unresolved cached request whenever a tile finishes loading" shape
// TerrainDataManager already established for its own two-point path
// classifications, applied here to this feature's single-origin sweeps
// instead. Kept as its own class (mirroring how TerrainDataManager
// itself sits in a separate file from PathProfile/LineOfSight's plain
// functions) rather than folded into HorizonSweep.h, since this one
// owns Qt object identity/signals and a QNetworkAccessManager-owning
// SrtmTileLoader child, while HorizonSweep.h/.cpp stays plain,
// dependency-light geometry+trig, freely callable from a test with no
// QObject machinery at all (see test_horizon_sweep.cpp).
//
// Deliberately owns its OWN SrtmTileLoader rather than sharing
// AppController::terrainDataManager()'s instance: TerrainDataManager
// exposes no general-purpose accessor to its own loader (only
// tileLoaderForTest(), explicitly a test-only seam per that method's
// own doc comment -- reusing a test-only accessor from real UI code
// would be exactly backwards). A second loader is a harmless,
// deliberate choice, not a missed reuse opportunity: both instances
// read/write the SAME on-disk tile cache (QStandardPaths::
// AppDataLocation + "/srtm/", keyed by applicationName/
// organizationName, not per-instance -- see SrtmTileLoader.h's own doc
// comment), so a tile the live contest terrain feature already fetched
// is immediately available here too, and vice versa, with no explicit
// sharing code needed at all. This is the exact same reasoning
// SettingsDialog::m_gpsElevationLoader already documents for its own
// independent loader instance.
class SiteHorizonManager : public QObject {
    Q_OBJECT

public:
    explicit SiteHorizonManager(QObject* parent = nullptr);

    // Requests (or returns the cached) horizon sweep for the site at
    // (originLat, originLon), with the given ground elevation and eye/
    // antenna height above it (both metres). The cache key is the
    // origin lat/lon rounded to 4 decimal places (~11m at this
    // latitude -- see originKey()'s own comment on why that precision
    // is more than enough); a second call for a coordinate that rounds
    // to the SAME key with different elevation/eye-height values
    // returns the FIRST call's cached sweep rather than recomputing,
    // exactly the same "the (grid, frequency) pair is the cache key"
    // posture TerrainDataManager::classification() already documents
    // for its own request cache -- this manager does not support
    // asking about the same site with two different assumed heights at
    // once.
    //
    // Returns the immediately-known result -- every bearing Unknown the
    // first time a given origin is asked about (unless every needed
    // tile happens to already be in the local disk cache, in which case
    // it can resolve synchronously), updating asynchronously via
    // sweepChanged() once outstanding tiles finish loading and more (or
    // all) bearings can be classified for real. A bearing can also stay
    // permanently Unknown if the origin sits over a genuine SRTM void
    // in every sampled direction -- rare, but a real possibility this
    // manager does not mask.
    QVector<HorizonSector> sweep(double originLat, double originLon, double originGroundElevationM,
                                  double eyeHeightAboveGroundM);

    // Test-only access to the owned SrtmTileLoader, so a test can
    // inject synthetic tile data before calling sweep() -- same
    // "*ForTest" convention TerrainDataManager::tileLoaderForTest()
    // already established for exactly this purpose.
    SrtmTileLoader& tileLoaderForTest() { return *m_loader; }

signals:
    // Emitted once per cached request that still had at least one
    // Unknown bearing before this recompute pass -- NOT necessarily
    // meaning that request's sweep actually changed (a tile fetch that
    // fails, or that lands in an area still short of covering every
    // remaining bearing's samples, can leave the result identical) --
    // same "re-check, don't guarantee a real change" posture
    // TerrainDataManager::recomputeAll() takes internally before
    // deciding whether to emit its own classificationChanged(), except
    // this manager emits eagerly rather than diffing the result first
    // (a full 36-entry HorizonSector vector has no cheap equality check
    // worth writing solely to suppress an occasional harmless extra
    // repaint -- SiteComparisonDialog's own refreshSlot() re-painting
    // against unchanged data is not visibly different from not
    // repainting at all).
    void sweepChanged(double originLat, double originLon);

private:
    struct Request {
        double originLat = 0.0;
        double originLon = 0.0;
        double originGroundElevationM = 0.0;
        double eyeHeightAboveGroundM = 0.0;
        QVector<HorizonSector> result;
    };

    // Re-runs computeHorizonSweep() for every cached request that still
    // has at least one Unknown bearing -- a request whose sweep already
    // fully resolved is skipped entirely (nothing a newly-loaded tile
    // could still change about it), unlike TerrainDataManager::
    // recomputeAll(), which always re-runs everything because its own
    // recompute() is cheap enough (one path, one classification) not to
    // bother with this optimization; a 36-bearing x 14-sample sweep is
    // enough heavier work that skipping fully-resolved origins is worth
    // doing here.
    void recomputeAll();
    // Cache key: both coordinates rounded to 4 decimal places (roughly
    // 11m of latitude at these latitudes -- see calculateDistanceKm()'s
    // own scale) and joined with a comma. Far tighter than this
    // feature's own SRTM1 sample spacing (~30m) needs to be to treat
    // two practically-identical origins (e.g. the same KnownSite picked
    // twice, or a manual coordinate typed to more decimal places than
    // GPS itself is accurate to) as the same cached sweep, while still
    // keeping two genuinely different nearby sites (e.g. "Grünberg
    // Gasthaus" and "Grünberg, 80m östlich vom Haus" -- both real
    // entries in KnownSites.h, a few dozen metres apart) as separate
    // requests.
    static QString originKey(double lat, double lon);

    SrtmTileLoader* m_loader;
    QMap<QString, Request> m_requests; // keyed by originKey()
};

} // namespace Contestprogramm
