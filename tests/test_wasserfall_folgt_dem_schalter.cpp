// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/test_wasserfall_folgt_dem_schalter.cpp  (Contestprogramm)
// =================================================================
//
// Die Regel heißt: kein Spektrum, kein Wasserfall-Panel. Sie stand seit
// PR #67 in applyWaterfallVisibility -- und galt nur auf der freien
// Fläche. Im Seitenbereich liegt ein Panel als SEITE, und eine Seite
// lässt sich nicht verstecken: der Reiter bleibt, dahinter eine leere
// Fläche. Live gesehen am 2026-11-02, bei ausgeschaltetem Spektrum.
//
// Zwei Löcher, beide hier festgehalten:
//
//   1. Beim Start lief applyWaterfallVisibility VOR
//      restoreSideAreaState -- das Panel wurde versteckt und danach als
//      Seite wieder hineingelegt.
//   2. Beim Umschalten in den Einstellungen wurde ein Panel im
//      Seitenbereich nur versteckt, nicht herausgenommen.
//
// Jetzt verlässt das Panel bei „aus" den Seitenbereich, und bei „an"
// kommt es dorthin zurück, wo es lag.
//
// Modification history (Contestprogramm):
//   2026-11-03 — Original von Martin Fischer (OE5SOS), KI-gestützt
//                 über Anthropic Claude.
// =================================================================

#include <QtTest>

#include <QApplication>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "ui/MainWindow.h"
#include "ui/PanelContainerWidget.h"
#include "ui/SideAreaWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {
const QString kId = QStringLiteral("wasserfall");
}

class TestWasserfallFolgtDemSchalter : public QObject
{
    Q_OBJECT

private slots:
    void ausHeisstAuchImSeitenbereichWeg();
    void anBringtEsDorthinZurueck();
    void beimStartGewinntDerSchalterGegenDieGespeicherteSeite();

private:
    static std::unique_ptr<AppController> baueSteuerung(QTemporaryDir& dir,
                                                        const QString& datei,
                                                        bool spektrumAn)
    {
        auto c = std::make_unique<AppController>();
        if (!c->openDatabase(dir.filePath(datei))) { return nullptr; }
        ContestSettings s = c->settings();
        s.ownCallsign     = QStringLiteral("OE5SOS");
        s.ownGrid         = QStringLiteral("JN67UT");
        s.rigctldHost.clear();
        s.rotor1Enabled   = false;
        s.rotor2Enabled   = false;
        s.esmEnabled      = false;
        s.spectrumEnabled = spektrumAn;
        c->setSettings(s);
        return c;
    }

    static void schalteSpektrum(AppController& c, MainWindow& fenster, bool an)
    {
        ContestSettings s = c.settings();
        s.spectrumEnabled = an;
        c.setSettings(s);
        // Was openSettingsDialog nach dem OK tut.
        fenster.applyWaterfallVisibilityForTest();
    }
};

// ── Aus heißt auch im Seitenbereich: weg ────────────────────────────
void TestWasserfallFolgtDemSchalter::ausHeisstAuchImSeitenbereichWeg()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir, QStringLiteral("aus.sqlite"), /*spektrumAn=*/true);
    QVERIFY(c);
    MainWindow fenster(*c);

    PanelContainerWidget* panel = fenster.panelForTest(kId);
    QVERIFY(panel);
    SideAreaWidget* bereich = fenster.sideAreaForTest();
    QVERIFY(bereich);

    // Das Panel in den Seitenbereich legen, wie es der Bediener tut.
    fenster.putPanelIntoSideAreaForTest(kId);
    QVERIFY(bereich->hasPage(kId));

    schalteSpektrum(*c, fenster, false);

    QVERIFY2(!bereich->hasPage(kId),
             "das Panel liegt bei ausgeschaltetem Spektrum noch als Seite im "
             "Seitenbereich -- ein Reiter ueber einer leeren Flaeche");
    QVERIFY2(panel->isHidden(), "das Panel ist bei ausgeschaltetem Spektrum sichtbar");
}

// ── An bringt es dorthin zurück, wo es lag ──────────────────────────
void TestWasserfallFolgtDemSchalter::anBringtEsDorthinZurueck()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir, QStringLiteral("an.sqlite"), /*spektrumAn=*/true);
    QVERIFY(c);
    MainWindow fenster(*c);
    SideAreaWidget* bereich = fenster.sideAreaForTest();
    QVERIFY(bereich);

    fenster.putPanelIntoSideAreaForTest(kId);
    QVERIFY(bereich->hasPage(kId));

    schalteSpektrum(*c, fenster, false);
    QVERIFY(!bereich->hasPage(kId));

    schalteSpektrum(*c, fenster, true);
    QVERIFY2(bereich->hasPage(kId),
             "nach dem Wiedereinschalten liegt das Panel nicht mehr im "
             "Seitenbereich, wo der Bediener es hingelegt hatte");
    PanelContainerWidget* panel = fenster.panelForTest(kId);
    QVERIFY(panel);
    QVERIFY(!panel->isHidden());
}

// ── Beim Start gewinnt der Schalter gegen die gespeicherte Seite ────
//
// Der Fall, der live zu sehen war: der Seitenbereich hat den Wasserfall
// als Seite gespeichert, das Spektrum ist aus. Früher lief
// applyWaterfallVisibility zu früh und restoreSideAreaState legte die
// Seite danach wieder hinein.
void TestWasserfallFolgtDemSchalter::beimStartGewinntDerSchalterGegenDieGespeicherteSeite()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString datei = QStringLiteral("start.sqlite");

    // Erster Lauf: Spektrum an, Panel in den Seitenbereich, Fenster zu.
    {
        auto c = baueSteuerung(dir, datei, /*spektrumAn=*/true);
        QVERIFY(c);
        MainWindow fenster(*c);
        fenster.putPanelIntoSideAreaForTest(kId);
        QVERIFY(fenster.sideAreaForTest()->hasPage(kId));
        // Spektrum ausschalten, ohne die Sichtbarkeitsregel anzuwenden --
        // so sieht die Datenbank aus, wenn jemand die Einstellung in einer
        // älteren Fassung oder von Hand umgestellt hat.
        ContestSettings s = c->settings();
        s.spectrumEnabled = false;
        c->setSettings(s);
    }

    // Zweiter Lauf: dieselbe Datenbank. Die gespeicherte Seitenliste
    // nennt den Wasserfall, der Schalter sagt aus.
    auto c = baueSteuerung(dir, datei, /*spektrumAn=*/false);
    QVERIFY(c);
    MainWindow fenster(*c);
    SideAreaWidget* bereich = fenster.sideAreaForTest();
    QVERIFY(bereich);
    PanelContainerWidget* panel = fenster.panelForTest(kId);
    QVERIFY(panel);

    QVERIFY2(!bereich->hasPage(kId),
             "nach dem Start liegt der Wasserfall als Seite im Seitenbereich, "
             "obwohl das Spektrum aus ist -- die Wiederherstellung hat die "
             "Sichtbarkeitsregel ueberholt");
    QVERIFY2(panel->isHidden(), "das Panel ist nach dem Start sichtbar, obwohl aus");
}

QTEST_MAIN(TestWasserfallFolgtDemSchalter)
#include "test_wasserfall_folgt_dem_schalter.moc"
