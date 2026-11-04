// =================================================================
// tests/test_auto_pruefung.cpp  (Contestprogramm)
// =================================================================
//
// „grundsaetzlich sollten alle versionen einen automatischen updater
//  haben!" (Betreiber, 2026-11-03)
//
// Der Updater selbst (app/SelfUpdater.h) ist seit 2026-09-21 fertig
// und hat seinen eigenen Pruefstand (test_self_updater.cpp). Was hier
// geprueft wird, ist das, was bis 2026-11-03 FEHLTE: dass ueberhaupt
// jemand fragt, ohne dass der Bediener ein Menue aufmacht -- und dass
// das Ergebnis sichtbar dasteht.
//
// Der Kern ist nicht „er findet die neue Fassung" (das kann der
// Updater), sondern: NACH DEM START STEHT DORT ETWAS. Drei Lagen, drei
// Texte, und keine davon ist Stille:
//
//   Haken aus          -> „Pruefung aus"
//   Haken an, Eigenbau -> „Eigenbau" (dieses Pruefprogramm laeuft nicht
//                         aus einem App-Paket, also gibt es nichts zu
//                         tauschen -- der Grund steht im Tooltip)
//   neue Fassung da    -> anklickbarer Verweis, bernstein, NICHT blau
//
// Gegenprobe gegen die alte Fassung: die ersten beiden Pruefungen
// kommen ohne jedes neue Symbol aus (der Schalter wird direkt in die
// Einstellungstabelle geschrieben), laufen also auch gegen den Stand
// vor dieser Aenderung -- und fallen dort durch, weil in der
// Statuszeile nur „v0.4.1" steht und sonst nichts.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-11-04 — New. AI-assisted via Anthropic Claude Code, operator
//                 Ralph Martin Fischer.
// =================================================================

#include <QtTest>

#include <QApplication>
#include <QLabel>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "app/SelfUpdater.h"
#include "data/ContestDatabase.h"
#include "ui/MainWindow.h"
#include "ui/StyleKit.h"

using namespace Contestprogramm;

namespace {

// Die Fassungsanzeige in der Statuszeile. Sie heisst seit 2026-09-30
// "versionStatus"; findet der Pruefstand sie nicht, ist das ein
// Befund und keine Entschuldigung.
QLabel* fassungsanzeige(MainWindow& fenster)
{
    return fenster.findChild<QLabel*>(QStringLiteral("versionStatus"));
}

} // namespace

class TestAutoPruefung : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void hakenAusZeigtPruefungAus();
    void hakenAnZeigtDenStandUndNichtNichts();
    void neueFassungIstAnklickbarUndNichtBlau();
    void derHakenUeberlebtDenNeustart();
};

void TestAutoPruefung::initTestCase()
{
    Style::setActiveTheme(ContestSettings().colorTheme);
    qApp->setStyleSheet(Style::appStyleSheet());
}

// Haken aus: dort steht „Pruefung aus" -- nicht nichts. Eine fehlende
// Meldung wuerde sonst fuer „alles aktuell" genommen.
void TestAutoPruefung::hakenAusZeigtPruefungAus()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("aus.sqlite"))));
    // Absichtlich ueber die Einstellungstabelle und nicht ueber das
    // neue Feld: so laeuft dieser Pruefstand auch gegen die Fassung
    // vor dieser Aenderung (Gegenprobe, siehe Kopf).
    controller.database().setSettingValue(QStringLiteral("update_check_on_startup"),
                                          QStringLiteral("0"));
    ContestSettings geladen;
    geladen.loadFrom(controller.database());
    controller.setSettings(geladen);

    MainWindow fenster(controller);
    QLabel* anzeige = fassungsanzeige(fenster);
    QVERIFY2(anzeige, "keine Fassungsanzeige in der Statuszeile");
    QVERIFY2(anzeige->text().contains(QStringLiteral("Pruefung aus")),
             qPrintable(QStringLiteral("Statuszeile sagt nichts ueber die Pruefung: \"%1\"")
                            .arg(anzeige->text())));
}

