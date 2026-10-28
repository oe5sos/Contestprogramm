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
    void gearbeiteteAusblendenUndDerGewaehlteRing();
    void einGeschaetzterOrtIstKeinLocator();
    void derGradStehtInDerScheibeUndWaechstMit();
    void dieRueckkeuleStehtInDerKarte();
    void dieScheibeIstMehrAlsEinKreis();
    void inDerScheibeIstNichtsGrau();
    void dieScheibeDraengtDieLegendeNichtWeg();
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

// ── Gearbeitete ausblenden, und der gewaehlte Ring ──────────────────
//
// Betreiber am 2026-10-26: „die gespotteten soll man als option ein
// und ausblenden koennen, genau so wie die bereits erreichten qso im
// contest." Und: „wenn ich 500 km auswaehle, moechte ich den runden
// ring (500 km) in gelb/braun -- unsere farben sehen."
void TestKarteKurzwelle::gearbeiteteAusblendenUndDerGewaehlteRing()
{
    MapWidget karte;
    karte.setOwnGrid(QStringLiteral("JN67VV"));
    karte.setLabelsOnlyNeededEnabled(true);
    // Die Rasterfelder bleiben draussen: sie haben ihren eigenen
    // Schalter („Gearbeitete Felder faerben"), und ein gearbeitetes
    // QSO faerbt sonst sein ganzes Feld -- dann misst dieser Lauf die
    // Flaeche statt den Punkt.
    karte.setWorkedCellsLayerVisible(false);
    karte.resize(600, 600);

    MapWidget::Station s;
    s.callsign = QStringLiteral("DL1ABC");
    s.grid     = QStringLiteral("JN68AA");
    s.worked   = true;
    s.freqHz   = 144300000;

    const QImage leer = karte.grab().toImage();
    karte.setStations({s});
    const QImage mit = karte.grab().toImage();
    QVERIFY2(leer != mit, "das gearbeitete QSO wird gar nicht gezeichnet");

    // Beide Schalter sind an, solange niemand sie ausschaltet.
    QVERIFY2(karte.preferencesText().contains(QStringLiteral("worked=1")),
             qPrintable(karte.preferencesText()));
    karte.setWorkedVisible(false);
    // Das Bild muss sich wieder aendern. Auf Gleichheit mit dem leeren
    // Bild laesst sich nicht pruefen: eine gearbeitete Station setzt
    // auch die ODX-Zahl in der Spalte und toent ihr Rasterfeld, und
    // beides bleibt stehen -- zu Recht, es gilt ja weiter.
    QVERIFY2(karte.grab().toImage() != mit,
             "der Schalter „Gearbeitete anzeigen\" aendert nichts am Bild");
    QVERIFY2(karte.preferencesText().contains(QStringLiteral("worked=0")),
             qPrintable(karte.preferencesText()));
    karte.setWorkedVisible(true);

    // Der gewaehlte Radius wird in Bernstein gezogen: wechselt die
    // Reichweite, aendert sich das Bild auch ohne Stationen.
    karte.setStations({});
    karte.setVisibleRangeKm(500.0);
    const QImage bei500 = karte.grab().toImage();
    karte.setVisibleRangeKm(750.0);
    QVERIFY2(karte.grab().toImage() != bei500,
             "der Ring folgt der gewaehlten Reichweite nicht");
}

