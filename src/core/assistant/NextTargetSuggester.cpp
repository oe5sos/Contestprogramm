#include "core/assistant/NextTargetSuggester.h"

namespace Contestprogramm {
namespace NextTargetSuggester {

std::optional<Suggestion> suggest(const QVector<Candidate>& candidates)
{
    if (candidates.isEmpty()) {
        return std::nullopt;
    }

    int bestIndex = 0;
    for (int i = 1; i < candidates.size(); ++i) {
        if (candidates.at(i).score > candidates.at(bestIndex).score) {
            bestIndex = i;
        }
    }

    const Candidate& best = candidates.at(bestIndex);
    Suggestion result;
    result.candidate = best.candidate;
    result.score = best.score;
    result.geo = best.geo;
    return result;
}

} // namespace NextTargetSuggester
} // namespace Contestprogramm
