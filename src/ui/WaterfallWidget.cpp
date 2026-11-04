#include "ui/WaterfallWidget.h"

#include "ui/StyleKit.h"

#include <QPaintEvent>
#include <QPainter>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QShowEvent>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Contestprogramm {

namespace {

// Was ein Byte bedeutet: dBm + 200 (Longpath, von piHPSDR übernommen).
constexpr int kDbmVersatz = 200;

int dbmAus(quint8 wert) { return int(wert) - kDbmVersatz; }

} // namespace

WaterfallWidget::WaterfallWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(160, 90);
    setAttribute(Qt::WA_OpaquePaintEvent);   // kein Hintergrund darunter nötig
}

void WaterfallWidget::legeBildNeuAn()
{
    const int b = std::max(1, width());
    // Spur oben, Skala dazwischen, der Rest Wasserfall.
    const int h = std::max(1, height() - int(height() * kSpurAnteil) - kSkalaHoehe);
    if (m_wasserfall.width() == b && m_wasserfall.height() == h) { return; }
    m_wasserfall = QImage(b, h, QImage::Format_RGB32);
    m_wasserfall.fill(QColor(Style::kPanelBg()));
    m_gefuellteZeilen = 0;
}

// ── Die Farbe ────────────────────────────────────────────────────────────
//
// Vom Hintergrund des Panels über ein gedämpftes Bernstein bis zum hellen
// Bernstein der Hausfarbe. Bewusst EIN Farbton statt eines Regenbogens:
// der Hausstil lebt davon, dass Farbe selten ist und dann etwas bedeutet,
// und ein Regenbogen behauptet Unterschiede, wo nur Pegel stehen.
//
// Alle drei Werte kommen aus StyleKit, folgen also dem gewählten Thema
// statt hier festgeschrieben zu sein.
// ── Die Farbe, wenn der Betreiber sie will ───────────────────────────────
//
// Die Vorgabe-Palette von Longpath, Stufe fuer Stufe: Schwarz, Dunkelblau,
// Blau, Cyan, Gruen, Gelb, Rot. Uebernommen aus Longpath
// src/gui/SpectrumWidget.cpp (kDefaultStops, "From AetherSDR
// SpectrumWidget.cpp:43-51", ten9876/AetherSDR, GPLv3 -- dieselbe Lizenz
// wie dieses Programm). Gleiche Stuetzstellen, gleiche lineare
// Zwischenfarbe mit Rundung, damit ein Signal hier dieselbe Farbe hat wie
// druebe im Longpath-Fenster.
namespace {
struct FarbStuetze { double pos; int r, g, b; };
constexpr FarbStuetze kLongpathStuetzen[] = {
    {0.00,   0,   0,   0},    // Schwarz
    {0.15,   0,   0, 128},    // Dunkelblau
    {0.30,   0,  64, 255},    // Blau
    {0.45,   0, 200, 255},    // Cyan
    {0.60,   0, 220,   0},    // Gruen
    {0.80, 255, 255,   0},    // Gelb
    {1.00, 255,   0,   0},    // Rot
};

QColor longpathFarbe(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    constexpr int n = int(sizeof(kLongpathStuetzen) / sizeof(kLongpathStuetzen[0]));
    for (int i = 0; i < n - 1; ++i) {
        const FarbStuetze& lo = kLongpathStuetzen[i];
        const FarbStuetze& hi = kLongpathStuetzen[i + 1];
        if (t <= hi.pos) {
            const double spanne = hi.pos - lo.pos;
            const double u = (spanne > 0.0) ? (t - lo.pos) / spanne : 0.0;
            return QColor(int(std::lround(lo.r + u * (hi.r - lo.r))),
                          int(std::lround(lo.g + u * (hi.g - lo.g))),
                          int(std::lround(lo.b + u * (hi.b - lo.b))));
        }
    }
    const FarbStuetze& letzte = kLongpathStuetzen[n - 1];
    return QColor(letzte.r, letzte.g, letzte.b);
}
}  // namespace

void WaterfallWidget::setFarbig(bool farbig)
{
    if (m_farbig == farbig) { return; }
    m_farbig = farbig;
    // Das stehende Bild ist in der alten Farbe gemalt; neu anlegen, statt
    // zwei Paletten uebereinander laufen zu lassen.
    legeBildNeuAn();
    update();
}

