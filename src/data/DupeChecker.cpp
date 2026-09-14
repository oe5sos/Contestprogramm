#include "data/DupeChecker.h"

#include "data/ContestDatabase.h"

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

    if (dupeScope.contains(QStringLiteral("callsign"), Qt::CaseInsensitive)) {
        conditions << QStringLiteral("UPPER(TRIM(callsign)) = UPPER(TRIM(:callsign))");
    }
    if (dupeScope.contains(QStringLiteral("band"), Qt::CaseInsensitive)) {
        conditions << QStringLiteral("UPPER(TRIM(band)) = UPPER(TRIM(:band))");
    }
    if (dupeScope.contains(QStringLiteral("mode"), Qt::CaseInsensitive)) {
        conditions << QStringLiteral("UPPER(TRIM(mode)) = UPPER(TRIM(:mode))");
    }

    const QString sql = QStringLiteral("SELECT id FROM qsos WHERE %1 LIMIT 1").arg(conditions.join(QStringLiteral(" AND ")));

    QSqlQuery query(m_database.db());
    query.prepare(sql);
    query.bindValue(QStringLiteral(":contest_id"), contestId);
    if (dupeScope.contains(QStringLiteral("callsign"), Qt::CaseInsensitive)) {
        query.bindValue(QStringLiteral(":callsign"), callsign);
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
        return false;
    }
    return query.next();
}

} // namespace Contestprogramm
