#include "data/MultiplierTracker.h"

#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"

#include <QSqlQuery>

namespace Contestprogramm {

MultiplierTracker::MultiplierTracker(ContestDatabase& database)
    : m_database(database)
{
}

QString MultiplierTracker::multiplierKeyForGrid(const QString& grid)
{
    const QString trimmed = grid.trimmed().toUpper();
    if (trimmed.size() < 4) {
        return trimmed;
    }
    return trimmed.left(4);
}

void MultiplierTracker::recompute(const QString& contestId, const ContestDefinition& definition)
{
    m_bands = definition.bands();
    m_workedByBand.clear();
    for (const QString& band : m_bands) {
        m_workedByBand.insert(band, QSet<QString>());
    }

    if (definition.multiplierField() != QStringLiteral("grid")) {
        // Not implemented yet -- see the class comment. Every band's
        // worked set stays empty rather than fabricating a DXCC-based
        // multiplier basis this pass never asked for.
        return;
    }

    QSqlQuery query(m_database.db());
    query.prepare(QStringLiteral(
        "SELECT DISTINCT band, grid_square FROM qsos "
        "WHERE contest_id = :contest_id AND grid_square IS NOT NULL AND grid_square != '' "
        // A QSO marked invalid (see QsoRecord::isInvalid) no longer
        // counts toward a multiplier either -- same exclusion
        // DupeChecker applies.
        "AND is_invalid = 0"));
    query.bindValue(QStringLiteral(":contest_id"), contestId);
    if (!query.exec()) {
        return;
    }
    while (query.next()) {
        const QString band = query.value(0).toString();
        const QString key = multiplierKeyForGrid(query.value(1).toString());
        if (key.isEmpty()) {
            continue;
        }
        m_workedByBand[band].insert(key);
    }
}

QSet<QString> MultiplierTracker::workedMultipliers(const QString& band) const
{
    return m_workedByBand.value(band);
}

int MultiplierTracker::totalMultiplierCount() const
{
    QSet<QString> allKeys;
    for (auto it = m_workedByBand.constBegin(); it != m_workedByBand.constEnd(); ++it) {
        allKeys.unite(it.value());
    }
    return allKeys.size();
}

bool MultiplierTracker::isNeededMultiplier(const QString& band, const QString& grid) const
{
    const QString key = multiplierKeyForGrid(grid);
    if (key.isEmpty()) {
        return false;
    }
    return !m_workedByBand.value(band).contains(key);
}

} // namespace Contestprogramm