QColor WaterfallWidget::farbe(quint8 wert) const
{
    const int db = dbmAus(wert);
    if (m_farbig) {
        // Wie Longpath: unter dem Boden Schwarz, nicht der Panelgrund --
        // die Palette beginnt bei Schwarz, und ein Grund in Panelfarbe
        // saesse als Stufe im Verlauf.
        return longpathFarbe(double(db - kBodenDbm) / double(kDeckeDbm - kBodenDbm));
    }
    if (db <= kBodenDbm) { return QColor(Style::kPanelBg()); }

    const double t = std::clamp(double(db - kBodenDbm) / double(kDeckeDbm - kBodenDbm),
                                0.0, 1.0);
    const QColor kalt  = QColor(Style::kPanelBg());
    const QColor mitte = QColor(Style::kAmberDim());
    const QColor heiss = QColor(Style::kAmberText());

    // Zwei Abschnitte: unten kalt→gedämpft, oben gedämpft→hell. Die
    // untere Hälfte ist der Bereich, in dem Rauschen lebt; sie soll ruhig
    // bleiben, damit ein Signal sichtbar heraussticht.
    const QColor von = (t < 0.5) ? kalt : mitte;
    const QColor bis = (t < 0.5) ? mitte : heiss;
    const double u = (t < 0.5) ? (t * 2.0) : ((t - 0.5) * 2.0);

    return QColor(int(von.red()   + u * (bis.red()   - von.red())),
                  int(von.green() + u * (bis.green() - von.green())),
                  int(von.blue()  + u * (bis.blue()  - von.blue())));
}

void WaterfallWidget::zeigeSpektrum(const QVector<quint8>& werte)
{
    if (werte.isEmpty()) { return; }
    m_letztesSpektrum = werte;

    legeBildNeuAn();
    if (m_wasserfall.isNull() || m_wasserfall.height() < 2) { update(); return; }

    // Eine Zeile nach unten schieben. QImage hat dafür nichts (scroll()
    // ist QWidget), also von Hand -- von unten nach oben kopiert, damit
    // sich die Zeilen nicht gegenseitig überschreiben.
    const int zeilenBytes = m_wasserfall.bytesPerLine();
    for (int y = m_wasserfall.height() - 1; y > 0; --y) {
        std::memcpy(m_wasserfall.scanLine(y), m_wasserfall.scanLine(y - 1),
                    size_t(zeilenBytes));
    }

    // Oberste Zeile neu. Die Punktzahl des Rahmens muss nicht zur Breite
    // passen -- sie tut es im Normalfall, weil wir sie genannt haben, aber
    // ein Rahmen von vorhin kann noch mit der alten Breite unterwegs sein.
    // Darum wird gestreckt statt vorausgesetzt.
    auto* zeile = reinterpret_cast<QRgb*>(m_wasserfall.scanLine(0));
    const int b = m_wasserfall.width();
    const int n = werte.size();
    for (int x = 0; x < b; ++x) {
        const int i = (b == n) ? x : std::min(n - 1, int(qint64(x) * n / b));
        zeile[x] = farbe(werte.at(i)).rgb();
    }
    m_gefuellteZeilen = std::min(m_gefuellteZeilen + 1, m_wasserfall.height());
    update();
}

// ── Die Lage ─────────────────────────────────────────────────────────────

void WaterfallWidget::setLage(double mitteHz, double spanneHz, double vfoHz,
                              double txHz, bool sendet)
{
    if (m_mitteHz == mitteHz && m_spanneHz == spanneHz && m_vfoHz == vfoHz
        && m_txHz == txHz && m_sendet == sendet) {
        return;
    }
    m_mitteHz  = mitteHz;
    m_spanneHz = spanneHz;
    m_vfoHz    = vfoHz;
    m_txHz     = txHz;
    m_sendet   = sendet;
    update();
}

// Die Spalte einer Frequenz. Das Bild deckt mitte +- spanne/2 ab, die
// linke Kante ist die tiefste Frequenz -- so verdichtet Longpath es
// (TciServer::onSpektrumBereit: mittig beschnitten, Bins von tief nach
// hoch). -1, wenn die Lage unbekannt ist oder die Frequenz nicht im Bild.
int WaterfallWidget::xFuerFrequenz(double hz) const
{
    if (m_mitteHz <= 0.0 || m_spanneHz <= 0.0 || hz <= 0.0) { return -1; }
    const double tief = m_mitteHz - m_spanneHz * 0.5;
    const double anteil = (hz - tief) / m_spanneHz;
    if (anteil < 0.0 || anteil > 1.0) { return -1; }
    const int b = std::max(1, width());
    return std::min(b - 1, int(anteil * b));
}

