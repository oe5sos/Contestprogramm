#include "data/MultiplierTracker.h"

#include "data/ContestDatabase.h"
#include "core/CallsignPrefix.h"
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

    m_basis = definition.multiplierField();
    const bool byGrid = m_basis == QStringLiteral("grid");
    const bool byPrefix = m_basis == QStringLiteral("prefix");
    if (!byGrid && !byPrefix) {
        // Eine Grundlage, die dieser Zähler nicht kennt (oder gar
        // keine): die Listen bleiben leer, statt eine zu erfinden.
        return;
    }

    QSqlQuery query(m_database.db());
    // Ein ungültig gesetztes QSO (QsoRecord::isInvalid) zählt auch für
    // den Multiplikator nicht mehr -- dieselbe Ausnahme, die auch
    // DupeChecker macht.
    query.prepare(byGrid
        ? QStringLiteral("SELECT DISTINCT band, grid_square FROM qsos "
                         "WHERE contest_id = :contest_id AND grid_square IS NOT NULL AND grid_square != '' "
                         "AND is_invalid = 0")
        : QStringLiteral("SELECT DISTINCT band, callsign FROM qsos "
                         "WHERE contest_id = :contest_id AND callsign IS NOT NULL AND callsign != '' "
                         "AND is_invalid = 0"));
    query.bindValue(QStringLiteral(":contest_id"), contestId);
    if (!query.exec()) {
        return;
    }
    while (query.next()) {
        const QString band = query.value(0).toString();
        const QString value = query.value(1).toString();
        const QString key = byGrid ? multiplierKeyForGrid(value) : wpxPrefix(value);
        if (key.isEmpty()) {
            continue;
        }
        m_workedByBand[band].insert(key);
    }
}

QString MultiplierTracker::multiplierKeyFor(const QString& grid, const QString& callsign) const
{
    if (m_basis == QStringLiteral("grid")) {
        return multiplierKeyForGrid(grid);
    }
    if (m_basis == QStringLiteral("prefix")) {
        return wpxPrefix(callsign);
    }
    return QString();
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

bool MultiplierTracker::isNeededMultiplier(const QString& band, const QString& grid, const QString& callsign) const
{
    const QString key = multiplierKeyFor(grid, callsign);
    if (key.isEmpty()) {
        return false;
    }
    return !m_workedByBand.value(band).contains(key);
}

} // namespace Contestprogramm
