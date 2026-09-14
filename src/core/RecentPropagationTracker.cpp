#include "core/RecentPropagationTracker.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace Contestprogramm {

void RecentPropagationTracker::recordQso(const QString& band, double bearingDeg, const QDateTime& whenUtc)
{
    Entry entry;
    entry.band = band;
    entry.bearingDeg = bearingDeg;
    entry.whenUtc = whenUtc;
    m_entries.append(entry);
    pruneExpired(whenUtc);
}

bool RecentPropagationTracker::hasRecentOpeningNear(const QString& band, double bearingDeg,
                                                     const QDateTime& nowUtc) const
{
    pruneExpired(nowUtc);
    for (const Entry& entry : std::as_const(m_entries)) {
        if (entry.band.compare(band, Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (angularDifferenceDeg(entry.bearingDeg, bearingDeg) <= kBearingToleranceDeg) {
            return true;
        }
    }
    return false;
}

double RecentPropagationTracker::angularDifferenceDeg(double a, double b)
{
    const double diff = std::fmod(std::abs(a - b), 360.0);
    return diff > 180.0 ? 360.0 - diff : diff;
}

void RecentPropagationTracker::pruneExpired(const QDateTime& nowUtc) const
{
    if (!nowUtc.isValid()) {
        return;
    }
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                                    [&](const Entry& entry) {
                                        return !entry.whenUtc.isValid()
                                            || entry.whenUtc.secsTo(nowUtc) > kWindowSeconds;
                                    }),
                     m_entries.end());
}

} // namespace Contestprogramm
