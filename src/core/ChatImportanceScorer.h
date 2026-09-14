#pragma once

#include <QDateTime>

namespace Contestprogramm {

// Per-row importance score for ChatFeedModel, per the plan's "Adaptive
// Chat-Filterung" section (b): "jede eingehende Chat-/Spot-Zeile bekommt
// fortlaufend denselben Score wie der NextTargetSuggester (Erreichbarkeit
// x Neuheit x Multiplier-Wert x Aktualität)". Reachability is already a
// binary hard filter upstream (see ChatFeedModel/GeoFilter -- an
// out-of-range candidate never reaches this scorer at all), so this is
// the remaining factors: dupe status, needed-multiplier status, spot
// recency, and (2026-09-12) the plan's own "Rate-Potenzial aus einem
// frischen eigenen QSO" refinement -- a small boost when a candidate's
// bearing falls near a QSO the operator just worked on the same band
// (RecentPropagationTracker), since a real completed QSO is stronger
// live evidence of an open path than any static range/terrain estimate.
// This closes a gap this file's own comment previously disclosed as
// unbuilt.
//
// A pure function -- no ChatFeedModel/database dependency -- so it is
// directly unit-testable and reusable from both ChatFeedModel instances
// (ON4KST and cluster feeds share this scorer, per the plan's two-feed
// UI direction).
namespace ChatImportanceScorer {

// A dupe (already worked, per DupeChecker) is the lowest importance --
// it is never something the assistant should draw attention back to.
// In ChatFeedModel's default filtered view a dupe is already hard-
// excluded before scoring even runs; this constant exists so the
// scorer itself is correct in isolation (e.g. for a raw-feed caller)
// and so the pure function is fully specified for testing.
constexpr double kDupeScore = 0.0;

// Baseline for a reachable, not-yet-worked candidate.
constexpr double kBaseScore = 0.5;

// Added when the candidate's grid is a still-needed multiplier for its
// band (MultiplierTracker::isNeededMultiplier) -- an already-worked
// multiplier gets no boost ("unweighted multiplier = normal, needed
// multiplier = boosted importance", per the plan).
constexpr double kMultiplierBoost = 0.3;

// Recency bonus, linearly decayed to zero by kRecencyHorizonSeconds --
// a spot/chat line that just arrived is more actionable than one that
// has been sitting for a quarter of an hour and may already be gone.
constexpr double kMaxRecencyBonus = 0.2;
constexpr int kRecencyHorizonSeconds = 900; // 15 minutes

// Added when RecentPropagationTracker reports a recently-worked QSO on
// this band near this candidate's own bearing -- deliberately smaller
// than kMultiplierBoost (the plan's own wording, "hebt... leicht an" /
// "raises... slightly", not "as strongly as a needed multiplier").
constexpr double kPropagationBoost = 0.15;

// `timestampUtc` is the candidate's own timestamp; `nowUtc` is passed in
// (rather than read internally) so the function stays pure/testable. An
// invalid or future `timestampUtc` is treated as age 0 (full bonus)
// rather than producing a negative age. `recentPropagationNearby` is
// the RecentPropagationTracker::hasRecentOpeningNear() result for this
// candidate's own band+bearing -- passed in, not looked up here, so
// this function stays free of any tracker dependency.
double score(bool isDupe, bool isNeededMultiplier, bool recentPropagationNearby, const QDateTime& timestampUtc,
             const QDateTime& nowUtc);

} // namespace ChatImportanceScorer

} // namespace Contestprogramm
