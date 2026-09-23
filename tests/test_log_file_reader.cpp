#include <QtTest>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/EdiExporter.h"
#include "data/LogFileReader.h"
#include "data/QsoRecord.h"

#include <memory>

using namespace Contestprogramm;

namespace {

const char* kIaruJson = R"JSON(
{
  "id": "IARU_R1_VHF_UHF", "name": "IARU Region 1 VHF/UHF Contest", "bands": ["144", "432"],
  "dupe_scope": ["callsign", "band", "mode"],
  "exchange_fields": [
    { "key": "rst",    "label": "RST",  "type": "rst" },
    { "key": "serial", "label": "Nr.",  "type": "int", "auto_increment": true },
    { "key": "grid",   "label": "Grid", "type": "grid6" }
  ]
}
)JSON";

QsoRecord makeQso(const QString& call, const QString& band, const QString& mode, const QString& time, int serial,
                  const QString& grid, double km)
{
    QsoRecord r;
    r.callsign = call;
    r.band = band;
    r.mode = mode;
    r.timestampUtc = QStringLiteral("2026-10-03T%1:00Z").arg(time);
    r.gridSquare = grid;
    r.distanceKm = km;
    r.serialSent = serial;
    r.serialRcvd = 7;
    r.rstSent = mode == QStringLiteral("CW") ? QStringLiteral("599") : QStringLiteral("59");
    r.rstRcvd = r.rstSent;
    r.exchangeRcvd = QStringLiteral("%1 007 %2").arg(r.rstRcvd, grid);
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    return r;
}

} // namespace

class TestLogFileReader : public QObject
{
    Q_OBJECT

private slots:
    void detectsFormats();
    void bandLabels();
    void ediRoundTripThroughTheExporter();
    void parsesAdifFromAnotherLogger();
    void controllerWritesALastBackupOnTheWayOut();
};

void TestLogFileReader::detectsFormats()
{
    QCOMPARE(LogFileReader::detect("[REG1TEST;1]\r\nTName=x\r\n"), LogFileReader::Format::Edi);
    QCOMPARE(LogFileReader::detect("<ADIF_VER:5>3.1.4<EOH>\n<CALL:6>DL1ABC<EOR>\n"), LogFileReader::Format::Adif);
    QCOMPARE(LogFileReader::detect("nothing", QStringLiteral("x.EDI")), LogFileReader::Format::Edi);
    QCOMPARE(LogFileReader::detect("nothing", QStringLiteral("log.adi")), LogFileReader::Format::Adif);
    QCOMPARE(LogFileReader::detect("nothing", QStringLiteral("log.txt")), LogFileReader::Format::Unknown);
    QVERIFY(LogFileReader::parse("nothing", QStringLiteral("log.txt")).isEmpty());
}

void TestLogFileReader::bandLabels()
{
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("144 MHz")), QStringLiteral("144"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("432 MHz")), QStringLiteral("432"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("1,3 GHz")), QStringLiteral("1296"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("2m")), QStringLiteral("144"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("70CM")), QStringLiteral("432"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("144")), QStringLiteral("144"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("HF")), QString());
    // Kurzwelle: ein ADIF aus einem anderen Logbuch schreibt "20M",
    // nicht "14". Bis 2026-09-23 kam hier ein leeres Band heraus und
    // das QSO landete beim Import ohne Band im Log.
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("20M")), QStringLiteral("14"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("160m")), QStringLiteral("1.8"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("80m")), QStringLiteral("3.5"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("40m")), QStringLiteral("7"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("30m")), QStringLiteral("10"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("17m")), QStringLiteral("18"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("15m")), QStringLiteral("21"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("12m")), QStringLiteral("24"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("10m")), QStringLiteral("28"));
    // Und die beiden Bänder, deren eigener Name keine ganze Zahl ist --
    // die fielen an der alten toInt()-Prüfung durch.
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("1.8")), QStringLiteral("1.8"));
    QCOMPARE(LogFileReader::bandFromLabel(QStringLiteral("3.5")), QStringLiteral("3.5"));
}

