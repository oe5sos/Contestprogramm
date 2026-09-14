#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

namespace Contestprogramm {

// Phase 1.5 "Betriebsassistent" -- the plan's own explicit refinement,
// previously disclosed as unbuilt in both ChatImportanceScorer.h and
// NextTargetSuggester.h's doc comments: "nach Abschluss eines QSO
// fließt dessen Peilung/Band als Live-Beleg für offene Ausbreitung in
// diese Richtung ein und hebt vorübergehend Kandidaten mit ähnlicher
// Peilung leicht an (ein erfolgreiches QSO ist ein stärkerer Beleg als
// jede statische Reichweitenannahme)."
//
// A small rolling window of (band, bearing, when) tuples -- deliberately
// NOT a ContestDatabase table: a real VHF/UHF tropo/Es opening lasts
// minutes to perhaps an hour, not the rest of a 24h contest, so this is
// meant to fade with the opening itself. A plain in-memory ring that
// prunes expired entries on every access is the right shape for that,
// not persisted state.
class RecentPropagationTracker {
public:
    // How long a logged QSO's bearing keeps boosting similarly-bearing
    // candidates. Long enough to matter for a real opening still in
    // progress, short enough that the boost fades once the opening
    // plausibly has too, rather than permanently favouring one
    // direction for the rest of the contest.
    static constexpr int kWindowSeconds = 20 * 60; // 20 min

    // How close (in degrees, circular) a candidate's own bearing must
    // be to a recently-worked QSO's to count as "the same opening" --
    // wide enough to cover realistic path scatter for one opening,
    // narrow enough that it does not just mean "any direction that
    // worked recently".
    static constexpr double kBearingToleranceDeg = 20.0;

    // Records a just-logged QSO's band + own-station bearing.
    void recordQso(const QString& band, double bearingDeg, const QDateTime& whenUtc);

    // True if `band`+`bearingDeg` falls within kBearingToleranceDeg of
    // any QSO recorded within the last kWindowSeconds of `nowUtc`.
    // Band-scoped -- a 144 MHz opening says nothing about 432 MHz.
    bool hasRecentOpeningNear(const QString& band, double bearingDeg, const QDateTime& nowUtc) const;

    // Circular angular difference in degrees, always in [0, 180] --
    // e.g. 350° and 10° are 20° apart, not 340°. Exposed as a pure
    // static helper so it is directly unit-testable on its own.
    static double angularDifferenceDeg(double a, double b);

private:
    struct Entry {
        QString band;
        double bearingDeg = 0.0;
        QDateTime whenUtc;
    };

    // Drops every entry older than kWindowSeconds relative to `nowUtc`.
    // mutable: hasRecentOpeningNear() is logically const (a pure query
    // from the caller's point of view) but still prunes expired entries
    // as a side effect, the same "prune on read" shape SrtmTileLoader's
    // own tile cache eviction already uses in this codebase.
    void pruneExpired(const QDateTime& nowUtc) const;

    mutable QVector<Entry> m_entries;
};

} // namespace Contestprogramm
