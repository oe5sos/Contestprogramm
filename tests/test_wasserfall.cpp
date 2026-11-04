// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/test_wasserfall.cpp  (Contestprogramm)
// =================================================================
//
// Betreiber am 2026-11-01: „WICHTIG ist, dass das logprogramm nie
// abstürzt. sollte es probleme geben, muss ... die logs autoamtsich
// gespeichert weren. die anderen sachen ist alles nett, aber nciht so
// primär."
//
// Darum prüft dieser Lauf NICHT in erster Linie, dass der Wasserfall
// schön aussieht, sondern dass er nichts umwerfen kann. Das Spektrum
// kommt über eine Netzverbindung von einem anderen Programm; alles,
// was dort hereinkommt, ist fremd. Ein Rahmen, der mehr Punkte
// verspricht als er Bytes mitbringt, ist genau der Fall, in dem ein
// ungeprüfter Leser über das Ende des Puffers hinausliest -- und
// damit das Logbuch mitnimmt.
//
// ── Die Gegenprobe ──────────────────────────────────────────────────
//
// `einUngepruefterLeserWuerdeUeberlaufen` rechnet nach, was ein Leser
// ohne die Längenprüfung getan hätte: er hätte bei einem Rahmen mit
// 8192 versprochenen und 10 mitgelieferten Punkten 8182 Bytes hinter
// dem Puffer gelesen. Der Prüfstand belegt damit, dass die Prüfung
// nicht nur da ist, sondern den Unterschied macht.
//
// Modification history (Contestprogramm):
//   2026-11-01 — Original von Martin Fischer (OE5SOS), KI-gestützt
//                 über Anthropic Claude.
// =================================================================

#include <QtTest>

#include <cstdlib>

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QSignalSpy>
#include <QVector>

#include "core/SpectrumClient.h"
#include "ui/StyleKit.h"
#include "ui/WaterfallWidget.h"

using namespace Contestprogramm;

namespace {

// Baut einen TCI-Binärrahmen so, wie Longpath ihn schickt: 64 Byte
// Kopf, danach ein Byte je Punkt. Die Felder stehen an denselben
// Versätzen wie in Longpaths buildSpectrumPayload.
QByteArray baueRahmen(int punkteImKopf, int punkteWirklich,
                      quint32 stromTyp = 100, quint32 abtastTyp = 100,
                      quint32 fps = 10, quint8 fuellwert = 150)
{
    QByteArray rahmen(SpectrumClient::kKopfBytes + punkteWirklich, char(0));
    auto schreibe = [&rahmen](int versatz, quint32 wert) {
        auto* p = reinterpret_cast<quint8*>(rahmen.data()) + versatz;
        p[0] = quint8(wert & 0xFF);
        p[1] = quint8((wert >> 8) & 0xFF);
        p[2] = quint8((wert >> 16) & 0xFF);
        p[3] = quint8((wert >> 24) & 0xFF);
    };
    schreibe(4, fps);
    schreibe(8, abtastTyp);
    schreibe(20, quint32(punkteImKopf));
    schreibe(24, stromTyp);
    for (int i = 0; i < punkteWirklich; ++i) {
        rahmen[SpectrumClient::kKopfBytes + i] = char(fuellwert);
    }
    return rahmen;
}

} // namespace

class TestWasserfall : public QObject
{
    Q_OBJECT

private slots:
    // ── Das Lesen eines Rahmens ─────────────────────────────────────
    void einGueltigerRahmenKommtDurch();
    void einUngepruefterLeserWuerdeUeberlaufen();
    void einZuKurzerRahmenWirdVerworfen();
    void einFremderStromWirdVerworfen();
    void eineFremdeAbtastartWirdVerworfen();
    void einePunktzahlAusserhalbWirdVerworfen();
    void muellFaelltDurchOhneTreffer();

    // ── Das Panel ───────────────────────────────────────────────────
    void dieFarbeFolgtDemPegel();
    void inFarbeFolgtEsLongpath();
    void dieLageKommtAusDenTextzeilen();
    void dieSkalaBildetFrequenzenAb();
    void einSpektrumAndererBreiteWirftNichtsUm();
    void dieBreiteMeldetSichFuerDieVerdichtung();
    void ohneVerbindungStehtEsEhrlichDa();
};

