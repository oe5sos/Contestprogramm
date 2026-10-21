#include "core/terrain/SiteHorizonManager.h"

#include "core/terrain/SrtmTileLoader.h"

#include <algorithm>

namespace Contestprogramm {

SiteHorizonManager::SiteHorizonManager(QObject* parent)
    : QObject(parent)
    , m_loader(new SrtmTileLoader(this))
{
    connect(m_loader, &SrtmTileLoader::tileLoaded, this, &SiteHorizonManager::recomputeAll);
    // A failed fetch still needs a re-check pass: recomputeAll() will
    // find that tile still not loaded and correctly leave the affected
    // bearings at Unknown (see computeHorizonSweep()'s own doc comment)
    // rather than hanging forever with no re-evaluation at all -- same
    // reasoning TerrainDataManager's constructor documents for its own
    // identical connection.
    connect(m_loader, &SrtmTileLoader::tileLoadFailed, this, &SiteHorizonManager::recomputeAll);
}

QString SiteHorizonManager::originKey(double lat, double lon)
{
    return QString::number(lat, 'f', 4) + QLatin1Char(',') + QString::number(lon, 'f', 4);
}

QVector<HorizonSector> SiteHorizonManager::sweep(double originLat, double originLon,
                                                  double originGroundElevationM, double eyeHeightAboveGroundM)
{
    const QString key = originKey(originLat, originLon);
    auto it = m_requests.find(key);
    if (it != m_requests.end()) {
        return it.value().result;
    }

    Request request;
    request.originLat = originLat;
    request.originLon = originLon;
    request.originGroundElevationM = originGroundElevationM;
    request.eyeHeightAboveGroundM = eyeHeightAboveGroundM;
    it = m_requests.insert(key, request);

    it.value().result = computeHorizonSweep(*m_loader, originLat, originLon, originGroundElevationM,
                                              eyeHeightAboveGroundM);
    return it.value().result;
}

void SiteHorizonManager::recomputeAll()
{
    for (auto it = m_requests.begin(); it != m_requests.end(); ++it) {
        Request& request = it.value();
        const bool hadUnknown = request.result.isEmpty()
            || std::any_of(request.result.cbegin(), request.result.cend(), [](const HorizonSector& sector) {
                   return sector.classification == LineOfSightClass::Unknown;
               });
        if (!hadUnknown) {
            continue; // fully resolved already -- no newly-loaded tile can change this sweep further
        }
        request.result = computeHorizonSweep(*m_loader, request.originLat, request.originLon,
                                              request.originGroundElevationM, request.eyeHeightAboveGroundM);
        emit sweepChanged(request.originLat, request.originLon);
    }
}

} // namespace Contestprogramm
