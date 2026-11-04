#pragma once

#include <QColor>
#include <QImage>
#include <QVector>
#include <QWidget>

class QPainter;

namespace Contestprogramm {

// Der Wasserfall als Panel im Logbuch.
//
// Oben eine schmale Spur des aktuellen Spektrums, darunter der Wasserfall.
// Die Werte kommen fertig von Longpath (core/SpectrumClient.h); hier wird
// nicht gerechnet, nur gefärbt und geschoben.
//
// ── Warum so wenig darin steht ──────────────────────────────────────────
//
// Betreiber am 2026-11-01: „WICHTIG ist, dass das logprogramm nie
// abstürzt. ... die anderen sachen ist alles nett, aber nicht so primär."
//
// Das ist die Bauvorschrift für dieses Panel. Es hat kein QRhi, keinen
// eigenen Faden, keine FFT, keine Mausbedienung, die etwas sendet. Es
// nimmt einen Vektor entgegen, malt ihn und vergisst ihn. Was es nicht
// gibt, kann das Logbuch nicht umwerfen.
//
// Die einzige Richtung nach außen ist breiteGeaendert(): nur dieses
// Widget weiß, wie viele Bildpunkte es hat, und genau das ist der Hebel,
// mit dem Longpath den Strom klein hält (siehe SpectrumClient).
class WaterfallWidget : public QWidget {
    Q_OBJECT

public:
    // Anzeigebereich in dBm. Fest, weil ein Regler dafür Bedienung wäre
    // und das hier ein Nebenfenster im Logbuch ist, kein Pult. -130 ist
    // etwa das Rauschen einer ruhigen 2-m-Station, -30 eine sehr starke
    // Ortsstation; dazwischen liegt alles, was im Contest vorkommt.
    static constexpr int kBodenDbm = -130;
    static constexpr int kDeckeDbm = -30;

    // Anteil der Höhe, den die Spur oben bekommt.
    static constexpr double kSpurAnteil = 0.32;

    // Hoehe der Frequenzskala zwischen Spur und Wasserfall, in Bildpunkten.
    static constexpr int kSkalaHoehe = 16;

    explicit WaterfallWidget(QWidget* parent = nullptr);

    // Ein Bild, ein Wert je Bildpunkt, roh als byte (dBm + 200).
    void zeigeSpektrum(const QVector<quint8>& werte);

    void setVerbunden(bool verbunden);

    // Farbig wie Longpath (Vorgabe-Palette dort) statt des Bernsteintons.
    // Eine Option, kein Umbau: beides bleibt, der Betreiber waehlt in
    // Datei > Einstellungen > Wasserfall (2026-11-03).
    void setFarbig(bool farbig);
    bool istFarbig() const { return m_farbig; }

    // Die Lage (siehe SpectrumClient::Lage): Bildmitte und Breite fuer die
    // Skala, Sendefrequenz und VFO fuer die Marke. Betreiber 2026-11-03:
    // "frequenz sollte angezeigt werden, genauso wo ich dann sende bzw. wo
    // meine frequenz steht." 0 = unbekannt: dann keine Skala, keine Marke
    // -- lieber nichts als etwas Falsches.
    void setLage(double mitteHz, double spanneHz, double vfoHz, double txHz,
                 bool sendet);

    // Pruefnaht: die Bildpunktspalte einer Frequenz, -1 wenn unbekannt
    // oder ausserhalb des Bildes.
    int xFuerFrequenzFuerTest(double hz) const { return xFuerFrequenz(hz); }
    // Pruefnaht: die Skalenschrittweite in Hz fuer die aktuelle Breite.
    double skalenSchrittFuerTest() const { return skalenSchrittHz(); }
    bool istVerbunden() const { return m_verbunden; }

    // Prüfnaht: die Farbe, die ein Wert bekommt. Einzeln prüfbar, damit
    // der Prüfstand die Zuordnung festhalten kann, ohne zu malen.
    QColor farbeFuerTest(quint8 wert) const { return farbe(wert); }

    // Prüfnaht: wie viele Zeilen der Wasserfall schon trägt.
    int zeilenFuerTest() const { return m_gefuellteZeilen; }

signals:
    // Die Breite in Bildpunkten hat sich geändert. Wer den Strom holt,
    // schickt daraufhin ein neues spectrum_start.
    void breiteGeaendert(int bildpunkte);

protected:
    void paintEvent(QPaintEvent* ereignis) override;
    void resizeEvent(QResizeEvent* ereignis) override;
    void showEvent(QShowEvent* ereignis) override;

private:
    QColor farbe(quint8 wert) const;
    void   legeBildNeuAn();
    int    xFuerFrequenz(double hz) const;
    double skalenSchrittHz() const;
    void   maleSkalaUndMarke(QPainter& p, int spurHoehe);

    QImage          m_wasserfall;
    QVector<quint8> m_letztesSpektrum;
    int             m_gefuellteZeilen = 0;
    bool            m_verbunden = false;
    bool            m_farbig = false;
    double          m_mitteHz  = 0.0;
    double          m_spanneHz = 0.0;
    double          m_vfoHz    = 0.0;
    double          m_txHz     = 0.0;
    bool            m_sendet   = false;
};

} // namespace Contestprogramm