// ── Ein gültiger Rahmen kommt durch ─────────────────────────────────
void TestWasserfall::einGueltigerRahmenKommtDurch()
{
    const QByteArray rahmen = baueRahmen(256, 256, 100, 100, 12, 150);
    int fps = 0;
    const QVector<quint8> werte = SpectrumClient::leseRahmenFuerTest(rahmen, &fps);
    QCOMPARE(werte.size(), 256);
    QCOMPARE(werte.first(), quint8(150));
    QCOMPARE(werte.last(), quint8(150));
    // Im Spektrumrahmen trägt das Feld an Versatz 4 die BILDRATE -- ein
    // Spektrum hat keine Abtastrate.
    QCOMPARE(fps, 12);
    // 150 - 200 = -50 dBm. Die Umrechnung macht der Empfänger, nicht
    // der Leser; hier steht nur, dass das Byte unangetastet ankommt.
    QCOMPARE(int(werte.first()) - SpectrumClient::kDbmVersatz, -50);
}

// ── Die Gegenprobe: was ohne die Längenprüfung geschähe ─────────────
//
// Dies ist der Prüfpunkt, an dem die Bedingung des Betreibers hängt.
// Der Rahmen behauptet im Kopf 8192 Punkte und bringt 10 mit. Ein
// Leser, der dem Kopf glaubt, liest 8182 Bytes hinter dem Ende des
// Puffers -- im besten Fall Müll, im schlechten ein Absturz, und
// mitten im Contest nimmt der das Logbuch mit.
void TestWasserfall::einUngepruefterLeserWuerdeUeberlaufen()
{
    const QByteArray rahmen = baueRahmen(8192, 10);

    // So weit wäre ein ungeprüfter Leser gelaufen:
    const int vorhanden = rahmen.size() - SpectrumClient::kKopfBytes;
    const int versprochen = 8192;
    QCOMPARE(vorhanden, 10);
    QCOMPARE(versprochen - vorhanden, 8182);

    // Und das ist, was wirklich passiert.
    QVERIFY(SpectrumClient::leseRahmenFuerTest(rahmen).isEmpty());

    // Auch der umgekehrte Fall: mehr Bytes als versprochen. Der Rahmen
    // gehört dann nicht zu uns -- vielleicht ein anderer Strom mit
    // zufällig passenden Nummern -- und wird nicht gedeutet.
    QVERIFY(SpectrumClient::leseRahmenFuerTest(baueRahmen(64, 256)).isEmpty());
}

void TestWasserfall::einZuKurzerRahmenWirdVerworfen()
{
    // Kürzer als der Kopf. Vor der ersten Längenprüfung wird kein Byte
    // gelesen -- auch nicht bei einem leeren Rahmen.
    QVERIFY(SpectrumClient::leseRahmenFuerTest(QByteArray()).isEmpty());
    QVERIFY(SpectrumClient::leseRahmenFuerTest(QByteArray(1, char(0))).isEmpty());
    QVERIFY(SpectrumClient::leseRahmenFuerTest(
                QByteArray(SpectrumClient::kKopfBytes - 1, char(0))).isEmpty());
    // Genau der Kopf, keine Nutzlast: Punktzahl 0 liegt unter kMinPunkte.
    QVERIFY(SpectrumClient::leseRahmenFuerTest(
                QByteArray(SpectrumClient::kKopfBytes, char(0))).isEmpty());
}

void TestWasserfall::einFremderStromWirdVerworfen()
{
    // Dieselbe Leitung trägt auch die Ströme, die TCI selbst kennt
    // (IQ, Audio). Ein Rahmen daraus ist kein Fehler, nur nicht unserer.
    for (quint32 fremd : {quint32(0), quint32(1), quint32(2), quint32(99), quint32(101)}) {
        QVERIFY2(SpectrumClient::leseRahmenFuerTest(baueRahmen(256, 256, fremd)).isEmpty(),
                 qPrintable(QStringLiteral("Stromtyp %1 wurde angenommen").arg(fremd)));
    }
}

void TestWasserfall::eineFremdeAbtastartWirdVerworfen()
{
    // Richtiger Strom, falsche Kodierung: 16-bit-Werte wären doppelt so
    // viele Bytes. Würde das durchgehen, stünde im Wasserfall Unsinn.
    QVERIFY(SpectrumClient::leseRahmenFuerTest(baueRahmen(256, 256, 100, 0)).isEmpty());
    QVERIFY(SpectrumClient::leseRahmenFuerTest(baueRahmen(256, 256, 100, 99)).isEmpty());
}

