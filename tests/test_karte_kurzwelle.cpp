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

#include <QSignalSpy>

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
    void dieRotorscheibenSindOptionUndDrehbar();
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

// ── Die Rotorscheiben in der Zahlenspalte ───────────────────────────
//
// Betreiber am 2026-10-25: „man sollte als option die beiden rotoren
// unterhalb der sonne hier im fenster ein/ausblenden koennen, dann
// kann man auf wunsch ein fenster sparen. aber dort sollte man auch
// den rotor drehen koennen." Aus drei Blaettern hat er „a" gewaehlt:
// zwei kleine Scheiben.
void TestKarteKurzwelle::dieRotorscheibenSindOptionUndDrehbar()
{
    MapWidget karte;
    karte.resize(900, 600);

    // Aus, solange niemand sie einschaltet -- wer das Rotorfenster
    // offen hat, will sie nicht doppelt.
    const QString frisch = karte.preferencesText();
    QVERIFY2(frisch.contains(QStringLiteral("rotordials=0")), qPrintable(frisch));
    QVERIFY2(frisch.contains(QStringLiteral("rotorback=0")), qPrintable(frisch));

    karte.setOwnGrid(QStringLiteral("JN67VV"));
    karte.setRotor1Heading(true, 290.0, QStringLiteral("144"));
    const QImage ohne = karte.grab().toImage();

    karte.setRotorDialsVisible(true);
    const QImage mit = karte.grab().toImage();
    QVERIFY2(ohne != mit, "die Scheibe wird nicht gezeichnet");

    // Die Rueckrichtung ist ein EIGENER Schalter.
    karte.setRotorDialsBackward(true);
    QVERIFY2(karte.grab().toImage() != mit, "die Rueckrichtung aendert nichts am Bild");
    const QString gemerkt = karte.preferencesText();
    QVERIFY2(gemerkt.contains(QStringLiteral("rotordials=1")), qPrintable(gemerkt));
    QVERIFY2(gemerkt.contains(QStringLiteral("rotorback=1")), qPrintable(gemerkt));

    // Und ein Klick in die Scheibe verlangt eine Drehung. Der Winkel
    // steht in der Nachricht -- oben in der Scheibe sind 0 Grad.
    QSignalSpy spion(&karte, &MapWidget::rotorRotateRequested);
    QVERIFY(spion.isValid());
    // Die Scheibe liegt in der Zahlenspalte rechts; ihre Lage merkt
    // sich das Widget beim Zeichnen, also erst zeichnen lassen.
    karte.grab();
    const QRectF scheibe = karte.rotorDialRectForTest(1);
    QVERIFY2(!scheibe.isEmpty(), "die Scheibe hat keine Flaeche");
    const QPointF oben(scheibe.center().x(), scheibe.top() + 4.0);
    QTest::mouseClick(&karte, Qt::LeftButton, Qt::NoModifier, oben.toPoint());
    QCOMPARE(spion.count(), 1);
    QCOMPARE(spion.first().at(0).toInt(), 1);
    const double grad = spion.first().at(1).toDouble();
    QVERIFY2(grad < 5.0 || grad > 355.0,
             qPrintable(QStringLiteral("oben in der Scheibe sind 0 Grad, gemeldet wurden %1").arg(grad)));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestKarteKurzwelle tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_karte_kurzwelle.moc"
