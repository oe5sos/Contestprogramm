// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/test_spots_kleiner_und_abschaltbar.cpp  (Contestprogramm)
// =================================================================
//
// Betreiber am 2026-10-24, nach sechs QSOs in CQ WW SSB: „meine logs
// werden nicht automatisch in der karte angezeigt. weiters sind die
// symbole für die spots sehr groß, ggf. andere variante plus ein ein
// und ausblenden als option." Und kurz darauf: „also ich sehe nur
// große blaue icons aber keine einzig markiertes log von mir."
//
// Nachgemessen am Malcode war es genau umgekehrt zu dem, was zählt:
//
//   eigenes QSO, Ort geschätzt   gefüllter Punkt  r = 2,0
//   Spot von jemand anderem      Ring             r = 4,0
//                                + Hof (gepunktet) r = 7,0
//
// Auf Kurzwelle tauscht keine Station einen Locator, also ist dort
// JEDER Ort geschätzt -- das eigene Log war damit der kleinste Punkt
// der ganzen Karte, und der Hof, der die Ausnahme meinen sollte, lag
// um jede einzelne Station.
//
// Geprüft wird hier mit Bildpunkten, nicht mit Augenmaß: zwei Bilder,
// die sich NUR in den Stationen unterscheiden, und gezählt wird, was
// sich geändert hat.
//
// Modification history (Contestprogramm):
//   2026-10-24 — Original von Martin Fischer (OE5SOS), KI-gestützt
//                 über Anthropic Claude.
// =================================================================

#include <QtTest>

#include <QApplication>
#include <QImage>

#include "ui/MapWidget.h"

using namespace Contestprogramm;

class TestSpotsKleinerUndAbschaltbar : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void spotsAusLaesstKeineSpurZurueck();
    void dasEigeneQsoIstNichtKleinerAlsEinSpot();
    void beideSchalterUeberlebenEinenNeustart();

private:
    // Ein Ort nahe genug am eigenen, dass er in der Vorgabe-Reichweite
    // (400 km) liegt -- sonst zeichnet die Karte gar nichts, und jeder
    // Vergleich hier waere grün aus dem falschen Grund.
    static MapWidget::Station bei(const QString& ruf, bool gearbeitet)
    {
        MapWidget::Station s;
        s.callsign = ruf;
        s.grid = QStringLiteral("JN68AA");
        s.approximate = true; // der Kurzwellenfall: nur das Land bekannt
        s.worked = gearbeitet;
        s.freqHz = 14195000;
        // Ohne Zeitstempel setzt die Karte keine frische Beschriftung
        // -- hier soll nur das SYMBOL gezaehlt werden.
        return s;
    }

    static void richteEin(MapWidget& karte)
    {
        karte.setOwnGrid(QStringLiteral("JN67VV"));
        // Beschriftung nur fuer neue Multiplikatoren: keine der
        // Stationen hier ist einer, also schreibt die Karte nichts --
        // gezaehlt werden damit wirklich nur die Punkte und Ringe.
        karte.setLabelsOnlyNeededEnabled(true);
        // Die Rasterfelder gehen aus: ein gearbeitetes QSO faerbt sonst
        // sein ganzes Feld ein, und der erste Lauf dieses Pruefstands
        // hat genau das gemessen -- 914 Bildpunkte fuer einen Punkt mit
        // r = 3. Gruen war er, nur nicht aus dem Grund, der hier
        // belegt werden soll.
        karte.setWorkedCellsLayerVisible(false);
        karte.resize(600, 600);
    }

    static QImage bild(MapWidget& karte)
    {
        QCoreApplication::processEvents();
        return karte.grab().toImage();
    }

    /// Bildpunkte, in denen sich zwei gleich grosse Bilder
    /// unterscheiden -- auf `nur` beschraenkt, wenn angegeben.
    static int unterschied(const QImage& a, const QImage& b, const QRect& nur = {})
    {
        if (a.size() != b.size()) { return -1; }
        const QRect bereich = nur.isNull() ? QRect(QPoint(0, 0), a.size()) : nur.intersected(QRect(QPoint(0, 0), a.size()));
        int n = 0;
        for (int y = bereich.top(); y <= bereich.bottom(); ++y) {
            for (int x = bereich.left(); x <= bereich.right(); ++x) {
                if (a.pixel(x, y) != b.pixel(x, y)) { ++n; }
            }
        }
        return n;
    }

    /// Der Kasten, in dem sich zwei Bilder unterscheiden.
    static QRect kasten(const QImage& a, const QImage& b)
    {
        QRect r;
        for (int y = 0; y < a.height(); ++y) {
            for (int x = 0; x < a.width(); ++x) {
                if (a.pixel(x, y) != b.pixel(x, y)) {
                    r = r.isNull() ? QRect(x, y, 1, 1) : r.united(QRect(x, y, 1, 1));
                }
            }
        }
        return r;
    }
};

