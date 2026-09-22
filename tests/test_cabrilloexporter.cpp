#include <QtTest>

#include "BuildInfo.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QLineEdit>
#include <QTemporaryDir>

#include "app/ContestSettings.h"
#include "data/CabrilloExporter.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/QsoRecord.h"
#include "ui/CabrilloExportDialog.h"

using namespace Contestprogramm;

namespace {

// Mirrors resources/contest_definitions/oe_vhf_uhf.json -- embedded here
// so this test does not depend on the resource file's on-disk location.
const char* kOeVhfUhfJson = R"JSON(
{
  "id": "OE_VHF_UHF",
  "name": "ÖVSV OE VHF/UHF Contest",
  "bands": ["144", "432"],
  "dupe_scope": ["callsign", "band", "mode"],
  "exchange_fields": [
    { "key": "serial", "label": "Serial", "type": "int", "auto_increment": true },
    { "key": "grid",   "label": "Grid",   "type": "grid6" }
  ]
}
)JSON";

} // namespace

class TestCabrilloExporter : public QObject
{
    Q_OBJECT

private slots:
    void exportsGoldenBlock();
    void invalidQsoIsExcluded();
    void hfLogCarriesKilohertzAndTheContestsOwnName();
    void categoriesComeFromTheDialogAndAreRemembered();
};

void TestCabrilloExporter::exportsGoldenBlock()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("cabrillo.sqlite")), QStringLiteral("cabrillo_golden")));

    QString defError;
    const ContestDefinition definition = ContestDefinition::loadFromJson(QByteArray(kOeVhfUhfJson), &defError);
    QVERIFY2(definition.isValid(), qPrintable(defError));

    ContestSettings settings;
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN77QT");

    QsoRecord first;
    first.callsign = QStringLiteral("OE1ABC");
    first.band = QStringLiteral("144");
    first.mode = QStringLiteral("SSB");
    first.timestampUtc = QStringLiteral("2026-06-13T12:05:00Z");
    first.exchangeSent = QStringLiteral("001 JN77QT");
    first.exchangeRcvd = QStringLiteral("002 JN88TC");
    first.contestId = definition.id();
    QVERIFY(db.insertQso(first));

    QsoRecord second;
    second.callsign = QStringLiteral("OE3XYZ");
    second.band = QStringLiteral("144");
    second.mode = QStringLiteral("SSB");
    second.timestampUtc = QStringLiteral("2026-06-13T12:11:00Z");
    second.exchangeSent = QStringLiteral("002 JN77QT");
    second.exchangeRcvd = QStringLiteral("017 JN66OS");
    second.contestId = definition.id();
    QVERIFY(db.insertQso(second));

    CabrilloExporter exporter(db);
    const QString actual = exporter.exportContest(definition.id(), definition, settings);

    const QString expected =
        QStringLiteral("START-OF-LOG: 3.0\n")
        + QStringLiteral("CALLSIGN: OE5SOS\n")
        + QStringLiteral("CONTEST: OE_VHF_UHF\n")
        + QStringLiteral("CATEGORY-OPERATOR: SINGLE-OP\n")
        + QStringLiteral("CATEGORY-ASSISTED: NON-ASSISTED\n")
        + QStringLiteral("CATEGORY-BAND: 2M\n")
        + QStringLiteral("CATEGORY-MODE: PH\n")
        + QStringLiteral("CATEGORY-POWER: LOW\n")
        + QStringLiteral("CATEGORY-STATION: FIXED\n")
        + QStringLiteral("CATEGORY-TRANSMITTER: ONE\n")
        + QStringLiteral("GRID-LOCATOR: JN77QT\n")
        + QStringLiteral("CREATED-BY: Contestprogramm " CONTESTPROGRAMM_VERSION "\n")
        + QStringLiteral("QSO: 144 PH 2026-06-13 1205 OE5SOS 001 JN77QT OE1ABC 002 JN88TC\n")
        + QStringLiteral("QSO: 144 PH 2026-06-13 1211 OE5SOS 002 JN77QT OE3XYZ 017 JN66OS\n")
        + QStringLiteral("END-OF-LOG:\n");

    QCOMPARE(actual, expected);
}

