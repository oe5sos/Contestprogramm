#include "core/terrain/TerrainDataManager.h"

#include "core/Maidenhead.h"
#include "core/terrain/PathProfile.h"
#include "core/terrain/SrtmTileLoader.h"

#include <QSet>

namespace Contestprogramm {

namespace {
// Coarse pass purely to discover which SRTM tiles a path crosses --
// deliberately much coarser than samplePathProfile()'s own default
// (200) since this loop only needs distinct TILE NAMES, not elevation
// accuracy; 50 points is plenty to catch every tile a realistic VHF/UHF
// contest path (a handful to a few hundred km) crosses without missing
// a narrow sliver near a tile boundary.
constexpr int kTileDiscoverySamples = 50;
} // namespace

TerrainDataManager::TerrainDataManager(QObject* parent)
    : QObject(parent)
    , m_loader(new SrtmTileLoader(this))
{
    connect(m_loader, &SrtmTileLoader::tileLoaded, this, &TerrainDataManager::recomputeAll);
    // A failed tile fetch still needs a re-check pass: recompute() will
    // find that tile still not loaded and correctly leave the affected
    // requests at Unknown (see its own doc comment) rather than hanging
    // forever with no re-evaluation at all.
    connect(m_loader, &SrtmTileLoader::tileLoadFailed, this, &TerrainDataManager::recomputeAll);
}

void TerrainDataManager::setOwnStation(const QString& ownGrid, double ownElevationM, double ownAntennaHeightM)
{
    m_ownGrid = ownGrid;
    m_ownElevationM = ownElevationM;
    m_ownAntennaHeightM = ownAntennaHeightM;
    recomputeAll();
}

QString TerrainDataManager::requestKey(const QString& otherGrid, double frequencyMhz)
{
    return otherGrid.toUpper() + QLatin1Char('@') + QString::number(frequencyMhz, 'f', 3);
}

LineOfSightClass TerrainDataManager::classification(const QString& otherGrid, double frequencyMhz,
                                                      double otherElevationM, double otherAntennaHeightM)
{
    const QString key = requestKey(otherGrid, frequencyMhz);
    auto it = m_requests.find(key);
    if (it != m_requests.end()) {
        return it.value().result;
    }

    Request request;
    request.otherGrid = otherGrid;
    request.frequencyMhz = frequencyMhz;
    request.otherElevationM = otherElevationM;
    request.otherAntennaHeightM = otherAntennaHeightM;
    it = m_requests.insert(key, request);

    recompute(otherGrid, frequencyMhz, it.value());
    return it.value().result;
}

void TerrainDataManager::recomputeAll()
{
    for (auto it = m_requests.begin(); it != m_requests.end(); ++it) {
        const LineOfSightClass before = it.value().result;
        recompute(it.value().otherGrid, it.value().frequencyMhz, it.value());
        if (it.value().result != before) {
            emit classificationChanged(it.value().otherGrid, it.value().frequencyMhz, it.value().result);
        }
    }
}

QVector<LineOfSightClass> TerrainDataManager::sectorSweep(double frequencyMhz)
{
    QVector<LineOfSightClass> sectors(360, LineOfSightClass::Unknown);
    if (!isValidGridSquare(m_ownGrid)) {
        return sectors;
    }

    double ownLat = 0.0;
    double ownLon = 0.0;
    calculateLatLonFromGridSquare(m_ownGrid, ownLat, ownLon);

    for (int degree = 0; degree < 360; ++degree) {
        double lat = 0.0;
        double lon = 0.0;
        destinationPoint(ownLat, ownLon, static_cast<double>(degree), kSectorSweepDistanceKm, lat, lon);
        const QString sectorGrid = gridSquareFromLatLon(lat, lon);
        sectors[degree] = classification(sectorGrid, frequencyMhz);
    }
    return sectors;
}

void TerrainDataManager::recompute(const QString& otherGrid, double frequencyMhz, Request& request)
{
    if (!isValidGridSquare(m_ownGrid) || !isValidGridSquare(otherGrid)) {
        return; // nothing sensible to compute -- leave result as-is (Unknown)
    }

    double lat1 = 0.0;
    double lon1 = 0.0;
    double lat2 = 0.0;
    double lon2 = 0.0;
    calculateLatLonFromGridSquare(m_ownGrid, lat1, lon1);
    calculateLatLonFromGridSquare(otherGrid, lat2, lon2);

    QSet<QString> neededTiles;
    for (int i = 0; i < kTileDiscoverySamples; ++i) {
        const double fraction = static_cast<double>(i) / static_cast<double>(kTileDiscoverySamples - 1);
        double lat = lat1;
        double lon = lon1;
        if (i == kTileDiscoverySamples - 1) {
            lat = lat2;
            lon = lon2;
        } else if (i > 0) {
            intermediatePointOnGreatCircle(lat1, lon1, lat2, lon2, fraction, lat, lon);
        }
        // Idempotent -- SrtmTileLoader itself no-ops a repeat request
        // for a tile that's already loaded or already in flight.
        m_loader->ensureTileAvailable(lat, lon);
        neededTiles.insert(SrtmTileLoader::tileNameForLatLon(lat, lon));
    }

    for (const QString& tile : std::as_const(neededTiles)) {
        if (!m_loader->isTileLoaded(tile)) {
            return; // still waiting on at least one tile
        }
    }

    const QVector<ElevationSample> profile = samplePathProfile(*m_loader, lat1, lon1, lat2, lon2);
    request.result = classifyLineOfSight(profile, m_ownElevationM, m_ownAntennaHeightM, request.otherElevationM,
                                          request.otherAntennaHeightM, frequencyMhz);
}

} // namespace Contestprogramm