// ── Ein geschaetzter Ort ist kein Locator ───────────────────────────
//
// Betreiber am 2026-10-26, nach dem Contest: „weiters stimmen einige
// locator nicht automatisch mit den rufzeichen zusammen."
//
// Fuer jede Station ohne getauschten Locator traegt die Karte den
// Mittelpunkt des LANDES, damit der Punkt ueberhaupt irgendwo sitzt.
// Ein Klick darauf schrieb ihn bisher als Locator ins Austauschfeld
// -- und von dort ging er ins Log und ins Cabrillo.
void TestKarteKurzwelle::einGeschaetzterOrtIstKeinLocator()
{
    MapWidget karte;
    karte.setOwnGrid(QStringLiteral("JN67VV"));
    karte.resize(600, 600);

    MapWidget::Station geschaetzt;
    geschaetzt.callsign    = QStringLiteral("9A1AA");
    geschaetzt.grid        = QStringLiteral("JN75"); // Mittelpunkt Kroatiens
    geschaetzt.approximate = true;
    geschaetzt.worked      = false;
    geschaetzt.freqHz      = 144300000;
    karte.setStations({geschaetzt});
    karte.grab(); // zeichnen, damit die Punkte eine Lage haben

    QSignalSpy spion(&karte, &MapWidget::candidateActivated);
    QVERIFY(spion.isValid());

    // Den Punkt anklicken: seine Lage kennt nur die Karte, also ueber
    // das Bild suchen -- die einzige Stelle, die sich gegenueber der
    // leeren Karte geaendert hat.
    MapWidget leer;
    leer.setOwnGrid(QStringLiteral("JN67VV"));
    leer.resize(600, 600);
    const QImage ohne = leer.grab().toImage();
    const QImage mit  = karte.grab().toImage();
    QPoint treffer;
    for (int y = 0; y < ohne.height() && treffer.isNull(); ++y) {
        for (int x = 0; x < ohne.width(); ++x) {
            if (ohne.pixel(x, y) != mit.pixel(x, y)) { treffer = QPoint(x, y); break; }
        }
    }
    QVERIFY2(!treffer.isNull(), "die Station wird gar nicht gezeichnet");
    QTest::mouseClick(&karte, Qt::LeftButton, Qt::NoModifier, treffer);

    QCOMPARE(spion.count(), 1);
    QCOMPARE(spion.first().at(0).toString(), QStringLiteral("9A1AA"));
    QCOMPARE(spion.first().at(1).toString(), QStringLiteral("JN75"));
    QVERIFY2(spion.first().at(3).toBool(),
             "die Karte meldet den Landesmittelpunkt als echten Locator");
}

// ── Entwurf D: der Grad steht in der Scheibe ────────────────────────
//
// Betreiber am 2026-10-26: „vielleicht lassen sich auch die grad
// digital anzeigen. vielleicht im rotor innerhalb. sollte mit der
// groesse mitwachsen." Und zur Dopplung: „ich sehe gerade, dass rotor
// 1 und rotor 2 im fenster ersichtlich ist! vielleicht gibt es eine
// andere anordnung."
void TestKarteKurzwelle::derGradStehtInDerScheibeUndWaechstMit()
{
    MapWidget karte;
    karte.setOwnGrid(QStringLiteral("JN67VV"));
    karte.setRotor1Heading(true, 290.0, QStringLiteral("144"));
    karte.resize(900, 600);

    // Ohne Scheiben steht der Winkel oben als Text; mit Scheiben
    // steht er IN der Scheibe -- und oben nicht mehr. Dass sich das
    // Bild ueberhaupt aendert, zeigt schon der Schalter-Prueflauf;
    // hier geht es um die Flaeche, die der Textblock frei macht.
    const QImage ohne = karte.grab().toImage();
    karte.setRotorDialsVisible(true);
    const QImage mit = karte.grab().toImage();
    QVERIFY2(ohne != mit, "die Scheiben aendern nichts");

    // Mit der Groesse wachsen: eine breitere Spalte traegt eine
    // groessere Scheibe, und damit eine groessere Zahl.
    const QRectF klein = karte.rotorDialRectForTest(1);
    QVERIFY2(!klein.isEmpty(), "keine Scheibe gezeichnet");
    karte.resize(1400, 900);
    karte.grab();
    const QRectF gross = karte.rotorDialRectForTest(1);
    QVERIFY2(!gross.isEmpty(), "nach dem Vergroessern keine Scheibe");
    QVERIFY2(gross.width() >= klein.width(),
             qPrintable(QStringLiteral("die Scheibe waechst nicht mit: %1 -> %2")
                            .arg(klein.width()).arg(gross.width())));
}

