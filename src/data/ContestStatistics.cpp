#include "data/ContestStatistics.h"

#include "data/QsoRecord.h"

#include <QMap>
#include <QTimeZone>

#include <algorithm>

namespace Contestprogramm {

ContestStatistics computeContestStatistics(const QVector<QsoRecord>& records,
                                           const QString& ownGrid,
                                           const QStringList& bandOrder,
                                           const QString& scoring,
                                           int maxLongest)
{
    ContestStatistics stats;
    stats.score = computeContestScore(records, ownGrid, bandOrder, scoring);

    QMap<qint64, HourStats> byHour; // keyed by hour start, epoch seconds
    qint64 firstHour = 0;
    qint64 lastHour = 0;
    double kmSum = 0.0;
    int kmCount = 0;

    for (const QsoRecord& record : records) {
        if (record.isInvalid || record.isDupe) {
            continue;
        }
        const QDateTime ts = QDateTime::fromString(record.timestampUtc, Qt::ISODate).toUTC();
        if (ts.isValid()) {
            const qint64 hourStart = (ts.toSecsSinceEpoch() / 3600) * 3600;
            HourStats& hour = byHour[hourStart];
            hour.hourStartUtc = QDateTime::fromSecsSinceEpoch(hourStart, QTimeZone::utc());
            hour.qsos += 1;
            hour.points += qsoPoints(record, ownGrid, scoring);
            firstHour = firstHour == 0 ? hourStart : std::min(firstHour, hourStart);
            lastHour = std::max(lastHour, hourStart);
        }
        const int km = qsoDistancePoints(record, ownGrid);
        if (km > 0) {
            kmSum += km;
            ++kmCount;
            OdxEntry entry;
            entry.callsign = record.callsign.trimmed().toUpper();
            entry.grid = record.gridSquare.trimmed().toUpper();
            entry.band = record.band;
            entry.timestampUtc = record.timestampUtc;
            entry.km = km;
            stats.longest.append(entry);
        }
    }

    if (firstHour != 0) {
        for (qint64 h = firstHour; h <= lastHour; h += 3600) {
            HourStats hour = byHour.value(h);
            hour.hourStartUtc = QDateTime::fromSecsSinceEpoch(h, QTimeZone::utc());
            stats.hours.append(hour);
            if (hour.qsos > stats.bestHourQsos) {
                stats.bestHourQsos = hour.qsos;
                stats.bestHourStartUtc = hour.hourStartUtc;
            }
        }
    }

    std::stable_sort(stats.longest.begin(), stats.longest.end(), [](const OdxEntry& a, const OdxEntry& b) {
        if (a.km != b.km) {
            return a.km > b.km;
        }
        return a.timestampUtc < b.timestampUtc;
    });
    if (stats.longest.size() > maxLongest) {
        stats.longest.resize(maxLongest);
    }
    stats.averageKm = kmCount > 0 ? kmSum / kmCount : 0.0;
    return stats;
}

} // namespace Contestprogramm
