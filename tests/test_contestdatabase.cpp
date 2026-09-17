#include <QtTest>

#include <QCoreApplication>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTemporaryDir>

#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"

using namespace Contestprogramm;

class TestContestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void schemaIsCreated();
    void insertAndQueryRoundTrip();
    void nullableFieldsRoundTripAsNull();
    void indexIsUsedForDupeLookup();
    void settingsKeyValueRoundTrip();
    void nextSerialIncrementsPerContest();
    void rstAndInvalidRoundTrip();
    void qsoByIdReturnsSingleRecord();
    void updateQsoCallsignPersists();
    void updateQsoExchangeRcvdPersists();
    void setQsoInvalidTogglesFlag();
    void updateQsoTimestampPersistsAndCountsWrites();
    void archiveContestMovesQsosAndKeepsTheirLocators();
    void preExistingTableWithoutNewColumnsIsMigrated();
};

void TestContestDatabase::schemaIsCreated()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("schema.sqlite")), QStringLiteral("test_schema")));

    QSqlQuery query(db.db());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type='table' AND name IN ('qsos', 'settings')")));
    int tableCount = 0;
    while (query.next()) {
        ++tableCount;
    }
    QCOMPARE(tableCount, 2);

    QVERIFY(query.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type='index' AND name IN "
        "('idx_qsos_callsign_band_mode', 'idx_qsos_grid', 'idx_qsos_contest')")));
    int indexCount = 0;
    while (query.next()) {
        ++indexCount;
    }
    QCOMPARE(indexCount, 3);
}

void TestContestDatabase::insertAndQueryRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("roundtrip.sqlite")), QStringLiteral("test_roundtrip")));

    QsoRecord record;
    record.callsign = QStringLiteral("OE1ABC");
    record.band = QStringLiteral("144");
    record.mode = QStringLiteral("SSB");
    record.timestampUtc = QStringLiteral("2026-06-13T12:05:00Z");
    record.freqHz = 144300000;
    record.gridSquare = QStringLiteral("JN88TC");
    record.distanceKm = 170.65;
    record.bearingDeg = 78.21;
    record.serialSent = 1;
    record.serialRcvd = 2;
    record.exchangeSent = QStringLiteral("001 JN77QT");
    record.exchangeRcvd = QStringLiteral("002 JN88TC");
    record.contestId = QStringLiteral("OE_VHF_UHF");
    record.isDupe = false;
    record.source = QStringLiteral("manual");
    record.notes = QStringLiteral("first contact");

    QVERIFY(db.insertQso(record));
    QVERIFY(record.id > 0);

    const QVector<QsoRecord> fetched = db.qsosForContest(QStringLiteral("OE_VHF_UHF"));
    QCOMPARE(fetched.size(), 1);
    const QsoRecord& back = fetched.first();
    QCOMPARE(back.id, record.id);
    QCOMPARE(back.callsign, record.callsign);
    QCOMPARE(back.band, record.band);
    QCOMPARE(back.mode, record.mode);
    QCOMPARE(back.timestampUtc, record.timestampUtc);
    QVERIFY(back.freqHz.has_value());
    QCOMPARE(*back.freqHz, *record.freqHz);
    QCOMPARE(back.gridSquare, record.gridSquare);
    QVERIFY(back.distanceKm.has_value());
    QCOMPARE(*back.distanceKm, *record.distanceKm);
    QVERIFY(back.bearingDeg.has_value());
    QCOMPARE(*back.bearingDeg, *record.bearingDeg);
    QVERIFY(back.serialSent.has_value());
    QCOMPARE(*back.serialSent, *record.serialSent);
    QVERIFY(back.serialRcvd.has_value());
    QCOMPARE(*back.serialRcvd, *record.serialRcvd);
    QCOMPARE(back.exchangeSent, record.exchangeSent);
    QCOMPARE(back.exchangeRcvd, record.exchangeRcvd);
    QCOMPARE(back.contestId, record.contestId);
    QCOMPARE(back.isDupe, record.isDupe);
    QCOMPARE(back.source, record.source);
    QCOMPARE(back.notes, record.notes);

    QCOMPARE(db.qsoCountForContest(QStringLiteral("OE_VHF_UHF")), 1);
    QCOMPARE(db.qsoCountForContest(QStringLiteral("IARU_R1_VHF_UHF")), 0);
}

