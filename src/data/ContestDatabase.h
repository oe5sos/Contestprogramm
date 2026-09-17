#pragma once

#include "data/QsoRecord.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QHash>
#include <QString>
#include <QVector>

#include <optional>

namespace Contestprogramm {

// Owns one QSqlDatabase(SQLITE) connection and the schema in it. See the
// project plan's "SQLite-Schema (Phase 1)" section for the exact table
// definitions this creates.
class ContestDatabase {
public:
    // Result shape for knownExchangeForCallsign() -- deliberately not
    // the full QsoRecord (id/source/notes/... are meaningless for a
    // "what did we last hear from this call" lookup), just the fields
    // UnifiedLogWidget's grid-autofill needs to prefill.
    struct KnownCallsignExchange {
        QString gridSquare;
        QString exchangeRcvd;
        std::optional<int> serialRcvd;
    };

    ContestDatabase() = default;
    ~ContestDatabase();

    ContestDatabase(const ContestDatabase&) = delete;
    ContestDatabase& operator=(const ContestDatabase&) = delete;

    // Opens (creating if necessary) the SQLite file at `path` and
    // ensures the schema exists. `connectionName` must be unique across
    // the process (Qt keys QSqlDatabase connections by name) -- tests
    // that open several independent databases in one process need
    // distinct names. Returns false and sets lastError() on failure.
    bool open(const QString& path, const QString& connectionName = QStringLiteral("qt_sql_default_connection"));
    void close();
    bool isOpen() const;
    QString lastError() const;

    // Inserts `record`; on success sets record.id to the new row id and
    // returns true.
    bool insertQso(QsoRecord& record);
    QVector<QsoRecord> qsosForContest(const QString& contestId) const;
    // Single QSO by id -- used to refresh just the affected row after a
    // history-row hand-correction or invalid-toggle (see LogTableModel::
    // updateRecord / UnifiedLogWidget's history-row editing) without
    // re-fetching (and resetting) the whole contest's log.
    std::optional<QsoRecord> qsoById(int id) const;
    // Subset of qsosForContest() with a non-empty grid_square -- feeds
    // MapWidget's worked-station markers (see MainWindow's wiring),
    // which cannot place a QSO with no known grid anywhere on the map.
    // Filtered in SQL rather than by the caller so a large log does not
    // need to pull every column-less row just to discard it client-side.
    QVector<QsoRecord> qsosWithGrid(const QString& contestId) const;
    int qsoCountForContest(const QString& contestId) const;

    // Count of QSOs for contestId with timestamp_utc >= sinceUtc, for
    // RateMeterWidget's "last N minutes" windows. Relies on
    // timestamp_utc always being written in the same UTC ISO-8601
    // ("...Z") form (see insertQso callers), so a plain string
    // comparison in SQL orders correctly without parsing dates back out.
    int qsoCountSince(const QString& contestId, const QDateTime& sinceUtc) const;

    // Next serial number to send for `contestId`: 1 + the highest
    // serial_sent already logged for that contest (1 if none logged
    // yet). Used to auto-fill the "serial" exchange field.
    int nextSerialForContest(const QString& contestId) const;

    // Grid-autofill lookup, per the plan's UI section
    // ("Grid-Autofill bei bekanntem Rufzeichen"): the most recent
    // logged QSO with this callsign in this contest, if any --
    // UnifiedLogWidget prefills Grid/Exchange-received from it when the
    // operator types a callsign already worked (e.g. on another band).
    // Normalized the same way DupeChecker::isDupe is (trimmed,
    // case-folded callsign).
    std::optional<KnownCallsignExchange> knownExchangeForCallsign(const QString& callsign, const QString& contestId) const;

    // Hand-correct an already-logged QSO's callsign in place -- see
    // UnifiedLogWidget's history-row Call cell and this task's report
    // (DXLog.net's own real scope: the "Nr"/exchange fields are
    // editable in place in the log grid, date/time/frequency/operator
    // go through a separate dialog -- out of scope here, see the
    // report).
    bool updateQsoCallsign(int id, const QString& callsign, QString* errorOut = nullptr);

