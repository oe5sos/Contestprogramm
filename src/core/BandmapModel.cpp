#include "core/BandmapModel.h"

#include "core/BandUtils.h"

#include <algorithm>

namespace Contestprogramm {

void BandmapModel::addSpot(const SpotCandidate& candidate)
{
    const QString call = candidate.callsign.trimmed().toUpper();
    if (call.isEmpty() || candidate.freqHz <= 0) {
        return;
    }
    BandmapSpot spot;
    spot.callsign = call;
    spot.grid = candidate.grid.trimmed().toUpper();
    spot.freqHz = candidate.freqHz;
    spot.timestampUtc = candidate.timestampUtc.isValid() ? candidate.timestampUtc : QDateTime::currentDateTimeUtc();
    spot.source = candidate.source;
    // A re-spot without a grid must not lose the grid an earlier one had.
    const auto existing = m_spots.constFind(call);
    if (existing != m_spots.constEnd() && spot.grid.isEmpty()) {
        spot.grid = existing->grid;
    }
    m_spots.insert(call, spot);
}

QVector<BandmapSpot> BandmapModel::spotsForBand(const QString& band, const QDateTime& nowUtc)
{
    const QDateTime cutoff = nowUtc.addSecs(-60 * qint64(m_maxAgeMinutes));
    QVector<BandmapSpot> result;
    for (auto it = m_spots.begin(); it != m_spots.end();) {
        if (it->timestampUtc < cutoff) {
            it = m_spots.erase(it);
            continue;
        }
        if (bandLabelForFrequencyHz(it->freqHz) == band) {
            result.append(*it);
        }
        ++it;
    }
    std::sort(result.begin(), result.end(), [](const BandmapSpot& a, const BandmapSpot& b) {
        if (a.freqHz != b.freqHz) {
            return a.freqHz < b.freqHz;
        }
        return a.callsign < b.callsign;
    });
    return result;
}

} // namespace Contestprogramm
