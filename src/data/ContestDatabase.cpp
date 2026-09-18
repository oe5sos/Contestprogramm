#include "data/ContestDatabase.h"

#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace Contestprogramm {

namespace {

bool execOrRecord(QSqlQuery& query, const QString& sql, QString& lastError)
{
    if (!query.exec(sql)) {
        lastError = query.lastError().text();
        return false;
    }
    return true;
}

} // namespace

ContestDatabase::~ContestDatabase()
{
    close();
}

bool ContestDatabase::open(const QString& path, const QString& connectionName)
{
    close();

    m_connectionName = connectionName;
    if (QSqlDatabase::contains(m_connectionName)) {
        m_db = QSqlDatabase::database(m_connectionName);
    } else {
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    }
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    m_open = true;
    if (!ensureSchema()) {
        close();
        return false;
    }
    return true;
}

void ContestDatabase::close()
{
    if (!m_open) {
        return;
    }
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
    m_open = false;
}

bool ContestDatabase::isOpen() const
{
    return m_open;
}

QString ContestDatabase::lastError() const
{
    return m_lastError;
}

bool ContestDatabase::ensureSchema()
{
    QSqlQuery query(m_db);

    // WAL + NORMAL synchronous per the plan: fine for a single-process
    // desktop logger, avoids the fsync-per-write cost of the FULL
    // default while still being crash-safe for the common case.
    if (!execOrRecord(query, QStringLiteral("PRAGMA journal_mode = WAL"), m_lastError)) {
        return false;
    }
    if (!execOrRecord(query, QStringLiteral("PRAGMA synchronous = NORMAL"), m_lastError)) {
        return false;
    }

    static const QString kCreateQsos = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS qsos ("
        "    id             INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    callsign       TEXT    NOT NULL,"
        "    band           TEXT    NOT NULL,"
        "    mode           TEXT    NOT NULL,"
        "    timestamp_utc  TEXT    NOT NULL,"
        "    freq_hz        INTEGER,"
        "    grid_square    TEXT,"
        "    distance_km    REAL,"
        "    bearing_deg    REAL,"
        "    serial_sent    INTEGER,"
        "    serial_rcvd    INTEGER,"
        "    rst_sent       TEXT,"
        "    rst_rcvd       TEXT,"
        "    exchange_sent  TEXT,"
        "    exchange_rcvd  TEXT,"
        "    contest_id     TEXT    NOT NULL,"
        "    is_dupe        INTEGER NOT NULL DEFAULT 0,"
        "    is_invalid     INTEGER NOT NULL DEFAULT 0,"
        "    source         TEXT    NOT NULL DEFAULT 'manual',"
        "    notes          TEXT"
        ")");
    if (!execOrRecord(query, kCreateQsos, m_lastError)) {
        return false;
    }

    // Migration for a `qsos` table that already existed before
    // rst_sent/rst_rcvd/is_invalid were added (see this task's report):
    // CREATE TABLE IF NOT EXISTS above does not retroactively add
    // columns to Martin's real, already-running database (see
    // MainWindow.cpp's PanelLayout_log comment for another example of
    // this same already-running-database constraint). PRAGMA
    // table_info never errors even on a table that already has every
    // column, so this is safe to run unconditionally on every open().
    {
        QSqlQuery info(m_db);
        if (!execOrRecord(info, QStringLiteral("PRAGMA table_info(qsos)"), m_lastError)) {
            return false;
        }
        QSet<QString> existingColumns;
        while (info.next()) {
            existingColumns.insert(info.value(QStringLiteral("name")).toString());
        }
        struct Migration { QString column; QString ddl; };
        const QVector<Migration> migrations = {
            {QStringLiteral("rst_sent"), QStringLiteral("ALTER TABLE qsos ADD COLUMN rst_sent TEXT")},
            {QStringLiteral("rst_rcvd"), QStringLiteral("ALTER TABLE qsos ADD COLUMN rst_rcvd TEXT")},
            {QStringLiteral("is_invalid"), QStringLiteral("ALTER TABLE qsos ADD COLUMN is_invalid INTEGER NOT NULL DEFAULT 0")},
        };
        for (const Migration& migration : migrations) {
            if (!existingColumns.contains(migration.column)) {
                QSqlQuery alter(m_db);
                if (!execOrRecord(alter, migration.ddl, m_lastError)) {
                    return false;
                }
            }
        }
    }

    if (!execOrRecord(query,
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_qsos_callsign_band_mode ON qsos(callsign, band, mode)"),
            m_lastError)) {
        return false;
    }
    if (!execOrRecord(query,
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_qsos_grid ON qsos(grid_square)"),
            m_lastError)) {
        return false;
    }
    if (!execOrRecord(query,
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_qsos_contest ON qsos(contest_id)"),
            m_lastError)) {
        return false;
    }

    static const QString kCreateSettings = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS settings ("
        "    key   TEXT PRIMARY KEY,"
        "    value TEXT"
        ")");
    if (!execOrRecord(query, kCreateSettings, m_lastError)) {
        return false;
    }

    // Locally-known callsign -> grid table (see ContestDatabase::
    // ImportedLocator / core/CallsignLocatorLookup.h): imported once from
    // a CSV (N1MM+ "Call History File" concept) and/or grown by caching
    // successful external QRZ/HamQTH lookups, so it stays fully queryable
    // offline for the rest of the contest either way.
    static const QString kCreateImportedLocators = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS imported_locators ("
        "    callsign TEXT PRIMARY KEY,"
        "    grid     TEXT NOT NULL,"
        "    name     TEXT"
        ")");
    if (!execOrRecord(query, kCreateImportedLocators, m_lastError)) {
        return false;
    }

    return true;
}