// Ein "runder" Schritt, der hoechstens acht Beschriftungen ins Bild
// bringt -- und bei einem schmalen Panel weniger, weil jede Zahl rund
// 60 Bildpunkte braucht und sonst in die naechste laeuft. Bei 200 kHz und
// 1000 Bildpunkten sind das 25 kHz, bei 48 kHz 10 kHz, bei 256 Bildpunkten
// und 200 kHz 50 kHz.
double WaterfallWidget::skalenSchrittHz() const
{
    if (m_spanneHz <= 0.0) { return 0.0; }
    const double maxMarken = std::min(8.0, std::max(2.0, width() / 60.0));
    static constexpr double kSchritte[] = {
        1000, 2000, 2500, 5000, 10000, 20000, 25000, 50000,
        100000, 200000, 250000, 500000, 1000000, 2000000, 5000000 };
    for (const double schritt : kSchritte) {
        if (m_spanneHz / schritt <= maxMarken) { return schritt; }
    }
    return 10000000.0;
}

void WaterfallWidget::maleSkalaUndMarke(QPainter& p, int spurHoehe)
{
    const int oben = spurHoehe;                 // Oberkante der Skala
    const int unten = oben + kSkalaHoehe;       // Oberkante des Wasserfalls
    p.fillRect(QRect(0, oben, width(), kSkalaHoehe), QColor(Style::kPanelBg()));
    if (m_mitteHz <= 0.0 || m_spanneHz <= 0.0) { return; }

    // ── Die Skala ────────────────────────────────────────────────────
    const double schritt = skalenSchrittHz();
    const double tief = m_mitteHz - m_spanneHz * 0.5;
    const double hoch = m_mitteHz + m_spanneHz * 0.5;
    p.setFont(Style::monoFont(font(), 10));
    const QFontMetrics fm(p.font());
    p.setPen(QColor(Style::kTextTertiary()));
    // Erste Marke auf dem ersten Vielfachen des Schritts im Bild.
    for (double f = std::ceil(tief / schritt) * schritt; f <= hoch; f += schritt) {
        const int x = xFuerFrequenz(f);
        if (x < 0) { continue; }
        p.drawLine(x, oben, x, oben + 3);
        // MHz mit so vielen Stellen, wie der Schritt braucht: 25 kHz -> 3.
        const int stellen = (schritt >= 100000.0) ? 1 : (schritt >= 10000.0 ? 2 : 3);
        const QString text = QString::number(f / 1.0e6, 'f', stellen);
        const int breite = fm.horizontalAdvance(text);
        int tx = x - breite / 2;
        tx = std::clamp(tx, 0, std::max(0, width() - breite));
        p.drawText(QPoint(tx, unten - 3), text);
    }

    // ── Die eigene Frequenz: wo gesendet wird ────────────────────────
    //
    // tx_frequency ist die Frequenz, auf der Longpath sendet -- bei Split
    // nicht das VFO. Beide, wenn sie auseinanderliegen: die Sendefrequenz
    // voll, das VFO gestrichelt. Beim Senden wird die Marke breiter und
    // bekommt "TX" -- in der Hausfarbe, nicht in Rot (kein Alarm, nur
    // ein Zustand).
    const int xTx = xFuerFrequenz(m_txHz);
    const int xVfo = xFuerFrequenz(m_vfoHz);
    if (xVfo >= 0 && std::abs(m_vfoHz - m_txHz) > 1.0) {
        QPen stift{QColor(Style::kAmberDim())};
        stift.setStyle(Qt::DashLine);
        p.setPen(stift);
        p.drawLine(xVfo, 0, xVfo, height());
    }
    if (xTx >= 0) {
        QPen stift{QColor(Style::kAmberText())};
        stift.setWidth(m_sendet ? 3 : 1);
        p.setPen(stift);
        p.drawLine(xTx, 0, xTx, height());

        const QString text = QString::number(m_txHz / 1.0e6, 'f', 3)
            + (m_sendet ? QStringLiteral("  TX") : QString());
        p.setFont(Style::monoFont(font(), 11, QFont::DemiBold));
        const QFontMetrics fm2(p.font());
        const int breite = fm2.horizontalAdvance(text) + 6;
        // Rechts neben der Linie, am oberen Rand; am rechten Bildrand nach
        // links ausweichen, damit die Zahl nicht abgeschnitten wird.
        int tx = (xTx + 4 + breite <= width()) ? xTx + 4 : xTx - 4 - breite;
        tx = std::max(0, tx);
        const QRect kasten(tx, 2, breite, fm2.height() + 2);
        p.fillRect(kasten, QColor(Style::kPanelBg()));
        p.drawText(kasten, Qt::AlignCenter, text);
    }
}

