// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/test_jedes_panel_ausblendbar.cpp  (Contestprogramm)
// =================================================================
//
// Betreiber am 2026-10-23: „weiters möchte ich jedes fenster auch
// löschen, sprich anzeigen oder nicht anzeigen können. ausblenden
// meine ich!"
//
// Neun der zehn Panels standen in Fenster › Panels. Der
// SEITENBEREICH nicht -- ausgerechnet der, der die linke Spalte
// einnimmt und als erstes im Weg steht, wenn man Platz braucht.
//
// Dieser Lauf zählt nicht Einträge ab, sondern vergleicht gegen die
// Panels, die das Fenster WIRKLICH angemeldet hat. Eine Liste von
// Hand abzuzählen hieße, beim nächsten neuen Panel wieder nichts zu
// merken.
//
// Modification history (Contestprogramm):
//   2026-10-23 — Original von Martin Fischer (OE5SOS), KI-gestützt
//                 über Anthropic Claude.
// =================================================================

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "ui/MainWindow.h"
#include "ui/PanelContainerWidget.h"
#include "ui/PanelLayoutManager.h"

#include <memory>

using namespace Contestprogramm;

class TestJedesPanelAusblendbar : public QObject
{
    Q_OBJECT

private slots:
    void jedesAngemeldetePanelHatEinenSchalter();
    void derSeitenbereichLaesstSichAusUndWiederEinschalten();
    void ausgeblendetBleibtAusgeblendetNachEinemNeustart();

private:
    std::unique_ptr<AppController> baueSteuerung(QTemporaryDir& dir, const QString& datei)
    {
        auto c = std::make_unique<AppController>();
        if (!c->openDatabase(dir.filePath(datei))) { return nullptr; }
        ContestSettings s = c->settings();
        s.ownCallsign   = QStringLiteral("OE5SOS");
        s.ownGrid       = QStringLiteral("JN67UT");
        s.rigctldHost.clear();
        s.rotor1Enabled = false;
        s.rotor2Enabled = false;
        s.esmEnabled    = false;
        c->setSettings(s);
        return c;
    }
};

void TestJedesPanelAusblendbar::jedesAngemeldetePanelHatEinenSchalter()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir, QStringLiteral("panels.sqlite"));
    QVERIFY(c);

    MainWindow fenster(*c);

    // Alle Schalter, die das Menü anbietet.
    QSet<QString> imMenue;
    for (QAction* a : fenster.findChildren<QAction*>()) {
        const QString name = a->objectName();
        if (name.startsWith(QStringLiteral("panelAction_"))) {
            imMenue.insert(name.mid(QStringLiteral("panelAction_").size()));
        }
    }
    QVERIFY2(!imMenue.isEmpty(), "Fenster › Panels hat gar keine Einträge");

    // Und alle Panels, die das Fenster angemeldet hat. DAS ist der
    // Maßstab -- nicht eine Liste in diesem Prüflauf.
    const QStringList angemeldet = fenster.panelIdsForTest();
    QVERIFY2(angemeldet.size() >= 9,
             qPrintable(QStringLiteral("nur %1 Panels angemeldet").arg(angemeldet.size())));

    QStringList ohneSchalter;
    for (const QString& id : angemeldet) {
        // Die CW-Makro-Reihe hat einen eigenen Haken im Fenster-
        // Optionsmenü („CW-Makros anzeigen"). Zwei Schalter für eine
        // Sache würden früher oder später uneins -- sie bleibt
        // deshalb bewusst draußen.
        if (id == QStringLiteral("cwMacroRow")) { continue; }
        if (!imMenue.contains(id)) { ohneSchalter << id; }
    }
    QVERIFY2(ohneSchalter.isEmpty(),
             qPrintable(QStringLiteral("ohne Schalter zum Ausblenden: %1")
                            .arg(ohneSchalter.join(QStringLiteral(", ")))));
}

