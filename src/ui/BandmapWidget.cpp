#include "ui/BandmapWidget.h"

#include "core/BandUtils.h"
#include "ui/StyleKit.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include <algorithm>

namespace Contestprogramm {

namespace {

constexpr int kAxisX = 58;       // the axis line; tick labels sit left of it
constexpr int kLabelX = 70;      // where callsign labels start
constexpr int kTopPad = 10;
constexpr int kBottomPad = 10;
constexpr int kLabelHeight = 16;
constexpr qint64 kMinSpanHz = 60000;
constexpr qint64 kMarginHz = 5000;

struct Segment {
    const char* band;
    qint64 lowHz;
    qint64 highHz;
};
// The usual narrow-band (SSB/CW) segments -- the range shown when
// neither a spot nor the rig says anything else. Only bands whose
// worked portion is a lot narrower than the allocation need an entry
// here; everything else falls back to bandRangeHz() below, which for
// an HF band IS the worked portion.
const Segment kSegments[] = {
    {"1.8", 1810000, 1850000},
    {"28", 28000000, 28700000},
    {"50", 50000000, 50400000},
    {"70", 70000000, 70300000},
    {"144", 144000000, 144400000},
    {"432", 432000000, 432400000},
    {"1296", 1296000000, 1296400000},
};

QString frequencyLabel(qint64 hz)
{
    // "144.250" -- MHz with kHz resolution, what the tick marks need.
    const qint64 khz = hz / 1000;
    return QStringLiteral("%1.%2").arg(khz / 1000).arg(khz % 1000, 3, 10, QLatin1Char('0'));
}

} // namespace

BandmapWidget::BandmapWidget(QWidget* parent)
    : QWidget(parent)
{
    Style::applyPanelFrameStyle(this);
    setMinimumHeight(120);
    setMouseTracking(false);
    recomputeRange();
}

void BandmapWidget::setBand(const QString& band)
{
    m_band = band;
    recomputeRange();
    layoutLabels();
    update();
}

void BandmapWidget::setSpots(const QVector<BandmapSpot>& spots)
{
    m_spots = spots;
    recomputeRange();
    layoutLabels();
    update();
}

void BandmapWidget::setOwnFrequencyHz(qint64 hz)
{
    m_ownHz = hz;
    recomputeRange();
    layoutLabels();
    update();
}

void BandmapWidget::recomputeRange()
{
    qint64 low = 0;
    qint64 high = 0;
    for (const BandmapSpot& spot : m_spots) {
        low = low == 0 ? spot.freqHz : std::min(low, spot.freqHz);
        high = std::max(high, spot.freqHz);
    }
    if (m_ownHz > 0) {
        low = low == 0 ? m_ownHz : std::min(low, m_ownHz);
        high = std::max(high, m_ownHz);
    }
    if (low == 0) {
        for (const Segment& segment : kSegments) {
            if (m_band == QLatin1String(segment.band)) {
                m_lowHz = segment.lowHz;
                m_highHz = segment.highHz;
                return;
            }
        }
        // No narrow segment for this band: the whole allocation. Before
        // the HF bands existed this fell through to 0 .. 60 kHz, which
        // drew an axis labelled "0.000" -- a band the program knows is
        // never a blank scale any more.
        qint64 bandLow = 0;
        qint64 bandHigh = 0;
        if (bandRangeHz(m_band, bandLow, bandHigh)) {
            m_lowHz = bandLow;
            m_highHz = bandHigh;
            return;
        }
        m_lowHz = 0;
        m_highHz = kMinSpanHz;
        return;
    }
    low -= kMarginHz;
    high += kMarginHz;
    if (high - low < kMinSpanHz) {
        const qint64 centre = (low + high) / 2;
        low = centre - kMinSpanHz / 2;
        high = centre + kMinSpanHz / 2;
    }
    m_lowHz = low;
    m_highHz = high;
}

int BandmapWidget::yForHz(qint64 hz) const
{
    const int usable = std::max(1, height() - kTopPad - kBottomPad);
    const double span = double(std::max<qint64>(1, m_highHz - m_lowHz));
    return kTopPad + int(std::lround((double(hz - m_lowHz) / span) * usable));
}

void BandmapWidget::layoutLabels()
{
    m_labelRects.clear();
    const QFontMetrics metrics(Style::monoFont(font(), Style::kFontSmall));
    int lastBottom = -1;
    for (const BandmapSpot& spot : m_spots) {
        int y = yForHz(spot.freqHz) - kLabelHeight / 2;
        if (y < lastBottom) {
            y = lastBottom;
        }
        const int width = metrics.horizontalAdvance(spot.callsign) + 8;
        m_labelRects.append(QRect(kLabelX, y, width, kLabelHeight));
        lastBottom = y + kLabelHeight;
    }
}

void BandmapWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutLabels();
}

void BandmapWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QFont tickFont = Style::monoFont(font(), Style::kFontCaption);
    const QFont labelFont = Style::monoFont(font(), Style::kFontSmall);
    const QColor axisColor(Style::kBorder());
    const QColor tickText(Style::kTextScale());

    // Axis
    painter.setPen(QPen(axisColor, 1));
    painter.drawLine(kAxisX, kTopPad, kAxisX, height() - kBottomPad);

    // Ticks: 10 kHz up to 200 kHz of span, 50 kHz beyond.
    const qint64 span = m_highHz - m_lowHz;
    const qint64 step = span <= 200000 ? 10000 : 50000;
    painter.setFont(tickFont);
    const QFontMetrics tickMetrics(tickFont);
    for (qint64 hz = ((m_lowHz + step - 1) / step) * step; hz <= m_highHz; hz += step) {
        const int y = yForHz(hz);
        painter.setPen(QPen(axisColor, 1));
        painter.drawLine(kAxisX - 4, y, kAxisX, y);
        painter.setPen(tickText);
        const QString text = frequencyLabel(hz);
        painter.drawText(kAxisX - 8 - tickMetrics.horizontalAdvance(text), y + tickMetrics.ascent() / 2 - 1, text);
    }

    // Own frequency: amber marker across the axis, the one "measured"
    // value on this canvas.
    if (m_ownHz > 0) {
        const int y = yForHz(m_ownHz);
        painter.setPen(QPen(QColor(Style::kAmberText()), 2));
        painter.drawLine(kAxisX - 12, y, width() - 8, y);
    }

    // Spots
    painter.setFont(labelFont);
    const QFontMetrics labelMetrics(labelFont);
    for (int i = 0; i < m_spots.size() && i < m_labelRects.size(); ++i) {
        const BandmapSpot& spot = m_spots.at(i);
        const QRect& rect = m_labelRects.at(i);
        const int freqY = yForHz(spot.freqHz);
        painter.setPen(QPen(axisColor, 1));
        painter.drawLine(kAxisX, freqY, kLabelX - 4, rect.center().y());
        QFont f = labelFont;
        f.setStrikeOut(spot.worked);
        painter.setFont(f);
        // Drei Zustände, drei Farben: gearbeitet ist durchgestrichen und
        // grau, ein fehlender Multiplikator steht in Bernstein (er zählt
        // mehr als ein QSO), alles andere hell.
        QColor color(Style::kTextPrimary());
        if (spot.worked) {
            color = QColor(Style::kTextInactive());
        } else if (spot.neededMultiplier) {
            color = QColor(Style::kAmberText());
        }
        painter.setPen(color);
        painter.drawText(rect.left() + 4, rect.top() + (kLabelHeight + labelMetrics.ascent() - labelMetrics.descent()) / 2,
                         spot.callsign);
    }
}

void BandmapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    for (int i = 0; i < m_labelRects.size() && i < m_spots.size(); ++i) {
        if (m_labelRects.at(i).contains(event->pos())) {
            const BandmapSpot& spot = m_spots.at(i);
            emit spotActivated(spot.callsign, spot.grid, spot.freqHz);
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

} // namespace Contestprogramm