void TestWasserfall::einePunktzahlAusserhalbWirdVerworfen()
{
    // Unter der Untergrenze.
    QVERIFY(SpectrumClient::leseRahmenFuerTest(
                baueRahmen(SpectrumClient::kMinPunkte - 1,
                           SpectrumClient::kMinPunkte - 1)).isEmpty());
    // Über der Obergrenze -- der Fall, in dem eine verdorbene Zahl eine
    // Riesenbelegung auslösen würde.
    QVERIFY(SpectrumClient::leseRahmenFuerTest(
                baueRahmen(SpectrumClient::kMaxPunkte + 1,
                           SpectrumClient::kMaxPunkte + 1)).isEmpty());
    // Die Grenzen selbst gehen durch.
    QCOMPARE(SpectrumClient::leseRahmenFuerTest(
                 baueRahmen(SpectrumClient::kMinPunkte,
                            SpectrumClient::kMinPunkte)).size(),
             SpectrumClient::kMinPunkte);
    QCOMPARE(SpectrumClient::leseRahmenFuerTest(
                 baueRahmen(SpectrumClient::kMaxPunkte,
                            SpectrumClient::kMaxPunkte)).size(),
             SpectrumClient::kMaxPunkte);
}

// ── Müll fällt durch, ohne je ein Byte als Pegel zu deuten ──────────
void TestWasserfall::muellFaelltDurchOhneTreffer()
{
    // Zufallsrahmen jeder Länge. Entscheidend ist nicht, dass nichts
    // durchkommt (ein Zufallskopf KANN zufällig stimmen), sondern dass
    // ein durchgekommener Rahmen auch wirklich passt -- und dass der
    // Leser dabei nicht abstürzt.
    QRandomGenerator wuerfel(1101);
    for (int lauf = 0; lauf < 2000; ++lauf) {
        const int laenge = int(wuerfel.bounded(0, 400));
        QByteArray rahmen(laenge, char(0));
        for (int i = 0; i < laenge; ++i) {
            rahmen[i] = char(wuerfel.bounded(0, 256));
        }
        const QVector<quint8> werte = SpectrumClient::leseRahmenFuerTest(rahmen);
        if (!werte.isEmpty()) {
            QCOMPARE(rahmen.size() - SpectrumClient::kKopfBytes, werte.size());
            QVERIFY(werte.size() >= SpectrumClient::kMinPunkte);
            QVERIFY(werte.size() <= SpectrumClient::kMaxPunkte);
        }
    }
}

// ── Die Farbe folgt dem Pegel ───────────────────────────────────────
void TestWasserfall::dieFarbeFolgtDemPegel()
{
    WaterfallWidget panel;

    const quint8 boden = quint8(WaterfallWidget::kBodenDbm + SpectrumClient::kDbmVersatz);
    const quint8 decke = quint8(WaterfallWidget::kDeckeDbm + SpectrumClient::kDbmVersatz);

    // Boden und darunter: der Hintergrund des Panels, nicht Schwarz --
    // sonst stünde ein schwarzes Loch im Thema.
    QCOMPARE(panel.farbeFuerTest(boden), QColor(Style::kPanelBg()));
    QCOMPARE(panel.farbeFuerTest(quint8(boden - 20)), QColor(Style::kPanelBg()));

    // Decke und darüber: die helle Hausfarbe.
    QCOMPARE(panel.farbeFuerTest(decke), QColor(Style::kAmberText()));
    QCOMPARE(panel.farbeFuerTest(255), QColor(Style::kAmberText()));

    // Dazwischen monoton heller. Eine Rampe, die irgendwo zurückgeht,
    // würde ein stärkeres Signal schwächer aussehen lassen.
    int vorher = -1;
    for (int wert = boden; wert <= decke; ++wert) {
        const QColor farbe = panel.farbeFuerTest(quint8(wert));
        const int helligkeit = farbe.red() + farbe.green() + farbe.blue();
        QVERIFY2(helligkeit >= vorher,
                 qPrintable(QStringLiteral("Pegel %1 ist dunkler als %2")
                                .arg(wert - SpectrumClient::kDbmVersatz)
                                .arg(wert - 1 - SpectrumClient::kDbmVersatz)));
        vorher = helligkeit;
    }
    // Und am Ende wirklich heller als am Anfang.
    const QColor kalt = panel.farbeFuerTest(boden);
    const QColor heiss = panel.farbeFuerTest(decke);
    QVERIFY(heiss.red() + heiss.green() + heiss.blue()
            > kalt.red() + kalt.green() + kalt.blue());
}