void TestJedesPanelAusblendbar::derSeitenbereichLaesstSichAusUndWiederEinschalten()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir, QStringLiteral("seitenbereich.sqlite"));
    QVERIFY(c);

    MainWindow fenster(*c);
    fenster.show();
    QVERIFY(QTest::qWaitForWindowExposed(&fenster));

    auto* schalter = fenster.findChild<QAction*>(QStringLiteral("panelAction_sidearea"));
    QVERIFY2(schalter, "kein Schalter für den Seitenbereich");

    auto* panel = fenster.panelForTest(QStringLiteral("sidearea"));
    QVERIFY2(panel, "der Seitenbereich ist gar nicht angemeldet");

    // Aus -- und er ist wirklich weg, nicht nur abgehakt.
    schalter->setChecked(false);
    QTRY_VERIFY_WITH_TIMEOUT(panel->isHidden(), 3000);

    // Und wieder ein.
    schalter->setChecked(true);
    QTRY_VERIFY_WITH_TIMEOUT(!panel->isHidden(), 3000);
}

// Betreiber am 2026-10-28, mit Bild: „der chatraum ist ueberall
// automatisch, ich kann ihn nicht ausblenden."
//
// Der Schalter war da und wirkte auch -- er hielt nur nicht. Die
// Sichtbarkeit wohnt im Layoutprofil, und dessen Panelliste wurde beim
// Anlegen von Hand aufgezaehlt (MainWindow.cpp, LayoutProfileManager-
// Konstruktor). Das Chat-Panel kam spaeter dazu und stand nie darin:
// gespeichert wurde es nie, und beim naechsten Anwenden eines Profils
// blieb es unberuehrt -- also sichtbar, in jedem der drei Profile.
//
// Darum prueft dieser Lauf nicht eine Liste, sondern das Verhalten:
// ausblenden, Fenster weg, Fenster neu -- es muss weg bleiben. Das
// fasst jedes kuenftige Panel mit, ohne dass hier jemand nachtraegt.
void TestJedesPanelAusblendbar::ausgeblendetBleibtAusgeblendetNachEinemNeustart()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir, QStringLiteral("ueberdauern.sqlite"));
    QVERIFY(c);

    QStringList ausgeblendet;
    {
        MainWindow fenster(*c);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster));

        for (QAction* a : fenster.findChildren<QAction*>()) {
            const QString name = a->objectName();
            if (!name.startsWith(QStringLiteral("panelAction_"))) { continue; }
            const QString id = name.mid(QStringLiteral("panelAction_").size());
            // Die CW-Makro-Reihe haengt an ihrer eigenen Einstellung,
            // der Seitenbereich steht nur da, wenn etwas drin ist --
            // beide fuehren ihre Sichtbarkeit nicht im Profil.
            if (id == QStringLiteral("cwMacroRow")
                || id == QStringLiteral("sidearea")) { continue; }
            PanelContainerWidget* panel = fenster.panelForTest(id);
            if (panel == nullptr || panel->isHidden()) { continue; }
            a->setChecked(false);
            QTRY_VERIFY_WITH_TIMEOUT(panel->isHidden(), 3000);
            ausgeblendet << id;
        }
        QVERIFY2(ausgeblendet.contains(QStringLiteral("chat")),
                 "das Chat-Panel war gar nicht sichtbar -- der Fall traegt nichts");
    }

    // Dasselbe Profil, dieselbe Datenbank, neues Fenster.
    MainWindow wieder(*c);
    wieder.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wieder));

    QStringList zurueckgekehrt;
    for (const QString& id : ausgeblendet) {
        PanelContainerWidget* panel = wieder.panelForTest(id);
        if (panel != nullptr && !panel->isHidden()) { zurueckgekehrt << id; }
    }
    QVERIFY2(zurueckgekehrt.isEmpty(),
             qPrintable(QStringLiteral("ausgeblendet und trotzdem wieder da: %1")
                            .arg(zurueckgekehrt.join(QStringLiteral(", ")))));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestJedesPanelAusblendbar tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_jedes_panel_ausblendbar.moc"