// ── Die Rueckseite der Keule, in der Karte ──────────────────────────
//
// Betreiber am 2026-10-28, mit Bild: „der beam strahlt auf der
// rueckseite auch. dies wollte ich auch farblich, aber im anderen
// design markieren."
//
// Der Schalter „Rueckrichtung zeigen" gab es da schon -- er wirkte
// aber nur in den kleinen Scheiben der Zahlenspalte, nicht in der
// Karte selbst. Dieser Lauf haelt beide Haelften fest: ohne den
// Schalter genau eine Keule je Antenne, mit ihm eine zweite, 180 Grad
// gegenueber und als Rueckseite gekennzeichnet.
void TestKarteKurzwelle::dieRueckkeuleStehtInDerKarte()
{
    MapWidget karte;
    karte.resize(900, 600);
    karte.setOwnGrid(QStringLiteral("JN67VV"));
    karte.setRotor1Heading(true, 290.0, QStringLiteral("144"));

    auto rueckseiten = [](const QVector<QPair<double, bool>>& keulen) {
        QVector<double> out;
        for (const auto& k : keulen) {
            if (k.second) { out.append(k.first); }
        }
        return out;
    };

    const QVector<QPair<double, bool>> ohne = karte.beamAzimuthsForTest();
    QVERIFY2(!ohne.isEmpty(), "die Karte zeichnet gar keine Keule");
    QVERIFY2(rueckseiten(ohne).isEmpty(),
             "ohne den Schalter darf keine Rueckkeule da sein");
    const QImage bildOhne = karte.grab().toImage();

    karte.setRotorDialsBackward(true);
    const QVector<QPair<double, bool>> mit = karte.beamAzimuthsForTest();
    QCOMPARE(mit.size(), ohne.size() * 2);

    const QVector<double> hinten = rueckseiten(mit);
    QCOMPARE(hinten.size(), ohne.size());
    // 290 Grad vorne heisst 110 Grad hinten.
    QVERIFY2(!hinten.isEmpty() && qAbs(hinten.first() - 110.0) < 0.5,
             qPrintable(QStringLiteral("Rueckkeule bei %1 Grad statt 110")
                            .arg(hinten.isEmpty() ? -1.0 : hinten.first())));

    QVERIFY2(karte.grab().toImage() != bildOhne,
             "die Rueckkeule aendert nichts am Bild");
}