bool ContestDatabase::insertQso(QsoRecord& record)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO qsos "
        "(callsign, band, mode, timestamp_utc, freq_hz, grid_square, distance_km, bearing_deg, "
        " serial_sent, serial_rcvd, rst_sent, rst_rcvd, exchange_sent, exchange_rcvd, contest_id, is_dupe, "
        " is_invalid, source, notes) "
        "VALUES "
        "(:callsign, :band, :mode, :timestamp_utc, :freq_hz, :grid_square, :distance_km, :bearing_deg, "
        " :serial_sent, :serial_rcvd, :rst_sent, :rst_rcvd, :exchange_sent, :exchange_rcvd, :contest_id, :is_dupe, "
        " :is_invalid, :source, :notes)"));

    query.bindValue(QStringLiteral(":callsign"), record.callsign);
    query.bindValue(QStringLiteral(":band"), record.band);
    query.bindValue(QStringLiteral(":mode"), record.mode);
    query.bindValue(QStringLiteral(":timestamp_utc"), record.timestampUtc);
    query.bindValue(QStringLiteral(":freq_hz"),
                     record.freqHz ? QVariant(static_cast<qlonglong>(*record.freqHz)) : QVariant(QMetaType(QMetaType::LongLong)));
    query.bindValue(QStringLiteral(":grid_square"), record.gridSquare);
    query.bindValue(QStringLiteral(":distance_km"),
                     record.distanceKm ? QVariant(*record.distanceKm) : QVariant(QMetaType(QMetaType::Double)));
    query.bindValue(QStringLiteral(":bearing_deg"),
                     record.bearingDeg ? QVariant(*record.bearingDeg) : QVariant(QMetaType(QMetaType::Double)));
    query.bindValue(QStringLiteral(":serial_sent"),
                     record.serialSent ? QVariant(*record.serialSent) : QVariant(QMetaType(QMetaType::Int)));
    query.bindValue(QStringLiteral(":serial_rcvd"),
                     record.serialRcvd ? QVariant(*record.serialRcvd) : QVariant(QMetaType(QMetaType::Int)));
    query.bindValue(QStringLiteral(":rst_sent"), record.rstSent);
    query.bindValue(QStringLiteral(":rst_rcvd"), record.rstRcvd);
    query.bindValue(QStringLiteral(":exchange_sent"), record.exchangeSent);
    query.bindValue(QStringLiteral(":exchange_rcvd"), record.exchangeRcvd);
    query.bindValue(QStringLiteral(":contest_id"), record.contestId);
    query.bindValue(QStringLiteral(":is_dupe"), record.isDupe ? 1 : 0);
    query.bindValue(QStringLiteral(":is_invalid"), record.isInvalid ? 1 : 0);
    query.bindValue(QStringLiteral(":source"), record.source);
    query.bindValue(QStringLiteral(":notes"), record.notes);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    record.id = query.lastInsertId().toInt();
    ++m_qsoWriteCounter;
    return true;
}

