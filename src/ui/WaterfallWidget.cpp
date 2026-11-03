#include "ui/WaterfallWidget.h"

#include "ui/StyleKit.h"

#include <QPaintEvent>
#include <QPainter>
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
    const int h = std::max(1, int(height() * (1.0 - kSpurAnteil)));
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

    // ── Der Wasserfall darunter ──────────────────────────────────────
    if (!m_wasserfall.isNull()) {
        p.drawImage(QPoint(0, spurHoehe), m_wasserfall);
    }

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
