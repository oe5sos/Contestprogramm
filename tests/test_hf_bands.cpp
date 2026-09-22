// Kurzwelle: die Bandtabelle (core/BandUtils.h) kennt seit diesem Stand
// 1,8 bis 24 MHz, und das Übungslog resources/contest_definitions/
// uebung_kurzwelle.json ist die erste Definition, die sie benutzt.
//
// Der Live-Teil unten ist der eigentliche Grund für diesen Prüfstand:
// vorher blieb MainWindow::applyRigFrequency() bei einer unbekannten
// Frequenz stumm auf dem zuletzt gesetzten Band stehen (Absicht: "Gerät
// kurz auf KW geparkt" soll das UHF-Log nicht mitziehen) -- mit einem
// Gerät, das wirklich auf 14 MHz arbeitet, hieß das: jedes QSO im
// falschen Band, mit der falschen laufenden Nummer, im falschen
// Dupe-Topf, ohne eine einzige Warnung.

#include <QtTest>

#include <QApplication>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/BandUtils.h"
#include "data/ContestDefinition.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

class TestHfBands : public QObject
{
    Q_OBJECT

private slots:
    void tableNamesTheHfBands();
    void gapsBetweenTheBandsStayUnknown();
    void practiceDefinitionLoads();
    void rigOnHfMovesTheLogToThatBand();
    void freshDatabaseDoesNotStartInThePracticeLog();
};

void TestHfBands::tableNamesTheHfBands()
{
    QCOMPARE(bandLabelForFrequencyHz(1840000), QStringLiteral("1.8"));
    QCOMPARE(bandLabelForFrequencyHz(3520000), QStringLiteral("3.5"));
    QCOMPARE(bandLabelForFrequencyHz(7025000), QStringLiteral("7"));
    QCOMPARE(bandLabelForFrequencyHz(10120000), QStringLiteral("10"));
    QCOMPARE(bandLabelForFrequencyHz(14205000), QStringLiteral("14"));
    QCOMPARE(bandLabelForFrequencyHz(18100000), QStringLiteral("18"));
    QCOMPARE(bandLabelForFrequencyHz(21300000), QStringLiteral("21"));
    QCOMPARE(bandLabelForFrequencyHz(24950000), QStringLiteral("24"));
    // Unverändert: was die Tabelle vorher schon konnte.
    QCOMPARE(bandLabelForFrequencyHz(28500000), QStringLiteral("28"));
    QCOMPARE(bandLabelForFrequencyHz(144300000), QStringLiteral("144"));

    qint64 low = 0;
    qint64 high = 0;
    QVERIFY(bandRangeHz(QStringLiteral("14"), low, high));
    QCOMPARE(low, 14000000LL);
    QCOMPARE(high, 14350000LL);
    QCOMPARE(bandBaseHz(QStringLiteral("1.8")), 1800000LL);
    QCOMPARE(bandBaseHz(QStringLiteral("3.5")), 3500000LL);

    // Aufsteigend, tiefstes zuerst -- worauf sich die Kopfzeile von
    // knownBands() festlegt und was die Transverter-Auswahl anzeigt.
    const QStringList bands = knownBands();
    QCOMPARE(bands.first(), QStringLiteral("1.8"));
    QVERIFY(bands.indexOf(QStringLiteral("7")) < bands.indexOf(QStringLiteral("14")));
    QVERIFY(bands.indexOf(QStringLiteral("28")) < bands.indexOf(QStringLiteral("144")));
    QCOMPARE(bands.last(), QStringLiteral("10368"));
}

void TestHfBands::gapsBetweenTheBandsStayUnknown()
{
    // Rundfunk und Sonstiges zwischen den Amateurbändern: kein Band.
    QCOMPARE(bandLabelForFrequencyHz(1500000), QString());
    QCOMPARE(bandLabelForFrequencyHz(5300000), QString());  // 60 m: bewusst nicht in der Tabelle
    QCOMPARE(bandLabelForFrequencyHz(9500000), QString());
    QCOMPARE(bandLabelForFrequencyHz(13000000), QString());
    QCOMPARE(bandLabelForFrequencyHz(27500000), QString());
    QCOMPARE(bandLabelForFrequencyHz(0), QString());
}