namespace {

QsoRecord recordFromQuery(const QSqlQuery& query)
{
    QsoRecord record;
    record.id = query.value(QStringLiteral("id")).toInt();
    record.callsign = query.value(QStringLiteral("callsign")).toString();
    record.band = query.value(QStringLiteral("band")).toString();
    record.mode = query.value(QStringLiteral("mode")).toString();
    record.timestampUtc = query.value(QStringLiteral("timestamp_utc")).toString();
    const QVariant freq = query.value(QStringLiteral("freq_hz"));
    if (!freq.isNull()) {
        record.freqHz = freq.toLongLong();
    }
    record.gridSquare = query.value(QStringLiteral("grid_square")).toString();
    const QVariant distance = query.value(QStringLiteral("distance_km"));
    if (!distance.isNull()) {
        record.distanceKm = distance.toDouble();
    }
    const QVariant bearing = query.value(QStringLiteral("bearing_deg"));
    if (!bearing.isNull()) {
        record.bearingDeg = bearing.toDouble();
    }
    const QVariant serialSent = query.value(QStringLiteral("serial_sent"));
    if (!serialSent.isNull()) {
        record.serialSent = serialSent.toInt();
    }
    const QVariant serialRcvd = query.value(QStringLiteral("serial_rcvd"));
    if (!serialRcvd.isNull()) {
        record.serialRcvd = serialRcvd.toInt();
    }
    record.rstSent = query.value(QStringLiteral("rst_sent")).toString();
    record.rstRcvd = query.value(QStringLiteral("rst_rcvd")).toString();
    record.exchangeSent = query.value(QStringLiteral("exchange_sent")).toString();
    record.exchangeRcvd = query.value(QStringLiteral("exchange_rcvd")).toString();
    record.contestId = query.value(QStringLiteral("contest_id")).toString();
    record.isDupe = query.value(QStringLiteral("is_dupe")).toInt() != 0;
    record.isInvalid = query.value(QStringLiteral("is_invalid")).toInt() != 0;
    record.source = query.value(QStringLiteral("source")).toString();
    record.notes = query.value(QStringLiteral("notes")).toString();
    return record;
}

} // namespace

QVector<QsoRecord> ContestDatabase::qsosForContest(const QString& contestId) const
{
    QVector<QsoRecord> records;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT * FROM qsos WHERE contest_id = :contest_id ORDER BY id ASC"));
    query.bindValue(QStringLiteral(":contest_id"), contestId);
    if (!query.exec()) {
        return records;
    }
    while (query.next()) {
        records.append(recordFromQuery(query));
    }
    return records;
}

QVector<QsoRecord> ContestDatabase::qsosWithGrid(const QString& contestId) const
{
    QVector<QsoRecord> records;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT * FROM qsos WHERE contest_id = :contest_id "
        "AND grid_square IS NOT NULL AND TRIM(grid_square) != '' ORDER BY id ASC"));
    query.bindValue(QStringLiteral(":contest_id"), contestId);
    if (!query.exec()) {
        return records;
    }
    while (query.next()) {
        records.append(recordFromQuery(query));
    }
    return records;
}

int ContestDatabase::qsoCountForContest(const QString& contestId) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM qsos WHERE contest_id = :contest_id"));
    query.bindValue(QStringLiteral(":contest_id"), contestId);
    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

int ContestDatabase::qsoCountSince(const QString& contestId, const QDateTime& sinceUtc) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM qsos WHERE contest_id = :contest_id AND timestamp_utc >= :since"));
    query.bindValue(QStringLiteral(":contest_id"), contestId);
    query.bindValue(QStringLiteral(":since"), sinceUtc.toUTC().toString(Qt::ISODate));
    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

