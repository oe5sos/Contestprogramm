#pragma once

#include "data/QsoRecord.h"

#include <QDateTime>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QTimer;

namespace Contestprogramm {

class ContestDatabase;

// One pass over this contest's own logged QSOs (see
// ContestDatabase::qsosForContest) -- computes everything
// RateMeterWidget's display needs (rolling 10-min/hour/total counts, a
// trend comparing the current vs. the PRECEDING 10-minute window, and
// per-band/per-mode tallies) in one place, as a free function over
// plain data rather than a live database connection -- so it is
// unit-testable directly (see tests/test_rate_breakdown.cpp) without a
// QSqlDatabase fixture, the same "pure function over records" pattern
// DupeChecker/Maidenhead already established in this codebase.
struct RateBreakdown {
    int last10Min = 0;
    int lastHour = 0;
    int total = 0;
    enum class Trend { Up, Down, Flat };
    Trend trend = Trend::Flat;
    // Band/mode tallies, sorted by count descending (most-active first),
    // name ascending as a tie-breaker -- deterministic, and reads as
    // "what's carrying the rate right now" at a glance.
    QVector<QPair<QString, int>> byBand;
    QVector<QPair<QString, int>> byMode;
};

RateBreakdown computeRateBreakdown(const QVector<QsoRecord>& records, const QDateTime& nowUtc);

// "QSOs in the last 10 min / last hour / total" display, per the plan's
// UI section ("Rate-Meter: letzte 10 Min / letzte Stunde / gesamt"),
// enriched with a rate trend indicator and a band/mode breakdown
// (operator, 2026-09-12: "Reicheres Rate/Score-Fenster (Trend,
// Band/Mode-Aufschlüsselung)"). Recomputed on a periodic timer from
// ContestDatabase -- no push-exact-second precision needed.
class RateMeterWidget : public QWidget {
    Q_OBJECT

public:
    explicit RateMeterWidget(QWidget* parent = nullptr);

    // `database` is a non-owning pointer; the caller keeps it alive for
    // as long as this widget exists (mirrors how MainWindow already
    // holds ContestDatabase/AppController references elsewhere).
    void setSource(ContestDatabase* database, const QString& contestId);

    // What the "Punkte"/"ODX" rows need beyond the records themselves
    // (see data/ContestScoring.h): the own locator for distances, the
    // definition's band order, and its scoring rule. Without a valid
    // own grid the rows show a dash rather than a wrong zero.
    void setScoring(const QString& ownGrid, const QStringList& bandOrder, const QString& scoring);

public slots:
    void refresh();

signals:
    // Emitted at the end of every refresh() with the last-10-minutes
    // count this widget already computes, per the plan's Adaptive-
    // Chat-Filterung section (c): "read the current QSO rate from
    // RateMeterWidget... (last-10-minutes count is already computed
    // there)". MainWindow forwards this into both ChatFeedModel
    // instances' setCurrentRatePerTenMinutes() rather than duplicating
    // the qsoCountSince() query. Unchanged by the richer breakdown --
    // still just the last10Min count, same meaning as before.
    void last10MinRateChanged(int count);

private:
    ContestDatabase* m_database = nullptr;
    QString m_contestId;
    QString m_ownGrid;
    QStringList m_bandOrder;
    QString m_scoring = QStringLiteral("distance_km");
    QTimer* m_timer;
    QLabel* m_headlineLabel;
    QLabel* m_bandLabel;
    QLabel* m_modeLabel;
    QLabel* m_scoreLabel;
    QLabel* m_odxLabel;
};

} // namespace Contestprogramm