void TestContestDatabase::nullableFieldsRoundTripAsNull()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("nulls.sqlite")), QStringLiteral("test_nulls")));

    QsoRecord record;
    record.callsign = QStringLiteral("OE2XYZ");
    record.band = QStringLiteral("432");
    record.mode = QStringLiteral("FM");
    record.timestampUtc = QStringLiteral("2026-06-13T13:00:00Z");
    record.contestId = QStringLiteral("OE_VHF_UHF");
    // freqHz, gridSquare-derived distance/bearing, serials all left unset.

    QVERIFY(db.insertQso(record));

    const QVector<QsoRecord> fetched = db.qsosForContest(QStringLiteral("OE_VHF_UHF"));
    QCOMPARE(fetched.size(), 1);
    QVERIFY(!fetched.first().freqHz.has_value());
    QVERIFY(!fetched.first().distanceKm.has_value());
    QVERIFY(!fetched.first().bearingDeg.has_value());
    QVERIFY(!fetched.first().serialSent.has_value());
    QVERIFY(!fetched.first().serialRcvd.has_value());
}

void TestContestDatabase::indexIsUsedForDupeLookup()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("index.sqlite")), QStringLiteral("test_index")));

    QSqlQuery plan(db.db());
    QVERIFY(plan.exec(QStringLiteral(
        "EXPLAIN QUERY PLAN SELECT id FROM qsos WHERE callsign = 'OE1ABC' AND band = '144' AND mode = 'SSB'")));
    QString planText;
    while (plan.next()) {
        planText += plan.value(plan.record().indexOf(QStringLiteral("detail"))).toString();
        planText += QLatin1Char('\n');
    }
    QVERIFY2(planText.contains(QStringLiteral("idx_qsos_callsign_band_mode")),
             qPrintable(QStringLiteral("query plan did not use the dupe index:\n%1").arg(planText)));
}

void TestContestDatabase::settingsKeyValueRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("settings.sqlite")), QStringLiteral("test_settings")));

    QCOMPARE(db.settingValue(QStringLiteral("own_callsign"), QStringLiteral("none")), QStringLiteral("none"));
    db.setSettingValue(QStringLiteral("own_callsign"), QStringLiteral("OE5SOS"));
    QCOMPARE(db.settingValue(QStringLiteral("own_callsign")), QStringLiteral("OE5SOS"));

    // Overwrite should replace, not duplicate the row.
    db.setSettingValue(QStringLiteral("own_callsign"), QStringLiteral("OE5XYZ"));
    QCOMPARE(db.settingValue(QStringLiteral("own_callsign")), QStringLiteral("OE5XYZ"));

    QSqlQuery count(db.db());
    QVERIFY(count.exec(QStringLiteral("SELECT COUNT(*) FROM settings WHERE key='own_callsign'")));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 1);
}

void TestContestDatabase::nextSerialIncrementsPerContest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("serial.sqlite")), QStringLiteral("test_serial")));

    QCOMPARE(db.nextSerialForContest(QStringLiteral("OE_VHF_UHF")), 1);

    QsoRecord r1;
    r1.callsign = QStringLiteral("OE1AAA");
    r1.band = QStringLiteral("144");
    r1.mode = QStringLiteral("SSB");
    r1.timestampUtc = QStringLiteral("2026-06-13T12:00:00Z");
    r1.contestId = QStringLiteral("OE_VHF_UHF");
    r1.serialSent = db.nextSerialForContest(QStringLiteral("OE_VHF_UHF"));
    QVERIFY(db.insertQso(r1));
    QCOMPARE(db.nextSerialForContest(QStringLiteral("OE_VHF_UHF")), 2);

    // A different contest's serials are independent.
    QCOMPARE(db.nextSerialForContest(QStringLiteral("IARU_R1_VHF_UHF")), 1);
}

