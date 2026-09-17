#include <QtTest>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/EdiExporter.h"
#include "data/QsoRecord.h"

using namespace Contestprogramm;

namespace {

// Mirrors resources/contest_definitions/iaru_r1_vhf_uhf.json -- embedded
// so the test does not depend on the resource file's on-disk location
// (same approach as test_cabrilloexporter.cpp).
const char* kIaruJson = R"JSON(
{
  "id": "IARU_R1_VHF_UHF",
  "name": "IARU Region 1 VHF/UHF Contest",
  "bands": ["144", "432"],
  "dupe_scope": ["callsign", "band", "mode"],
  "exchange_fields": [
    { "key": "rst",    "label": "RST",  "type": "rst" },
    { "key": "serial", "label": "Nr.",  "type": "int", "auto_increment": true },
    { "key": "grid",   "label": "Grid", "type": "grid6" }
  ]
}
)JSON";

ContestSettings feuerkogelSettings()
{
    ContestSettings settings;
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.ownElevationM = 1592.0;
    settings.antennaHeightM = 10.0;
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    return settings;
}

EdiStationInfo sampleStation()
{
    EdiStationInfo station;
    station.section = QStringLiteral("SINGLE");
    station.club = QStringLiteral("ADL 505");
    station.locationLine1 = QStringLiteral("Feuerkogel, 1592 m");
    station.name = QStringLiteral("Martin Fischer");
    station.street = QStringLiteral("Musterstraße 1");
    station.postalCode = QStringLiteral("4810");
    station.city = QStringLiteral("Gmunden");
    station.country = QStringLiteral("Austria");
    station.email = QStringLiteral("oe5sos@example.org");
    station.txEquipment = QStringLiteral("TS-590");
    station.powerWatts = 100;
    station.antenna = QStringLiteral("2 x 12 el. Yagi");
    return station;
}

QsoRecord makeQso(const QString& call, const QString& band, const QString& mode, const QString& time,
                  int serialSent, int serialRcvd, const QString& grid, double km)
{
    const QString rst = mode == QStringLiteral("CW") ? QStringLiteral("599") : QStringLiteral("59");
    QsoRecord r;
    r.callsign = call;
    r.band = band;
    r.mode = mode;
    r.timestampUtc = QStringLiteral("2026-10-03T%1:00Z").arg(time);
    r.gridSquare = grid;
    r.distanceKm = km;
    r.serialSent = serialSent;
    r.serialRcvd = serialRcvd;
    r.rstSent = rst;
    r.rstRcvd = rst;
    r.exchangeSent = QStringLiteral("%1 %2 JN67UT").arg(rst).arg(serialSent, 3, 10, QLatin1Char('0'));
    r.exchangeRcvd = QStringLiteral("%1 %2 %3").arg(rst).arg(serialRcvd, 3, 10, QLatin1Char('0')).arg(grid);
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    return r;
}

} // namespace

class TestEdiExporter : public QObject
{
    Q_OBJECT

private slots:
    void bandLabelsUseReg1testSpellings();
    void modeCodesFollowTheSpec();
    void exportsGoldenBandFile();
    void secondBandGetsItsOwnFileAndSharedDates();
    void invalidQsoIsLeftOutEverywhere();
    void stationInfoRoundTripsThroughTheDatabase();
    void suggestedFileNamesAreFilesystemSafe();
};

void TestEdiExporter::bandLabelsUseReg1testSpellings()
{
    QCOMPARE(EdiExporter::bandLabel(QStringLiteral("144")), QStringLiteral("144 MHz"));
    QCOMPARE(EdiExporter::bandLabel(QStringLiteral("432")), QStringLiteral("432 MHz"));
    QCOMPARE(EdiExporter::bandLabel(QStringLiteral("1296")), QStringLiteral("1,3 GHz"));
    QCOMPARE(EdiExporter::bandLabel(QStringLiteral("10368")), QStringLiteral("10 GHz"));
    QCOMPARE(EdiExporter::bandLabel(QStringLiteral(" 70 ")), QStringLiteral("70 MHz"));
    QCOMPARE(EdiExporter::bandLabel(QStringLiteral("999")), QStringLiteral("999 MHz"));
}

void TestEdiExporter::modeCodesFollowTheSpec()
{
    QCOMPARE(EdiExporter::modeCode(QStringLiteral("SSB")), 1);
    QCOMPARE(EdiExporter::modeCode(QStringLiteral("usb")), 1);
    QCOMPARE(EdiExporter::modeCode(QStringLiteral("CW")), 2);
    QCOMPARE(EdiExporter::modeCode(QStringLiteral("AM")), 5);
    QCOMPARE(EdiExporter::modeCode(QStringLiteral("FM")), 6);
    QCOMPARE(EdiExporter::modeCode(QStringLiteral("RTTY")), 7);
    QCOMPARE(EdiExporter::modeCode(QStringLiteral("FT8")), 0);
    QCOMPARE(EdiExporter::modeCode(QString()), 0);
}