// ── „man sieht nur einen kreis" ─────────────────────────────────────
//
// Betreiber am 2026-10-28, mit Bild. Zwei Ursachen, beide messbar:
// die Scheibe hatte keine eigene Flaeche (sie lag in der Farbe der
// Spalte), und die Gradzahl stand in der MITTE -- bei 0 Grad, dem
// Stand nach jedem Start, deckte ihr Kasten genau die Nadel zu.
//
// Entwurf A (aus fuenf Blaettern gewaehlt) behebt beides. Dieser Lauf
// haelt die zwei Punkte fest, nicht das Aussehen: eine eigene, vom
// Spaltengrund verschiedene Flaeche, und ueber der Mitte etwas
// Helleres als der Grund, naemlich die Nadel.
void TestKarteKurzwelle::dieScheibeIstMehrAlsEinKreis()
{
    MapWidget karte;
    karte.resize(900, 620);
    karte.setOwnGrid(QStringLiteral("JN67VV"));
    karte.setRotorDialsVisible(true);
    // Die Keule zeigt nach Osten, damit sie dem N oben nicht in die
    // Pruefung faellt: bei 0 Grad liegt sie genau darauf und macht die
    // Stelle auch ohne Schrift hell (so gemessen, 2026-10-28).
    karte.setRotor1Heading(true, 90.0, QStringLiteral("144"));

    const QImage bild = karte.grab().toImage();
    const QRectF scheibe = karte.rotorDialRectForTest(1);
    QVERIFY2(!scheibe.isEmpty(), "die Scheibe hat keine Flaeche");

    const QPoint mitte = scheibe.center().toPoint();
    // Ein Punkt in der Scheibe, abseits von Keule, Nadel und Zahl:
    // auf halbem Weg nach Westen.
    const QRgb innen = bild.pixel(mitte.x() - int(scheibe.width() * 0.28), mitte.y());
    // Und einer ausserhalb, im Grund der Spalte.
    const QRgb grund = bild.pixel(int(scheibe.right()) + 6, mitte.y());
    QVERIFY2(innen != grund,
             "die Scheibe traegt dieselbe Farbe wie die Spalte -- sie ist nur ein Ring");

    // Und die Himmelsrichtungen: ohne sie liest man die Richtung nur
    // aus der Gradzahl ab, und der Ring ist Zierde. N steht oben,
    // innen am Rand -- dort muss Schrift liegen, also etwas deutlich
    // Helleres als der Grund.
    const int rText = int(scheibe.width() / 2.0) - 17;
    int hellstes = 0;
    for (int dx = -9; dx <= 9; ++dx) {
        for (int dy = -8; dy <= 8; ++dy) {
            hellstes = qMax(hellstes, qGray(bild.pixel(mitte.x() + dx, mitte.y() - rText + dy)));
        }
    }
    const int hellInnen = qGray(innen);
    QVERIFY2(hellstes > hellInnen + 40,
             qPrintable(QStringLiteral("oben am Rand steht kein N (hellstes %1, Grund %2)")
                            .arg(hellstes).arg(hellInnen)));
}


// ── „Die Farbe geht ab, leider nur grau" ────────────────────────────
//
// Betreiber am 2026-10-28, nachdem Entwurf A gebaut war. Er hatte
// recht: bernsteinfarben war nur die Keule. Ring (kBorder), Skala
// (kTextScale), Himmelsrichtungen (kTextTertiary) und die Nadel eines
// getrennten Rotors (kTextInactive) waren allesamt Grautoene, und die
// nehmen auf einer kleinen Scheibe den groesseren Teil der Flaeche
// ein.
//
// Gemessen wird die Eigenschaft, nicht der Farbwert: alles in der
// Scheibe, was ueberhaupt zu sehen ist, muss WARM sein -- mehr Rot als
// Blau. Ein Grauton hat beides gleich. Das gilt auch fuer den
// getrennten Rotor: sein Zustand steht in der Helligkeit, nicht in
// einem Wechsel nach Grau.
void TestKarteKurzwelle::inDerScheibeIstNichtsGrau()
{
    MapWidget karte;
    karte.resize(900, 620);
    karte.setOwnGrid(QStringLiteral("JN67VV"));
    karte.setRotorDialsVisible(true);
    karte.setRotorDialsBackward(true);

    for (bool amDraht : {true, false}) {
        karte.setRotor1Heading(true, 290.0, QStringLiteral("144"));
        karte.setRotorLinkLive(1, amDraht);
        const QImage bild = karte.grab().toImage();
        const QRectF scheibe = karte.rotorDialRectForTest(1);
        QVERIFY2(!scheibe.isEmpty(), "die Scheibe hat keine Flaeche");

        const QPointF mitte = scheibe.center();
        const double r = scheibe.width() / 2.0;
        int kalt = 0;
        int warm = 0;
        QRgb schlimmster = 0;
        for (int dy = int(-r); dy <= int(r); ++dy) {
            for (int dx = int(-r); dx <= int(r); ++dx) {
                if (dx * dx + dy * dy > int(r * r)) { continue; }
                const QRgb px = bild.pixel(int(mitte.x()) + dx, int(mitte.y()) + dy);
                // Nur was sich ueberhaupt abhebt; der Grund selbst ist
                // fast schwarz und hat keine Farbe zu zeigen.
                if (qGray(px) < 30) { continue; }
                if (qRed(px) > qBlue(px) + 8) {
                    ++warm;
                } else {
                    ++kalt;
                    if (schlimmster == 0) { schlimmster = px; }
                }
            }
        }
        QVERIFY2(warm > 200,
                 qPrintable(QStringLiteral("am Draht=%1: fast nichts zu sehen in der Scheibe (%2 Punkte)")
                                .arg(amDraht).arg(warm)));
        QVERIFY2(kalt * 20 < warm,
                 qPrintable(QStringLiteral("am Draht=%1: %2 graue gegen %3 warme Punkte, z.B. #%4")
                                .arg(amDraht).arg(kalt).arg(warm)
                                .arg(QString::number(schlimmster & 0xffffff, 16))));
    }
}