// The RST exchange field (see this task's report -- new in both shipped
// contest_definitions/*.json) and the invalid-mark toggle (DXLog.net
// deliberately has no delete function for a logged QSO, dxlog.net/docs/
// index.php/Menu_Edit, verified for this task) both get real dedicated
// columns, round-tripping the same way every other QsoRecord field
// already does.
void TestContestDatabase::rstAndInvalidRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("rst.sqlite")), QStringLiteral("test_rst")));

    QsoRecord record;
    record.callsign = QStringLiteral("OE1ABC");
    record.band = QStringLiteral("144");
    record.mode = QStringLiteral("CW");
    record.timestampUtc = QStringLiteral("2026-06-13T12:05:00Z");
    record.contestId = QStringLiteral("OE_VHF_UHF");
    record.rstSent = QStringLiteral("599");
    record.rstRcvd = QStringLiteral("589");
    record.isInvalid = true;

    QVERIFY(db.insertQso(record));
    const QVector<QsoRecord> fetched = db.qsosForContest(QStringLiteral("OE_VHF_UHF"));
    QCOMPARE(fetched.size(), 1);
    QCOMPARE(fetched.first().rstSent, QStringLiteral("599"));
    QCOMPARE(fetched.first().rstRcvd, QStringLiteral("589"));
    QVERIFY(fetched.first().isInvalid);
}

void TestContestDatabase::qsoByIdReturnsSingleRecord()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("byid.sqlite")), QStringLiteral("test_byid")));

    QsoRecord record;
    record.callsign = QStringLiteral("OE1ABC");
    record.band = QStringLiteral("144");
    record.mode = QStringLiteral("SSB");
    record.timestampUtc = QStringLiteral("2026-06-13T12:05:00Z");
    record.contestId = QStringLiteral("OE_VHF_UHF");
    QVERIFY(db.insertQso(record));

    const auto found = db.qsoById(record.id);
    QVERIFY(found.has_value());
    QCOMPARE(found->callsign, QStringLiteral("OE1ABC"));

    QVERIFY(!db.qsoById(record.id + 999).has_value());
}

// Hand-correcting a logged QSO's callsign in place -- see
// UnifiedLogWidget's History row Call cell.
void TestContestDatabase::updateQsoCallsignPersists()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("updatecall.sqlite")), QStringLiteral("test_updatecall")));

    QsoRecord record;
    record.callsign = QStringLiteral("OE1ABC");
    record.band = QStringLiteral("144");
    record.mode = QStringLiteral("SSB");
    record.timestampUtc = QStringLiteral("2026-06-13T12:05:00Z");
    record.contestId = QStringLiteral("OE_VHF_UHF");
    QVERIFY(db.insertQso(record));

    QVERIFY(db.updateQsoCallsign(record.id, QStringLiteral("OE1XYZ")));
    const auto found = db.qsoById(record.id);
    QVERIFY(found.has_value());
    QCOMPARE(found->callsign, QStringLiteral("OE1XYZ"));
}

// Hand-correcting a logged QSO's received exchange -- see
// UnifiedLogWidget's History row Exch Emp. cell / MainWindow::
// handleHistoryExchangeRcvdEditRequested.
void TestContestDatabase::updateQsoExchangeRcvdPersists()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("updateexch.sqlite")), QStringLiteral("test_updateexch")));

    QsoRecord record;
    record.callsign = QStringLiteral("OE1ABC");
    record.band = QStringLiteral("144");
    record.mode = QStringLiteral("CW");
    record.timestampUtc = QStringLiteral("2026-06-13T12:05:00Z");
    record.contestId = QStringLiteral("OE_VHF_UHF");
    record.exchangeRcvd = QStringLiteral("599 001 JN77QT");
    record.gridSquare = QStringLiteral("JN77QT");
    record.serialRcvd = 1;
    record.rstRcvd = QStringLiteral("599");
    QVERIFY(db.insertQso(record));

    QVERIFY(db.updateQsoExchangeRcvd(record.id, QStringLiteral("599 002 JN88TC"), QStringLiteral("JN88TC"), 2,
                                      QStringLiteral("599"), 123.4, 56.7));

    const auto found = db.qsoById(record.id);
    QVERIFY(found.has_value());
    QCOMPARE(found->exchangeRcvd, QStringLiteral("599 002 JN88TC"));
    QCOMPARE(found->gridSquare, QStringLiteral("JN88TC"));
    QVERIFY(found->serialRcvd.has_value());
    QCOMPARE(*found->serialRcvd, 2);
    QCOMPARE(found->rstRcvd, QStringLiteral("599"));
    QVERIFY(found->distanceKm.has_value());
    QCOMPARE(*found->distanceKm, 123.4);
    QVERIFY(found->bearingDeg.has_value());
    QCOMPARE(*found->bearingDeg, 56.7);
}

