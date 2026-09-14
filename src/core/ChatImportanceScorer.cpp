#include "core/ChatImportanceScorer.h"

#include <algorithm>

namespace Contestprogramm {

namespace ChatImportanceScorer {

double score(bool isDupe, bool isNeededMultiplier, bool recentPropagationNearby, const QDateTime& timestampUtc,
             const QDateTime& nowUtc)
{
    if (isDupe) {
        return kDupeScore;
    }

    double result = kBaseScore;
    if (isNeededMultiplier) {
        result += kMultiplierBoost;
    }
    if (recentPropagationNearby) {
        result += kPropagationBoost;
    }

    if (timestampUtc.isValid() && nowUtc.isValid()) {
        const qint64 ageSeconds = std::max<qint64>(0, timestampUtc.secsTo(nowUtc));
        const double fraction = 1.0 - std::min<double>(1.0, static_cast<double>(ageSeconds) / kRecencyHorizonSeconds);
        result += kMaxRecencyBonus * fraction;
    } else {
        // No usable timestamp -- treat as brand new rather than
        // penalizing a candidate for missing metadata.
        result += kMaxRecencyBonus;
    }

    return result;
}

} // namespace ChatImportanceScorer

} // namespace Contestprogramm
