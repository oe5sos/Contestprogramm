#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace Contestprogramm {

class ContestDatabase;

// Sub-second dupe checks against the indexed `qsos` table (see
// idx_qsos_callsign_band_mode in ContestDatabase). Deliberately not the
// in-memory-rebuild approach Longpath's WorkedBefore uses -- that one
// was built for award tracking, not for a live per-keystroke check
// during a contest.
//
// Normalization mirrors Longpath's AdifLog::isSameQso: callsign is
// trimmed + upper-cased, band/mode are trimmed and compared case-
// insensitively (see src/core/AdifLog.cpp in NereusSDR).
class DupeChecker {
public:
    explicit DupeChecker(ContestDatabase& database);

    // Checks whether a QSO with this callsign/band/mode already exists
    // for `contestId`, using the given dupe_scope field list (from the
    // active ContestDefinition -- not hardcoded, per the plan). Only
    // "callsign", "band" and "mode" are meaningful scope entries, since
    // those are the only columns the schema carries for this purpose;
    // any other entries in `dupeScope` are ignored.
    // Returns false (not a dupe) both on a genuine miss AND on a SQL
    // failure -- callers that need to tell the two apart (a real "not a
    // dupe" must never be silently confused with "the query itself
    // failed", since the former is safe to log as a fresh QSO and the
    // latter is not something to guess about) check lastError()
    // immediately after: empty means a genuine result, non-empty means
    // this call's `false` is not trustworthy.
    bool isDupe(const QString& callsign,
                const QString& band,
                const QString& mode,
                const QString& contestId,
                const QStringList& dupeScope) const;

    // The id of the EARLIEST valid QSO the dupe rule matches -- the one
    // the operator actually logged the station under (its number and
    // time are what the entry row reports back, 2026-09-21: "sollte ein
    // dupe kommen soll sofort die nummer stehen, mit der ich geloggt
    // habe, inkl. uhrzeit"). std::nullopt when there is none, or when
    // the query failed (lastError() then says so). isDupe() is this,
    // reduced to has_value().
    std::optional<int> firstMatchId(const QString& callsign,
                                    const QString& band,
                                    const QString& mode,
                                    const QString& contestId,
                                    const QStringList& dupeScope) const;

    // Empty after a successful isDupe() call (including a genuine
    // "not a dupe" result); the query's error text after a failed one.
    // Cleared at the start of every isDupe() call, so it always reflects
    // only the most recent call.
    QString lastError() const { return m_lastError; }

private:
    ContestDatabase& m_database;
    // mutable: isDupe() is logically const (it only reads), but still
    // needs to record a failure for lastError() to report.
    mutable QString m_lastError;
};

} // namespace Contestprogramm
