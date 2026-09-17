#pragma once

#include "data/ContestScoring.h"

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Contestprogramm {

struct QsoRecord;

// What DXLog.net's Statistics window and N1MM+'s Score Summary show
// after (and during) a contest, computed as a pure function over the
// records like ContestScoring: QSOs and km per UTC hour of the contest,
// the per-band summary, and the longest QSOs. The score part is the
// same ContestScore the panel and the EDI file use.
struct HourStats {
    QDateTime hourStartUtc; // whole hour
    int qsos = 0;
    qint64 points = 0;
};

struct OdxEntry {
    QString callsign;
    QString grid;
    QString band;
    QString timestampUtc;
    int km = 0;
};

struct ContestStatistics {
    ContestScore score;
    QVector<HourStats> hours;   // every hour from the first to the last QSO, empty hours included
    QVector<OdxEntry> longest;  // best first, at most maxLongest
    int bestHourQsos = 0;
    QDateTime bestHourStartUtc;
    double averageKm = 0.0;     // over valid QSOs with a known distance
};

ContestStatistics computeContestStatistics(const QVector<QsoRecord>& records,
                                           const QString& ownGrid,
                                           const QStringList& bandOrder,
                                           const QString& scoring = QStringLiteral("distance_km"),
                                           int maxLongest = 10);

} // namespace Contestprogramm