// DXLog.net deliberately has no delete function for a logged QSO -- the
// invalid flag is what marks a bad contact instead, and it must be
// freely toggleable both ways (see UnifiedLogWidget's Status-cell
// click, which is a toggle, not a one-way mark).
void TestContestDatabase::archiveContestMovesQsosAndKeepsTheirLocators()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("archive.sqlite")), QStringLiteral("test_archive")));

    for (int i = 0; i < 3; ++i) {
        QsoRecord record;
        record.callsign = QStringLiteral("OE%1ABC").arg(i);
        record.band = QStringLiteral("144");
        record.mode = QStringLiteral("SSB");
        record.timestampUtc = QStringLiteral("2025-10-04T1%1:00:00Z").arg(i);
        record.gridSquare = QStringLiteral("JN58SD");
        record.serialSent = i + 1;
        record.contestId = QStringLiteral("IARU_R1_VHF_UHF");
        QVERIFY(db.insertQso(record));
    }
    QCOMPARE(db.nextSerialForContest(QStringLiteral("IARU_R1_VHF_UHF")), 4);

    QString error;
    QCOMPARE(db.archiveContest(QStringLiteral("IARU_R1_VHF_UHF"), QStringLiteral("IARU_R1_VHF_UHF@2025-10-04"), &error), 3);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    // The active contest is empty again, serials restart, nothing lost.
    QCOMPARE(db.qsoCountForContest(QStringLiteral("IARU_R1_VHF_UHF")), 0);
    QCOMPARE(db.nextSerialForContest(QStringLiteral("IARU_R1_VHF_UHF")), 1);
    QCOMPARE(db.qsoCountForContest(QStringLiteral("IARU_R1_VHF_UHF@2025-10-04")), 3);
    QCOMPARE(db.contestIdsInLog(), QStringList{QStringLiteral("IARU_R1_VHF_UHF@2025-10-04")});
    // ...and the locator memory still knows the stations.
    QCOMPARE(db.lastKnownGridForCallsign(QStringLiteral("OE1ABC")).value_or(QString()), QStringLiteral("JN58SD"));
    // Same id twice, or an empty one: refused, nothing changes.
    QVERIFY(db.archiveContest(QStringLiteral("X"), QStringLiteral("X"), &error) < 0);
    QCOMPARE(db.archiveContest(QStringLiteral("IARU_R1_VHF_UHF"), QStringLiteral("IARU_R1_VHF_UHF@x"), &error), 0);
}

void TestContestDatabase::updateQsoTimestampPersistsAndCountsWrites()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("timestamp.sqlite")), QStringLiteral("test_timestamp")));
    QCOMPARE(db.qsoWriteCounter(), 0);

    QsoRecord record;
    record.callsign = QStringLiteral("OE1ABC");
    record.band = QStringLiteral("144");
    record.mode = QStringLiteral("SSB");
    record.timestampUtc = QStringLiteral("2026-06-13T12:05:00Z");
    record.contestId = QStringLiteral("OE_VHF_UHF");
    QVERIFY(db.insertQso(record));
    QCOMPARE(db.qsoWriteCounter(), 1);

    QVERIFY(db.updateQsoTimestamp(record.id, QStringLiteral("2026-06-13T11:58:00Z")));
    QCOMPARE(db.qsoWriteCounter(), 2);
    const auto fetched = db.qsoById(record.id);
    QVERIFY(fetched.has_value());
    QCOMPARE(fetched->timestampUtc, QStringLiteral("2026-06-13T11:58:00Z"));

    // Settings writes are not QSO writes -- the backup must not wake
    // up for a saved panel position.
    db.setSettingValue(QStringLiteral("x"), QStringLiteral("y"));
    QCOMPARE(db.qsoWriteCounter(), 2);
    QVERIFY(db.setQsoInvalid(record.id, true));
    QCOMPARE(db.qsoWriteCounter(), 3);
}