// ── „zurück 180°" stand auf „GEARBEITET" ────────────────────────────
//
// Betreiber am 2026-10-28, mit Bild. Die Legende steht fest am
// unteren Rand der Zahlenspalte und weicht keiner Scheibe aus; die
// Schranke, die ueber eine weitere Scheibe entscheidet, liess ihr
// pauschal 24 Punkte. Sie braucht aber zwei oder drei Zeilen zu je
// 16, je nachdem ob „Berge verdecken" dabei ist -- und die Zeile
// „zurück N°" unter der Scheibe war in der Rechnung gar nicht drin.
//
// Gemessen wird an mehreren Hoehen, weil der Fehler nur auftritt, wo
// es knapp wird: der Block der Scheiben muss ueber der Legende enden.
void TestKarteKurzwelle::dieScheibeDraengtDieLegendeNichtWeg()
{
    for (int hoehe : {520, 560, 600, 620, 660, 700, 760}) {
        MapWidget karte;
        karte.resize(900, hoehe);
        karte.setOwnGrid(QStringLiteral("JN67VV"));
        karte.setRotorDialsVisible(true);
        karte.setRotorDialsBackward(true);
        // Mit Horizontprofil hat die Legende DREI Zeilen statt zwei --
        // genau der Fall aus Martins Bild. Ohne es ging die alte
        // Schranke um sechs Punkte auf, und der Lauf haette nichts
        // gefunden (2026-10-28 gemessen).
        karte.setHorizonProfile(QVector<double>(360, 2.0));
        karte.setHorizonLayerVisible(true);
        karte.setRotor1Heading(true, 0.0, QStringLiteral("144"));
        karte.setRotor2Heading(true, 120.0, QStringLiteral("432"));
        karte.grab();

        const double unten = karte.rotorBlockBottomForTest();
        const double legende = karte.legendTopForTest();
        if (unten <= 0.0) { continue; }  // bei dieser Hoehe passt keine Scheibe
        QVERIFY2(unten <= legende,
                 qPrintable(QStringLiteral("Hoehe %1: die Scheiben enden bei %2, die Legende "
                                           "beginnt schon bei %3")
                                .arg(hoehe).arg(unten, 0, 'f', 0).arg(legende, 0, 'f', 0)));

        // Und beide Rotoren sind da. Die Schranke allein haette das
        // Gegenteil bewirkt: strenger gerechnet, waere Rotor 2 oefter
        // weggefallen als vorher. Darum richtet sich der Durchmesser
        // nach dem Platz, der fuer alle anstehenden bleibt.
        QVERIFY2(!karte.rotorDialRectForTest(2).isEmpty(),
                 qPrintable(QStringLiteral("Hoehe %1: Rotor 2 fehlt, obwohl er angeschlossen ist")
                                .arg(hoehe)));
    }
}


int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestKarteKurzwelle tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_karte_kurzwelle.moc"
