#pragma once

#include <QVector>

namespace Contestprogramm {

// The rate-derived visibility threshold from the plan's "Adaptive
// Chat-Filterung" section (c): "Die Schwelle ist nicht fix, sondern von
// der Betriebslage abhängig" -- quiet operating shows nearly everything
// (a low bar), busy operating (a pile-up while running) shows only the
// top-scored fraction so the operator is not distracted mid-run.
//
// A pure function of (scores, tempo) -- no ChatFeedModel/RateMeterWidget
// dependency -- per the plan's explicit ask: "eine reine Funktion aus
// Rate+Scores auf einen Sichtbarkeits-Filter, nicht in einem
// UI-Event-Handler versteckt." All thresholds are named constants here,
// tunable in one place rather than scattered magic numbers.
namespace ChatVisibilityPolicy {

// QSO/10min boundaries, per the plan's own examples ("below 2 QSO/10min
// = quiet", "above 6 QSO/10min = busy").
constexpr double kQuietRateThreshold = 2.0;
constexpr double kBusyRateThreshold = 6.0;

// Minimum score to stay visible while quiet/moderate -- a low bar, per
// the plan ("Ruhig -> Schwelle sinkt, Feed zeigt mehr"). Moderate sits
// between the two named tiers, per the plan's "2-3 tier" allowance.
constexpr double kQuietMinScore = 0.05;
constexpr double kModerateMinScore = 0.30;

// While busy, only the top fraction (by score) of survivors stays
// visible, per the plan ("Feed zeigt... nur die Top-gescorten"). At
// least one row always stays visible so a lone candidate is never
// silently dropped to zero rows.
constexpr double kBusyTopFraction = 0.30;

enum class Tempo { Quiet, Moderate, Busy };

Tempo tempoForRate(double qsoPerTenMinutes);

// `scores` are the already-hard-filtered (reachable, not-yet-worked)
// candidates' importance scores, in any order. Returns a same-size,
// same-order mask: true means the row at that index stays visible at
// `tempo`.
QVector<bool> visibilityMask(const QVector<double>& scores, Tempo tempo);

} // namespace ChatVisibilityPolicy

} // namespace Contestprogramm
