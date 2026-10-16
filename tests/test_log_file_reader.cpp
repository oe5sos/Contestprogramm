#include <QtTest>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/AdifExporter.h"
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
    r.exchangeSent = QStringLiteral("%1 %2 JN67UT").arg(r.rstSent).arg(serial, 3, 10, QLatin1Char('0'));
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
    void adifRundlaufDurchDenEigenenExport();
    void seitenbandSchreibweisenWerdenZurBetriebsart();
    void ediOhneBetriebsartLiestSichTrotzdem();
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

// Der Rundlauf durch die EIGENEN Dateien: was der ADIF-Export
// schreibt, muss der eigene Leser wieder hereinbekommen. Für EDI steht
// das seit jeher fest (oben), für ADIF nicht -- dabei ist das der Weg,
// den das Journal nach einem Absturz nimmt
// (MainWindow::restoreFromJournal liest ADIF).
void TestLogFileReader::adifRundlaufDurchDenEigenenExport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("adif_rund.sqlite")), QStringLiteral("adif_rund")));

    ContestSettings settings;
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");

    QsoRecord q1 = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"),
                           QStringLiteral("14:01"), 1, QStringLiteral("JN58SD"), 187.4);
    QsoRecord q2 = makeQso(QStringLiteral("OK2XYZ"), QStringLiteral("432"), QStringLiteral("CW"),
                           QStringLiteral("23:59"), 2, QStringLiteral("JN88TC"), 214.6);
    QVERIFY(db.insertQso(q1));
    QVERIFY(db.insertQso(q2));

    const QByteArray adif = AdifExporter(db).exportContest(settings.activeContestId, settings).toUtf8();
    const QVector<ImportedQso> zurueck = LogFileReader::parse(adif, QStringLiteral("journal.adi"));
    QCOMPARE(zurueck.size(), 2);

    QCOMPARE(zurueck.at(0).callsign, QStringLiteral("DL1ABC"));
    QCOMPARE(zurueck.at(0).band, QStringLiteral("144"));
    QCOMPARE(zurueck.at(0).mode, QStringLiteral("SSB"));
    QCOMPARE(zurueck.at(0).grid, QStringLiteral("JN58SD"));
    QCOMPARE(zurueck.at(0).timestampUtc, q1.timestampUtc);
    QCOMPARE(zurueck.at(0).rstSent, q1.rstSent);
    QCOMPARE(zurueck.at(0).rstRcvd, q1.rstRcvd);
    QCOMPARE(zurueck.at(0).serialSent, q1.serialSent.value_or(0));
    QCOMPARE(zurueck.at(0).serialRcvd, q1.serialRcvd.value_or(0));
    QCOMPARE(zurueck.at(0).contestId, q1.contestId);

    QCOMPARE(zurueck.at(1).callsign, QStringLiteral("OK2XYZ"));
    QCOMPARE(zurueck.at(1).band, QStringLiteral("432"));
    QCOMPARE(zurueck.at(1).mode, QStringLiteral("CW"));
    QCOMPARE(zurueck.at(1).timestampUtc, q2.timestampUtc);

    // Und der getauschte Text selbst. Er steht in der Journalzeile
    // (STX_STRING/SRX_STRING, siehe data/AdifExporter.cpp) -- wird er
    // beim Lesen übergangen, steht im wiederhergestellten Log in den
    // Austauschspalten nichts, obwohl alles dagewesen wäre.
    QCOMPARE(zurueck.at(0).exchangeSent, q1.exchangeSent);
    QCOMPARE(zurueck.at(0).exchangeRcvd, q1.exchangeRcvd);
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

// Fremde Logprogramme schreiben in ADIF die SEITENBAND-Schreibweise
// ("USB"/"LSB"), nicht die Betriebsart. Unverändert übernommen ist das
// für dieses Programm eine eigene Betriebsart -- und damit ist das
// nächste QSO mit derselben Station kein Dupe mehr (der Dupe-Umfang
// führt die Betriebsart mit, siehe resources/contest_definitions).
// Ein nicht erkanntes Dupe kostet nicht nur die Punkte, es wird in der
// Prüfung als nicht abgezogenes Dupe angerechnet.
void TestLogFileReader::seitenbandSchreibweisenWerdenZurBetriebsart()
{
    const QByteArray adif(
        "<ADIF_VER:5>3.1.4<EOH>\n"
        "<CALL:6>DL1ABC<BAND:2>2m<MODE:3>USB<QSO_DATE:8>20251004<TIME_ON:4>0930<EOR>\n"
        "<CALL:6>DL2ABC<BAND:2>2m<MODE:3>LSB<QSO_DATE:8>20251004<TIME_ON:4>0931<EOR>\n"
        "<CALL:6>DL3ABC<BAND:2>2m<MODE:3>CWR<QSO_DATE:8>20251004<TIME_ON:4>0932<EOR>\n"
        "<CALL:6>DL4ABC<BAND:2>2m<MODE:5>RTTYR<QSO_DATE:8>20251004<TIME_ON:4>0933<EOR>\n"
        "<CALL:6>DL5ABC<BAND:2>2m<MODE:2>PH<QSO_DATE:8>20251004<TIME_ON:4>0934<EOR>\n"
        "<CALL:6>DL6ABC<BAND:2>2m<MODE:3>FT8<QSO_DATE:8>20251004<TIME_ON:4>0935<EOR>\n");
    const QVector<ImportedQso> qsos = LogFileReader::parseAdif(adif);
    QCOMPARE(qsos.size(), 6);
    QCOMPARE(qsos.at(0).mode, QStringLiteral("SSB"));
    QCOMPARE(qsos.at(1).mode, QStringLiteral("SSB"));
    QCOMPARE(qsos.at(2).mode, QStringLiteral("CW"));
    QCOMPARE(qsos.at(3).mode, QStringLiteral("RTTY"));
    QCOMPARE(qsos.at(4).mode, QStringLiteral("SSB"));
    // Was eine eigene Betriebsart IST, bleibt eine: FT8 ist nicht SSB,
    // und ein Umschreiben "auf gut Glück" wäre hier falsch.
    QCOMPARE(qsos.at(5).mode, QStringLiteral("FT8"));
}

// Und die EDI-Seite derselben Sache: die Spezifikation erlaubt eine
// leere Modusspalte ausdruecklich ("If the mode is not important it can
// be left blank"), der Code 0 heisst "non of below". So ein QSO kommt
// ohne Betriebsart herein -- und muss sich trotzdem speichern lassen,
// siehe tests/test_contestdatabase.cpp
// (einQsoOhneBetriebsartGehtNichtVerloren).
void TestLogFileReader::ediOhneBetriebsartLiestSichTrotzdem()
{
    const QByteArray edi(
        "[REG1TEST;1]\r\n"
        "PBand=144 MHz\r\n"
        "[QSORecords;2]\r\n"
        "261003;1401;DL1ABC;0;59;001;59;003;;JN58SD;188;;N;;\r\n"
        "261003;1405;OK2XYZ;;59;002;59;004;;JN99AA;300;;N;;\r\n");
    const QVector<ImportedQso> qsos = LogFileReader::parseEdi(edi);
    QCOMPARE(qsos.size(), 2);
    QCOMPARE(qsos.at(0).callsign, QStringLiteral("DL1ABC"));
    QVERIFY(qsos.at(0).mode.isEmpty());
    QVERIFY(qsos.at(1).mode.isEmpty());
    QCOMPARE(qsos.at(1).band, QStringLiteral("144"));
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
