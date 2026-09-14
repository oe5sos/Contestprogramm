#include <QtTest>

#include <QApplication>
#include <QTemporaryDir>

#include "app/ContestSettings.h"
#include "core/CallsignLocatorLookup.h"
#include "data/ContestDatabase.h"
#include "ui/SettingsDialog.h"

using namespace Contestprogramm;

// Covers Martin's live "OE1W eingeben sollte automatisch JN77TX zeigen"
// report -- the two extra callsign->grid lookup tiers behind
// CallsignLocatorLookup (core/CallsignLocatorLookup.h): the local/
// imported_locators table (tier 2, always offline-safe) and the pure
// QRZ/HamQTH XML parsing that feeds tier 3 (the actual QNetworkAccessManager
// round trip is a thin, deliberately NOT-unit-tested wrapper around this
// parsing -- see the header's own class comment -- so no test here needs
// a live network or a real QRZ/HamQTH account; that verification is a
// separate manual step only Martin can do).
class TestCallsignLocatorLookup : public QObject
{
    Q_OBJECT

private slots:
    void csvImportValidRowsLand();
    void csvImportSkipsMalformedLinesAndTolerantesHeader();
    void localLookupServesImportedRow();
    void localLookupServesCachedExternalResult();
    void localLookupMissReturnsNullopt();

    void qrzSessionXmlParsesKeyOnSuccess();
    void qrzSessionXmlParsesErrorOnFailure();
    void qrzLookupXmlParsesGridOnSuccess();
    void qrzLookupXmlDetectsSessionFailure();
    void qrzLookupXmlNotFoundIsNotTreatedAsSessionFailure();

    void hamQthSessionXmlParsesSessionIdOnSuccess();
    void hamQthSessionXmlParsesErrorOnFailure();
    void hamQthLookupXmlParsesGridOnSuccess();
    void hamQthLookupXmlDetectsSessionFailure();
    void hamQthLookupXmlNotFoundIsNotTreatedAsSessionFailure();

    void settingsRoundTripsProviderAndCredentials();
    void settingsDialogRoundTripsCallbookFields();
};

namespace {

const QString kCsvFixture = QStringLiteral(
    "callsign,grid,name\n"        // header row -- must be tolerated, not imported
    "OE1W,JN77TX,Bernd\n"         // valid, all three columns
    "oe3xyz , jn88oa\n"           // valid, two columns, mixed case/whitespace
    "OE9ZZZ,NOTAGRID\n"           // malformed grid -- must be skipped
    ",JN77TX\n"                   // empty callsign -- must be skipped
    "OE5SOS\n");                  // only one column -- must be skipped

} // namespace

void TestCallsignLocatorLookup::csvImportValidRowsLand()
{
    const CallsignLocatorLookup::CsvParseResult parsed = CallsignLocatorLookup::parseCsv(kCsvFixture);
    QCOMPARE(parsed.rows.size(), 2);
    QCOMPARE(parsed.rows.at(0).callsign, QStringLiteral("OE1W"));
    QCOMPARE(parsed.rows.at(0).grid, QStringLiteral("JN77TX"));
    QCOMPARE(parsed.rows.at(0).name, QStringLiteral("Bernd"));
    QCOMPARE(parsed.rows.at(1).callsign, QStringLiteral("OE3XYZ"));
    QCOMPARE(parsed.rows.at(1).grid, QStringLiteral("JN88OA"));
    QVERIFY(parsed.rows.at(1).name.isEmpty()); // no third column
}

void TestCallsignLocatorLookup::csvImportSkipsMalformedLinesAndTolerantesHeader()
{
    const CallsignLocatorLookup::CsvParseResult parsed = CallsignLocatorLookup::parseCsv(kCsvFixture);
    // header + invalid-grid row + empty-callsign row + one-column row = 4
    QCOMPARE(parsed.skipped, 4);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("csv_import.sqlite")), QStringLiteral("csv_import")));
    CallsignLocatorLookup lookup(db);

    const QString csvPath = dir.filePath(QStringLiteral("locators.csv"));
    QFile file(csvPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(kCsvFixture.toUtf8());
    file.close();

    QString error;
    const CallsignLocatorLookup::ImportSummary summary = lookup.importCsvFile(csvPath, &error);
    QVERIFY(error.isEmpty());
    QCOMPARE(summary.imported, 2);
    QCOMPARE(summary.skipped, 4);
}

void TestCallsignLocatorLookup::localLookupServesImportedRow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("local_lookup.sqlite")), QStringLiteral("local_lookup")));
    CallsignLocatorLookup lookup(db);

    const QString csvPath = dir.filePath(QStringLiteral("locators.csv"));
    QFile file(csvPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(kCsvFixture.toUtf8());
    file.close();
    QVERIFY(lookup.importCsvFile(csvPath).imported == 2);

    // Case/whitespace-normalized, same convention as
    // ContestDatabase::knownExchangeForCallsign.
    const auto found = lookup.lookupLocal(QStringLiteral(" oe1w "));
    QVERIFY(found.has_value());
    QCOMPARE(*found, QStringLiteral("JN77TX"));
}

