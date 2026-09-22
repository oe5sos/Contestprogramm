#pragma once

#include "data/ContestScoring.h"
#include "data/QsoRecord.h"

#include <QDateTime>
#include <QPair>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QPainter;
class QTimer;

namespace Contestprogramm {

class ContestDatabase;

// One pass over this contest's own logged QSOs (see
// ContestDatabase::qsosForContest) -- computes everything
// RateMeterWidget's display needs (rolling 10-min/hour/total counts, a
// trend comparing the current vs. the PRECEDING 10-minute window,
// per-band/per-mode tallies, the 10-minute histogram behind the
// sparkline and the best clock hour) in one place, as a free function
// over plain data rather than a live database connection -- so it is
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
    // QSOs per 10-minute bucket over the last six hours, oldest first;
    // the last bucket is the running ten minutes. Always kSparkBuckets
    // long, so the sparkline's x axis never moves.
    static constexpr int kSparkBuckets = 36;
    QVector<int> perTenMinutes;
    // The busiest UTC clock hour of the log so far -- a "best hour" is a
    // clock hour, the way every contest program reports it -- and how
    // many QSOs it held. The start stays invalid while the log is empty.
    int bestHourQsos = 0;
    QDateTime bestHourStartUtc;
};

RateBreakdown computeRateBreakdown(const QVector<QsoRecord>& records, const QDateTime& nowUtc);

// "QSOs in the last 10 min / last hour / total" display, per the plan's
// UI section ("Rate-Meter: letzte 10 Min / letzte Stunde / gesamt"),
// enriched with a rate trend indicator and a band/mode breakdown
// (operator, 2026-09-12: "Reicheres Rate/Score-Fenster (Trend,
// Band/Mode-Aufschlüsselung)"), the claimed score and the ODX.
// Recomputed on a periodic timer from ContestDatabase -- no
// push-exact-second precision needed.
//
// Painted as one instrument rather than a column of labels, so the
// panel can follow its own size: a low, wide panel becomes the counter
// strip (glass chips, per-band bars, the rate with a six-hour
// sparkline); anything else the tile grid, with as many tiles as the
// height affords (operator, 2026-09-21, choosing sheets 1 and 2
// together: "ich möchte selbst mit Kleiner- und Größerziehen des
// Fensters, dass sich das automatisch anpasst").
class RateMeterWidget : public QWidget {
    Q_OBJECT

public:
    enum class Layout { Strip, Tiles };
    // The layout a widget of `size` paints -- chosen from the size
    // alone, never set by hand.
    static Layout layoutFor(const QSize& size);

    explicit RateMeterWidget(QWidget* parent = nullptr);

    // `database` is a non-owning pointer; the caller keeps it alive for
    // as long as this widget exists (mirrors how MainWindow already
    // holds ContestDatabase/AppController references elsewhere).
    void setSource(ContestDatabase* database, const QString& contestId);

    // What the "Punkte"/"ODX" readings need beyond the records
    // themselves (see data/ContestScoring.h): the own locator for
    // distances, the definition's band order, and its scoring rule.
    // Without a valid own grid the readings show a dash rather than a
    // wrong zero.
    // `multiplierBasis` ist ContestDefinition::multiplierField(): die
    // letzte Kachel zählt danach -- Locator-Großfelder auf UKW,
    // WPX-Präfixe auf Kurzwelle, und ohne Multiplikator entfällt sie.
    void setScoring(const QString& ownGrid, const QStringList& bandOrder, const QString& scoring,
                    const QString& multiplierBasis = QStringLiteral("grid"));

    // Everything currently shown, one reading per line ("QSOs 47 (144:
    // 31 · 432: 16)"), in the tile order -- the test hook now that the
    // readings are painted rather than held in QLabels (see
    // tests/test_rate_meter_score.cpp).
    QString readingsText() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

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

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    // One reading, the way both layouts show it: a caps caption, the
    // value (with an optional small unit after it) and a quiet line
    // under it, in a long and a short form for narrow tiles.
    struct Reading {
        QString caption;
        QString value;
        QString unit;
        QString sub;
        QString subShort;
        QString valueColor;
    };
    QVector<Reading> readings() const;
    QString bandLine(bool withNames, bool points) const;
    void paintStrip(QPainter& painter, const QRect& area) const;
    void paintTiles(QPainter& painter, const QRect& area) const;

    ContestDatabase* m_database = nullptr;
    QString m_contestId;
    QString m_ownGrid;
    QStringList m_bandOrder;
    QString m_scoring = QStringLiteral("distance_km");
    QString m_multiplierBasis = QStringLiteral("grid");
    QTimer* m_timer;

    // The last refresh(), kept for painting.
    bool m_hasSource = false;
    RateBreakdown m_breakdown;
    bool m_scoreKnown = false;
    ContestScore m_score;
    int m_largeSquares = 0; // Multiplikatoren über alle Bänder, nach m_multiplierBasis
    QHash<QString, int> m_multipliersByBand; // dieselbe Grundlage, je Band
};

} // namespace Contestprogramm