    // Hand-correct an already-logged QSO's received exchange: the
    // composed text plus the individual grid_square/serial_rcvd/
    // rst_rcvd columns the other read paths (MultiplierTracker,
    // grid-autofill, distance/bearing) actually use, kept in sync with
    // it. distanceKm/bearingDeg are recomputed by the caller (MainWindow,
    // which knows ContestSettings::ownGrid) and passed in already
    // computed, same division of labour handleLogRequested() already
    // uses at initial log time.
    bool updateQsoExchangeRcvd(int id, const QString& exchangeRcvd, const QString& gridSquare,
                                const std::optional<int>& serialRcvd, const QString& rstRcvd,
                                const std::optional<double>& distanceKm, const std::optional<double>& bearingDeg,
                                QString* errorOut = nullptr);

    // DXLog.net deliberately has no delete function for a logged QSO --
    // "in the spirit of honest contest logging... you don't" (dxlog.net/
    // docs/index.php/Menu_Edit, verified for this task) -- a QSO is
    // marked invalid instead, never removed. See DupeChecker/
    // MultiplierTracker/CabrilloExporter/AdifExporter for where
    // is_invalid is then excluded.
    bool setQsoInvalid(int id, bool invalid, QString* errorOut = nullptr);

    // Corrects a logged QSO's time (ISO-8601 UTC, same form insertQso()
    // stores) -- the one field DXLog.net/N1MM+ hand-corrections need
    // beyond call/exchange: the log time is when Enter was pressed,
    // not always when the QSO happened.
    bool updateQsoTimestamp(int id, const QString& timestampUtc, QString* errorOut = nullptr);

    // Incremented by every QSO write (insert/update/invalid-toggle) --
    // LogBackup compares it against the value at its last backup, so an
    // idle log (or one where only layout/settings rows change) does not
    // produce a new backup file every five minutes.
    int qsoWriteCounter() const { return m_qsoWriteCounter; }

    // A consistent copy of the whole database as of now -- SQLite's
    // own VACUUM INTO, which includes everything still sitting in the
    // WAL (a plain file copy of the .sqlite would silently miss the
    // last QSOs). The target must not exist yet.
    bool backupTo(const QString& path, QString* errorOut = nullptr);

    // Locally-known callsign -> grid (+ optional operator name), backed
    // by the `imported_locators` table -- the second lookup tier in
    // MainWindow::handleCallsignLookupRequested, after the operator's
    // own log (knownExchangeForCallsign above) and before any external
    // QRZ/HamQTH network lookup (see core/CallsignLocatorLookup.h).
    // Populated either by a one-time CSV import (SettingsDialog's
    // "Locator-Liste importieren...", the same concept N1MM+ calls a
    // Call History File) or by CallsignLocatorLookup caching a
    // successful external-lookup result -- a live lookup is exactly as
    // reusable as an imported one, so both write through here and both
    // are then served synchronously/offline from this same table.
    struct ImportedLocator {
        QString grid;
        QString name;
    };

    // Case/whitespace-normalized match, same UPPER(TRIM(...)) convention
    // as knownExchangeForCallsign/DupeChecker::isDupe.
    std::optional<ImportedLocator> importedLocatorForCallsign(const QString& callsign) const;
    // Inserts or overwrites callsign's entry (callsign/grid are stored
    // trimmed+uppercased; `name` may be empty).
    void upsertImportedLocator(const QString& callsign, const QString& grid, const QString& name = QString());
    // Every entry, callsign -> grid (grid may be empty) -- the Check
    // Partial index (core/CheckPartialIndex.h) takes the whole table
    // once per contest switch/logged QSO rather than one lookup per
    // keystroke.
    QHash<QString, QString> allImportedLocators() const;

    // `settings` key/value table.
    QString settingValue(const QString& key, const QString& defaultValue = QString()) const;
    void setSettingValue(const QString& key, const QString& value);

    QSqlDatabase& db();
    const QSqlDatabase& db() const;

private:
    bool ensureSchema();

    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_lastError;
    bool m_open = false;
    int m_qsoWriteCounter = 0;
};

} // namespace Contestprogramm
