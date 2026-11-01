// Eine Generalprobe auf Kurzwelle: Band wechseln, zwei QSOs auf zwei
// Bändern loggen, beides exportieren -- durch dieselben Wege, die am
// Contestwochenende laufen. Die Einzelteile haben eigene Prüfstände;
// dieser hier fragt, ob sie zusammen das Richtige ergeben.

#include <QtTest>

#include <QApplication>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/AdifExporter.h"
#include "data/CabrilloExporter.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

class TestKurzwelleGeneralprobe : public QObject
{
    Q_OBJECT

private slots:
    void twoBandsTwoQsosAndBothExports();
};

void TestKurzwelleGeneralprobe::twoBandsTwoQsosAndBothExports()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = std::make_unique<AppController>();
    QVERIFY(controller->openDatabase(dir.filePath(QStringLiteral("generalprobe.sqlite"))));
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("KW_UEBUNG");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    controller->setSettings(settings);

    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    const auto qsy = [&](const QString& text) {
        log->setCallsign(text);
        emit log->logRequested();
    };
    const auto logQso = [&](const QString& call, const QString& serial) {
        log->setCallsign(call);
        log->setExchangeFieldValue(QStringLiteral("rst"), QStringLiteral("599"));
        log->setExchangeFieldValue(QStringLiteral("serial"), serial);
        emit log->logRequested();
    };

    // 20 m, ein QSO. Ohne Funkgerät ist die Zahl im Rufzeichenfeld der
    // einzige Weg dorthin.
    qsy(QStringLiteral("14045"));
    QCOMPARE(window.currentBand(), QStringLiteral("14"));
    logQso(QStringLiteral("DL1ABC"), QStringLiteral("001"));

    // 40 m, das zweite.
    qsy(QStringLiteral("7.120"));
    QCOMPARE(window.currentBand(), QStringLiteral("7"));
    logQso(QStringLiteral("W1AW"), QStringLiteral("002"));

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("KW_UEBUNG"));
    QCOMPARE(qsos.size(), 2);
    QCOMPARE(qsos.at(0).callsign, QStringLiteral("DL1ABC"));
    QCOMPARE(qsos.at(0).band, QStringLiteral("14"));
    QCOMPARE(qsos.at(1).callsign, QStringLiteral("W1AW"));
    QCOMPARE(qsos.at(1).band, QStringLiteral("7"));
    // serial_scope "contest": die eigene Nummer läuft über beide Bänder
    // durch, sie fängt auf 40 m nicht wieder bei 1 an.
    QCOMPARE(qsos.at(0).serialSent.value_or(0), 1);
    QCOMPARE(qsos.at(1).serialSent.value_or(0), 2);

    // ADIF: Wellenlängen, Rapport, eigene Station.
    AdifExporter adif(controller->database());
    const QString adifText = adif.exportContest(QStringLiteral("KW_UEBUNG"), controller->settings());
    QVERIFY2(adifText.contains(QStringLiteral("<BAND:3>20m ")), qPrintable(adifText));
    QVERIFY2(adifText.contains(QStringLiteral("<BAND:3>40m ")), qPrintable(adifText));
    // 14,045 liegt im CW-Teil des Bandes, 7,120 im Telefonieteil: ohne
    // Funkgerät kommt die Betriebsart aus dem Bandplan, und mit ihr der
    // vorbelegte Rapport.
    QCOMPARE(qsos.at(0).mode, QStringLiteral("CW"));
    QCOMPARE(qsos.at(0).rstSent, QStringLiteral("599"));
    QCOMPARE(qsos.at(1).mode, QStringLiteral("SSB"));
    QCOMPARE(qsos.at(1).rstSent, QStringLiteral("59"));
    QVERIFY2(adifText.contains(QStringLiteral("<RST_SENT:3>599 ")), qPrintable(adifText));
    QVERIFY2(adifText.contains(QStringLiteral("<STATION_CALLSIGN:6>OE5SOS ")), qPrintable(adifText));
    QVERIFY2(adifText.contains(QStringLiteral("<MY_GRIDSQUARE:6>JN67UT ")), qPrintable(adifText));
    QVERIFY2(!adifText.contains(QStringLiteral("<BAND:2>14 ")), qPrintable(adifText));

    // Cabrillo: Kilohertz unter 30 MHz, und beide Bänder im Kopf.
    const ContestDefinition* def = controller->findContestDefinition(QStringLiteral("KW_UEBUNG"));
    QVERIFY(def);
    CabrilloExporter cabrillo(controller->database());
    const QString cabrilloText =
        cabrillo.exportContest(QStringLiteral("KW_UEBUNG"), *def, controller->settings());
    QVERIFY2(cabrilloText.contains(QStringLiteral("CATEGORY-BAND: ALL")), qPrintable(cabrilloText));
    // Die eingetippte Frequenz steht im Log, nicht die Bandkante:
    // ohne Funkgerät ist sie die einzige, die es gibt.
    QVERIFY2(cabrilloText.contains(QStringLiteral("QSO: 14045")), qPrintable(cabrilloText));
    QVERIFY2(cabrilloText.contains(QStringLiteral("QSO: 7120")), qPrintable(cabrilloText));
    QVERIFY2(!cabrilloText.contains(QStringLiteral("QSO: 14000")), qPrintable(cabrilloText));
    QVERIFY2(adifText.contains(QStringLiteral("<FREQ:9>14.045000")), qPrintable(adifText));
    QVERIFY2(cabrilloText.contains(QStringLiteral("GRID-LOCATOR: JN67UT")), qPrintable(cabrilloText));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestKurzwelleGeneralprobe tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_kurzwelle_generalprobe.moc"
