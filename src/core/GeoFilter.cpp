#include "core/GeoFilter.h"

#include "core/Maidenhead.h"
#include "core/terrain/TerrainDataManager.h"

namespace Contestprogramm {

void GeoFilter::setOwnGrid(const QString& grid)
{
    m_ownGrid = grid;
}

void GeoFilter::setRadiusKm(double radiusKm)
{
    m_radiusKm = radiusKm;
}

void GeoFilter::setTerrainDataManager(TerrainDataManager* manager, double frequencyMhz)
{
    m_terrainDataManager = manager;
    m_terrainFrequencyMhz = frequencyMhz;
}

GeoFilter::Result GeoFilter::classify(const SpotCandidate& candidate) const
{
    Result result;
    if (!isValidGridSquare(m_ownGrid) || !isValidGridSquare(candidate.grid)) {
        return result; // distanceKnown == false, inRange == true (default)
    }
    result.distanceKnown = true;
    result.distanceKm = calculateDistanceKm(m_ownGrid, candidate.grid);
    result.bearingDeg = calculateBearingInDegrees(m_ownGrid, candidate.grid);
    result.inRange = result.distanceKm <= m_radiusKm;

    if (m_terrainDataManager) {
        result.terrain = m_terrainDataManager->classification(candidate.grid, m_terrainFrequencyMhz);
        if (result.terrain == LineOfSightClass::Blocked) {
            // Hard exclude -- Marginal/Clear/Unknown all leave `inRange`
            // exactly as the radius check alone already decided it (see
            // this method's own doc comment in the header).
            result.inRange = false;
        }
    }
    return result;
}

} // namespace Contestprogramm