// Haken an: in diesem Pruefprogramm gibt es kein App-Paket, das sich
// selbst ersetzen koennte -- also „Eigenbau", mit dem Grund im
// Tooltip. Entscheidend ist, dass ueberhaupt ein Zustand dasteht und
// die Anzeige nicht bei der nackten Fassungsnummer bleibt.
void TestAutoPruefung::hakenAnZeigtDenStandUndNichtNichts()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("an.sqlite"))));
    // Nichts setzen: an ist der Normalfall nach dem ersten Start.
    MainWindow fenster(controller);
    QLabel* anzeige = fassungsanzeige(fenster);
    QVERIFY2(anzeige, "keine Fassungsanzeige in der Statuszeile");
    const QString text = anzeige->text();
    QVERIFY2(text.contains(QStringLiteral("·")),
             qPrintable(QStringLiteral("kein Zustand neben der Fassung: \"%1\"").arg(text)));
    QVERIFY2(text.contains(QStringLiteral("Eigenbau")),
             qPrintable(QStringLiteral("Zustand ist nicht der erwartete: \"%1\"").arg(text)));
    // Der Grund gehoert in den Tooltip, nicht auf die Leiste.
    QVERIFY2(anzeige->toolTip().contains(QStringLiteral("Keine Pruefung")),
             qPrintable(QStringLiteral("Tooltip ohne Grund: \"%1\"").arg(anzeige->toolTip())));
    // Und die Pruefung darf dabei nicht ins Netz gegangen sein: ohne
    // Ziel gibt es auch nichts abzufragen. Geprueft wird das daran,
    // dass UpdateTarget::detect() hier wirklich unbrauchbar ist --
    // sonst stimmt die Lage dieses Pruefstands nicht mehr und die
    // Aussage oben waere zufaellig.
    QVERIFY2(!UpdateTarget::detect().usable(),
             "dieser Pruefstand laeuft aus einem App-Paket -- Annahme hinfaellig");
}

// Die neue Fassung ist ein Verweis, den man anklicken kann -- und sie
// ist bernstein, nicht blau. Qt faerbt Verweise von sich aus blau;
// Blau ist in diesem Haus keine Zustandsfarbe (Betreiber mehrfach,
// zuletzt 2026-11-02).
void TestAutoPruefung::neueFassungIstAnklickbarUndNichtBlau()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("neu.sqlite"))));
    MainWindow fenster(controller);
    QLabel* anzeige = fassungsanzeige(fenster);
    QVERIFY(anzeige);

    QVERIFY(QMetaObject::invokeMethod(&fenster, "zeigeFassungsstand",
                                      Q_ARG(QString, QStringLiteral("9.9.9 verfuegbar")),
                                      Q_ARG(bool, true),
                                      Q_ARG(QString, QStringLiteral("Auf GitHub liegt v9.9.9."))));
    const QString text = anzeige->text();
    QVERIFY2(text.contains(QStringLiteral("9.9.9 verfuegbar")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("<a href=")),
             qPrintable(QStringLiteral("die neue Fassung ist nicht anklickbar: \"%1\"").arg(text)));
    QVERIFY2(text.contains(Style::kAmberText()),
             qPrintable(QStringLiteral("die Farbe steht nicht im Anker: \"%1\"").arg(text)));
    // Keine der blauen Qt-Vorgaben, und auch kein eigenes Blau.
    for (const QString& blau : {QStringLiteral("#0000ff"), QStringLiteral("blue"),
                                 QStringLiteral("#0057"), QStringLiteral("#1e90ff")}) {
        QVERIFY2(!text.contains(blau, Qt::CaseInsensitive), qPrintable(text));
    }
    // Und der Verweis muss wirklich anklickbar sein -- ein Anker in
    // einem Label mit abgeschalteter Mausinteraktion ist nur Text.
    QVERIFY(anzeige->textInteractionFlags().testFlag(Qt::LinksAccessibleByMouse));
}

// Der Haken ueberlebt den Neustart: geschrieben, neu geladen, noch da.
// Ohne das waere die Abschaltung ein Versprechen fuer eine Sitzung.
void TestAutoPruefung::derHakenUeberlebtDenNeustart()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString pfad = dir.filePath(QStringLiteral("haken.sqlite"));
    {
        ContestDatabase db;
        QVERIFY(db.open(pfad));
        ContestSettings s;
        QVERIFY2(s.updateCheckOnStartup, "die Pruefung muss von Haus aus an sein");
        s.updateCheckOnStartup = false;
        s.saveTo(db);
    }
    {
        ContestDatabase db;
        QVERIFY(db.open(pfad));
        ContestSettings s;
        s.loadFrom(db);
        QVERIFY2(!s.updateCheckOnStartup, "der Haken war nach dem Neustart wieder an");
    }
}

QTEST_MAIN(TestAutoPruefung)
#include "test_auto_pruefung.moc"
