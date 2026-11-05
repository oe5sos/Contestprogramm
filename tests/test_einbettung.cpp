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
#include <QPushButton>
#include <QMainWindow>
#include <QMenuBar>
#include <QSignalSpy>

#include <QApplication>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "app/Datenverzeichnis.h"
#include "ui/MainWindow.h"
#include "ui/PanelContainerWidget.h"

using namespace Contestprogramm;

class TestEinbettung : public QObject
{
    Q_OBJECT

private slots:
    void dasDatenverzeichnisHaengtNichtAmProzessnamen();
    void dieUmgebungGehtVor();
    void fensterUndControllerEntstehenImFremdenProzess();
    void panelsLassenSichVonAussenAbloesen();
    void rahmenlosSchwebendIstEckigMitRueckweg();
    void geschwisterDesOberstenFenstersUndDasKreuz();
    void eingebettetBleibtDieMenueleisteImFenster();
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

// Betreiber 2026-11-04: "alle panels haben hier noch runde kanten, dies
// bitte auf eckig umstellen." In Longpath schweben die Panels ohne
// Systemrahmen (FramelessWindowHint), eckig, und tragen den Rueckweg
// (↙) selbst, weil es das ✕ des Systems nicht mehr gibt. Eigenstaendig
// (Fahne aus) bleibt alles wie gehabt: Systemrahmen, kein ↙.
void TestEinbettung::rahmenlosSchwebendIstEckigMitRueckweg()
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

    auto rueckweg = [](QWidget* panel) -> QPushButton* {
        for (QPushButton* b : panel->findChildren<QPushButton*>()) {
            if (b->objectName() == QStringLiteral("panelHeaderDockButton")
                || b->objectName() == QStringLiteral("panelOverlayDockButton")) {
                return b;
            }
        }
        return nullptr;
    };

    // Eigenstaendig: Systemrahmen, kein Rueckweg in der Kopfzeile.
    QVERIFY(!PanelContainerWidget::rahmenlosSchwebend());
    fenster.setzePanelSchwebend(QStringLiteral("chat"), true);
    QWidget* chat = fenster.panelFenster(QStringLiteral("chat"));
    QVERIFY(chat && chat->isWindow());
    QVERIFY(!(chat->windowFlags() & Qt::FramelessWindowHint));
    QPushButton* knopf = rueckweg(chat);
    QVERIFY(!knopf || !knopf->isVisible());
    fenster.setzePanelSchwebend(QStringLiteral("chat"), false);

    // In Longpath: rahmenlos, eckig, ↙ sichtbar, und ↙ dockt zurueck.
    PanelContainerWidget::setzeRahmenlosSchwebend(true);
    fenster.setzePanelSchwebend(QStringLiteral("chat"), true);
    QVERIFY(chat->isWindow());
    QVERIFY2(chat->windowFlags() & Qt::FramelessWindowHint, "ohne Systemrahmen");
    QVERIFY(chat->testAttribute(Qt::WA_MacAlwaysShowToolWindow));
    QVERIFY2(chat->styleSheet().contains(QStringLiteral("border-radius: 0px")),
             qPrintable(QStringLiteral("eckig erwartet, Stil: %1").arg(chat->styleSheet())));
    knopf = rueckweg(chat);
    QVERIFY2(knopf && knopf->isVisible(), "der Rueckweg (↙) muss da sein");
    knopf->click();
    QVERIFY2(!chat->isWindow(), "↙ dockt das Panel zurueck");
    QVERIFY(!(chat->windowFlags() & Qt::FramelessWindowHint));
    QVERIFY(!chat->styleSheet().contains(QStringLiteral("border-radius: 0px")));

    // Auch das chromlose Panel (Karte) bekommt den Rueckweg.
    fenster.setzePanelSchwebend(QStringLiteral("map"), true);
    QWidget* karte = fenster.panelFenster(QStringLiteral("map"));
    QVERIFY(karte && karte->isWindow());
    QPushButton* kartenKnopf = rueckweg(karte);
    QVERIFY2(kartenKnopf && kartenKnopf->isVisible(), "auch die Karte braucht ↙");
    kartenKnopf->click();
    QVERIFY(!karte->isWindow());
    PanelContainerWidget::setzeRahmenlosSchwebend(false);
}