void TestEdiExporter::exportsGoldenBandFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("edi.sqlite")), QStringLiteral("edi_golden")));

    QString defError;
    const ContestDefinition definition = ContestDefinition::loadFromJson(QByteArray(kIaruJson), &defError);
    QVERIFY2(definition.isValid(), qPrintable(defError));

    QsoRecord q1 = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("14:01"),
                           1, 3, QStringLiteral("JN58SD"), 187.4);
    QsoRecord q2 = makeQso(QStringLiteral("OE3XYZ"), QStringLiteral("144"), QStringLiteral("CW"), QStringLiteral("14:11"),
                           2, 17, QStringLiteral("JN88TC"), 214.6);
    // Same station, same band and mode again: a dupe stays in the file,
    // scores nothing and is flagged "D".
    QsoRecord q3 = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("14:20"),
                           3, 9, QStringLiteral("JN58SD"), 187.4);
    q3.isDupe = true;
    // Another band: must not appear in the 144 file at all.
    QsoRecord q4 = makeQso(QStringLiteral("OE5XYZ"), QStringLiteral("432"), QStringLiteral("SSB"), QStringLiteral("15:00"),
                           4, 1, QStringLiteral("JN67UT"), 0.0);
    // Operators type "3", not "003" -- the composed text keeps that,
    // the serial column must still be 003 and the exchange column empty.
    q1.exchangeRcvd = QStringLiteral("59 3 JN58SD");
    QVERIFY(db.insertQso(q1));
    QVERIFY(db.insertQso(q2));
    QVERIFY(db.insertQso(q3));
    QVERIFY(db.insertQso(q4));

    EdiExporter exporter(db);
    const QString actual = exporter.exportBand(definition.id(), QStringLiteral("144"), definition, feuerkogelSettings(),
                                               sampleStation());

    const QStringList expectedLines = {
        QStringLiteral("[REG1TEST;1]"),
        QStringLiteral("TName=IARU Region 1 VHF/UHF Contest"),
        QStringLiteral("TDate=20261003;20261003"),
        QStringLiteral("PCall=OE5SOS"),
        QStringLiteral("PWWLo=JN67UT"),
        QStringLiteral("PExch="),
        QStringLiteral("PAdr1=Feuerkogel, 1592 m"),
        QStringLiteral("PAdr2="),
        QStringLiteral("PSect=SINGLE"),
        QStringLiteral("PBand=144 MHz"),
        QStringLiteral("PClub=ADL 505"),
        QStringLiteral("RName=Martin Fischer"),
        QStringLiteral("RCall=OE5SOS"),
        QStringLiteral("RAdr1=Musterstraße 1"),
        QStringLiteral("RAdr2="),
        QStringLiteral("RPoCo=4810"),
        QStringLiteral("RCity=Gmunden"),
        QStringLiteral("RCoun=Austria"),
        QStringLiteral("RPhon="),
        QStringLiteral("RHBBS=oe5sos@example.org"),
        QStringLiteral("MOpe1="),
        QStringLiteral("MOpe2="),
        QStringLiteral("STXEq=TS-590"),
        QStringLiteral("SPowe=100"),
        QStringLiteral("SRXEq="),
        QStringLiteral("SAnte=2 x 12 el. Yagi"),
        QStringLiteral("SAntH=10;1592"),
        QStringLiteral("CQSOs=2;1"),
        QStringLiteral("CQSOP=402"),
        QStringLiteral("CWWLs=2;0;1"),
        QStringLiteral("CWWLB=0"),
        QStringLiteral("CExcs=0;0;1"),
        QStringLiteral("CExcB=0"),
        QStringLiteral("CDXCs=0;0;1"),
        QStringLiteral("CDXCB=0"),
        QStringLiteral("CToSc=402"),
        QStringLiteral("CODXC=OE3XYZ;JN88TC;215"),
        QStringLiteral("[Remarks]"),
        QStringLiteral("Created by Contestprogramm"),
        QStringLiteral("[QSORecords;3]"),
        QStringLiteral("261003;1401;DL1ABC;1;59;001;59;003;;JN58SD;187;;N;;"),
        QStringLiteral("261003;1411;OE3XYZ;2;599;002;599;017;;JN88TC;215;;N;;"),
        QStringLiteral("261003;1420;DL1ABC;1;59;003;59;009;;JN58SD;0;;;;D"),
    };
    const QString expected = expectedLines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
    QCOMPARE(actual, expected);

    // Latin-1 is what the format's readers expect; the one non-ASCII
    // character above must survive the encoding the dialog writes.
    QVERIFY(actual.toLatin1().contains("Musterstra\xdf" "e 1"));
}

