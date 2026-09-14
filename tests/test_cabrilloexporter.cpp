#include <QtTest>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "app/ContestSettings.h"
#include "data/CabrilloExporter.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/QsoRecord.h"

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
    const QString actual = exporter.exportContest(definition.id(), definition, settings, QStringLiteral("LOW"));

    const QString expected =
        QStringLiteral("START-OF-LOG: 3.0\n")
        + QStringLiteral("CALLSIGN: OE5SOS\n")
        + QStringLiteral("CONTEST: OE_VHF_UHF\n")
        + QStringLiteral("CATEGORY-OPERATOR: SINGLE-OP\n")
        + QStringLiteral("CATEGORY-BAND: 144\n")
        + QStringLiteral("CATEGORY-MODE: PH\n")
        + QStringLiteral("CATEGORY-POWER: LOW\n")
        + QStringLiteral("CATEGORY-STATION: FIXED\n")
        + QStringLiteral("LOCATION: JN77QT\n")
        + QStringLiteral("CREATED-BY: Contestprogramm 1.0\n")
        + QStringLiteral("QSO: 144 PH 2026-06-13 1205 OE5SOS 001 JN77QT OE1ABC 002 JN88TC\n")
        + QStringLiteral("QSO: 144 PH 2026-06-13 1211 OE5SOS 002 JN77QT OE3XYZ 017 JN66OS\n")
        + QStringLiteral("END-OF-LOG:\n");

    QCOMPARE(actual, expected);
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
    const QString actual = exporter.exportContest(definition.id(), definition, settings, QStringLiteral("LOW"));

    QVERIFY(actual.contains(QStringLiteral("OE1ABC")));
    QVERIFY(!actual.contains(QStringLiteral("OE9ZZZ")));
}

// Not QTEST_APPLESS_MAIN: QSqlDatabase requires a live QCoreApplication
// instance (thread-affinity bookkeeping in the SQLite driver), which
// APPLESS_MAIN deliberately does not create.
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestCabrilloExporter tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_cabrilloexporter.moc"