// Betreiber 2026-11-04: "nicht alle offenen fenster lassen sich durch
// anklicken in den vordergrund schieben" -- und: "alle fenster muessen
// sich mit X schliessen lassen!" Haengt dieses Hauptfenster selbst an
// einem Elternfenster (Longpath), wird ein schwebendes Panel ein
// Geschwister am OBERSTEN Fenster, nicht ein Kind des Logbuchs. Und das
// ✕ dockt es zurueck und blendet es aus; setzePanelSchwebend holt es
// wieder sichtbar hervor.
void TestEinbettung::geschwisterDesOberstenFenstersUndDasKreuz()
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

    QWidget oberstes;
    oberstes.resize(800, 600);
    oberstes.show();
    QVERIFY(QTest::qWaitForWindowExposed(&oberstes, 10000));
    auto* fenster = new MainWindow(controller, &oberstes);
    fenster->setWindowFlags((fenster->windowFlags() & ~Qt::WindowType_Mask) | Qt::Tool);
    PanelContainerWidget::setzeRahmenlosSchwebend(true);

    fenster->setzePanelSchwebend(QStringLiteral("chat"), true);
    QWidget* chat = fenster->panelFenster(QStringLiteral("chat"));
    QVERIFY(chat && chat->isWindow());
    QCOMPARE(chat->parentWidget(), static_cast<QWidget*>(&oberstes));

    QPushButton* kreuz = nullptr;
    for (QPushButton* b : chat->findChildren<QPushButton*>()) {
        if (b->objectName() == QStringLiteral("panelHeaderCloseButton")) { kreuz = b; }
    }
    QVERIFY2(kreuz && kreuz->isVisible(), "das ✕ muss in der Kopfzeile stehen");
    QSignalSpy versteckt(fenster->panelFenster(QStringLiteral("chat")) ? qobject_cast<PanelContainerWidget*>(chat) : nullptr,
                         &PanelContainerWidget::versteckt);
    kreuz->click();
    QVERIFY(!fenster->istPanelSchwebend(QStringLiteral("chat")));
    QVERIFY2(!chat->isVisible(), "nach ✕ ist das Panel ausgeblendet");
    QVERIFY(!fenster->istPanelSichtbar(QStringLiteral("chat")));
    QCOMPARE(versteckt.count(), 1);

    // Der Haken holt es wieder: sichtbar und schwebend.
    fenster->setzePanelSchwebend(QStringLiteral("chat"), true);
    QVERIFY(chat->isWindow());
    QVERIFY(fenster->istPanelSichtbar(QStringLiteral("chat")));

    // Chromloses Panel (Karte): ✕ als Knopf neben dem Schloss.
    fenster->setzePanelSchwebend(QStringLiteral("map"), true);
    QWidget* karte = fenster->panelFenster(QStringLiteral("map"));
    // Kopfzeile oder (chromlos) Overlay -- je nachdem, wie die Karte
    // gerade gebaut ist; beide tragen das ✕.
    QPushButton* kartenKreuz = nullptr;
    for (QPushButton* b : karte->findChildren<QPushButton*>()) {
        if (b->objectName() == QStringLiteral("panelOverlayCloseButton")
            || b->objectName() == QStringLiteral("panelHeaderCloseButton")) { kartenKreuz = b; }
    }
    QVERIFY2(kartenKreuz && kartenKreuz->isVisible(), "auch die Karte braucht ✕");
    kartenKreuz->click();
    QVERIFY(!karte->isWindow());
    QVERIFY(!karte->isVisible());

    PanelContainerWidget::setzeRahmenlosSchwebend(false);
    fenster->setzePanelSchwebend(QStringLiteral("chat"), false);
    delete fenster;
}

QTEST_MAIN(TestEinbettung)
#include "test_einbettung.moc"

// 2026-11-05, am Geraet gesehen: mit dem Log-Fenster vorn zeigte der Mac
// "Datei / Fenster / Hilfe" -- die Leiste des Contestprogramms statt der
// von Longpath. Eingebettet bleibt die Menueleiste im Fenster.
void TestEinbettung::eingebettetBleibtDieMenueleisteImFenster()
{
    QTemporaryDir ordner;
    QVERIFY(ordner.isValid());
    AppController controller;
    QString fehler;
    QVERIFY2(controller.openDatabase(datenbankPfad(ordner.path()), &fehler),
             qPrintable(fehler));

    QMainWindow oberstes;
    auto* eingebettet = new MainWindow(controller, &oberstes);
    QVERIFY(!eingebettet->menuBar()->isNativeMenuBar());

    MainWindow eigenstaendig(controller);
    // eigenstaendig: die Qt-Vorgabe der Plattform (auf dem Mac die Leiste
    // des Programms, offscreen keine) -- nichts erzwungen
    QMenuBar vorgabe;
    QCOMPARE(eigenstaendig.menuBar()->isNativeMenuBar(), vorgabe.isNativeMenuBar());
}
