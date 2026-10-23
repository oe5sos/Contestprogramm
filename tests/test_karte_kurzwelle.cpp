// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/test_karte_kurzwelle.cpp  (Contestprogramm)
// =================================================================
//
// Betreiber am 2026-10-23, nach zwei Runden Entwürfen: „3c".
//
// Das war das Blatt mit den Punkten in BANDFARBE und der Beschriftung
// nur für neue Länder. Der Grund stand im Entwurf: auf 10 000 km
// drängen sich zwanzig Rufzeichen auf wenigen Bildpunkten, und die
// Farbe sagt mehr als der Name — sie sagt, wo das Band offen ist.
//
// Geprüft wird hier, was sich ohne Augen prüfen lässt: die
// Bandzuordnung, dass beide Schalter aus sind, solange niemand sie
// einschaltet, und dass sie einen Neustart überleben. Wie es AUSSIEHT,
// entscheiden die Entwurfsblätter und der Betreiber.
//
// Modification history (Contestprogramm):
//   2026-10-23 — Original von Martin Fischer (OE5SOS), KI-gestützt
//                 über Anthropic Claude.
// =================================================================

#include <QtTest>

#include <QAction>
#include <QApplication>

#include "ui/MapWidget.h"

using namespace Contestprogramm;

class TestKarteKurzwelle : public QObject
{
    Q_OBJECT

private slots:
    void beideSchalterSindAusUndUeberlebenEinenNeustart();
    void dieStationKenntDenNeuenMultiplikator();
};

// ── Aus, bis jemand sie einschaltet ─────────────────────────────────
//
// Auf UKW wären beide eine Verschlechterung: ein Band, und jeder Name
// zählt. Sie dürfen also nichts verändern, solange niemand sie will --
// und wer sie will, soll sie nicht nach jedem Start neu suchen.
void TestKarteKurzwelle::beideSchalterSindAusUndUeberlebenEinenNeustart()
{
    MapWidget karte;

    const QString frisch = karte.preferencesText();
    QVERIFY2(frisch.contains(QStringLiteral("bandcolors=0")), qPrintable(frisch));
    QVERIFY2(frisch.contains(QStringLiteral("labelsneeded=0")), qPrintable(frisch));

    karte.setBandColorsEnabled(true);
    karte.setLabelsOnlyNeededEnabled(true);
    const QString gemerkt = karte.preferencesText();
    QVERIFY2(gemerkt.contains(QStringLiteral("bandcolors=1")), qPrintable(gemerkt));
    QVERIFY2(gemerkt.contains(QStringLiteral("labelsneeded=1")), qPrintable(gemerkt));

    // Ein zweites Fenster, wie nach einem Neustart: es übernimmt den
    // gemerkten Stand.
    MapWidget wieder;
    wieder.applyPreferencesText(gemerkt);
    QCOMPARE(wieder.preferencesText().contains(QStringLiteral("bandcolors=1")), true);
    QCOMPARE(wieder.preferencesText().contains(QStringLiteral("labelsneeded=1")), true);

    // Und wieder aus lässt sich auch.
    wieder.setBandColorsEnabled(false);
    QVERIFY(wieder.preferencesText().contains(QStringLiteral("bandcolors=0")));
}

// ── Die Station trägt die Antwort mit ───────────────────────────────
//
// `neededMultiplier` ist das Feld, an dem die Beschriftungsregel
// hängt. Es gehört zur Station, nicht in die Karte: die Karte weiß
// nichts über Multiplikatoren, und MainWindow fragt dafür den
// MultiplierTracker.
void TestKarteKurzwelle::dieStationKenntDenNeuenMultiplikator()
{
    MapWidget::Station s;
    // Voreinstellung: nein. Eine Station, über die niemand etwas
    // gesagt hat, ist kein neues Land -- und wird damit bei
    // eingeschalteter Regel NICHT beschriftet.
    QCOMPARE(s.neededMultiplier, false);

    s.callsign = QStringLiteral("VK9XYZ");
    s.grid = QStringLiteral("OH29");
    s.freqHz = 14195000;
    s.neededMultiplier = true;

    MapWidget karte;
    karte.setOwnGrid(QStringLiteral("JN67VV"));
    karte.setStations({s});
    QCOMPARE(karte.stations().size(), 1);
    QCOMPARE(karte.stations().first().neededMultiplier, true);
    QCOMPARE(karte.stations().first().freqHz, 14195000);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestKarteKurzwelle tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_karte_kurzwelle.moc"
