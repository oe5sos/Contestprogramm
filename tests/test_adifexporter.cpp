#include <QtTest>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "app/ContestSettings.h"
#include "data/AdifExporter.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"

using namespace Contestprogramm;

class TestAdifExporter : public QObject
{
    Q_OBJECT

private slots:
    void exportsGoldenBlockWithExactFrequencyAndGracefulNullFreq();
    void invalidQsoIsExcluded();
    void shortwaveBandsGetTheirAdifNames();
    void reportExchangeAndOwnStationAreWritten();
};

void TestAdifExporter::exportsGoldenBlockWithExactFrequencyAndGracefulNullFreq()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("adif.sqlite")), QStringLiteral("adif_golden")));

    // Has an exact CAT-derived frequency -- the whole point of this
    // exporter per AdifExporter.h: Cabrillo only ever gets the "144"
    // band code, ADIF gets the real 145.200000 MHz value.
    QsoRecord withFreq;
    withFreq.callsign = QStringLiteral("OE1ABC");
    withFreq.band = QStringLiteral("144");
    withFreq.mode = QStringLiteral("SSB");
    withFreq.timestampUtc = QStringLiteral("2026-06-13T12:05:00Z");
    withFreq.freqHz = 145200000;
    withFreq.gridSquare = QStringLiteral("JN88TC");
    withFreq.contestId = QStringLiteral("OE_VHF_UHF");
    withFreq.serialSent = 1;
    withFreq.serialRcvd = 2;
    QVERIFY(db.insertQso(withFreq));

    // freq_hz NULL, and no grid/serial_rcvd either -- must degrade
    // gracefully (fields simply omitted), never crash or emit garbage
    // like "<FREQ:1>0" or an empty-but-present tag.
    QsoRecord withoutFreq;
    withoutFreq.callsign = QStringLiteral("OE3XYZ");
    withoutFreq.band = QStringLiteral("432");
    withoutFreq.mode = QStringLiteral("FM");
    withoutFreq.timestampUtc = QStringLiteral("2026-06-13T12:11:00Z");
    withoutFreq.contestId = QStringLiteral("OE_VHF_UHF");
    withoutFreq.serialSent = 2;
    QVERIFY(db.insertQso(withoutFreq));

    AdifExporter exporter(db);
    // Leere Einstellungen: die Stationsfelder entfallen, genau wie jedes
    // andere leere Feld hier -- der Blockvergleich unten zeigt es.
    const ContestSettings settings;
    const QString actual = exporter.exportContest(QStringLiteral("OE_VHF_UHF"), settings);

    const QString expected =
        QStringLiteral("Contestprogramm ADIF export\n")
        + QStringLiteral("<ADIF_VER:5>3.1.4 <PROGRAMID:15>Contestprogramm <EOH>\n")
        + QStringLiteral("<CALL:6>OE1ABC <QSO_DATE:8>20260613 <TIME_ON:6>120500 <BAND:2>2m <MODE:3>SSB "
                          "<FREQ:10>145.200000 <GRIDSQUARE:6>JN88TC <CONTEST_ID:10>OE_VHF_UHF "
                          "<STX:1>1 <SRX:1>2 <EOR>\n")
        + QStringLiteral("<CALL:6>OE3XYZ <QSO_DATE:8>20260613 <TIME_ON:6>121100 <BAND:4>70cm <MODE:2>FM "
                          "<CONTEST_ID:10>OE_VHF_UHF <STX:1>2 <EOR>\n");

    QCOMPARE(actual, expected);

    // Explicit, narrower assertions on top of the golden-file compare,
    // per the plan's own emphasis: exact Hz->MHz conversion, and no
    // FREQ tag at all when freq_hz is NULL.
    QVERIFY(actual.contains(QStringLiteral("<FREQ:10>145.200000")));
    QVERIFY(!actual.mid(actual.indexOf(QStringLiteral("OE3XYZ"))).contains(QStringLiteral("FREQ")));
}

// DXLog.net deliberately has no delete function for a logged QSO
// (dxlog.net/docs/index.php/Menu_Edit, verified for this task) -- a QSO
// marked invalid stays in the operator's own log but must not appear in
// an exported ADIF file.
void TestAdifExporter::invalidQsoIsExcluded()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("adif_invalid.sqlite")), QStringLiteral("adif_invalid")));

    QsoRecord good;
    good.callsign = QStringLiteral("OE1ABC");
    good.band = QStringLiteral("144");
    good.mode = QStringLiteral("SSB");
    good.timestampUtc = QStringLiteral("2026-06-13T12:05:00Z");
    good.contestId = QStringLiteral("OE_VHF_UHF");
    QVERIFY(db.insertQso(good));

    QsoRecord invalid;
    invalid.callsign = QStringLiteral("OE9ZZZ");
    invalid.band = QStringLiteral("144");
    invalid.mode = QStringLiteral("SSB");
    invalid.timestampUtc = QStringLiteral("2026-06-13T12:20:00Z");
    invalid.contestId = QStringLiteral("OE_VHF_UHF");
    QVERIFY(db.insertQso(invalid));
    QVERIFY(db.setQsoInvalid(invalid.id, true));

    AdifExporter exporter(db);
    const QString actual = exporter.exportContest(QStringLiteral("OE_VHF_UHF"), ContestSettings());

    QVERIFY(actual.contains(QStringLiteral("OE1ABC")));
    QVERIFY(!actual.contains(QStringLiteral("OE9ZZZ")));
}