// ── In Farbe folgt es Longpath ──────────────────────────────────────
//
// Betreiber am 2026-11-03: "die farben soll als option auch wie bei
// longpath in farbe angezeigt werden." Die Option schaltet auf Longpaths
// Vorgabe-Palette um; die Stuetzstellen sind dieselben wie dort, also
// muss ein Pegel hier dieselbe Farbe bekommen wie im Longpath-Fenster.
// Ohne die Option bleibt alles beim Bernstein (dieFarbeFolgtDemPegel).
void TestWasserfall::inFarbeFolgtEsLongpath()
{
    WaterfallWidget panel;
    const quint8 boden = quint8(WaterfallWidget::kBodenDbm + SpectrumClient::kDbmVersatz);
    const quint8 decke = quint8(WaterfallWidget::kDeckeDbm + SpectrumClient::kDbmVersatz);

    // Vorgabe: aus, Bernstein wie bisher.
    QVERIFY(!panel.istFarbig());
    QCOMPARE(panel.farbeFuerTest(decke), QColor(Style::kAmberText()));

    panel.setFarbig(true);
    QVERIFY(panel.istFarbig());

    // Longpaths Stuetzstellen (SpectrumWidget.cpp, kDefaultStops):
    // Boden Schwarz, 30 % Blau, 60 % Gruen, Decke Rot.
    QCOMPARE(panel.farbeFuerTest(boden),             QColor(0, 0, 0));
    QCOMPARE(panel.farbeFuerTest(quint8(boden - 20)), QColor(0, 0, 0));
    QCOMPARE(panel.farbeFuerTest(quint8(boden + 30)), QColor(0, 64, 255));
    QCOMPARE(panel.farbeFuerTest(quint8(boden + 60)), QColor(0, 220, 0));
    QCOMPARE(panel.farbeFuerTest(decke),             QColor(255, 0, 0));
    QCOMPARE(panel.farbeFuerTest(255),               QColor(255, 0, 0));

    // Dazwischen kein Sprung: benachbarte Pegel liegen je Kanal hoechstens
    // eine Stuetzstellen-Steigung auseinander (steilste: 255 ueber 15 dB).
    QColor vorher = panel.farbeFuerTest(boden);
    for (int wert = boden + 1; wert <= decke; ++wert) {
        const QColor jetzt = panel.farbeFuerTest(quint8(wert));
        QVERIFY2(std::abs(jetzt.red()   - vorher.red())   <= 18
              && std::abs(jetzt.green() - vorher.green()) <= 18
              && std::abs(jetzt.blue()  - vorher.blue())  <= 18,
                 qPrintable(QStringLiteral("Sprung bei Pegel %1 dBm")
                                .arg(wert - SpectrumClient::kDbmVersatz)));
        vorher = jetzt;
    }

    // Und zurueck: wieder Bernstein, nichts bleibt haengen.
    panel.setFarbig(false);
    QCOMPARE(panel.farbeFuerTest(decke), QColor(Style::kAmberText()));
    QCOMPARE(panel.farbeFuerTest(boden), QColor(Style::kPanelBg()));
}

