#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace Contestprogramm {

struct QsoRecord;

// The claimed score of a contest log, computed the way the IARU-R1 /
// ÖVSV / DARC VHF-UHF rules count: 1 point per kilometre between
// locator centres (truncated, plus 1), summed per band, no multipliers. (MultiplierTracker's
// worked/needed large-square grid is a "which squares are still
// missing" aid, not a score factor -- that ARRL-style rule set is not
// what these contests use.) A pure function over records, like
// computeRateBreakdown() and DupeChecker, so it is unit-testable
// without a database and the same numbers feed both the live score
// panel (RateMeterWidget) and the submitted EDI header (EdiExporter) --
// the score on screen is the score in the file, by construction.
//
// ContestDefinition::scoring() selects the rule: "distance_km" (the
// default, above) or "qso_count" (1 point per valid QSO). Records
// marked invalid never count; dupes are counted as dupes and score 0.
struct BandScore {
    QString band;
    int validQsos = 0;
    int dupes = 0;
    qint64 points = 0;
    int largeSquares = 0; // distinct 4-character WWLs among the valid QSOs
    QString odxCall;
    QString odxGrid;
    int odxKm = 0;
};

struct ContestScore {
    QVector<BandScore> bands; // in the definition's band order, unlisted bands last
    int validQsos = 0;
    int dupes = 0;
    qint64 points = 0;
    QString odxCall;
    QString odxGrid;
    QString odxBand;
    int odxKm = 0;

    const BandScore* band(const QString& band) const;
};

// Die Entfernung einer Verbindung fuer die Wertung, ganze km
// (abgeschnitten): die gespeicherte (beim Loggen nach iaruQrbKm
// gerechnet, bei jedem Wechsel des eigenen Locators fuer das laufende
// Log neu -- ContestDatabase::recomputeDistances), sonst aus `ownGrid`
// gerechnet. -1, wenn der empfangene Locator nicht sechsstellig ist
// (IARU R1 GC 2023, 1.9.1) oder sich nichts rechnen laesst.
int qsoDistanceKm(const QsoRecord& record, const QString& ownGrid);

// Distance points for one record, the IARU Region 1 way: the distance
// truncated to whole kilometres plus 1 -- a same-square contact scores
// 1. 0 when the QSO is incomplete: no full 6-digit locator on either
// side, or no received serial number (REG1TEST: incomplete QSOs are
// claimed with 0 points; IARU R1 GC 2023, 1.9.1).
int qsoDistancePoints(const QsoRecord& record, const QString& ownGrid);

// Points for one record under `scoring` ("distance_km" / "qso_count").
// A dupe always scores 0.
int qsoPoints(const QsoRecord& record, const QString& ownGrid, const QString& scoring);

ContestScore computeContestScore(const QVector<QsoRecord>& records,
                                 const QString& ownGrid,
                                 const QStringList& bandOrder,
                                 const QString& scoring = QStringLiteral("distance_km"));

} // namespace Contestprogramm