// Bis 2026-09-23 kannte der Export nur 144/432/1296 und schrieb für
// alles andere die nackte Megahertz-Zahl hin -- ein Kurzwellen-QSO kam
// also als BAND=14 heraus, und das ist in ADIF kein Band. Hier steht
// jedes Band der Tabelle in core/BandUtils.cpp einmal drin.
void TestAdifExporter::shortwaveBandsGetTheirAdifNames()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("adif_hf.sqlite")), QStringLiteral("adif_hf")));

    const QVector<QPair<QString, QString>> expected{
        {QStringLiteral("1.8"), QStringLiteral("160m")}, {QStringLiteral("3.5"), QStringLiteral("80m")},
        {QStringLiteral("7"), QStringLiteral("40m")},    {QStringLiteral("10"), QStringLiteral("30m")},
        {QStringLiteral("14"), QStringLiteral("20m")},   {QStringLiteral("18"), QStringLiteral("17m")},
        {QStringLiteral("21"), QStringLiteral("15m")},   {QStringLiteral("24"), QStringLiteral("12m")},
        {QStringLiteral("28"), QStringLiteral("10m")},   {QStringLiteral("50"), QStringLiteral("6m")},
        {QStringLiteral("70"), QStringLiteral("4m")},    {QStringLiteral("144"), QStringLiteral("2m")},
        {QStringLiteral("432"), QStringLiteral("70cm")}, {QStringLiteral("1296"), QStringLiteral("23cm")},
        {QStringLiteral("2320"), QStringLiteral("13cm")}, {QStringLiteral("3400"), QStringLiteral("9cm")},
        {QStringLiteral("5760"), QStringLiteral("6cm")}, {QStringLiteral("10368"), QStringLiteral("3cm")},
    };

    int minute = 0;
    for (const auto& pair : expected) {
        QsoRecord qso;
        qso.callsign = QStringLiteral("OE1ABC");
        qso.band = pair.first;
        qso.mode = QStringLiteral("CW");
        qso.timestampUtc = QStringLiteral("2026-06-13T12:%1:00Z").arg(minute++, 2, 10, QLatin1Char('0'));
        qso.contestId = QStringLiteral("KW_UEBUNG");
        QVERIFY(db.insertQso(qso));
    }

    AdifExporter exporter(db);
    const QString actual = exporter.exportContest(QStringLiteral("KW_UEBUNG"), ContestSettings());

    for (const auto& pair : expected) {
        const QString tag =
            QStringLiteral("<BAND:%1>%2 ").arg(pair.second.toUtf8().size()).arg(pair.second);
        QVERIFY2(actual.contains(tag), qPrintable(QStringLiteral("fehlt: %1 (Band %2)").arg(tag, pair.first)));
    }
    // Und keine nackte Zahl mehr -- die war der Fehler.
    QVERIFY(!actual.contains(QStringLiteral("<BAND:2>14 ")));
}

// Rapport, getauschter Text und die eigene Station stehen seit
// 2026-09-23 mit in der Datei: ohne sie trägt das Logbuch am anderen
// Ende 59 ein, verliert alles, was kein Zahlenfeld ist, und TQSL weiß
// nicht, wessen QSO es hochlädt.
void TestAdifExporter::reportExchangeAndOwnStationAreWritten()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("adif_station.sqlite")), QStringLiteral("adif_station")));

    QsoRecord qso;
    qso.callsign = QStringLiteral("W1AW");
    qso.band = QStringLiteral("14");
    qso.mode = QStringLiteral("CW");
    qso.timestampUtc = QStringLiteral("2026-06-13T12:05:00Z");
    qso.rstSent = QStringLiteral("599");
    qso.rstRcvd = QStringLiteral("579");
    qso.exchangeSent = QStringLiteral("599 001");
    qso.exchangeRcvd = QStringLiteral("579 042");
    qso.serialSent = 1;
    qso.serialRcvd = 42;
    qso.contestId = QStringLiteral("KW_UEBUNG");
    QVERIFY(db.insertQso(qso));

    ContestSettings settings;
    settings.ownCallsign = QStringLiteral("oe5sos");
    settings.ownGrid = QStringLiteral("jn67ut");

    AdifExporter exporter(db);
    const QString actual = exporter.exportContest(QStringLiteral("KW_UEBUNG"), settings);

    QVERIFY(actual.contains(QStringLiteral("<RST_SENT:3>599 ")));
    QVERIFY(actual.contains(QStringLiteral("<RST_RCVD:3>579 ")));
    QVERIFY(actual.contains(QStringLiteral("<STX_STRING:7>599 001 ")));
    QVERIFY(actual.contains(QStringLiteral("<SRX_STRING:7>579 042 ")));
    // Klein eingetippt, groß exportiert -- ADIF-Rufzeichen und
    // -Locator sind Großbuchstaben.
    QVERIFY(actual.contains(QStringLiteral("<STATION_CALLSIGN:6>OE5SOS ")));
    QVERIFY(actual.contains(QStringLiteral("<OPERATOR:6>OE5SOS ")));
    QVERIFY(actual.contains(QStringLiteral("<MY_GRIDSQUARE:6>JN67UT ")));
}

// Not QTEST_APPLESS_MAIN: QSqlDatabase requires a live QCoreApplication
// instance (thread-affinity bookkeeping in the SQLite driver), which
// APPLESS_MAIN deliberately does not create.
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestAdifExporter tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_adifexporter.moc"
