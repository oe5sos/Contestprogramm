#pragma once

#include "core/GeoFilter.h"
#include "core/SpotCandidate.h"

#include <QAbstractTableModel>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Contestprogramm {

class DupeChecker;
class MultiplierTracker;
class RecentPropagationTracker;

// Backs the "Spots & Chat" section of UnifiedLogWidget's feed table (one
// per spot source -- see ui/UnifiedLogWidget.h). Holds every
// SpotCandidate seen from one spot source (On4kstClient or
// DxClusterClient -- AppController owns two separate ChatFeedModel
// instances, one per source, per the plan's two-feed UI direction
// rather than one blended feed with a source column), tagged with
// GeoFilter's classification and an
// "already worked" flag from DupeChecker. Two visibility modes, per the
// plan's UI section ("Roh-Feed-Toggle (default aus)"):
//   - filtered (default): only in-range, not-yet-worked candidates,
//     further narrowed by the rate-derived adaptive threshold below --
//     matches the plan's Kontext wording ("alles andere ausgeblendet").
//   - raw: everything, unfiltered, bypassing both the hard exclusion
//     and the adaptive threshold; dupes are dimmed there via DupeRole
//     rather than removed.
//
// Adaptive chat filtering (plan section "Adaptive Chat-Filterung"):
//   (a) Hard exclusion is already what filtered mode does above (an
//       out-of-range or already-worked candidate is removed from
//       m_visibleIndices entirely, not merely dimmed) -- confirmed
//       against the plan's explicit "verify, don't assume" note; no
//       change was needed here. Terrain-obstructed classification does
//       not exist yet (Phase 2), so that half of the rule is a no-op.
//   (b) Each entry additionally carries an importance `score`
//       (core/ChatImportanceScorer.h): dupe status, needed-multiplier
//       status (via the optional MultiplierTracker), and spot recency.
//   (c) Among the hard-filter survivors, ChatVisibilityPolicy
//       (core/ChatVisibilityPolicy.h) applies a rate-derived threshold
//       on that score -- quiet shows nearly everything, busy shows only
//       the top-scored fraction. A pure function, not logic buried in
//       this class, so it stays independently testable.
//
// "Already worked" is decided per band, the way the IARU R1 rule
// counts ("once per band"): a spot with a frequency is checked on that
// band; a chat line without one (ON4KST's 144/432 room mixes both
// bands) counts as worked only when the station is in the log on EVERY
// band the active contest has -- as long as one band is still open,
// the station is still a candidate. (Until 2026-09-18 the check was
// callsign-only, which hid a station worked on 144 from the 432 side
// of a two-band contest.) The log's own per-QSO DupeChecker call in
// MainWindow uses the contest's real dupe_scope as before.
class ChatFeedModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { ColumnCallsign = 0, ColumnGrid, ColumnDistanceKm, ColumnBearingDeg, ColumnText, ColumnCount };
    enum Role { DupeRole = Qt::UserRole + 1, InRangeRole };

    explicit ChatFeedModel(GeoFilter& geoFilter, DupeChecker& dupeChecker, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    bool showRawFeed() const { return m_showRaw; }
    void setShowRawFeed(bool show);

    // Drives the "already worked" recheck (see the class comment for
    // why this is callsign-only); MainWindow/AppController call this
    // whenever the active contest changes.
    void setActiveContest(const QString& contestId);
    // The contest's bands, for the "worked on every band?" reading of
    // a candidate without a frequency (see the class comment). Empty
    // falls back to callsign-only.
    void setContestBands(const QStringList& bands);

    // Optional (nullptr = no multiplier boost, e.g. before
    // AppController has recomputed one yet). Non-owning; the caller
    // keeps it alive. Lets the importance scorer tell a still-needed
    // multiplier apart from one already worked on that band.
    void setMultiplierTracker(MultiplierTracker* tracker);

    // Optional (nullptr = no propagation boost). Non-owning; the caller
    // keeps it alive. Lets the importance scorer nudge up a candidate
    // whose own bearing falls near a QSO the operator just worked on
    // the same band -- see RecentPropagationTracker's own class comment
    // for the plan section this implements.
    void setRecentPropagationTracker(RecentPropagationTracker* tracker);

    // Current QSO rate, QSO/10min, from RateMeterWidget/
    // ContestDatabase::qsoCountSince -- drives ChatVisibilityPolicy's
    // quiet/moderate/busy threshold. Defaults to 0 (Quiet), so a caller
    // that never wires this up keeps today's "show everything reachable
    // and unworked" behavior.
    void setCurrentRatePerTenMinutes(double ratePerTenMinutes);

    const SpotCandidate& candidateAt(int row) const;
    // The same ChatImportanceScorer value rebuildVisibleRows() already
    // computes per entry (used internally to drive ChatVisibilityPolicy's
    // rate-adaptive threshold), exposed directly for NextTargetSuggester
    // (see core/assistant/NextTargetSuggester.h) -- lets it rank across
    // both ChatFeedModel instances (ON4KST + Cluster) without duplicating
    // this model's own DupeChecker/MultiplierTracker-dependent scoring
    // externally. 0.0 for an out-of-range row.
    double scoreAt(int row) const;
    // The GeoFilter::Result (distance/bearing/terrain) already computed
    // for this row, same reasoning as scoreAt() above -- lets a caller
    // like NextTargetSuggester show km/bearing for its winning candidate
    // without recomputing Maidenhead distance/bearing itself. A
    // default-constructed Result (distanceKnown == false) for an
    // out-of-range row.
    GeoFilter::Result geoAt(int row) const;
    // Re-derives EVERY already-seen entry's `worked` flag (DupeChecker)
    // and `score` (ChatImportanceScorer, including MultiplierTracker's
    // needed-multiplier boost) against the database/tracker's CURRENT
    // state, then rebuilds the visible-row set -- the same recompute
    // loop setActiveContest() already runs for a contest switch, now
    // exposed directly. Call this whenever a QSO is logged/edited/
    // invalidated: an entry's cached worked/score is otherwise only
    // ever set once, at the moment it first arrived via addCandidate(),
    // and silently goes stale the instant the operator logs that same
    // station (still shown as "not worked", a needed multiplier keeps
    // scoring as needed after being worked) -- see this project's own
    // 2026-09-12 audit finding for the concrete failure mode this
    // closes.
    void refreshWorkedAndScores();

public slots:
    void addCandidate(const SpotCandidate& candidate);

private:
    struct Entry {
        SpotCandidate candidate;
        GeoFilter::Result geo;
        bool worked = false;
        double score = 0.0;
    };

    double computeScore(const SpotCandidate& candidate, bool worked, const GeoFilter::Result& geo) const;
    void rebuildVisibleRows();
    bool computeWorked(const QString& callsign, qint64 freqHz) const;

    GeoFilter& m_geoFilter;
    DupeChecker& m_dupeChecker;
    MultiplierTracker* m_multiplierTracker = nullptr;
    RecentPropagationTracker* m_propagationTracker = nullptr;
    double m_ratePerTenMinutes = 0.0;
    QVector<Entry> m_allEntries;
    QVector<int> m_visibleIndices; // indices into m_allEntries
    bool m_showRaw = false;
    QString m_activeContestId;
    QStringList m_contestBands;
};

} // namespace Contestprogramm
