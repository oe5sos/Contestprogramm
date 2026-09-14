#include <QtTest>

#include <QCoreApplication>
#include <QTemporaryDir>

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
    const QString actual = exporter.exportContest(QStringLiteral("OE_VHF_UHF"));

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
    const QString actual = exporter.exportContest(QStringLiteral("OE_VHF_UHF"));

    QVERIFY(actual.contains(QStringLiteral("OE1ABC")));
    QVERIFY(!actual.contains(QStringLiteral("OE9ZZZ")));
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
