#pragma once

#include "core/GeoFilter.h"
#include "core/SpotCandidate.h"

#include <QVector>

#include <optional>

namespace Contestprogramm {

// Phase 1.5 "Betriebsassistent" (see the plan's own section of that
// name): picks the single best next target out of every candidate
// currently visible across both ChatFeedModel instances (ON4KST chat +
// classic DX cluster), so SuggestionPanel has one thing to show/draft a
// message for rather than the operator having to scan two feed tables
// themselves.
//
// Deliberately just "highest score wins" -- ChatFeedModel::scoreAt()
// already IS "Erreichbarkeit x Neuheit x Multiplier-Wert x Aktualität"
// (ChatImportanceScorer, shared with the adaptive chat-filter threshold
// per the plan's explicit "jede eingehende Chat-/Spot-Zeile bekommt
// fortlaufend denselben Score wie der NextTargetSuggester" direction),
// and a visible row is already hard-filtered to reachable + not-yet-
// worked (ChatFeedModel's own filtered mode). Re-deriving any of that
// here would duplicate logic that already lives in one place; this
// function's only job is the final argmax across the two feeds' visible
// rows. The plan's "Rate-Potenzial aus einem frischen eigenen QSO"
// refinement (2026-09-12) is implemented this same way -- not here, but
// upstream in ChatImportanceScorer::score()/RecentPropagationTracker,
// which scoreAt() above already folds in, so this function picks it up
// for free. "kein konkurrierender Zielwechsel während eines laufenden
// QSO" is a UI-timing concern (whether/when to call this at all) --
// MainWindow's own refresh cadence, not this pure ranking step.
//
// A free function over plain (candidate, score) pairs rather than
// ChatFeedModel references, so it is directly unit-testable with
// synthetic data (no QAbstractTableModel/QSqlDatabase fixture needed),
// the same "pure function over records" pattern RateMeterWidget's
// computeRateBreakdown() and DupeChecker/Maidenhead already established
// in this codebase.
namespace NextTargetSuggester {

// One visible row from either feed -- the caller assembles a QVector of
// these from ChatFeedModel::candidateAt()/scoreAt()/geoAt() across both
// ChatFeedModel instances (ON4KST + Cluster).
struct Candidate {
    SpotCandidate candidate;
    double score = 0.0;
    GeoFilter::Result geo;
};

struct Suggestion {
    SpotCandidate candidate;
    double score = 0.0;
    GeoFilter::Result geo;
};

// Returns the highest-scored entry, or nullopt when `candidates` is
// empty (no reachable, not-yet-worked, currently-visible station at all
// -- a genuinely different state from "everyone scored zero", which
// still returns the (arbitrary, tied) top entry).
std::optional<Suggestion> suggest(const QVector<Candidate>& candidates);

} // namespace NextTargetSuggester

} // namespace Contestprogramm