void TestSpotsKleinerUndAbschaltbar::initTestCase()
{
    // Die Vorgabe muss „Spots an" sein: wer nichts einstellt, soll die
    // Karte wie bisher sehen. Nur der Hof ist neu aus.
    MapWidget frisch;
    const QString text = frisch.preferencesText();
    QVERIFY2(text.contains(QStringLiteral("spots=1")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("approxhalo=0")), qPrintable(text));
}

// ── „ein und ausblenden als option" ─────────────────────────────────
void TestSpotsKleinerUndAbschaltbar::spotsAusLaesstKeineSpurZurueck()
{
    MapWidget karte;
    richteEin(karte);

    const QImage leer = bild(karte);

    karte.setStations({bei(QStringLiteral("9A1AA"), false)});
    const QImage mitSpot = bild(karte);
    const int ink = unterschied(leer, mitSpot);
    QVERIFY2(ink > 0, "die Karte zeichnet den Spot gar nicht -- dann belegt der Rest hier nichts");

    karte.setSpotsVisible(false);
    const QImage ohneSpot = bild(karte);
    QCOMPARE(unterschied(leer, ohneSpot), 0);

    // Und wieder ein, mit demselben Bild wie vorher.
    karte.setSpotsVisible(true);
    QCOMPARE(unterschied(mitSpot, bild(karte)), 0);
    qInfo() << "Bildpunkte eines Spots:" << ink;
}

// ── „nur große blaue icons aber keine einzig markiertes log von mir" ─
void TestSpotsKleinerUndAbschaltbar::dasEigeneQsoIstNichtKleinerAlsEinSpot()
{
    MapWidget karte;
    richteEin(karte);
    const QImage leer = bild(karte);

    // Erst den Spot: sein Kasten sagt, WO das Symbol sitzt. Das
    // gearbeitete QSO steht auf demselben Locator, also an derselben
    // Stelle -- und nur dort wird gezaehlt.
    //
    // Warum ueberhaupt beschraenkt: ein gearbeitetes QSO aendert auch
    // die Zahlenspalte am rechten Rand, und der erste Lauf dieses
    // Pruefstands hat genau die mitgezaehlt (914 Bildpunkte in einem
    // Kasten von 361 x 173 fuer einen Punkt mit r = 3). Gruen war er,
    // aber er hat die Zahlenspalte gemessen, nicht das Symbol.
    karte.setStations({bei(QStringLiteral("9A1AA"), false)});
    const QImage mitSpot = bild(karte);
    const QRect wo = kasten(leer, mitSpot).adjusted(-12, -12, 12, 12);
    QVERIFY2(!wo.isNull(), "der Spot zeichnet nichts");
    const int spot = unterschied(leer, mitSpot, wo);

    karte.setStations({bei(QStringLiteral("9A1AA"), true)});
    const int eigenes = unterschied(leer, bild(karte), wo);

    qInfo() << "Bildpunkte: eigenes QSO" << eigenes << "/ Spot" << spot;
    QVERIFY2(eigenes >= spot,
             qPrintable(QStringLiteral("das eigene QSO belegt %1 Bildpunkte, ein Spot %2 -- "
                                       "fremde Meldungen sind wieder groesser als das eigene Log")
                            .arg(eigenes).arg(spot)));
}

// ── Der Hof ist einschaltbar und beides bleibt gemerkt ──────────────
void TestSpotsKleinerUndAbschaltbar::beideSchalterUeberlebenEinenNeustart()
{
    MapWidget karte;
    richteEin(karte);
    karte.setStations({bei(QStringLiteral("9A1AA"), true)});
    const QImage ohneHof = bild(karte);

    karte.setApproximateHaloEnabled(true);
    const QImage mitHof = bild(karte);
    QVERIFY2(unterschied(ohneHof, mitHof) > 0,
             "der Hof laesst sich einschalten, aber die Karte sieht gleich aus");

    karte.setSpotsVisible(false);
    const QString gemerkt = karte.preferencesText();
    QVERIFY2(gemerkt.contains(QStringLiteral("spots=0")), qPrintable(gemerkt));
    QVERIFY2(gemerkt.contains(QStringLiteral("approxhalo=1")), qPrintable(gemerkt));

    MapWidget wieder;
    wieder.applyPreferencesText(gemerkt);
    const QString danach = wieder.preferencesText();
    QVERIFY2(danach.contains(QStringLiteral("spots=0")), qPrintable(danach));
    QVERIFY2(danach.contains(QStringLiteral("approxhalo=1")), qPrintable(danach));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestSpotsKleinerUndAbschaltbar tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_spots_kleiner_und_abschaltbar.moc"