void TestLogFileReader::ediRoundTripThroughTheExporter()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("rt.sqlite")), QStringLiteral("reader_rt")));
    QString error;
    const ContestDefinition def = ContestDefinition::loadFromJson(QByteArray(kIaruJson), &error);
    QVERIFY2(def.isValid(), qPrintable(error));

    QsoRecord a = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("14:01"), 1, QStringLiteral("JN58SD"), 187.4);
    QsoRecord b = makeQso(QStringLiteral("OE3XYZ"), QStringLiteral("144"), QStringLiteral("CW"), QStringLiteral("23:59"), 2, QStringLiteral("JN88TC"), 214.6);
    QsoRecord c = makeQso(QStringLiteral("OE5XYZ"), QStringLiteral("432"), QStringLiteral("FM"), QStringLiteral("15:00"), 3, QStringLiteral("JN67UT"), 0.0);
    QVERIFY(db.insertQso(a));
    QVERIFY(db.insertQso(b));
    QVERIFY(db.insertQso(c));

    ContestSettings settings;
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    EdiExporter exporter(db);
    const QByteArray file144 = exporter.exportBand(def.id(), QStringLiteral("144"), def, settings, EdiStationInfo()).toLatin1();

    const QVector<ImportedQso> qsos = LogFileReader::parse(file144, QStringLiteral("OE5SOS_144MHz.edi"));
    QCOMPARE(qsos.size(), 2);
    QCOMPARE(qsos.at(0).callsign, QStringLiteral("DL1ABC"));
    QCOMPARE(qsos.at(0).grid, QStringLiteral("JN58SD"));
    QCOMPARE(qsos.at(0).band, QStringLiteral("144"));
    QCOMPARE(qsos.at(0).mode, QStringLiteral("SSB"));
    QCOMPARE(qsos.at(0).timestampUtc, QStringLiteral("2026-10-03T14:01:00Z"));
    QCOMPARE(qsos.at(1).callsign, QStringLiteral("OE3XYZ"));
    QCOMPARE(qsos.at(1).mode, QStringLiteral("CW"));
    QCOMPARE(qsos.at(1).timestampUtc, QStringLiteral("2026-10-03T23:59:00Z"));
}

void TestLogFileReader::parsesAdifFromAnotherLogger()
{
    const QByteArray adif(
        "Generated by some logger\n<ADIF_VER:5>3.1.4<PROGRAMID:4>N1MM<EOH>\n"
        "<CALL:6>dl1abc <BAND:2>2m <MODE:3>SSB <QSO_DATE:8>20251004 <TIME_ON:6>093015 <GRIDSQUARE:6>jn58sd <EOR>\n"
        "<call:6>OE3XYZ<band:4>70cm<mode:2>CW<qso_date:8>20251004<time_on:4>1015<eor>\n"
        "<CALL:6>HB9ZZZ<BAND:2>2m<EOR>\n"
        "<BAND:2>2m<EOR>\n");
    const QVector<ImportedQso> qsos = LogFileReader::parseAdif(adif);
    QCOMPARE(qsos.size(), 3); // the record without a call is dropped
    QCOMPARE(qsos.at(0).callsign, QStringLiteral("DL1ABC"));
    QCOMPARE(qsos.at(0).grid, QStringLiteral("JN58SD"));
    QCOMPARE(qsos.at(0).band, QStringLiteral("144"));
    QCOMPARE(qsos.at(0).timestampUtc, QStringLiteral("2025-10-04T09:30:15Z"));
    QCOMPARE(qsos.at(1).band, QStringLiteral("432"));
    QCOMPARE(qsos.at(1).mode, QStringLiteral("CW"));
    QCOMPARE(qsos.at(1).timestampUtc, QStringLiteral("2025-10-04T10:15:00Z"));
    QVERIFY(qsos.at(1).grid.isEmpty());
    QCOMPARE(qsos.at(2).callsign, QStringLiteral("HB9ZZZ"));
    QVERIFY(qsos.at(2).timestampUtc.isEmpty());
}

void TestLogFileReader::controllerWritesALastBackupOnTheWayOut()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    {
        AppController controller;
        QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("app.sqlite"))));
        QsoRecord q = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("14:01"), 1, QStringLiteral("JN58SD"), 187.4);
        QVERIFY(controller.database().insertQso(q));
        QVERIFY(QDir(dir.filePath(QStringLiteral("backups"))).entryList({QStringLiteral("*.sqlite")}, QDir::Files).isEmpty());
    }
    QCOMPARE(QDir(dir.filePath(QStringLiteral("backups"))).entryList({QStringLiteral("*.sqlite")}, QDir::Files).size(), 1);
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestLogFileReader tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_log_file_reader.moc"
