#include "core/ChatVisibilityPolicy.h"

#include <algorithm>
#include <cmath>

namespace Contestprogramm {

namespace ChatVisibilityPolicy {

Tempo tempoForRate(double qsoPerTenMinutes)
{
    if (qsoPerTenMinutes < kQuietRateThreshold) {
        return Tempo::Quiet;
    }
    if (qsoPerTenMinutes >= kBusyRateThreshold) {
        return Tempo::Busy;
    }
    return Tempo::Moderate;
}

QVector<bool> visibilityMask(const QVector<double>& scores, Tempo tempo)
{
    QVector<bool> mask(scores.size(), false);
    if (scores.isEmpty()) {
        return mask;
    }

    if (tempo != Tempo::Busy) {
        const double minScore = (tempo == Tempo::Quiet) ? kQuietMinScore : kModerateMinScore;
        for (int i = 0; i < scores.size(); ++i) {
            mask[i] = scores.at(i) >= minScore;
        }
        return mask;
    }

    // Busy: keep only the top kBusyTopFraction of rows by score. At
    // least one row survives even when the fraction rounds down to
    // zero, so a lone candidate is never silently dropped to nothing.
    QVector<double> sortedDesc = scores;
    std::sort(sortedDesc.begin(), sortedDesc.end(), std::greater<double>());

    int keepCount = static_cast<int>(std::ceil(scores.size() * kBusyTopFraction));
    keepCount = std::clamp(keepCount, 1, static_cast<int>(scores.size()));
    const double cutoffScore = sortedDesc.at(keepCount - 1);

    for (int i = 0; i < scores.size(); ++i) {
        mask[i] = scores.at(i) >= cutoffScore;
    }
    return mask;
}

} // namespace ChatVisibilityPolicy

} // namespace Contestprogramm