void TestCallsignLocatorLookup::localLookupServesCachedExternalResult()
{
    // A tier-3 (QRZ/HamQTH) hit caches into the very same imported_locators
    // table via ContestDatabase::upsertImportedLocator (see
    // CallsignLocatorLookup::startLookupRequest) -- exercised directly
    // here without any real network, since the round trip itself is a
    // thin wrapper this class deliberately does not unit-test (see the
    // header's class comment).
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("cached_external.sqlite")), QStringLiteral("cached_external")));
    CallsignLocatorLookup lookup(db);

    QVERIFY(!lookup.lookupLocal(QStringLiteral("OE1W")).has_value());
    db.upsertImportedLocator(QStringLiteral("OE1W"), QStringLiteral("JN77TX"));

    const auto found = lookup.lookupLocal(QStringLiteral("OE1W"));
    QVERIFY(found.has_value());
    QCOMPARE(*found, QStringLiteral("JN77TX"));
}

void TestCallsignLocatorLookup::localLookupMissReturnsNullopt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("local_miss.sqlite")), QStringLiteral("local_miss")));
    CallsignLocatorLookup lookup(db);

    QVERIFY(!lookup.lookupLocal(QStringLiteral("OE9ZZZ")).has_value());
}

// -- QRZ.com XML fixtures ---------------------------------------------
//
// Response shapes verified live against qrz.com/XML/current_spec.html
// on 2026-09-10 (see CallsignLocatorLookup.h's class comment). The
// <QRZDatabase>/<Session> wrapper below matches the spec's own quoted
// examples; the lookup-success <grid> value is a fixture, not a claim
// about the real OE1W entry.

void TestCallsignLocatorLookup::qrzSessionXmlParsesKeyOnSuccess()
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\" ?>"
        "<QRZDatabase version=\"1.34\">"
        "  <Session>"
        "    <Key>2331uf894c4bd29f3923f3bacf02c532d7bd9</Key>"
        "    <Count>123</Count>"
        "    <GMTime>Sun Aug 16 03:51:47 2012</GMTime>"
        "  </Session>"
        "</QRZDatabase>");
    const auto result = CallsignLocatorLookup::parseQrzSessionXml(xml);
    QVERIFY(result.ok);
    QCOMPARE(result.sessionKey, QStringLiteral("2331uf894c4bd29f3923f3bacf02c532d7bd9"));
}

void TestCallsignLocatorLookup::qrzSessionXmlParsesErrorOnFailure()
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\" ?>"
        "<QRZDatabase version=\"1.34\">"
        "  <Session>"
        "    <Error>Username/password incorrect</Error>"
        "    <GMTime>Sun Nov 16 05:11:58 2003</GMTime>"
        "  </Session>"
        "</QRZDatabase>");
    const auto result = CallsignLocatorLookup::parseQrzSessionXml(xml);
    QVERIFY(!result.ok);
    QCOMPARE(result.error, QStringLiteral("Username/password incorrect"));
}

void TestCallsignLocatorLookup::qrzLookupXmlParsesGridOnSuccess()
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\" ?>"
        "<QRZDatabase version=\"1.34\">"
        "  <Session><Key>abc123</Key></Session>"
        "  <Callsign>"
        "    <call>OE1W</call>"
        "    <grid>JN77TX</grid>"
        "  </Callsign>"
        "</QRZDatabase>");
    const auto result = CallsignLocatorLookup::parseQrzLookupXml(xml);
    QVERIFY(result.found);
    QVERIFY(!result.sessionInvalid);
    QCOMPARE(result.grid, QStringLiteral("JN77TX"));
}

void TestCallsignLocatorLookup::qrzLookupXmlDetectsSessionFailure()
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\" ?>"
        "<QRZDatabase version=\"1.34\">"
        "  <Session><Error>Session Timeout</Error></Session>"
        "</QRZDatabase>");
    const auto result = CallsignLocatorLookup::parseQrzLookupXml(xml);
    QVERIFY(!result.found);
    QVERIFY(result.sessionInvalid);
}

void TestCallsignLocatorLookup::qrzLookupXmlNotFoundIsNotTreatedAsSessionFailure()
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\" ?>"
        "<QRZDatabase version=\"1.34\">"
        "  <Session><Key>abc123</Key><Error>Not found: OE9ZZZ</Error></Session>"
        "</QRZDatabase>");
    const auto result = CallsignLocatorLookup::parseQrzLookupXml(xml);
    QVERIFY(!result.found);
    QVERIFY(!result.sessionInvalid);
    QCOMPARE(result.error, QStringLiteral("Not found: OE9ZZZ"));
}

// -- HamQTH XML fixtures ------------------------------------------------
//
// Response shapes verified live against hamqth.com/developers.php on
// 2026-09-10 (see CallsignLocatorLookup.h's class comment).