int ContestDatabase::nextSerialForContest(const QString& contestId, const QString& band) const
{
    QSqlQuery query(m_db);
    if (band.isEmpty()) {
        query.prepare(QStringLiteral("SELECT MAX(serial_sent) FROM qsos WHERE contest_id = :contest_id"));
    } else {
        query.prepare(QStringLiteral("SELECT MAX(serial_sent) FROM qsos WHERE contest_id = :contest_id AND band = :band"));
        query.bindValue(QStringLiteral(":band"), band);
    }
    query.bindValue(QStringLiteral(":contest_id"), contestId);
    if (!query.exec() || !query.next() || query.value(0).isNull()) {
        return 1;
    }
    return query.value(0).toInt() + 1;
}

std::optional<ContestDatabase::KnownCallsignExchange> ContestDatabase::knownExchangeForCallsign(
    const QString& callsign, const QString& contestId) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT grid_square, exchange_rcvd, serial_rcvd FROM qsos "
        "WHERE UPPER(TRIM(callsign)) = UPPER(TRIM(:callsign)) AND contest_id = :contest_id "
        "ORDER BY timestamp_utc DESC LIMIT 1"));
    query.bindValue(QStringLiteral(":callsign"), callsign);
    query.bindValue(QStringLiteral(":contest_id"), contestId);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    KnownCallsignExchange result;
    result.gridSquare = query.value(0).toString();
    result.exchangeRcvd = query.value(1).toString();
    const QVariant serialRcvd = query.value(2);
    if (!serialRcvd.isNull()) {
        result.serialRcvd = serialRcvd.toInt();
    }
    return result;
}

std::optional<QsoRecord> ContestDatabase::qsoById(int id) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT * FROM qsos WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    return recordFromQuery(query);
}

bool ContestDatabase::updateQsoCallsign(int id, const QString& callsign, QString* errorOut)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE qsos SET callsign = :callsign WHERE id = :id"));
    query.bindValue(QStringLiteral(":callsign"), callsign);
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        if (errorOut) {
            *errorOut = m_lastError;
        }
        return false;
    }
    ++m_qsoWriteCounter;
    return true;
}

bool ContestDatabase::updateQsoExchangeRcvd(int id, const QString& exchangeRcvd, const QString& gridSquare,
                                             const std::optional<int>& serialRcvd, const QString& rstRcvd,
                                             const std::optional<double>& distanceKm,
                                             const std::optional<double>& bearingDeg, QString* errorOut)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE qsos SET exchange_rcvd = :exchange_rcvd, grid_square = :grid_square, "
        "serial_rcvd = :serial_rcvd, rst_rcvd = :rst_rcvd, distance_km = :distance_km, "
        "bearing_deg = :bearing_deg WHERE id = :id"));
    query.bindValue(QStringLiteral(":exchange_rcvd"), exchangeRcvd);
    query.bindValue(QStringLiteral(":grid_square"), gridSquare);
    query.bindValue(QStringLiteral(":serial_rcvd"), serialRcvd ? QVariant(*serialRcvd) : QVariant(QMetaType(QMetaType::Int)));
    query.bindValue(QStringLiteral(":rst_rcvd"), rstRcvd);
    query.bindValue(QStringLiteral(":distance_km"), distanceKm ? QVariant(*distanceKm) : QVariant(QMetaType(QMetaType::Double)));
    query.bindValue(QStringLiteral(":bearing_deg"), bearingDeg ? QVariant(*bearingDeg) : QVariant(QMetaType(QMetaType::Double)));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        if (errorOut) {
            *errorOut = m_lastError;
        }
        return false;
    }
    ++m_qsoWriteCounter;
    return true;
}

bool ContestDatabase::setQsoInvalid(int id, bool invalid, QString* errorOut)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE qsos SET is_invalid = :is_invalid WHERE id = :id"));
    query.bindValue(QStringLiteral(":is_invalid"), invalid ? 1 : 0);
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        if (errorOut) {
            *errorOut = m_lastError;
        }
        return false;
    }
    ++m_qsoWriteCounter;
    return true;
}

bool ContestDatabase::updateQsoTimestamp(int id, const QString& timestampUtc, QString* errorOut)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE qsos SET timestamp_utc = :timestamp_utc WHERE id = :id"));
    query.bindValue(QStringLiteral(":timestamp_utc"), timestampUtc);
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        if (errorOut) {
            *errorOut = m_lastError;
        }
        return false;
    }
    ++m_qsoWriteCounter;
    return true;
}