// Auf Kurzwelle trägt die erste Spalte der QSO-Zeile die Frequenz in
// kHz, nicht das Bandkürzel, CATEGORY-BAND spricht Wellenlängen, und
// CONTEST: muss der Name sein, auf den der Robot hört -- nicht die
// interne Kennung der Definition.
void TestCabrilloExporter::hfLogCarriesKilohertzAndTheContestsOwnName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("kw.sqlite")), QStringLiteral("cabrillo_hf")));

    QString error;
    const ContestDefinition definition = ContestDefinition::loadFromJson(R"JSON(
{
  "id": "KW_PROBE",
  "name": "Kurzwelle",
  "cabrillo_name": "CQ-WW-CW",
  "bands": ["14", "21"],
  "dupe_scope": ["callsign", "band", "mode"],
  "scoring": "qso_count",
  "exchange_fields": [
    { "key": "rst",    "label": "RST", "type": "rst" },
    { "key": "serial", "label": "Nr.", "type": "int", "auto_increment": true }
  ]
}
)JSON", &error);
    QVERIFY2(definition.isValid(), qPrintable(error));

    ContestSettings settings;
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");

    // Mit gemeldeter Frequenz: die zählt, auf 1 kHz genau.
    QsoRecord first;
    first.callsign = QStringLiteral("G3ABC");
    first.band = QStringLiteral("14");
    first.mode = QStringLiteral("CW");
    first.timestampUtc = QStringLiteral("2026-11-28T09:03:00Z");
    first.freqHz = 14025400LL;
    first.exchangeSent = QStringLiteral("599 001");
    first.exchangeRcvd = QStringLiteral("599 014");
    first.contestId = definition.id();
    QVERIFY(db.insertQso(first));

    // Ohne: die untere Bandgrenze, damit die Spalte nie leer bleibt.
    QsoRecord second;
    second.callsign = QStringLiteral("W1XYZ");
    second.band = QStringLiteral("21");
    second.mode = QStringLiteral("CW");
    second.timestampUtc = QStringLiteral("2026-11-28T09:40:00Z");
    second.exchangeSent = QStringLiteral("599 002");
    second.exchangeRcvd = QStringLiteral("599 221");
    second.contestId = definition.id();
    QVERIFY(db.insertQso(second));

    const QString actual = CabrilloExporter(db).exportContest(definition.id(), definition, settings);
    QVERIFY2(actual.contains(QStringLiteral("CONTEST: CQ-WW-CW\n")), qPrintable(actual));
    QVERIFY2(actual.contains(QStringLiteral("CATEGORY-BAND: ALL\n")), qPrintable(actual));
    QVERIFY2(actual.contains(QStringLiteral("QSO: 14025 CW 2026-11-28 0903 OE5SOS 599 001 G3ABC 599 014\n")),
             qPrintable(actual));
    QVERIFY2(actual.contains(QStringLiteral("QSO: 21000 CW 2026-11-28 0940 OE5SOS 599 002 W1XYZ 599 221\n")),
             qPrintable(actual));

    // Ein Ein-Band-Log nennt die Wellenlänge.
    QsoRecord onlyTwenty;
    const ContestDefinition single = ContestDefinition::loadFromJson(R"JSON(
{
  "id": "KW_PROBE_20",
  "name": "Kurzwelle 20 m",
  "bands": ["14"],
  "dupe_scope": ["callsign"],
  "scoring": "qso_count",
  "exchange_fields": [ { "key": "rst", "label": "RST", "type": "rst" } ]
}
)JSON", &error);
    QVERIFY2(single.isValid(), qPrintable(error));
    onlyTwenty = first;
    onlyTwenty.id = -1;
    onlyTwenty.contestId = single.id();
    QVERIFY(db.insertQso(onlyTwenty));
    const QString singleBand = CabrilloExporter(db).exportContest(single.id(), single, settings);
    QVERIFY2(singleBand.contains(QStringLiteral("CATEGORY-BAND: 20M\n")), qPrintable(singleBand));
    // Ohne eigenen Namen bleibt es bei der Kennung -- wie bisher.
    QVERIFY2(singleBand.contains(QStringLiteral("CONTEST: KW_PROBE_20\n")), qPrintable(singleBand));
}

// DXLog.net deliberately has no delete function for a logged QSO
// (dxlog.net/docs/index.php/Menu_Edit, verified for this task) -- a QSO
// marked invalid stays in the operator's own log but must not appear in
// a submitted Cabrillo file.
void TestCabrilloExporter::invalidQsoIsExcluded()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("cabrillo_invalid.sqlite")), QStringLiteral("cabrillo_invalid")));

    QString defError;
    const ContestDefinition definition = ContestDefinition::loadFromJson(QByteArray(kOeVhfUhfJson), &defError);
    QVERIFY2(definition.isValid(), qPrintable(defError));

    ContestSettings settings;
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN77QT");

    QsoRecord good;
    good.callsign = QStringLiteral("OE1ABC");
    good.band = QStringLiteral("144");
    good.mode = QStringLiteral("SSB");
    good.timestampUtc = QStringLiteral("2026-06-13T12:05:00Z");
    good.exchangeSent = QStringLiteral("001 JN77QT");
    good.exchangeRcvd = QStringLiteral("002 JN88TC");
    good.contestId = definition.id();
    QVERIFY(db.insertQso(good));

    QsoRecord invalid;
    invalid.callsign = QStringLiteral("OE9ZZZ");
    invalid.band = QStringLiteral("144");
    invalid.mode = QStringLiteral("SSB");
    invalid.timestampUtc = QStringLiteral("2026-06-13T12:20:00Z");
    invalid.exchangeSent = QStringLiteral("002 JN77QT");
    invalid.exchangeRcvd = QStringLiteral("099 KN05IX");
    invalid.contestId = definition.id();
    QVERIFY(db.insertQso(invalid));
    QVERIFY(db.setQsoInvalid(invalid.id, true));

    CabrilloExporter exporter(db);
    const QString actual = exporter.exportContest(definition.id(), definition, settings);

    QVERIFY(actual.contains(QStringLiteral("OE1ABC")));
    QVERIFY(!actual.contains(QStringLiteral("OE9ZZZ")));
}