void TestEdiExporter::secondBandGetsItsOwnFileAndSharedDates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("edi2.sqlite")), QStringLiteral("edi_bands")));

    QString defError;
    const ContestDefinition definition = ContestDefinition::loadFromJson(QByteArray(kIaruJson), &defError);
    QVERIFY2(definition.isValid(), qPrintable(defError));

    QsoRecord q1 = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("14:01"),
                           1, 3, QStringLiteral("JN58SD"), 187.4);
    QsoRecord q2 = makeQso(QStringLiteral("OE5XYZ"), QStringLiteral("432"), QStringLiteral("SSB"), QStringLiteral("15:00"),
                           2, 1, QStringLiteral("JN67UT"), 0.0);
    // Sunday morning, so TDate must span two days in BOTH band files.
    q2.timestampUtc = QStringLiteral("2026-10-04T06:30:00Z");
    QVERIFY(db.insertQso(q1));
    QVERIFY(db.insertQso(q2));

    EdiExporter exporter(db);
    QCOMPARE(exporter.bandsWithQsos(definition.id(), definition), (QStringList{QStringLiteral("144"), QStringLiteral("432")}));

    const QString file144 = exporter.exportBand(definition.id(), QStringLiteral("144"), definition, feuerkogelSettings(),
                                                sampleStation());
    const QString file432 = exporter.exportBand(definition.id(), QStringLiteral("432"), definition, feuerkogelSettings(),
                                                sampleStation());

    QVERIFY(file144.contains(QStringLiteral("TDate=20261003;20261004\r\n")));
    QVERIFY(file432.contains(QStringLiteral("TDate=20261003;20261004\r\n")));
    QVERIFY(file432.contains(QStringLiteral("PBand=432 MHz\r\n")));
    QVERIFY(file432.contains(QStringLiteral("[QSORecords;1]\r\n")));
    QVERIFY(file432.contains(QStringLiteral("261004;0630;OE5XYZ;1;59;002;59;001;;JN67UT;1;;N;;\r\n")));
    QVERIFY(!file432.contains(QStringLiteral("DL1ABC")));
    QVERIFY(file432.contains(QStringLiteral("CQSOP=1\r\n")));
    QVERIFY(file432.contains(QStringLiteral("CODXC=OE5XYZ;JN67UT;1\r\n")));
    QVERIFY(!file144.contains(QStringLiteral("OE5XYZ")));
}

void TestEdiExporter::invalidQsoIsLeftOutEverywhere()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("edi3.sqlite")), QStringLiteral("edi_invalid")));

    QString defError;
    const ContestDefinition definition = ContestDefinition::loadFromJson(QByteArray(kIaruJson), &defError);
    QVERIFY2(definition.isValid(), qPrintable(defError));

    QsoRecord good = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), QStringLiteral("14:01"),
                             1, 3, QStringLiteral("JN58SD"), 187.4);
    QsoRecord bogus = makeQso(QStringLiteral("OE9ZZZ"), QStringLiteral("432"), QStringLiteral("SSB"), QStringLiteral("14:05"),
                              2, 9, QStringLiteral("KN05IX"), 900.0);
    QVERIFY(db.insertQso(good));
    QVERIFY(db.insertQso(bogus));
    QVERIFY(db.setQsoInvalid(bogus.id, true));

    EdiExporter exporter(db);
    // The only 432 QSO is invalid, so there is no 432 file to write.
    QCOMPARE(exporter.bandsWithQsos(definition.id(), definition), QStringList{QStringLiteral("144")});
    const QString file144 = exporter.exportBand(definition.id(), QStringLiteral("144"), definition, feuerkogelSettings(),
                                                sampleStation());
    QVERIFY(!file144.contains(QStringLiteral("OE9ZZZ")));
    QVERIFY(file144.contains(QStringLiteral("CODXC=DL1ABC;JN58SD;187\r\n")));
}

void TestEdiExporter::stationInfoRoundTripsThroughTheDatabase()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("edi4.sqlite")), QStringLiteral("edi_station")));

    const EdiStationInfo saved = sampleStation();
    saved.saveTo(db);

    EdiStationInfo loaded;
    loaded.loadFrom(db);
    QCOMPARE(loaded.section, saved.section);
    QCOMPARE(loaded.club, saved.club);
    QCOMPARE(loaded.locationLine1, saved.locationLine1);
    QCOMPARE(loaded.name, saved.name);
    QCOMPARE(loaded.street, saved.street);
    QCOMPARE(loaded.postalCode, saved.postalCode);
    QCOMPARE(loaded.city, saved.city);
    QCOMPARE(loaded.country, saved.country);
    QCOMPARE(loaded.email, saved.email);
    QCOMPARE(loaded.txEquipment, saved.txEquipment);
    QCOMPARE(loaded.powerWatts, 100);
    QCOMPARE(loaded.antenna, saved.antenna);
}

void TestEdiExporter::suggestedFileNamesAreFilesystemSafe()
{
    QCOMPARE(EdiExporter::suggestedFileName(QStringLiteral("OE5SOS"), QStringLiteral("144")),
             QStringLiteral("OE5SOS_144MHz.edi"));
    QCOMPARE(EdiExporter::suggestedFileName(QStringLiteral("oe5sos/p"), QStringLiteral("432")),
             QStringLiteral("OE5SOS-P_432MHz.edi"));
    QCOMPARE(EdiExporter::suggestedFileName(QString(), QStringLiteral("144")), QStringLiteral("log_144MHz.edi"));
}

// Not QTEST_APPLESS_MAIN: QSqlDatabase requires a live QCoreApplication
// (see test_cabrilloexporter.cpp).
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestEdiExporter tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_ediexporter.moc"
