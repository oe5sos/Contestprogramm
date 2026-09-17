#include "data/ContestScoring.h"

#include "core/Maidenhead.h"
#include "data/QsoRecord.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace Contestprogramm {

namespace {

const QString kDistanceScoring = QStringLiteral("distance_km");

QString largeSquare(const QString& grid)
{
    return grid.trimmed().left(4).toUpper();
}

} // namespace

const BandScore* ContestScore::band(const QString& band) const
{
    for (const BandScore& score : bands) {
        if (score.band == band) {
            return &score;
        }
    }
    return nullptr;
}

int qsoDistancePoints(const QsoRecord& record, const QString& ownGrid)
{
    double km = -1.0;
    if (record.distanceKm) {
        km = *record.distanceKm;
    } else if (isValidGridSquare(ownGrid) && isValidGridSquare(record.gridSquare)) {
        km = calculateDistanceKm(ownGrid, record.gridSquare);
    }
    if (km < 0.0) {
        return 0;
    }
    return std::max(1, static_cast<int>(std::lround(km)));
}

int qsoPoints(const QsoRecord& record, const QString& ownGrid, const QString& scoring)
{
    if (record.isDupe || record.isInvalid) {
        return 0;
    }
    if (scoring == kDistanceScoring) {
        return qsoDistancePoints(record, ownGrid);
    }
    return 1;
}

ContestScore computeContestScore(const QVector<QsoRecord>& records,
                                 const QString& ownGrid,
                                 const QStringList& bandOrder,
                                 const QString& scoring)
{
    ContestScore result;
    QHash<QString, int> indexOfBand;
    const auto bandScore = [&](const QString& band) -> BandScore& {
        auto it = indexOfBand.find(band);
        if (it == indexOfBand.end()) {
            BandScore fresh;
            fresh.band = band;
            result.bands.append(fresh);
            it = indexOfBand.insert(band, result.bands.size() - 1);
        }
        return result.bands[it.value()];
    };
    // Definition order first, so an empty band still shows up in its
    // place (a 432 line reading "0" is information during a contest);
    // a band the definition does not list is appended when it appears.
    for (const QString& band : bandOrder) {
        bandScore(band);
    }

    QHash<QString, QSet<QString>> squaresByBand;
    for (const QsoRecord& record : records) {
        if (record.isInvalid) {
            continue;
        }
        BandScore& score = bandScore(record.band);
        if (record.isDupe) {
            ++score.dupes;
            ++result.dupes;
            continue;
        }
        const int points = qsoPoints(record, ownGrid, scoring);
        ++score.validQsos;
        ++result.validQsos;
        score.points += points;
        result.points += points;
        if (isValidGridSquare(record.gridSquare)) {
            squaresByBand[record.band].insert(largeSquare(record.gridSquare));
        }
        // ODX is always the longest distance, whatever the scoring rule.
        const int km = qsoDistancePoints(record, ownGrid);
        if (km > score.odxKm) {
            score.odxKm = km;
            score.odxCall = record.callsign.trimmed().toUpper();
            score.odxGrid = record.gridSquare.trimmed().toUpper();
        }
        if (km > result.odxKm) {
            result.odxKm = km;
            result.odxCall = score.odxCall;
            result.odxGrid = score.odxGrid;
            result.odxBand = record.band;
        }
    }
    for (BandScore& score : result.bands) {
        score.largeSquares = squaresByBand.value(score.band).size();
    }
    return result;
}

} // namespace Contestprogramm