int ContestDatabase::archiveContest(const QString& contestId, const QString& archiveId, QString* errorOut)
{
    if (contestId.isEmpty() || archiveId.isEmpty() || contestId == archiveId) {
        if (errorOut) {
            *errorOut = QStringLiteral("archive id must differ from the contest id");
        }
        return -1;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE qsos SET contest_id = :archive_id WHERE contest_id = :contest_id"));
    query.bindValue(QStringLiteral(":archive_id"), archiveId);
    query.bindValue(QStringLiteral(":contest_id"), contestId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        if (errorOut) {
            *errorOut = m_lastError;
        }
        return -1;
    }
    ++m_qsoWriteCounter;
    return query.numRowsAffected();
}

QStringList ContestDatabase::contestIdsInLog() const
{
    QStringList result;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT DISTINCT contest_id FROM qsos ORDER BY contest_id"))) {
        return result;
    }
    while (query.next()) {
        result << query.value(0).toString();
    }
    return result;
}

bool ContestDatabase::backupTo(const QString& path, QString* errorOut)
{
    QSqlQuery query(m_db);
    // The path goes in as a string literal: VACUUM INTO takes no bound
    // parameter in SQLite, so a quote in the path is doubled by hand.
    QString quoted = path;
    quoted.replace(QLatin1Char('\''), QStringLiteral("''"));
    if (!query.exec(QStringLiteral("VACUUM INTO '%1'").arg(quoted))) {
        m_lastError = query.lastError().text();
        if (errorOut) {
            *errorOut = m_lastError;
        }
        return false;
    }
    return true;
}

std::optional<QString> ContestDatabase::lastKnownGridForCallsign(const QString& callsign) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT grid_square FROM qsos "
        "WHERE UPPER(TRIM(callsign)) = UPPER(TRIM(:callsign)) AND is_invalid = 0 "
        "AND grid_square IS NOT NULL AND TRIM(grid_square) != '' "
        "ORDER BY timestamp_utc DESC LIMIT 1"));
    query.bindValue(QStringLiteral(":callsign"), callsign);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    return query.value(0).toString().trimmed().toUpper();
}

QHash<QString, QString> ContestDatabase::allImportedLocators() const
{
    QHash<QString, QString> result;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT callsign, grid FROM imported_locators"))) {
        return result;
    }
    while (query.next()) {
        const QString call = query.value(0).toString().trimmed().toUpper();
        if (!call.isEmpty()) {
            result.insert(call, query.value(1).toString().trimmed().toUpper());
        }
    }
    return result;
}

std::optional<ContestDatabase::ImportedLocator> ContestDatabase::importedLocatorForCallsign(const QString& callsign) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT grid, name FROM imported_locators WHERE UPPER(TRIM(callsign)) = UPPER(TRIM(:callsign))"));
    query.bindValue(QStringLiteral(":callsign"), callsign);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    ImportedLocator result;
    result.grid = query.value(0).toString();
    result.name = query.value(1).toString();
    return result;
}

void ContestDatabase::upsertImportedLocator(const QString& callsign, const QString& grid, const QString& name)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO imported_locators (callsign, grid, name) VALUES (:callsign, :grid, :name) "
        "ON CONFLICT(callsign) DO UPDATE SET grid = excluded.grid, name = excluded.name"));
    query.bindValue(QStringLiteral(":callsign"), callsign.trimmed().toUpper());
    query.bindValue(QStringLiteral(":grid"), grid.trimmed().toUpper());
    query.bindValue(QStringLiteral(":name"), name.trimmed());
    if (!query.exec()) {
        m_lastError = query.lastError().text();
    }
}

QString ContestDatabase::settingValue(const QString& key, const QString& defaultValue) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = :key"));
    query.bindValue(QStringLiteral(":key"), key);
    if (!query.exec() || !query.next()) {
        return defaultValue;
    }
    return query.value(0).toString();
}

void ContestDatabase::setSettingValue(const QString& key, const QString& value)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT INTO settings (key, value) VALUES (:key, :value) "
                                  "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    query.bindValue(QStringLiteral(":key"), key);
    query.bindValue(QStringLiteral(":value"), value);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
    }
}

QSqlDatabase& ContestDatabase::db()
{
    return m_db;
}

const QSqlDatabase& ContestDatabase::db() const
{
    return m_db;
}

} // namespace Contestprogramm
