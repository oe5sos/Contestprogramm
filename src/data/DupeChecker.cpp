#include "data/DupeChecker.h"

#include "data/ContestDatabase.h"
#include "core/CallsignPrefix.h"

#include <QSqlError>
#include <QSqlQuery>

namespace Contestprogramm {

DupeChecker::DupeChecker(ContestDatabase& database)
    : m_database(database)
{
}

bool DupeChecker::isDupe(const QString& callsign,
                          const QString& band,
                          const QString& mode,
                          const QString& contestId,
                          const QStringList& dupeScope) const
{
    return firstMatchId(callsign, band, mode, contestId, dupeScope).has_value();
}

std::optional<int> DupeChecker::firstMatchId(const QString& callsign,
                                             const QString& band,
                                             const QString& mode,
                                             const QString& contestId,
                                             const QStringList& dupeScope) const
{
    m_lastError.clear();

    // Trim + case-fold the same way AdifLog::isSameQso does, so a
    // callsign typed in lower case or with stray whitespace still
    // matches an existing log entry.
    QStringList conditions;
    conditions << QStringLiteral("contest_id = :contest_id");
    // A QSO marked invalid (see QsoRecord::isInvalid / this task's
    // report on DXLog.net's own "mark invalid, never delete" model) no
    // longer counts as a previous contact -- unconditional, not part of
    // dupeScope, since "was this ever really worked" isn't a per-contest
    // rule choice.
    conditions << QStringLiteral("is_invalid = 0");

    // Rufzeichen: nach dem GRUNDRUFZEICHEN (IARU R1 GC 2023, 1.2 --
    // S50AAA/P und DL/S50AAA sind dieselbe Station wie S50AAA). SQL
    // filtert nur grob vor (enthaelt das Grundrufzeichen), entschieden
    // wird unten mit baseCallsign() auf beiden Seiten.
    const bool byCall = dupeScope.contains(QStringLiteral("callsign"), Qt::CaseInsensitive);
    const QString base = baseCallsign(callsign);
    if (byCall) {
        conditions << QStringLiteral("UPPER(callsign) LIKE :callpat");
    }
    if (dupeScope.contains(QStringLiteral("band"), Qt::CaseInsensitive)) {
        conditions << QStringLiteral("UPPER(TRIM(band)) = UPPER(TRIM(:band))");
    }
    if (dupeScope.contains(QStringLiteral("mode"), Qt::CaseInsensitive)) {
        conditions << QStringLiteral("UPPER(TRIM(mode)) = UPPER(TRIM(:mode))");
    }

    const QString sql = QStringLiteral("SELECT id, callsign FROM qsos WHERE %1 ORDER BY id ASC")
                            .arg(conditions.join(QStringLiteral(" AND ")));

    QSqlQuery query(m_database.db());
    query.prepare(sql);
    query.bindValue(QStringLiteral(":contest_id"), contestId);
    if (byCall) {
        query.bindValue(QStringLiteral(":callpat"),
                        QStringLiteral("%") + base + QStringLiteral("%"));
    }
    if (dupeScope.contains(QStringLiteral("band"), Qt::CaseInsensitive)) {
        query.bindValue(QStringLiteral(":band"), band);
    }
    if (dupeScope.contains(QStringLiteral("mode"), Qt::CaseInsensitive)) {
        query.bindValue(QStringLiteral(":mode"), mode);
    }

    if (!query.exec()) {
        // A real failure (locked DB, disk error, ...) must never be
        // confused with a genuine "not a dupe" -- see lastError()'s own
        // doc comment. Callers that don't check lastError() keep today's
        // behaviour (treated as not-a-dupe); handleLogRequested() does
        // check it, so the operator sees a warning instead of an
        // invisible double QSO.
        m_lastError = query.lastError().text();
        return std::nullopt;
    }
    while (query.next()) {
        if (!byCall || baseCallsign(query.value(1).toString()) == base) {
            return query.value(0).toInt();
        }
    }
    return std::nullopt;
}

} // namespace Contestprogramm
