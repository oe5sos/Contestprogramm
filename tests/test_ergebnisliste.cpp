// Martin, 2026-09-27: "hierzu bitte alte kontestergebnisse und
// einrecihungen usw. verwenden. qrz. sollze ggf. diese ergänzen.
// wichtig jedoch die einreichung der ergebnsise, diese sind
// treffsicherer."
//
// Der Punkt trägt: der Locator einer Einreichung ist der Standort, VON
// DEM gefahren wurde. QRZ kennt den Heimatstandort -- auf UKW fährt
// dieselbe Station vom Berg, und das ist selten derselbe Locator.
//
// Dieser Prüfstand geht den ganzen Weg: eine Ergebnisliste im Format
// des ÖVSV-Auswerteservers einlesen, und danach schlägt die Eingabe
// den Contest-Locator vor.

#include <QtTest>

#include <QApplication>
#include <QFile>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/CallsignLocatorLookup.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

class TestErgebnisliste : public QObject
{
    Q_OBJECT

private slots:
    void importedResultsPrefillTheLocator();
    void ownLogBeatsTheImportedList();
};

namespace {

// Wörtlich aus dem CSV-Export des ÖVSV-Auswerteservers, um die hinteren
// Spalten gekürzt.
const char* kResultsCsv =
    "\"Section\",\"Band\",\"Rank\",\"Rank for prize\",\"Call\",\"WWL\",\"Claimed score\"\n"
    "\"SO-LP 145 MHz\",\"145 MHz\",\"1\",\"0\",\"OE5DIN\",\"JN78BL\",\"46964\"\n"
    "\"SO 145 MHz\",\"145 MHz\",\"1\",\"0\",\"OE3KAR/P\",\"JN67UV\",\"31220\"\n";

std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(file))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    controller->setSettings(settings);
    return controller;
}

QString writeResults(QTemporaryDir& dir, const QString& name, const char* text)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    file.write(text);
    file.close();
    return path;
}

} // namespace

void TestErgebnisliste::importedResultsPrefillTheLocator()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("ergebnis.sqlite"));
    QVERIFY(controller);

    const QString path = writeResults(dir, QStringLiteral("iaru_vhf_2025.csv"), kResultsCsv);
    QVERIFY(!path.isEmpty());
    QString error;
    const CallsignLocatorLookup::ImportSummary summary =
        controller->callsignLocatorLookup().importCsvFile(path, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(summary.imported, 2);
    QCOMPARE(summary.skipped, 0);

    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    // Eine Station, die nie gearbeitet wurde: der Locator kommt aus der
    // Einreichung.
    log->setCallsign(QStringLiteral("OE5DIN"));
    QTRY_COMPARE_WITH_TIMEOUT(log->exchangeReceived().value(QStringLiteral("grid")), QStringLiteral("JN78BL"), 3000);

    // Auch mit Zusatz am Schrägstrich, so wie eingereicht.
    log->setCallsign(QString());
    log->setExchangeFieldValue(QStringLiteral("grid"), QString());
    log->setCallsign(QStringLiteral("OE3KAR/P"));
    QTRY_COMPARE_WITH_TIMEOUT(log->exchangeReceived().value(QStringLiteral("grid")), QStringLiteral("JN67UV"), 3000);
}

// Die Rangfolge bleibt: was die Station uns selbst gegeben hat, schlägt
// jede Liste. Sie kann dieses Jahr von woanders fahren.
void TestErgebnisliste::ownLogBeatsTheImportedList()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("rangfolge.sqlite"));
    QVERIFY(controller);

    const QString path = writeResults(dir, QStringLiteral("alt.csv"), kResultsCsv);
    QVERIFY(!path.isEmpty());
    QVERIFY(controller->callsignLocatorLookup().importCsvFile(path).imported == 2);

    // Dieselbe Station, dieses Jahr von einem anderen Berg -- im
    // eigenen Log eines früheren Contests.
    QsoRecord r;
    r.callsign = QStringLiteral("OE5DIN");
    r.band = QStringLiteral("144");
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QStringLiteral("2026-03-07T12:00:00Z");
    r.gridSquare = QStringLiteral("JN68AA");
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF@2026-03-07");
    QVERIFY(controller->database().insertQso(r));

    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    log->setCallsign(QStringLiteral("OE5DIN"));
    QTRY_COMPARE_WITH_TIMEOUT(log->exchangeReceived().value(QStringLiteral("grid")), QStringLiteral("JN68AA"), 3000);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestErgebnisliste tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_ergebnisliste.moc"