void WaterfallWidget::setVerbunden(bool verbunden)
{
    if (m_verbunden == verbunden) { return; }
    m_verbunden = verbunden;
    update();
}

void WaterfallWidget::resizeEvent(QResizeEvent* ereignis)
{
    QWidget::resizeEvent(ereignis);
    const int alt = m_wasserfall.width();
    legeBildNeuAn();
    if (m_wasserfall.width() != alt) {
        emit breiteGeaendert(m_wasserfall.width());
    }
}

// ── Beim Sichtbarwerden die Breite melden ────────────────────────────────
//
// Nicht nur beim Größenändern. Ein Panel, das in der richtigen Größe
// eingeblendet wird, bekommt kein resizeEvent -- dann hätte sich die
// Breite nie gemeldet, und Longpath verdichtete weiter auf die
// Vorgabe von 256 Punkten, während hier 734 Bildpunkte stehen. Das
// Bild wäre nicht falsch, nur grob: fünf Punkte breit, wo einer
// genügt. Derselbe Fehler wie in Longpath am 2026-10-31, wo der
// Wasserfall erst nach einem Größenändern entstand.
void WaterfallWidget::showEvent(QShowEvent* ereignis)
{
    QWidget::showEvent(ereignis);
    legeBildNeuAn();
    emit breiteGeaendert(m_wasserfall.width());
}

void WaterfallWidget::paintEvent(QPaintEvent* /*ereignis*/)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(Style::kPanelBg()));

    const int spurHoehe = int(height() * kSpurAnteil);

    // ── Die Spur oben ────────────────────────────────────────────────
    if (!m_letztesSpektrum.isEmpty() && spurHoehe > 4) {
        // Geschweifte Klammern, nicht runde: QPen stift(QColor(f())) liest
        // der Übersetzer als Funktionsdeklaration (die "vexing parse").
        QPen stift{QColor(Style::kAmberText())};
        stift.setWidth(1);
        p.setPen(stift);
        p.setRenderHint(QPainter::Antialiasing, true);

        const int b = std::max(1, width());
        const int n = m_letztesSpektrum.size();
        QPolygon zug;
        zug.reserve(b);
        for (int x = 0; x < b; ++x) {
            const int i = (b == n) ? x : std::min(n - 1, int(qint64(x) * n / b));
            const double t = std::clamp(
                double(dbmAus(m_letztesSpektrum.at(i)) - kBodenDbm)
                    / double(kDeckeDbm - kBodenDbm), 0.0, 1.0);
            zug << QPoint(x, spurHoehe - 1 - int(t * (spurHoehe - 2)));
        }
        p.drawPolyline(zug);
        p.setRenderHint(QPainter::Antialiasing, false);
    }

    // ── Der Wasserfall darunter, unter der Skala ────────────────────
    if (!m_wasserfall.isNull()) {
        p.drawImage(QPoint(0, spurHoehe + kSkalaHoehe), m_wasserfall);
    }

    // ── Skala und die eigene Frequenz ────────────────────────────────
    maleSkalaUndMarke(p, spurHoehe);

    // ── Der ehrliche Hinweis, wenn nichts kommt ──────────────────────
    //
    // Ein schwarzes Feld ohne Erklärung ist der Fall, in dem jemand im
    // Contest Zeit mit Suchen verliert. Es steht leise da, in der
    // Nebenfarbe, nicht als Warnung -- es ist keine.
    if (!m_verbunden) {
        p.setPen(QColor(Style::kTextTertiary()));
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("kein Spektrum — Longpath nicht erreichbar"));
    }
}

} // namespace Contestprogramm
