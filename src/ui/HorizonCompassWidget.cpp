#include "ui/HorizonCompassWidget.h"

#include "ui/StyleKit.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace Contestprogramm {

namespace {

// Compass convention: 0deg is north (straight up), increasing
// clockwise -- the same convention RotorWidget::pointOnCircle() (a
// private method there, hence duplicated here rather than shared
// through a new header purely for one ten-line trig helper) already
// uses everywhere else this app draws a bearing on a circle.
QPointF pointOnCircle(const QPointF& center, double radius, double angleDeg)
{
    const double rad = qDegreesToRadians(angleDeg);
    return QPointF(center.x() + radius * std::sin(rad), center.y() - radius * std::cos(rad));
}

constexpr double kOuterMarginPx = 26.0; // room for the N/O/S/W labels outside the ring
constexpr double kInnerRadiusFraction = 0.35; // wedges are an annulus, not a full pie -- reads less cluttered

} // namespace

HorizonCompassWidget::HorizonCompassWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
}

void HorizonCompassWidget::setSectors(const QVector<HorizonSector>& sectors)
{
    m_sectors = sectors;
    update();
}

void HorizonCompassWidget::setPlaceholderText(const QString& text)
{
    m_placeholderText = text;
    update();
}

QSize HorizonCompassWidget::sizeHint() const
{
    return QSize(260, 260);
}

QSize HorizonCompassWidget::minimumSizeHint() const
{
    return QSize(200, 200);
}

void HorizonCompassWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(Style::kPanelBg()));

    const QPointF center(width() / 2.0, height() / 2.0);
    const double radius = std::max(10.0, std::min(width(), height()) / 2.0 - kOuterMarginPx);
    const double innerRadius = radius * kInnerRadiusFraction;

    if (m_sectors.isEmpty()) {
        // No site picked for this slot (or a sweep hasn't come back
        // even once yet) -- a plain, unmarked ring with nothing painted
        // over it, same "neutral, not an error" posture the Open/Clear
        // classification below already takes, plus the caller-supplied
        // placeholder text centred inside it.
        QPen ringPen{QColor(Style::kBorderSubtle())};
        ringPen.setWidthF(1.5);
        painter.setPen(ringPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, radius, radius);

        if (!m_placeholderText.isEmpty()) {
            painter.setPen(QColor(Style::kTextSecondary()));
            painter.setFont(Style::capsFont(font(), Style::kFontSmall));
            const QRectF textRect(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
            painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, m_placeholderText);
        }
    } else {
        const int sectorCount = m_sectors.size();
        const double stepDeg = 360.0 / static_cast<double>(sectorCount);

        // Operator, 2026-09-14, after live-testing the Loser parking lot
        // showed a near-solid red wheel and asked "das glaube ich
        // nicht" -- independently re-verified against real SRTM data
        // and the result WAS correct (30/36 sectors genuinely blocked,
        // the site sits mid-massif), but a uniform-radius wedge gives a
        // Blocked sector at 2 km (truly surrounded) and one at 120 km
        // (a single distant peak, still a real obstruction but a much
        // milder one) the exact same visual weight -- indistinguishable
        // from a real bug like the one this same session found and
        // fixed in SiteComparisonDialog earlier that day. Radius now
        // encodes distance-to-obstruction (sqrt scale: a nearby ridge a
        // few km out reads as a small red core, a peak only found at
        // 100+ km reads as a thin red rim near the outer edge), so the
        // shape of the wedge itself carries the "how close is the
        // nearest problem" answer, not just its existence.
        const double maxDistanceKm = defaultHorizonSweepDistancesKm().isEmpty()
            ? 180.0
            : defaultHorizonSweepDistancesKm().last();

        const auto radiusForDistance = [&](double distanceKm) {
            const double clamped = std::max(0.0, std::min(distanceKm, maxDistanceKm));
            const double fraction = maxDistanceKm > 0.0 ? std::sqrt(clamped / maxDistanceKm) : 0.0;
            return innerRadius + (radius - innerRadius) * fraction;
        };

        for (const HorizonSector& sector : m_sectors) {
            QColor fill;
            QColor stroke;
            double outerR = radius;
            switch (sector.classification) {
            case LineOfSightClass::Blocked:
                fill = QColor(Style::kRedBg());
                stroke = QColor(Style::kRedBorder());
                outerR = radiusForDistance(sector.obstructionDistanceKm);
                break;
            case LineOfSightClass::Marginal:
                fill = QColor(Style::kAmberBg());
                stroke = QColor(Style::kAmberBorder());
                outerR = radiusForDistance(sector.obstructionDistanceKm);
                break;
            case LineOfSightClass::Clear:
                // Unmarked/neutral, per this feature's own task brief --
                // a hair lighter than the panel behind it (kInsetBg,
                // the same "versenkt" surface StyleKit.h reserves for
                // this role) so the wedge boundary is still visible
                // without reading as a state colour. Drawn all the way
                // to the outer radius -- "open" means open for the
                // sweep's ENTIRE checked distance, not just up to some
                // near obstruction.
                fill = QColor(Style::kInsetBg());
                stroke = QColor(Style::kBorderSubtle());
                outerR = radius;
                break;
            case LineOfSightClass::Unknown:
                // Still waiting on SRTM tiles -- a muted, deliberately
                // dull grey (kTextInactive, already this codebase's own
                // "not yet known/not applicable" colour, e.g. badgeStyle()'s
                // disconnected pill) rather than either a state colour or
                // the same neutral Clear uses, so a mid-load sweep is
                // visually distinguishable from a genuinely open one.
                // Full radius (no distance is known yet to shrink it to).
                fill = QColor(Style::kTextInactive());
                fill.setAlpha(60);
                stroke = QColor(Style::kBorderSubtle());
                outerR = radius;
                break;
            }

            const double startDeg = sector.bearingDeg - stepDeg / 2.0;
            const double endDeg = sector.bearingDeg + stepDeg / 2.0;

            QPainterPath wedge;
            wedge.moveTo(pointOnCircle(center, innerRadius, startDeg));
            for (double deg = startDeg; deg <= endDeg + 0.01; deg += 2.0) {
                wedge.lineTo(pointOnCircle(center, outerR, std::min(deg, endDeg)));
            }
            for (double deg = endDeg; deg >= startDeg - 0.01; deg -= 2.0) {
                wedge.lineTo(pointOnCircle(center, innerRadius, std::max(deg, startDeg)));
            }
            wedge.closeSubpath();

            painter.setPen(QPen(stroke, 1.0));
            painter.setBrush(fill);
            painter.drawPath(wedge);
        }

        // Distance-ring guides so the radius encoding above is
        // readable rather than just "vaguely closer/farther" -- two
        // rings at fixed real distances (not sqrt-evenly-spaced
        // fractions, precisely BECAUSE the radius mapping is sqrt: a
        // linearly-spaced set of rings would land oddly close together
        // near the centre) with compact labels. Drawn AFTER the wedges,
        // dashed and low-contrast, so they read as an overlay scale
        // rather than competing with the classification colours for
        // attention.
        QPen guidePen{QColor(Style::kBorderSubtle())};
        guidePen.setWidthF(1.0);
        guidePen.setStyle(Qt::DashLine);
        painter.setPen(guidePen);
        painter.setBrush(Qt::NoBrush);
        struct DistanceGuide { double km; QString label; };
        const QVector<DistanceGuide> guides = {
            {50.0, QStringLiteral("50 km")},
            {std::min(150.0, maxDistanceKm), QStringLiteral("%1 km").arg(qRound(std::min(150.0, maxDistanceKm)))},
        };
        painter.setFont(Style::capsFont(font(), Style::kFontCaption));
        for (const DistanceGuide& guide : guides) {
            const double r = radiusForDistance(guide.km);
            painter.drawEllipse(center, r, r);
            // Label centred ON the ring at a fixed bearing (258 degrees
            // -- the SW-ish quadrant, clear of N/O/S/W and of every 30-
            // degree tick label, all of which sit further out anyway)
            // rather than floating in open space, so it stays anchored
            // to the ring it describes even as the widget resizes.
            // Operator, 2026-09-14: the first version right-aligned a
            // too-narrow (28px) box ending near a point just left of
            // north, which for a 6-character label like "150 km" at
            // this font size clipped the leading digits instead of
            // wrapping -- Qt's drawText() clips to the given rect
            // rather than shrinking the text to fit it. A wide (56px),
            // CENTRED box has enough room even for the longer label.
            const QPointF p = pointOnCircle(center, r, 258.0);
            painter.setPen(QColor(Style::kTextSecondary()));
            painter.drawText(QRectF(p.x() - 28.0, p.y() - 7.0, 56.0, 14.0), Qt::AlignCenter, guide.label);
            painter.setPen(guidePen);
        }

        QPen outerRingPen{QColor(Style::kBorder())};
        outerRingPen.setWidthF(1.5);
        painter.setPen(outerRingPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, radius, radius);
    }

    // Cardinal labels -- N/O/S/W at minimum, per this feature's own
    // task brief. kInstrumentFace is this codebase's own "needles/
    // ticks/labels on an instrument face, never the theme's accent
    // colour" convention (Style::kInstrumentFace()'s own doc comment).
    painter.setPen(QColor(Style::kInstrumentFace()));
    painter.setFont(Style::capsFont(font(), Style::kFontCaption));
    const QFontMetrics metrics(painter.font());
    struct CardinalLabel { double bearingDeg; QString text; };
    static const QVector<CardinalLabel> cardinals = {
        {0.0, QStringLiteral("N")},
        {90.0, QStringLiteral("O")},
        {180.0, QStringLiteral("S")},
        {270.0, QStringLiteral("W")},
    };
    for (const CardinalLabel& cardinal : cardinals) {
        const QPointF p = pointOnCircle(center, radius + kOuterMarginPx * 0.6, cardinal.bearingDeg);
        const QRect labelRect(qRound(p.x() - 12.0), qRound(p.y() - metrics.height() / 2.0), 24, metrics.height());
        painter.drawText(labelRect, Qt::AlignCenter, cardinal.text);
    }

    // Fine-grained degree ticks -- operator, 2026-09-14, offered right
    // after the very first live test of this widget ("kann noch
    // Gradzahlen ergaenzen") and now paired with the distance-guide
    // rings above: without these, "the obstruction is at 230 degrees"
    // in a sector's own summary sentence had nothing on the compass
    // itself to check it against. Every 30 degrees, skipping the four
    // cardinal bearings already labelled above (0/90/180/270) to avoid
    // doubled text at the same spot. Smaller and dimmer than the
    // cardinal labels (kFontCaption already IS this widget's smallest
    // size -- kTextSecondary instead of kInstrumentFace is the
    // remaining way to de-emphasise these relative to N/O/S/W) since
    // they are a precise reference, not the primary orientation cue.
    painter.setPen(QColor(Style::kTextSecondary()));
    for (int deg = 30; deg < 360; deg += 30) {
        if (deg % 90 == 0) {
            continue; // already drawn as a cardinal label above
        }
        const QPointF p = pointOnCircle(center, radius + kOuterMarginPx * 0.55, static_cast<double>(deg));
        const QRect labelRect(qRound(p.x() - 14.0), qRound(p.y() - metrics.height() / 2.0), 28, metrics.height());
        painter.drawText(labelRect, Qt::AlignCenter, QStringLiteral("%1°").arg(deg));
    }
}

} // namespace Contestprogramm