void TestCallsignLocatorLookup::hamQthSessionXmlParsesSessionIdOnSuccess()
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\"?>"
        "<HamQTH version=\"2.7\" xmlns=\"https://www.hamqth.com\">"
        "<session>"
        "<session_id>09b0ae90050be03c452ad235a1f2915ad684393c</session_id>"
        "</session>"
        "</HamQTH>");
    const auto result = CallsignLocatorLookup::parseHamQthSessionXml(xml);
    QVERIFY(result.ok);
    QCOMPARE(result.sessionKey, QStringLiteral("09b0ae90050be03c452ad235a1f2915ad684393c"));
}

void TestCallsignLocatorLookup::hamQthSessionXmlParsesErrorOnFailure()
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\"?>"
        "<HamQTH version=\"2.7\" xmlns=\"https://www.hamqth.com\">"
        "<session>"
        "<error>Wrong user name or password</error>"
        "</session>"
        "</HamQTH>");
    const auto result = CallsignLocatorLookup::parseHamQthSessionXml(xml);
    QVERIFY(!result.ok);
    QCOMPARE(result.error, QStringLiteral("Wrong user name or password"));
}

void TestCallsignLocatorLookup::hamQthLookupXmlParsesGridOnSuccess()
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\"?>"
        "<HamQTH version=\"2.7\" xmlns=\"https://www.hamqth.com\">"
        "<session><session_id>abc123</session_id></session>"
        "<search>"
        "<callsign>OE1W</callsign>"
        "<grid>JN77TX</grid>"
        "</search>"
        "</HamQTH>");
    const auto result = CallsignLocatorLookup::parseHamQthLookupXml(xml);
    QVERIFY(result.found);
    QVERIFY(!result.sessionInvalid);
    QCOMPARE(result.grid, QStringLiteral("JN77TX"));
}

void TestCallsignLocatorLookup::hamQthLookupXmlDetectsSessionFailure()
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\"?>"
        "<HamQTH version=\"2.7\" xmlns=\"https://www.hamqth.com\">"
        "<session><error>Session does not exist or expired</error></session>"
        "</HamQTH>");
    const auto result = CallsignLocatorLookup::parseHamQthLookupXml(xml);
    QVERIFY(!result.found);
    QVERIFY(result.sessionInvalid);
}

void TestCallsignLocatorLookup::hamQthLookupXmlNotFoundIsNotTreatedAsSessionFailure()
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\"?>"
        "<HamQTH version=\"2.7\" xmlns=\"https://www.hamqth.com\">"
        "<session><session_id>abc123</session_id></session>"
        "<search><error>Callsign not found</error></search>"
        "</HamQTH>");
    const auto result = CallsignLocatorLookup::parseHamQthLookupXml(xml);
    QVERIFY(!result.found);
    QVERIFY(!result.sessionInvalid);
    QCOMPARE(result.error, QStringLiteral("Callsign not found"));
}

// -- ContestSettings / SettingsDialog round trips -----------------------

void TestCallsignLocatorLookup::settingsRoundTripsProviderAndCredentials()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("callbook_settings.sqlite")), QStringLiteral("callbook_settings")));

    ContestSettings settings;
    QCOMPARE(settings.callbookProvider, ContestSettings::CallbookProvider::None); // off by default
    settings.callbookProvider = ContestSettings::CallbookProvider::Qrz;
    settings.callbookUsername = QStringLiteral("oe5sos");
    settings.callbookPassword = QStringLiteral("s3cret");
    settings.saveTo(db);

    ContestSettings reloaded;
    reloaded.loadFrom(db);
    QCOMPARE(reloaded.callbookProvider, ContestSettings::CallbookProvider::Qrz);
    QCOMPARE(reloaded.callbookUsername, QStringLiteral("oe5sos"));
    QCOMPARE(reloaded.callbookPassword, QStringLiteral("s3cret"));
}

void TestCallsignLocatorLookup::settingsDialogRoundTripsCallbookFields()
{
    ContestSettings initial;
    initial.ownCallsign = QStringLiteral("OE5SOS");
    initial.callbookProvider = ContestSettings::CallbookProvider::HamQth;
    initial.callbookUsername = QStringLiteral("oe5sos");
    initial.callbookPassword = QStringLiteral("hunter2");

    // No CallsignLocatorLookup passed (defaults to nullptr) -- the
    // "Locator-Liste importieren..." button is simply disabled then (see
    // SettingsDialog's constructor); the provider/username/password
    // fields themselves do not depend on it.
    SettingsDialog dialog(initial, {});
    const ContestSettings result = dialog.settings();
    QCOMPARE(result.callbookProvider, ContestSettings::CallbookProvider::HamQth);
    QCOMPARE(result.callbookUsername, QStringLiteral("oe5sos"));
    QCOMPARE(result.callbookPassword, QStringLiteral("hunter2"));
}

int main(int argc, char* argv[])
{
    // QApplication (not QTEST_APPLESS_MAIN/QCoreApplication): SettingsDialog
    // is a QDialog subclass, same requirement as test_grid_autofill.cpp's
    // UnifiedLogWidget construction.
    QApplication app(argc, argv);
    TestCallsignLocatorLookup tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_callsign_locator_lookup.moc"