void TestContestDatabase::setQsoInvalidTogglesFlag()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("toggleinvalid.sqlite")), QStringLiteral("test_toggleinvalid")));

    QsoRecord record;
    record.callsign = QStringLiteral("OE1ABC");
    record.band = QStringLiteral("144");
    record.mode = QStringLiteral("SSB");
    record.timestampUtc = QStringLiteral("2026-06-13T12:05:00Z");
    record.contestId = QStringLiteral("OE_VHF_UHF");
    QVERIFY(db.insertQso(record));
    QVERIFY(!db.qsoById(record.id)->isInvalid);

    QVERIFY(db.setQsoInvalid(record.id, true));
    QVERIFY(db.qsoById(record.id)->isInvalid);

    QVERIFY(db.setQsoInvalid(record.id, false));
    QVERIFY(!db.qsoById(record.id)->isInvalid);
}

// Martin's real, already-running database predates rst_sent/rst_rcvd/
// is_invalid (see ContestDatabase::ensureSchema()'s own migration
// comment) -- simulates that exact situation: a `qsos` table created
// with the OLD column set (opened directly via QSqlDatabase, bypassing
// ContestDatabase entirely, which would otherwise just create the new
// schema from scratch and never exercise the migration path at all),
// then opened through ContestDatabase, which must add the missing
// columns without losing the pre-existing row.
void TestContestDatabase::preExistingTableWithoutNewColumnsIsMigrated()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("migration.sqlite"));

    {
        QSqlDatabase rawDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("test_migration_raw"));
        rawDb.setDatabaseName(path);
        QVERIFY(rawDb.open());
        QSqlQuery create(rawDb);
        QVERIFY2(create.exec(QStringLiteral(
            "CREATE TABLE qsos ("
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
            "    exchange_sent  TEXT,"
            "    exchange_rcvd  TEXT,"
            "    contest_id     TEXT    NOT NULL,"
            "    is_dupe        INTEGER NOT NULL DEFAULT 0,"
            "    source         TEXT    NOT NULL DEFAULT 'manual',"
            "    notes          TEXT"
            ")")),
            qPrintable(create.lastError().text()));
        QSqlQuery insert(rawDb);
        insert.prepare(QStringLiteral(
            "INSERT INTO qsos (callsign, band, mode, timestamp_utc, contest_id) VALUES (:c, :b, :m, :t, :ct)"));
        insert.bindValue(QStringLiteral(":c"), QStringLiteral("OE1OLD"));
        insert.bindValue(QStringLiteral(":b"), QStringLiteral("144"));
        insert.bindValue(QStringLiteral(":m"), QStringLiteral("SSB"));
        insert.bindValue(QStringLiteral(":t"), QStringLiteral("2026-06-01T10:00:00Z"));
        insert.bindValue(QStringLiteral(":ct"), QStringLiteral("OE_VHF_UHF"));
        QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
        rawDb.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("test_migration_raw"));

    ContestDatabase db;
    QVERIFY2(db.open(path, QStringLiteral("test_migration")), qPrintable(db.lastError()));

    const QVector<QsoRecord> records = db.qsosForContest(QStringLiteral("OE_VHF_UHF"));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().callsign, QStringLiteral("OE1OLD"));
    QVERIFY(records.first().rstSent.isEmpty());
    QVERIFY(records.first().rstRcvd.isEmpty());
    QVERIFY(!records.first().isInvalid);

    // The migrated column is fully writable, not just readable-as-null.
    QVERIFY(db.setQsoInvalid(records.first().id, true));
    const auto reread = db.qsoById(records.first().id);
    QVERIFY(reread.has_value());
    QVERIFY(reread->isInvalid);
}

// Not QTEST_APPLESS_MAIN: QSqlDatabase requires a live QCoreApplication
// instance (thread-affinity bookkeeping in the SQLite driver), which
// APPLESS_MAIN deliberately does not create.
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestContestDatabase tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_contestdatabase.moc"