// ── Die Lage kommt aus den Textzeilen ────────────────────────────────
//
// Gemessen am 2026-11-03 gegen das laufende Longpath an der Flex: nach
// dem Handschlag kommen `dds:0,14160379;`, `vfo:0,0,14160379;`,
// `vfo:0,1,...;`, `tx_frequency:14160379;`, `trx:0,false;`, und mit dem
// ersten Bild `spectrum_span:0,200000;` und `spectrum_centre:0,...;`.
// `dds:` ist dabei das VFO, nicht die Bildmitte -- darum wird es hier
// NICHT gedeutet. Empfaenger 1 und VFO B fallen durch.
void TestWasserfall::dieLageKommtAusDenTextzeilen()
{
    SpectrumClient::Lage lage;
    QVERIFY(!SpectrumClient::deuteTextFuerTest(QStringLiteral("protocol:ExpertSDR3,2.0;"), lage));
    QVERIFY(!SpectrumClient::deuteTextFuerTest(QStringLiteral("dds:0,14160379;"), lage));
    QCOMPARE(lage.mitteHz, 0.0);

    // Mehrere Befehle in einer Nachricht, wie die Startfolge sie bringt.
    QVERIFY(SpectrumClient::deuteTextFuerTest(
        QStringLiteral("vfo:0,0,14160379;vfo:0,1,14170000;vfo:1,0,7100000;"), lage));
    QCOMPARE(lage.vfoHz, 14160379.0);

    QVERIFY(SpectrumClient::deuteTextFuerTest(QStringLiteral("tx_frequency:14160379;"), lage));
    QCOMPARE(lage.txHz, 14160379.0);
    QVERIFY(!SpectrumClient::deuteTextFuerTest(QStringLiteral("tx_frequency:14160379;"), lage));

    QVERIFY(SpectrumClient::deuteTextFuerTest(QStringLiteral("spectrum_span:0,200000;"), lage));
    QVERIFY(SpectrumClient::deuteTextFuerTest(QStringLiteral("spectrum_centre:0,14166500;"), lage));
    QCOMPARE(lage.spanneHz, 200000.0);
    QCOMPARE(lage.mitteHz, 14166500.0);
    QVERIFY(!SpectrumClient::deuteTextFuerTest(QStringLiteral("spectrum_centre:1,7085340;"), lage));
    QCOMPARE(lage.mitteHz, 14166500.0);

    QVERIFY(!lage.sendet);
    QVERIFY(SpectrumClient::deuteTextFuerTest(QStringLiteral("trx:0,true;"), lage));
    QVERIFY(lage.sendet);
    QVERIFY(!SpectrumClient::deuteTextFuerTest(QStringLiteral("trx:1,false;"), lage));
    QVERIFY(lage.sendet);
    QVERIFY(SpectrumClient::deuteTextFuerTest(QStringLiteral("trx:0,false;"), lage));
    QVERIFY(!lage.sendet);

    // Muell faellt durch, ohne etwas zu veraendern.
    const SpectrumClient::Lage vorher = lage;
    QVERIFY(!SpectrumClient::deuteTextFuerTest(QStringLiteral("spectrum_centre:0,abc;"), lage));
    QVERIFY(!SpectrumClient::deuteTextFuerTest(QStringLiteral(":::;;;,,,"), lage));
    QVERIFY(!SpectrumClient::deuteTextFuerTest(QString(), lage));
    QVERIFY(lage == vorher);
}

// ── Die Skala bildet Frequenzen ab ───────────────────────────────────
//
// Das Bild deckt mitte +- spanne/2 ab, links die tiefste Frequenz (so
// verdichtet Longpath). Ohne Lage gibt es keine Spalte -- lieber keine
// Skala als eine falsche.
void TestWasserfall::dieSkalaBildetFrequenzenAb()
{
    WaterfallWidget panel;
    panel.resize(1000, 300);

    QCOMPARE(panel.xFuerFrequenzFuerTest(14166500.0), -1);   // Lage unbekannt
    QCOMPARE(panel.skalenSchrittFuerTest(), 0.0);

    panel.setLage(14166500.0, 200000.0, 14160379.0, 14160379.0, false);
    QCOMPARE(panel.xFuerFrequenzFuerTest(14066500.0), 0);      // linke Kante
    QCOMPARE(panel.xFuerFrequenzFuerTest(14166500.0), 500);    // Mitte
    QCOMPARE(panel.xFuerFrequenzFuerTest(14266500.0), 999);    // rechte Kante
    QCOMPARE(panel.xFuerFrequenzFuerTest(14060000.0), -1);     // ausserhalb
    QCOMPARE(panel.xFuerFrequenzFuerTest(14300000.0), -1);
    // 200 kHz auf 1000 Bildpunkten -> 25-kHz-Schritte (acht Marken),
    // 48 kHz -> 10 kHz; auf 256 Bildpunkten passen nur vier: 50 kHz.
    QCOMPARE(panel.skalenSchrittFuerTest(), 25000.0);
    panel.setLage(14166500.0, 48000.0, 14160379.0, 14160379.0, false);
    QCOMPARE(panel.skalenSchrittFuerTest(), 10000.0);
    panel.resize(256, 300);
    panel.setLage(14166500.0, 200000.0, 14160379.0, 14160379.0, false);
    QCOMPARE(panel.skalenSchrittFuerTest(), 50000.0);
    panel.resize(1000, 300);

    // Eine Breite von 0 heisst unbekannt, nicht "unendlich schmal".
    panel.setLage(14166500.0, 0.0, 14160379.0, 14160379.0, false);
    QCOMPARE(panel.xFuerFrequenzFuerTest(14166500.0), -1);
}