// Die Angaben, die kein QSO beantworten kann: einmal im Fenster
// gewählt, in der Einstellungstabelle gemerkt und im Kopf der Datei
// wiederzufinden.
void TestCabrilloExporter::categoriesComeFromTheDialogAndAreRemembered()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("kat.sqlite")), QStringLiteral("cabrillo_kat")));

    // Vorbelegung, solange nichts gespeichert ist.
    const CabrilloCategories fresh = CabrilloCategories::load(db);
    QCOMPARE(fresh.operatorCategory, QStringLiteral("SINGLE-OP"));
    QCOMPARE(fresh.power, QStringLiteral("LOW"));
    QVERIFY(fresh.club.isEmpty());

    CabrilloExportDialog dialog(fresh);
    dialog.findChild<QComboBox*>(QStringLiteral("cabrilloPower"))->setCurrentText(QStringLiteral("QRP"));
    dialog.findChild<QComboBox*>(QStringLiteral("cabrilloAssisted"))->setCurrentText(QStringLiteral("ASSISTED"));
    dialog.findChild<QComboBox*>(QStringLiteral("cabrilloStation"))->setCurrentText(QStringLiteral("PORTABLE"));
    dialog.findChild<QLineEdit*>(QStringLiteral("cabrilloClub"))->setText(QStringLiteral("ADL 501"));
    dialog.findChild<QLineEdit*>(QStringLiteral("cabrilloEmail"))->setText(QStringLiteral("  oe5sos@example.at  "));
    const CabrilloCategories chosen = dialog.categories();
    QCOMPARE(chosen.power, QStringLiteral("QRP"));
    QCOMPARE(chosen.email, QStringLiteral("oe5sos@example.at")); // getrimmt
    chosen.save(db);

    // Beim nächsten Mal stehen sie wieder da.
    const CabrilloCategories again = CabrilloCategories::load(db);
    QCOMPARE(again.power, QStringLiteral("QRP"));
    QCOMPARE(again.assisted, QStringLiteral("ASSISTED"));
    QCOMPARE(again.station, QStringLiteral("PORTABLE"));
    QCOMPARE(again.club, QStringLiteral("ADL 501"));

    QString error;
    const ContestDefinition definition = ContestDefinition::loadFromJson(QByteArray(kOeVhfUhfJson), &error);
    QVERIFY2(definition.isValid(), qPrintable(error));
    ContestSettings settings;
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    const QString text = CabrilloExporter(db).exportContest(definition.id(), definition, settings, again);
    QVERIFY2(text.contains(QStringLiteral("CATEGORY-POWER: QRP\n")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("CATEGORY-ASSISTED: ASSISTED\n")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("CATEGORY-STATION: PORTABLE\n")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("CLUB: ADL 501\n")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("EMAIL: oe5sos@example.at\n")), qPrintable(text));

    // Leeres Feld, keine Zeile -- statt einer leeren CLUB-Zeile, die
    // ein Robot als Angabe liest.
    CabrilloCategories bare;
    const QString bareText = CabrilloExporter(db).exportContest(definition.id(), definition, settings, bare);
    QVERIFY2(!bareText.contains(QStringLiteral("CLUB:")), qPrintable(bareText));
    QVERIFY2(!bareText.contains(QStringLiteral("EMAIL:")), qPrintable(bareText));
}

// Not QTEST_APPLESS_MAIN: QSqlDatabase requires a live QCoreApplication
// instance (thread-affinity bookkeeping in the SQLite driver), which
// APPLESS_MAIN deliberately does not create. QApplication statt
// QCoreApplication seit 2026-09-22: der Kategorien-Dialog unten ist ein
// Fenster (QApplication ist selbst eine QCoreApplication, für die
// Datenbank ändert sich damit nichts).
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestCabrilloExporter tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_cabrilloexporter.moc"