void TestHfBands::practiceDefinitionLoads()
{
    QString error;
    const ContestDefinition def = ContestDefinition::loadFromFile(
        QStringLiteral(CONTESTPROGRAMM_SOURCE_DIR "/resources/contest_definitions/uebung_kurzwelle.json"), &error);
    QVERIFY2(def.isValid(), qPrintable(error));
    QCOMPARE(def.id(), QStringLiteral("KW_UEBUNG"));
    QCOMPARE(def.scoring(), QStringLiteral("qso_count"));
    // Eine Nummernfolge über alle Bänder -- die KW-Übung ist der erste
    // ausgelieferte Contest, der serial_scope "contest" benutzt.
    QCOMPARE(def.serialScope(), QStringLiteral("contest"));
    QVERIFY(def.dupeScope().contains(QStringLiteral("mode")));
    // Der Multiplikator, der auf Kurzwelle ohne fremde Tabelle
    // auskommt: der Präfix steckt im Rufzeichen (core/CallsignPrefix.h).
    QCOMPARE(def.multiplierField(), QStringLiteral("prefix"));
    QCOMPARE(def.bands().first(), QStringLiteral("1.8"));
    // Jedes Band der Definition muss die Bandtabelle auch kennen,
    // sonst kann keine Gerätefrequenz jemals darauf zeigen.
    for (const QString& band : def.bands()) {
        qint64 low = 0;
        qint64 high = 0;
        QVERIFY2(bandRangeHz(band, low, high), qPrintable(band));
    }
    // RST + Nummer, kein Locator: auf KW wird keiner getauscht.
    QCOMPARE(def.exchangeFields().size(), 2);
    QCOMPARE(def.exchangeFields().at(1).key, QStringLiteral("serial"));
    QVERIFY(def.exchangeFields().at(1).autoIncrement);
}

// AppController::openDatabase() setzt bei einer frischen Datenbank den
// ersten Contest in Dateinamen-Reihenfolge, der nicht auf eine Betriebsart
// festgelegt ist. Das Übungslog heißt deshalb uebung_kurzwelle.json und
// sortiert hinter jede echte Contestdefinition -- wer das Programm zum
// ersten Mal startet, landet weiter in einem VHF/UHF-Contest und nicht in
// einer Übung. Dieser Prüfstand hält die Regel fest, damit die nächste
// neue Definition nicht unbemerkt die Vorbelegung verschiebt.
void TestHfBands::freshDatabaseDoesNotStartInThePracticeLog()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("frisch.sqlite"))));
    const QString active = controller.settings().activeContestId;
    QVERIFY(!active.isEmpty());
    QVERIFY2(active != QStringLiteral("KW_UEBUNG"), qPrintable(active));
    const ContestDefinition* def = controller.findContestDefinition(active);
    QVERIFY(def);
    QVERIFY(def->bands().contains(QStringLiteral("144")) || def->bands().contains(QStringLiteral("432")));
}

void TestHfBands::rigOnHfMovesTheLogToThatBand()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = std::make_unique<AppController>();
    QVERIFY(controller->openDatabase(dir.filePath(QStringLiteral("hf.sqlite"))));
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("KW_UEBUNG");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    controller->setSettings(settings);
    QVERIFY2(controller->findContestDefinition(settings.activeContestId) != nullptr,
             "uebung_kurzwelle.json wurde nicht gefunden");

    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    int received = 100;
    const auto logQso = [&](const QString& call) {
        log->setCallsign(call);
        log->setExchangeFieldValue(QStringLiteral("serial"), QString::number(++received));
        emit log->logRequested();
        const QVector<QsoRecord> qsos = controller->database().qsosForContest(settings.activeContestId);
        return qsos.isEmpty() ? QsoRecord() : qsos.last();
    };

    emit controller->rigctldClient().frequencyChanged(14205000);
    const QsoRecord first = logQso(QStringLiteral("DL1ABC"));
    QCOMPARE(first.band, QStringLiteral("14"));
    QCOMPARE(first.serialSent.value_or(-1), 1);

    emit controller->rigctldClient().frequencyChanged(7052000);
    const QsoRecord second = logQso(QStringLiteral("G3XYZ"));
    QCOMPARE(second.band, QStringLiteral("7"));
    // serial_scope "contest": die Nummer läuft über den Bandwechsel
    // hinweg weiter, sie beginnt nicht wieder bei 001.
    QCOMPARE(second.serialSent.value_or(-1), 2);

    // 50 MHz gehört nicht zur Definition: das Band bleibt, wo es war
    // (die bestehende Regel in applyRigFrequency(), unverändert).
    emit controller->rigctldClient().frequencyChanged(50150000);
    QCOMPARE(logQso(QStringLiteral("SP9QQQ")).band, QStringLiteral("7"));
}

QTEST_MAIN(TestHfBands)
#include "test_hf_bands.moc"
