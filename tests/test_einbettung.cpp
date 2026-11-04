// tests/test_einbettung.cpp
//
// Das Contestprogramm als Fenster in einem FREMDEN Prozess (Longpath,
// seit 2026-11-04). Was dabei anders ist als eigenstaendig:
//
//   * applicationName/organizationName sind die des Wirts -- also darf
//     das Datenverzeichnis nicht an QStandardPaths::AppDataLocation
//     haengen (das zeigte auf Longpaths Ordner und das Log waere ein
//     anderes als eigenstaendig).
//   * Es gibt kein app-weites Stylesheet des Contestprogramms; der Wirt
//     hat sein eigenes. Fenster und Controller muessen trotzdem
//     entstehen und das Fenster muss sich zeigen.
//
// Betreiber am 2026-11-03: "es soll aber trotzdem alles auch einzeln
// funktionieren" -- darum aendert dieser Pruefstand nichts am
// eigenstaendigen Weg; main.cpp geht durch dieselben zwei Funktionen.

#include <QtTest>

#include <QApplication>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "app/Datenverzeichnis.h"
#include "ui/MainWindow.h"

using namespace Contestprogramm;

class TestEinbettung : public QObject
{
    Q_OBJECT

private slots:
    void dasDatenverzeichnisHaengtNichtAmProzessnamen();
    void dieUmgebungGehtVor();
    void fensterUndControllerEntstehenImFremdenProzess();
};

void TestEinbettung::dasDatenverzeichnisHaengtNichtAmProzessnamen()
{
    qunsetenv("CONTESTPROGRAMM_DATA_DIR");
    QApplication::setApplicationName(QStringLiteral("Longpath"));
    QApplication::setOrganizationName(QStringLiteral("Longpath"));
    const QString pfad = standardDatenverzeichnis();
    QVERIFY2(pfad.endsWith(QStringLiteral("/Contestprogramm/Contestprogramm")),
             qPrintable(QStringLiteral("Pfad im fremden Prozess: %1").arg(pfad)));
    QVERIFY2(!pfad.contains(QStringLiteral("/Longpath")),
             "Das Log darf nicht in Longpaths Ordner wandern");
    QCOMPARE(datenbankPfad(pfad), pfad + QStringLiteral("/contestprogramm.sqlite"));
}

void TestEinbettung::dieUmgebungGehtVor()
{
    QTemporaryDir ordner;
    QVERIFY(ordner.isValid());
    qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
    QCOMPARE(standardDatenverzeichnis(), ordner.path());
    qunsetenv("CONTESTPROGRAMM_DATA_DIR");
}

void TestEinbettung::fensterUndControllerEntstehenImFremdenProzess()
{
    QTemporaryDir ordner;
    QVERIFY(ordner.isValid());
    QApplication::setApplicationName(QStringLiteral("Longpath"));
    // Kein app.setStyleSheet(Style::appStyleSheet()) -- der Wirt hat seins.

    AppController controller;
    QString fehler;
    QVERIFY2(controller.openDatabase(datenbankPfad(ordner.path()), &fehler),
             qPrintable(fehler));
    // Wie test_vorgabe_ueberlappt_nicht: ohne Rufzeichen oeffnet das
    // Fenster die Einstellungen modal, und ohne diese Abschaltungen
    // greift es nach rigctld und Rotoren -- beides hat hier nichts zu
    // suchen.
    ContestSettings s = controller.settings();
    s.ownCallsign   = QStringLiteral("OE5SOS");
    s.ownGrid       = QStringLiteral("JN67UT");
    s.rigctldHost.clear();
    s.rotor1Enabled = false;
    s.rotor2Enabled = false;
    s.esmEnabled    = false;
    controller.setSettings(s);

    MainWindow fenster(controller);
    fenster.resize(1280, 800);
    fenster.show();
    QVERIFY(QTest::qWaitForWindowExposed(&fenster, 10000));
    QVERIFY(fenster.isVisible());
    QVERIFY2(!fenster.windowTitle().isEmpty(), "Das Fenster traegt einen Titel");
    fenster.close();
}

QTEST_MAIN(TestEinbettung)
#include "test_einbettung.moc"
