#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/EdiExporter.h"
#include "data/QsoRecord.h"
#include "ui/EdiExportDialog.h"

using namespace Contestprogramm;

namespace {

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

QsoRecord makeQso(const QString& call, const QString& band, const QString& grid, double km)
{
    QsoRecord r;
    r.callsign = call;
    r.band = band;
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QStringLiteral("2026-10-03T14:01:00Z");
    r.gridSquare = grid;
    r.distanceKm = km;
    r.serialSent = 1;
    r.serialRcvd = 1;
    r.rstSent = QStringLiteral("59");
    r.rstRcvd = QStringLiteral("59");
    r.exchangeSent = QStringLiteral("59 001 JN67UT");
    r.exchangeRcvd = QStringLiteral("59 001 ") + grid;
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    return r;
}

} // namespace

// Drives EdiExportDialog::exportNow() directly (a modal exec() cannot
// be driven headlessly -- same approach as test_contest_picker.cpp) and
// checks the two things MainWindow relies on: one file per band lands
// in the chosen folder, and the station paperwork is remembered for
// the next contest.
class TestEdiExportDialog : public QObject
{
    Q_OBJECT

private slots:
    void writesOneFilePerBandAndRemembersStationInfo();
};

void TestEdiExportDialog::writesOneFilePerBandAndRemembersStationInfo()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("dialog.sqlite")), QStringLiteral("edi_dialog")));

    QString defError;
    const ContestDefinition definition = ContestDefinition::loadFromJson(QByteArray(kIaruJson), &defError);
    QVERIFY2(definition.isValid(), qPrintable(defError));

    ContestSettings settings;
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = definition.id();

    QsoRecord q144 = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("JN58SD"), 187.4);
    QsoRecord q432 = makeQso(QStringLiteral("OE3XYZ"), QStringLiteral("432"), QStringLiteral("JN88TC"), 214.6);
    QVERIFY(db.insertQso(q144));
    QVERIFY(db.insertQso(q432));

    const QString outDir = dir.filePath(QStringLiteral("export"));

    {
        EdiExportDialog dialog(db, definition, settings);
        EdiStationInfo station;
        station.section = QStringLiteral("SINGLE");
        station.name = QStringLiteral("Martin Fischer");
        station.powerWatts = 100;
        dialog.setStationInfo(station);
        dialog.setOutputDirectory(outDir);
        dialog.setSkipSectionCheck(true); // e-mail/antenna left empty on purpose

        QVERIFY(dialog.exportNow());
        const QStringList written = dialog.writtenFiles();
        QCOMPARE(written.size(), 2);
        QCOMPARE(QFileInfo(written.at(0)).fileName(), QStringLiteral("OE5SOS_144MHz.edi"));
        QCOMPARE(QFileInfo(written.at(1)).fileName(), QStringLiteral("OE5SOS_432MHz.edi"));
    }

    QFile file(QDir(outDir).filePath(QStringLiteral("OE5SOS_432MHz.edi")));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    QVERIFY(bytes.startsWith("[REG1TEST;1]\r\n"));
    QVERIFY(bytes.contains("PBand=432 MHz\r\n"));
    QVERIFY(bytes.contains("PSect=SINGLE\r\n"));
    QVERIFY(bytes.contains("RName=Martin Fischer\r\n"));
    QVERIFY(bytes.contains("SPowe=100\r\n"));
    QVERIFY(bytes.contains(";OE3XYZ;"));
    QVERIFY(!bytes.contains("DL1ABC"));

    // A second dialog on the same database comes up pre-filled.
    EdiExportDialog again(db, definition, settings);
    QCOMPARE(again.stationInfo().section, QStringLiteral("SINGLE"));
    QCOMPARE(again.stationInfo().name, QStringLiteral("Martin Fischer"));
    QCOMPARE(again.stationInfo().powerWatts, 100);
    QCOMPARE(again.outputDirectory(), outDir);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestEdiExportDialog tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_edi_export_dialog.moc"
