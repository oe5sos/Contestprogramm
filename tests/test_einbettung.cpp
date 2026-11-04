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
    void panelsLassenSichVonAussenAbloesen();
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

// Betreiber 2026-11-04: "jedes panel vom contestprogramm sollte in
// longpath eigens hinzuzufuegen sein". Longpath holt ein Panel als
// eigenes Fenster, OHNE dieses Hauptfenster je zu zeigen -- also darf
// das Panel nicht mit 0x0 daherkommen, und es muss ein Fensterkind
// dieses Hauptfensters sein (Qt::Tool), damit es beim Wirt auf der Ebene
// der Panels liegt. Alte Fassung: die Methoden gibt es nicht.
void TestEinbettung::panelsLassenSichVonAussenAbloesen()
{
    QTemporaryDir ordner;
    QVERIFY(ordner.isValid());
    AppController controller;
    QString fehler;
    QVERIFY2(controller.openDatabase(datenbankPfad(ordner.path()), &fehler),
             qPrintable(fehler));
    ContestSettings s = controller.settings();
    s.ownCallsign   = QStringLiteral("OE5SOS");
    s.ownGrid       = QStringLiteral("JN67UT");
    s.rigctldHost.clear();
    s.rotor1Enabled = false;
    s.rotor2Enabled = false;
    s.esmEnabled    = false;
    controller.setSettings(s);

    MainWindow fenster(controller);
    // NICHT fenster.show(): genau der Fall "Panel ohne Hauptfenster".

    const QList<MainWindow::PanelEintrag> eintraege = MainWindow::panelEintraege();
    QVERIFY(eintraege.size() >= 8);
    for (const MainWindow::PanelEintrag& e : eintraege) {
        QVERIFY2(fenster.panelFenster(e.id) != nullptr,
                 qPrintable(QStringLiteral("Panel '%1' aus der Liste ist nicht angemeldet").arg(e.id)));
        QVERIFY(!e.titel.isEmpty());
    }

    QVERIFY(!fenster.istPanelSchwebend(QStringLiteral("chat")));
    fenster.setzePanelSchwebend(QStringLiteral("chat"), true);
    QWidget* chat = fenster.panelFenster(QStringLiteral("chat"));
    QVERIFY(chat);
    QVERIFY(fenster.istPanelSchwebend(QStringLiteral("chat")));
    QVERIFY2(chat->isWindow(), "als eigenes Fenster geloest");
    QCOMPARE(chat->windowFlags() & Qt::WindowType_Mask, Qt::Tool);
    QCOMPARE(chat->parentWidget(), &fenster);
    QVERIFY(QTest::qWaitForWindowExposed(chat, 10000));
    QVERIFY2(chat->width() >= 160 && chat->height() >= 120,
             qPrintable(QStringLiteral("Panel ohne Hauptfenster kam %1x%2 daher")
                            .arg(chat->width()).arg(chat->height())));
    QVERIFY2(!fenster.isVisible(), "das Hauptfenster bleibt dabei zu");

    // Auch aus dem Seitenbereich heraus.
    fenster.putPanelIntoSideAreaForTest(QStringLiteral("skeds"));
    fenster.setzePanelSchwebend(QStringLiteral("skeds"), true);
    QVERIFY(fenster.istPanelSchwebend(QStringLiteral("skeds")));
    QVERIFY(fenster.panelFenster(QStringLiteral("skeds"))->isWindow());

    fenster.setzePanelSchwebend(QStringLiteral("chat"), false);
    QVERIFY(!fenster.istPanelSchwebend(QStringLiteral("chat")));
    QVERIFY(!chat->isWindow());
    fenster.setzePanelSchwebend(QStringLiteral("skeds"), false);
}

QTEST_MAIN(TestEinbettung)
#include "test_einbettung.moc"