// ── Ein Spektrum anderer Breite wirft nichts um ─────────────────────
//
// Die Punktzahl wird vom Panel genannt, aber ein Rahmen von vorhin
// kann noch mit der alten Breite unterwegs sein -- und nach einem
// Größenändern ist er das regelmäßig. Gestreckt wird, nicht geglaubt.
void TestWasserfall::einSpektrumAndererBreiteWirftNichtsUm()
{
    WaterfallWidget panel;
    panel.resize(400, 300);

    for (int punkte : {1, 2, 16, 399, 400, 401, 8192}) {
        QVector<quint8> werte(punkte, quint8(150));
        panel.zeigeSpektrum(werte);   // darf nicht abstürzen
    }
    // Ein leeres Spektrum ist kein Bild und zählt nicht.
    const int vorher = panel.zeilenFuerTest();
    panel.zeigeSpektrum(QVector<quint8>());
    QCOMPARE(panel.zeilenFuerTest(), vorher);

    // Und das Panel im Nullformat -- der Fall beim Andocken/Ablösen.
    WaterfallWidget winzig;
    winzig.resize(0, 0);
    winzig.zeigeSpektrum(QVector<quint8>(256, quint8(150)));
}

// ── Die Breite meldet sich für die Verdichtung ──────────────────────
void TestWasserfall::dieBreiteMeldetSichFuerDieVerdichtung()
{
    WaterfallWidget panel;
    QSignalSpy horcher(&panel, &WaterfallWidget::breiteGeaendert);

    // ── Gefunden von genau diesem Prüfstand ─────────────────────────
    //
    // Erster Lauf: kein einziges Signal. Qt stellt einem noch nie
    // gezeigten Widget das resizeEvent nicht zu, sondern legt es
    // zurück -- und das Panel startet versteckt (das Spektrum ist ab
    // Werk aus). Wird es dann in der richtigen Größe eingeblendet,
    // gibt es überhaupt kein Größenändern: die Breite hätte sich nie
    // gemeldet, und Longpath verdichtete weiter auf seine Vorgabe von
    // 256 Punkten. Dasselbe Muster wie in Longpath am 2026-10-31
    // („Wasserfall brauchte einen Klick"). Darum meldet jetzt auch das
    // showEvent.
    panel.resize(400, 300);
    panel.show();
    QVERIFY2(!horcher.isEmpty(), "ohne diese Meldung verdichtet Longpath auf die falsche Punktzahl");
    QCOMPARE(horcher.last().at(0).toInt(), 400);

    panel.resize(640, 300);
    QCOMPARE(horcher.last().at(0).toInt(), 640);

    // Dieselbe Breite meldet sich nicht nochmal -- jede Meldung kostet
    // ein spectrum_start auf der Leitung.
    const int bisher = horcher.count();
    panel.resize(640, 200);
    QCOMPARE(horcher.count(), bisher);
}

// ── Ohne Verbindung steht es ehrlich da ─────────────────────────────
//
// Ein schwarzes Feld ohne Erklärung ist der Fall, in dem jemand im
// Contest Zeit mit Suchen verliert. Siehe auch das Gedächtnis:
// „Abwesenheit ist keine Aussage".
void TestWasserfall::ohneVerbindungStehtEsEhrlichDa()
{
    WaterfallWidget panel;
    QVERIFY(!panel.istVerbunden());
    panel.setVerbunden(true);
    QVERIFY(panel.istVerbunden());
    panel.setVerbunden(false);
    QVERIFY(!panel.istVerbunden());
}

QTEST_MAIN(TestWasserfall)
#include "test_wasserfall.moc"
