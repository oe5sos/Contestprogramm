#include "ui/MapWidget.h"

#include "core/Cities.h"
#include "core/CountryBorders.h"
#include "core/Maidenhead.h"
#include "ui/StyleKit.h"

#include <QCheckBox>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QPushButton>
#include <QRadialGradient>
#include <QSet>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace Contestprogramm {

namespace {
constexpr int kHeaderHeight = 28;
constexpr int kAccentBarWidth = 3;
// Two stacked rows of 22px each + a 6px gap between them -- see
// buildSettingsRow()'s own comment for why this grew from a single
// 26px row: ten checkboxes squeezed onto one line at this panel's
// 530px width had no room left for full labels and started visibly
// overlapping (operator, 2026-09-14: "grid, ring usw. teilweise
// überschrieben und nicht gut lesbar").
constexpr int kSettingsRowHeight = 50;
constexpr int kLegendHeight = 30;
constexpr int kCanvasMargin = 10;
constexpr double kMinVisibleRangeKm = 25.0;
constexpr double kMaxVisibleRangeKm = 3200.0;
// Zoom button step size -- operator, 2026-09-14: "mache schritte beim
// radius bitte alle 250km" (see zoomIn()/zoomOut() below).
constexpr double kVisibleRangeStepKm = 250.0;
// Operator, 2026-09-12: "die wichtigsten großen städte ab 150 km" --
// only cities at least this far from home are worth a reference label;
// closer ones are already familiar local geography (see
// drawCitiesLayer()'s own doc comment in MapWidget.h).
constexpr double kCityMinDistanceKm = 150.0;
// Operator, 2026-09-13: "alte Kontakte ausgrauen" -- a worked marker/
// grid-cell stays at full colour for the first kAgingStartMinutes,
// fades linearly toward grey through kAgingCompleteMinutes, and stays
// grey after that. Both are plain minutes, not tied to any contest-
// specific rate assumption -- a fixed, predictable curve the operator
// can learn once, not something that would need re-tuning per contest.
constexpr int kAgingStartMinutes = 30;
constexpr int kAgingCompleteMinutes = 180;
// How often the aging fade re-paints purely from wall-clock time
// passing (see m_agingRefreshTimer's own doc comment in the header) --
// a minute is far finer than the fade curve above actually needs to
// look smooth, cheap for a small hand-painted panel either way.
constexpr int kAgingRefreshIntervalMs = 60000;
// mousePressEvent()'s click tolerance around a station marker's own
// projected point -- generous relative to the marker's own on-screen
// radius (2.8-3.2px, see drawStations()) so clicking it does not
// require pixel-perfect accuracy.
constexpr double kStationClickTolerancePx = 9.0;
} // namespace

MapWidget::MapWidget(QWidget* parent)
    : QWidget(parent)
    , m_countryBorders(loadCountryBorders())
    , m_cities(loadCities())
{
    buildSettingsRow();

    // The header is hand-painted (see drawPanelHeader()), so the layout
    // reserves that strip via a top margin rather than a child widget --
    // the settings row is the only real widget parked in the layout;
    // the stretch below leaves the rest of the widget's area free of
    // any child, so the canvas/legend painting in paintEvent() has
    // nothing drawn on top of it.
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, kHeaderHeight, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_settingsRow);
    layout->addStretch(1);

    setMinimumSize(minimumSizeHint());
    refreshZoomLabel();

    // See m_agingRefreshTimer's own doc comment in the header -- only
    // this one layer needs a self-driven repaint tick; every other
    // layer already repaints in response to some other signal.
    m_agingRefreshTimer = new QTimer(this);
    m_agingRefreshTimer->setInterval(kAgingRefreshIntervalMs);
    connect(m_agingRefreshTimer, &QTimer::timeout, this, QOverload<>::of(&MapWidget::update));
    if (m_showAging) {
        m_agingRefreshTimer->start();
    }
}

void MapWidget::buildSettingsRow()
{
    m_settingsRow = new QWidget(this);
    m_settingsRow->setFixedHeight(kSettingsRowHeight);

    // Two stacked rows instead of one -- ten checkboxes on a single
    // 530px-wide line had no room left for full labels and started
    // visibly overlapping (operator, 2026-09-14, pointing at this exact
    // row: "grid, ring usw. teilweise überschrieben und nicht gut
    // lesbar"). Splitting into "map layers" (row 1) and "live/behaviour
    // toggles + zoom" (row 2) gives each checkbox roughly twice the
    // width the single-row layout could ever have offered, at the same
    // panel width -- so labels go back to their full, unabbreviated
    // German words instead of yet another round of shortening the same
    // ten words could never fully fix.
    auto* outer = new QVBoxLayout(m_settingsRow);
    outer->setContentsMargins(kAccentBarWidth + 9, 2, 8, 2);
    outer->setSpacing(4);
    auto* row1 = new QHBoxLayout();
    row1->setContentsMargins(0, 0, 0, 0);
    row1->setSpacing(10);
    auto* row2 = new QHBoxLayout();
    row2->setContentsMargins(0, 0, 0, 0);
    row2->setSpacing(10);
    outer->addLayout(row1);
    outer->addLayout(row2);

    const QFont checkFont = Style::capsFont(font(), Style::kFontCaption);

    m_gridCheck = new QCheckBox(QStringLiteral("Grid"), m_settingsRow);
    m_ringsCheck = new QCheckBox(QStringLiteral("Ringe"), m_settingsRow);
    m_spokesCheck = new QCheckBox(QStringLiteral("Speichen"), m_settingsRow);
    m_bordersCheck = new QCheckBox(QStringLiteral("Grenzen"), m_settingsRow);
    m_bordersCheck->setToolTip(QStringLiteral(
        "Staatsgrenzen/Küstenlinien (Natural Earth 1:110m)"));
    m_citiesCheck = new QCheckBox(QStringLiteral("Städte"), m_settingsRow);
    m_citiesCheck->setToolTip(QStringLiteral(
        "Wichtigste Großstädte ab 150 km Entfernung als Orientierungspunkte (Natural Earth 1:110m)"));

    m_workedCellsCheck = new QCheckBox(QStringLiteral("Gearbeitet"), m_settingsRow);
    m_workedCellsCheck->setToolTip(QStringLiteral(
        "Gearbeitete/gespottete Grid-Felder farbig markieren (unabhängig vom reinen Grid-Raster)"));
    // Two separate checkboxes, not one shared "Rotoren" toggle --
    // operator, 2026-09-14: "rotor 1 und rotor 2 zum ein und ausblenden"
    // (one heading needed to stay visible while the other was hidden,
    // which a single combined toggle could not express).
    m_rotor1HeadingCheck = new QCheckBox(QStringLiteral("Rotor 1"), m_settingsRow);
    m_rotor1HeadingCheck->setToolTip(QStringLiteral(
        "Aktuelle Peilung von Rotor 1 einzeichnen"));
    m_rotor2HeadingCheck = new QCheckBox(QStringLiteral("Rotor 2"), m_settingsRow);
    m_rotor2HeadingCheck->setToolTip(QStringLiteral(
        "Aktuelle Peilung von Rotor 2 einzeichnen"));
    m_agingCheck = new QCheckBox(QStringLiteral("Altern"), m_settingsRow);
    m_agingCheck->setToolTip(QStringLiteral(
        "Gearbeitete Stationen/Grid-Felder verblassen nach einer Weile zu Grau,\n"
        "damit aktuelle Aktivität nicht in alten Kontakten untergeht."));
    m_fitCheck = new QCheckBox(QStringLiteral("Füllen"), m_settingsRow);
    m_fitCheck->setToolTip(QStringLiteral(
        "Karte füllt das ganze Panel aus (Ringe werden bei nicht-quadratischem Panel elliptisch).\n"
        "Deaktiviert: feste Kreisform, mittig, mit Rand."));
    // The operator's own call on the app-wide checkbox style
    // (StyleKit::appStyleSheet's blue checked-indicator, correct per
    // HAUSSTIL for "an interactive/selected state" everywhere else):
    // for these layer toggles specifically, quieter and closer to the
    // neutral zoom buttons beside them reads better than a blue square
    // next to a mostly-monochrome instrument. Stays entirely in the
    // gray family -- no new hue introduced, just a local override of
    // this one widget's checked state. Indicator back up to 12px (was
    // briefly 10px during the single-row squeeze) now that two rows
    // give every checkbox real room again.
    const QString quietCheckboxStyle = QStringLiteral(
        "QCheckBox { color: %1; spacing: 6px; }"
        "QCheckBox::indicator { width: 12px; height: 12px; background: %2;"
        "  border: 1px solid %3; border-radius: 3px; }"
        "QCheckBox::indicator:checked { background: %4; border-color: %5; }")
        .arg(Style::kTextSecondary(), Style::kInsetBg(),
             Style::kInsetBorder(), Style::kTextScale(),
             Style::kTextSecondary());

    for (QCheckBox* check : {m_gridCheck, m_ringsCheck, m_spokesCheck, m_bordersCheck, m_citiesCheck}) {
        check->setChecked(true);
        check->setFont(checkFont);
        check->setStyleSheet(quietCheckboxStyle);
        row1->addWidget(check);
    }
    row1->addStretch(1);

    for (QCheckBox* check : {m_workedCellsCheck, m_rotor1HeadingCheck, m_rotor2HeadingCheck, m_agingCheck,
                              m_fitCheck}) {
        check->setChecked(true);
        check->setFont(checkFont);
        check->setStyleSheet(quietCheckboxStyle);
        row2->addWidget(check);
    }
    row2->addStretch(1);

    m_zoomOutButton = new QPushButton(QStringLiteral("−"), m_settingsRow);
    m_zoomInButton = new QPushButton(QStringLiteral("+"), m_settingsRow);
    for (QPushButton* button : {m_zoomOutButton, m_zoomInButton}) {
        button->setFixedSize(18, 18);
        button->setFont(Style::monoFont(font(), Style::kFontSmall, QFont::Bold));
    }
    m_zoomRangeLabel = new QLabel(m_settingsRow);
    m_zoomRangeLabel->setFont(Style::monoFont(font(), Style::kFontCaption));
    m_zoomRangeLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextTertiary()));
    m_zoomRangeLabel->setAlignment(Qt::AlignCenter);
    m_zoomRangeLabel->setFixedWidth(48);

    // Zoom cluster lives on row 2, beside the live/behaviour toggles --
    // row 1 is purely the static map-layer checkboxes.
    row2->addWidget(m_zoomOutButton);
    row2->addWidget(m_zoomRangeLabel);
    row2->addWidget(m_zoomInButton);

    connect(m_gridCheck, &QCheckBox::toggled, this, &MapWidget::setGridLayerVisible);
    connect(m_ringsCheck, &QCheckBox::toggled, this, &MapWidget::setRingsLayerVisible);
    connect(m_spokesCheck, &QCheckBox::toggled, this, &MapWidget::setSpokesLayerVisible);
    connect(m_workedCellsCheck, &QCheckBox::toggled, this, &MapWidget::setWorkedCellsLayerVisible);
    connect(m_rotor1HeadingCheck, &QCheckBox::toggled, this, &MapWidget::setRotor1HeadingLayerVisible);
    connect(m_rotor2HeadingCheck, &QCheckBox::toggled, this, &MapWidget::setRotor2HeadingLayerVisible);
    connect(m_bordersCheck, &QCheckBox::toggled, this, &MapWidget::setBordersLayerVisible);
    connect(m_citiesCheck, &QCheckBox::toggled, this, &MapWidget::setCitiesLayerVisible);
    connect(m_agingCheck, &QCheckBox::toggled, this, &MapWidget::setAgingEnabled);
    connect(m_fitCheck, &QCheckBox::toggled, this, &MapWidget::setFitToWindowEnabled);
    connect(m_zoomOutButton, &QPushButton::clicked, this, &MapWidget::zoomOut);
    connect(m_zoomInButton, &QPushButton::clicked, this, &MapWidget::zoomIn);
}

void MapWidget::refreshZoomLabel() const
{
    if (m_zoomRangeLabel) {
        m_zoomRangeLabel->setText(QStringLiteral("%1 km").arg(m_visibleRangeKm, 0, 'f', 0));
    }
}

void MapWidget::setOwnGrid(const QString& grid)
{
    if (m_ownGrid == grid) {
        return;
    }
    m_ownGrid = grid;
    update();
}

void MapWidget::setOwnLabel(const QString& label)
{
    if (m_ownLabel == label) {
        return;
    }
    m_ownLabel = label;
    update();
}

void MapWidget::setStations(const QVector<Station>& stations)
{
    m_stations = stations;
    update();
}

void MapWidget::setGridLayerVisible(bool visible)
{
    m_showGrid = visible;
    if (m_gridCheck && m_gridCheck->isChecked() != visible) {
        m_gridCheck->setChecked(visible);
    }
    update();
}

void MapWidget::setRingsLayerVisible(bool visible)
{
    m_showRings = visible;
    if (m_ringsCheck && m_ringsCheck->isChecked() != visible) {
        m_ringsCheck->setChecked(visible);
    }
    update();
}

void MapWidget::setSpokesLayerVisible(bool visible)
{
    m_showSpokes = visible;
    if (m_spokesCheck && m_spokesCheck->isChecked() != visible) {
        m_spokesCheck->setChecked(visible);
    }
    update();
}

void MapWidget::setWorkedCellsLayerVisible(bool visible)
{
    m_showWorkedCells = visible;
    if (m_workedCellsCheck && m_workedCellsCheck->isChecked() != visible) {
        m_workedCellsCheck->setChecked(visible);
    }
    update();
}

void MapWidget::setRotor1HeadingLayerVisible(bool visible)
{
    m_showRotor1Heading = visible;
    if (m_rotor1HeadingCheck && m_rotor1HeadingCheck->isChecked() != visible) {
        m_rotor1HeadingCheck->setChecked(visible);
    }
    update();
}

void MapWidget::setRotor2HeadingLayerVisible(bool visible)
{
    m_showRotor2Heading = visible;
    if (m_rotor2HeadingCheck && m_rotor2HeadingCheck->isChecked() != visible) {
        m_rotor2HeadingCheck->setChecked(visible);
    }
    update();
}

void MapWidget::setRotor1Heading(bool connected, double azimuthDeg, const QString& label)
{
    m_rotor1Connected = connected;
    m_rotor1AzimuthDeg = azimuthDeg;
    m_rotor1Label = label;
    update();
}

void MapWidget::setRotor2Heading(bool connected, double azimuthDeg, const QString& label)
{
    m_rotor2Connected = connected;
    m_rotor2AzimuthDeg = azimuthDeg;
    m_rotor2Label = label;
    update();
}

void MapWidget::setBordersLayerVisible(bool visible)
{
    m_showBorders = visible;
    if (m_bordersCheck && m_bordersCheck->isChecked() != visible) {
        m_bordersCheck->setChecked(visible);
    }
    update();
}

void MapWidget::setCitiesLayerVisible(bool visible)
{
    m_showCities = visible;
    if (m_citiesCheck && m_citiesCheck->isChecked() != visible) {
        m_citiesCheck->setChecked(visible);
    }
    update();
}

void MapWidget::setAgingEnabled(bool enabled)
{
    m_showAging = enabled;
    if (m_agingCheck && m_agingCheck->isChecked() != enabled) {
        m_agingCheck->setChecked(enabled);
    }
    if (m_agingRefreshTimer) {
        if (enabled) {
            m_agingRefreshTimer->start();
        } else {
            m_agingRefreshTimer->stop();
        }
    }
    update();
}

void MapWidget::setTerrainSectors(const QVector<LineOfSightClass>& sectorsByDegree)
{
    m_terrainSectors = sectorsByDegree;
    update();
}

void MapWidget::setFitToWindowEnabled(bool enabled)
{
    m_fitToWindow = enabled;
    if (m_fitCheck && m_fitCheck->isChecked() != enabled) {
        m_fitCheck->setChecked(enabled);
    }
    update();
}

void MapWidget::setVisibleRangeKm(double rangeKm)
{
    const double clamped = std::clamp(rangeKm, kMinVisibleRangeKm, kMaxVisibleRangeKm);
    if (qFuzzyCompare(m_visibleRangeKm, clamped)) {
        return;
    }
    m_visibleRangeKm = clamped;
    refreshZoomLabel();
    update();
}

void MapWidget::zoomIn()
{
    // Fixed 250 km steps, not halving/doubling -- operator, 2026-09-14:
    // "mache schritte beim radius bitte alle 250km" -- a predictable
    // ladder (250/500/750/1000/...) instead of one that lands on an
    // ever-finer set of values the smaller the range gets.
    setVisibleRangeKm(m_visibleRangeKm - kVisibleRangeStepKm);
}

void MapWidget::zoomOut()
{
    setVisibleRangeKm(m_visibleRangeKm + kVisibleRangeStepKm);
}

QPointF MapWidget::projectBearingDistance(double bearingDeg, double distanceKm, double visibleRangeKm,
                                           const QRectF& mapRect)
{
    // Azimuthal-equidistant: radius scales linearly with distance,
    // angle is the bearing itself -- exactly RotorWidget::
    // pointOnCircle()'s compass convention (0 deg = up, clockwise), so
    // a bearing reads the same way on both instruments. x and y scale
    // independently (half the rect's width/height, not a single
    // min(w,h)-based radius) -- for a square mapRect (every existing
    // caller/test before the "Füllen" toggle existed) that is pixel-
    // identical to the old uniform-radius formula; for a non-square
    // mapRect (MapWidget::canvasRect() in fit-to-window mode) it draws
    // a true ellipse instead of a circle letterboxed into unused
    // margins -- see MapWidget::setFitToWindowEnabled().
    const double halfWidth = mapRect.width() / 2.0;
    const double halfHeight = mapRect.height() / 2.0;
    const QPointF center = mapRect.center();
    const double safeRange = visibleRangeKm > 0.0 ? visibleRangeKm : 1.0;
    const double scale = distanceKm / safeRange;
    const double rad = qDegreesToRadians(bearingDeg);
    return QPointF(center.x() + scale * std::sin(rad) * halfWidth, center.y() - scale * std::cos(rad) * halfHeight);
}

QVector<QPointF> MapWidget::projectGridSquareCorners(const QString& gridSquare, double homeLat, double homeLon,
                                                       double visibleRangeKm, const QRectF& mapRect)
{
    double centerLat = 0.0;
    double centerLon = 0.0;
    calculateLatLonFromGridSquare(gridSquare.left(4), centerLat, centerLon);

    // A 4-character Maidenhead square is exactly 2 deg longitude by
    // 1 deg latitude (Maidenhead's own field/square encoding -- 20 deg
    // field / 10 = 2 deg per lon digit, 10 deg field / 10 = 1 deg per
    // lat digit -- see Maidenhead.cpp's calculateLatLonFromGridSquare).
    // This applies that well-known fixed size around the already-
    // computed centre rather than re-deriving the encoding here.
    constexpr double kHalfLonDeg = 1.0;
    constexpr double kHalfLatDeg = 0.5;
    const double lonOffsets[4] = {-kHalfLonDeg, kHalfLonDeg, kHalfLonDeg, -kHalfLonDeg};
    const double latOffsets[4] = {-kHalfLatDeg, -kHalfLatDeg, kHalfLatDeg, kHalfLatDeg};

    QVector<QPointF> corners;
    corners.reserve(4);
    for (int i = 0; i < 4; ++i) {
        const double cornerLat = centerLat + latOffsets[i];
        const double cornerLon = centerLon + lonOffsets[i];
        // Each corner gets its own great-circle bearing/distance from
        // home -- an azimuthal-equidistant projection, not a uniform
        // pixel-per-degree grid, so a square far from home renders as a
        // (correctly) skewed quadrilateral, not an axis-aligned
        // rectangle. This is the whole point of building this as a real
        // instrument rather than the plan's documented Map-B
        // alternative (a flat grid-code table with no projection at
        // all).
        const double bearing = calculateBearingInDegreesBetween(homeLat, homeLon, cornerLat, cornerLon);
        const double distance = calculateDistanceKmBetween(homeLat, homeLon, cornerLat, cornerLon);
        corners.append(projectBearingDistance(bearing, distance, visibleRangeKm, mapRect));
    }
    return corners;
}

QColor MapWidget::markerColor(bool worked)
{
    // Green = worked, blue = spotted/reachable-but-unworked -- per the
    // plan's explicit colour choice (N1MM+ uses blue/red for the same
    // two states; this project keeps red reserved for warnings, see
    // StyleKit.h's kRedBg comment).
    return worked ? QColor(Style::kGreenText()) : QColor(Style::kBlueBg());
}

QColor MapWidget::gridLabelColor(bool worked)
{
    // The grid-code label itself is amber for a worked station (the
    // plan's own wording: "grid-code label in amber/bold") and blue for
    // a spotted one, matching Map-A.dc.html's station-label styling.
    return worked ? QColor(Style::kAmberText()) : QColor(Style::kBlueBg());
}

QColor MapWidget::agedMarkerColor(const QColor& base, qint64 secondsSinceWorked)
{
    if (secondsSinceWorked <= 0) {
        // In the future (clock skew) or exactly now -- read as "just
        // happened", not as an error.
        return base;
    }
    const double minutes = static_cast<double>(secondsSinceWorked) / 60.0;
    if (minutes <= kAgingStartMinutes) {
        return base;
    }
    const QColor faded{Style::kTextInactive()};
    if (minutes >= kAgingCompleteMinutes) {
        QColor result = faded;
        result.setAlpha(base.alpha());
        return result;
    }
    const double t = (minutes - kAgingStartMinutes) / (kAgingCompleteMinutes - kAgingStartMinutes);
    // Channel-wise linear interpolation, alpha kept from `base` -- a
    // partly transparent base (e.g. the spotted grid-cell fill's own
    // setAlpha(50), reached here only if that cell later also gains a
    // worked station) still fades toward grey rather than snapping to
    // a different transparency.
    const auto lerp = [t](int fromChannel, int toChannel) {
        return fromChannel + static_cast<int>((toChannel - fromChannel) * t);
    };
    QColor result(lerp(base.red(), faded.red()), lerp(base.green(), faded.green()),
                  lerp(base.blue(), faded.blue()));
    result.setAlpha(base.alpha());
    return result;
}

QSize MapWidget::minimumSizeHint() const
{
    return QSize(300, kHeaderHeight + kSettingsRowHeight + 220 + kLegendHeight + 2 * kCanvasMargin);
}

QSize MapWidget::sizeHint() const
{
    return QSize(340, kHeaderHeight + kSettingsRowHeight + 260 + kLegendHeight + 2 * kCanvasMargin);
}

void MapWidget::drawPanelHeader(QPainter& painter) const
{
    const QRect headerRect(0, 0, width(), kHeaderHeight);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);

    QLinearGradient bg(0, 0, 0, kHeaderHeight);
    bg.setColorAt(0.0, QColor(Style::kPanelHeadTop()));
    bg.setColorAt(0.5, QColor(Style::kPanelHeadMid()));
    bg.setColorAt(1.0, QColor(Style::kPanelHeadBot()));
    painter.fillRect(headerRect, bg);

    painter.fillRect(QRect(0, 0, kAccentBarWidth, kHeaderHeight), QColor(Style::kAmberText()));

    painter.setPen(QColor(Style::kTitleBorder()));
    painter.drawLine(0, kHeaderHeight - 1, width(), kHeaderHeight - 1);

    painter.setFont(Style::capsFont(painter.font()));

    // Deliberately no own callsign/grid badge here -- this panel shows
    // worked/spotted grids, not the operator's own identity (2026-09-11).
    painter.setPen(QColor(Style::kTextSecondary()));
    const QRect textRect(kAccentBarWidth + 9, 0, width() - kAccentBarWidth - 18, kHeaderHeight);
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("Karte / Verbindungen"));

    painter.restore();
}

QRectF MapWidget::canvasRect() const
{
    const int settingsBottom = m_settingsRow ? m_settingsRow->geometry().bottom() + 1
                                              : kHeaderHeight + kSettingsRowHeight;
    const int top = std::max(settingsBottom, kHeaderHeight + kSettingsRowHeight) + kCanvasMargin;
    const int bottom = height() - kLegendHeight - kCanvasMargin;
    const int availableHeight = std::max(0, bottom - top);
    const int availableWidth = std::max(0, width() - 2 * kCanvasMargin);

    if (m_fitToWindow) {
        // Fill the whole available area -- the rings/grid squares below
        // scale independently in x and y (see projectBearingDistance),
        // so a non-square panel draws true ellipses here instead of a
        // circle stuck in the middle with empty margins either side.
        return QRectF(kCanvasMargin, top, availableWidth, availableHeight);
    }

    const int side = std::min(availableHeight, availableWidth);
    const double left = (width() - side) / 2.0;
    const double topF = top + std::max(0, availableHeight - side) / 2.0;
    return QRectF(left, topF, side, side);
}

QVector<MapWidget::GridCell> MapWidget::computeGridCells(const QRectF& area) const
{
    QVector<GridCell> cells;
    if (!isValidGridSquare(m_ownGrid)) {
        return cells;
    }

    double homeLat = 0.0;
    double homeLon = 0.0;
    calculateLatLonFromGridSquare(m_ownGrid, homeLat, homeLon);
    const QString homeCode = gridSquareFromLatLon(homeLat, homeLon).left(4);

    // Enough 4-character squares (2 deg lon x 1 deg lat each) to cover
    // the visible disk -- a conservative per-degree km estimate (squares
    // are non-uniform in screen size anyway once projected, that is the
    // point), clamped so an extreme zoomed-out range cannot generate an
    // unreasonable cell count.
    const int latSteps = std::min(10, static_cast<int>(std::ceil(m_visibleRangeKm / 100.0)) + 1);
    const int lonSteps = std::min(10, static_cast<int>(std::ceil(m_visibleRangeKm / 70.0)) + 1);

    QSet<QString> seen;
    for (int r = -latSteps; r <= latSteps; ++r) {
        const double targetLat = homeLat + r * 1.0;
        if (targetLat < -90.0 || targetLat > 90.0) {
            continue;
        }
        for (int c = -lonSteps; c <= lonSteps; ++c) {
            const double targetLon = homeLon + c * 2.0;
            const QString code = gridSquareFromLatLon(targetLat, targetLon).left(4);
            if (seen.contains(code)) {
                continue;
            }
            seen.insert(code);

            GridCell cell;
            cell.code = code;
            cell.isHome = (code == homeCode);
            cell.corners = projectGridSquareCorners(code, homeLat, homeLon, m_visibleRangeKm, area);
            for (const Station& station : m_stations) {
                if (!isValidGridSquare(station.grid) || station.grid.left(4).toUpper() != code) {
                    continue;
                }
                if (station.worked) {
                    cell.hasWorked = true;
                    if (station.workedAtUtc.isValid()
                        && (!cell.freshestWorkedAtUtc.isValid() || station.workedAtUtc > cell.freshestWorkedAtUtc)) {
                        cell.freshestWorkedAtUtc = station.workedAtUtc;
                    }
                } else {
                    cell.hasSpotted = true;
                }
            }
            cells.append(cell);
        }
    }
    return cells;
}

void MapWidget::drawGlow(QPainter& painter, const QRectF& area) const
{
    const QPointF center = area.center();
    const double halfWidth = area.width() / 2.0;
    const double halfHeight = area.height() / 2.0;
    if (halfWidth <= 0.0 || halfHeight <= 0.0) {
        return;
    }

    QColor hi{Style::kInstrumentGlowHi()};
    hi.setAlpha(70);
    QColor lo{Style::kInstrumentGlowLo()};
    lo.setAlpha(40);
    QColor fade(lo);
    fade.setAlpha(0);

    // QRadialGradient itself is always circular -- stretched into an
    // ellipse via a scaled painter transform (x by the width/height
    // ratio, so a radius of halfHeight in this scaled space becomes
    // halfWidth in real screen space), the same independent-x/y-scale
    // idea projectBearingDistance() already applies per point, just
    // done once here via a transform instead of per-vertex maths.
    painter.save();
    painter.translate(center);
    painter.scale(halfWidth / halfHeight, 1.0);
    QRadialGradient glow(QPointF(0.0, 0.0), halfHeight);
    glow.setColorAt(0.0, hi);
    glow.setColorAt(0.55, lo);
    glow.setColorAt(1.0, fade);
    painter.setPen(Qt::NoPen);
    painter.setBrush(glow);
    painter.drawEllipse(QPointF(0.0, 0.0), halfHeight, halfHeight);
    painter.restore();
}

void MapWidget::drawBordersLayer(QPainter& painter, const QRectF& area) const
{
    if (m_countryBorders.isEmpty() || !isValidGridSquare(m_ownGrid)) {
        return;
    }

    double homeLat = 0.0;
    double homeLon = 0.0;
    calculateLatLonFromGridSquare(m_ownGrid, homeLat, homeLon);

    // Operator, 2026-09-12 (after seeing the first cut live): the
    // original delta-4 land tint and kBorderSubtle/0.7px outline were
    // both too close to the sea fill to read at a glance -- "muss man
    // genau erkennen" (needs to be clearly recognisable, e.g. exactly
    // where Germany's border runs), not just technically present.
    // kTextTertiary is already the contrast level this palette uses for
    // secondary body text against these same panel/inset backgrounds in
    // every theme, so reusing it here keeps the outline legible without
    // hand-picking a colour that would only work in the dark themes.
    QColor landFill = Style::shiftL(QColor(Style::kInsetBg()), 14);
    landFill.setAlpha(160);
    // kTextTertiary at 1.2px (first pass) still lost the outline against
    // the grid/rings/spokes layers once those are on too -- all three
    // lean the same cool cyan-blue, so a same-toned gray outline reads
    // as "more of the same" rather than a distinct layer. kTextPrimary
    // (this palette's brightest, most neutral tone -- otherwise reserved
    // for body text) plus a wider stroke gives the border line a
    // materially different weight/brightness from every instrument
    // overlay it competes with, in every theme.
    QPen pen{QColor(Style::kTextPrimary())};
    pen.setWidthF(1.8);
    painter.setPen(pen);
    painter.setBrush(landFill);

    // Country-name labels (2026-09-13, operator: "pro land markieren")
    // share the border stroke's colour -- one visual identity for the
    // whole Grenzen layer -- but a plain (non-bold) weight so a label
    // never outweighs a real station/grid label at the same glance.
    const QFont nameFont = Style::monoFont(painter.font(), Style::kFontSmall);

    for (const CountryBorderRing& ring : m_countryBorders) {
        QPolygonF polygon;
        polygon.reserve(ring.points.size());
        for (const QPointF& lonLat : ring.points) {
            // core/CountryBorders.h's own ring points are (lon, lat) --
            // x() is longitude, y() is latitude -- matching the source
            // GeoJSON's axis order, so they pass into
            // calculateBearingInDegreesBetween/calculateDistanceKmBetween
            // (which both take lat first) with x/y swapped accordingly.
            const double bearing = calculateBearingInDegreesBetween(homeLat, homeLon, lonLat.y(), lonLat.x());
            const double distance = calculateDistanceKmBetween(homeLat, homeLon, lonLat.y(), lonLat.x());
            polygon.append(projectBearingDistance(bearing, distance, m_visibleRangeKm, area));
        }
        painter.drawPolygon(polygon);

        if (!ring.name.isEmpty()) {
            // Bench-found live, 2026-09-13 (operator screenshot at
            // 250 km range): centring on the FULL country's bounding
            // rect put the label outside the visible circle whenever
            // most of that country lies beyond the current zoom (e.g.
            // Österreich's own centroid, near Vienna, is well east of a
            // Salzkammergut-area home at this range) -- the border
            // outline itself is correctly clipped and visible, but its
            // name never was. Intersecting with the visible area first
            // keeps the label inside whatever part of the country is
            // actually on screen; only fall back to the untrimmed
            // centre if the country is fully outside the visible area
            // (drawText's own clip then hides it, same as before).
            const QRectF visibleBox = polygon.boundingRect().intersected(area);
            const QPointF centre = visibleBox.isEmpty() ? polygon.boundingRect().center() : visibleBox.center();
            painter.setFont(nameFont);
            painter.setPen(QColor(Style::kTextPrimary()));
            painter.drawText(QRectF(centre.x() - 60, centre.y() - 7, 120, 14),
                              Qt::AlignCenter, ring.name);
            // drawText's own pen/font calls don't touch the brush, but
            // painter.setPen above did overwrite the outline pen the
            // next ring's drawPolygon needs -- restore it so labelled
            // and unlabelled rings keep the identical outline.
            painter.setPen(pen);
        }
    }
}

void MapWidget::drawCitiesLayer(QPainter& painter, const QRectF& area) const
{
    if (m_cities.isEmpty() || !isValidGridSquare(m_ownGrid)) {
        return;
    }

    double homeLat = 0.0;
    double homeLon = 0.0;
    calculateLatLonFromGridSquare(m_ownGrid, homeLat, homeLon);

    const QFont capitalFont = Style::monoFont(painter.font(), Style::kFontCaption, QFont::Bold);
    const QFont regularFont = Style::monoFont(painter.font(), Style::kFontCaption);
    // Operator, live 2026-09-12 (same session as the border-contrast
    // fix above): the original plain filled dot in kTextSecondary/
    // kTextTertiary was "unübersichtlich" -- a 1.4-2px dim gray disc is
    // easy to lose against the grid/rings/border lines now sharing the
    // same canvas. Two changes: kTextPrimary for both (this palette's
    // brightest neutral -- still a neutral, not one of the amber/blue/
    // green semantic colours, so it still never competes with a real
    // worked/spotted station) and a ring-plus-centre-dot "city centre"
    // mark instead of a plain disc, so it reads as a place marker
    // rather than just another dot among the grid intersections.
    const QColor capitalColor(Style::kTextPrimary());
    const QColor cityColor(Style::kTextPrimary());

    for (const CityPoint& city : m_cities) {
        const double bearing = calculateBearingInDegreesBetween(homeLat, homeLon, city.lat, city.lon);
        const double distance = calculateDistanceKmBetween(homeLat, homeLon, city.lat, city.lon);
        // Operator: "die wichtigsten großen städte ab 150 km" -- and
        // never past the currently visible range, so a city never
        // shows up floating in a clipped rectangle corner outside the
        // map's own circular/elliptical rim (see canvasRect()).
        if (distance < kCityMinDistanceKm || distance > m_visibleRangeKm) {
            continue;
        }

        const QPointF point = projectBearingDistance(bearing, distance, m_visibleRangeKm, area);
        const QColor color = city.isCapital ? capitalColor : cityColor;
        const double ringRadius = city.isCapital ? 4.6 : (city.popMax >= 1000000 ? 3.8 : 3.0);
        const double centreRadius = city.isCapital ? 1.6 : 1.2;

        painter.setPen(QPen(color, city.isCapital ? 1.3 : 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(point, ringRadius, ringRadius);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(point, centreRadius, centreRadius);

        painter.setFont(city.isCapital ? capitalFont : regularFont);
        painter.setPen(color);
        painter.drawText(QRectF(point.x() + ringRadius + 3, point.y() - 7, 90, 14), Qt::AlignLeft | Qt::AlignVCenter,
                          city.name);
    }
}

void MapWidget::drawGridLayer(QPainter& painter, const QRectF& area) const
{
    const QVector<GridCell> cells = computeGridCells(area);
    const QFont labelFont = Style::monoFont(painter.font(), Style::kFontCaption);

    for (const GridCell& cell : cells) {
        if (cell.corners.size() != 4) {
            continue;
        }
        const QPolygonF polygon(cell.corners);

        QColor fill(Qt::transparent);
        QColor stroke{Style::kBorder()};
        qreal strokeWidth = 0.6;
        // Worked/spotted overlap on one cell is possible (several
        // stations sharing a 4-char square) -- worked wins visually,
        // since a confirmed contact is stronger information than an
        // open candidate. The home cell is deliberately NOT tinted here
        // any more (operator, 2026-09-14: "den eigenen grid nicht
        // markieren!") -- it renders exactly like any other untouched
        // cell; drawHomeMarker()'s own diamond+glow is still the "where
        // I am" indicator, a different, already-existing element, not
        // this grid-square fill.
        if (!m_showWorkedCells) {
            // Falls through with the plain transparent/kBorder look
            // every other empty cell already has.
        } else if (cell.hasWorked) {
            fill = QColor(Style::kGreenBg());
            stroke = QColor(Style::kGreenBorder());
            strokeWidth = 0.9;
            if (m_showAging && cell.freshestWorkedAtUtc.isValid()) {
                const qint64 secs = cell.freshestWorkedAtUtc.secsTo(QDateTime::currentDateTimeUtc());
                fill = agedMarkerColor(fill, secs);
                stroke = agedMarkerColor(stroke, secs);
            }
        } else if (cell.hasSpotted) {
            fill = QColor(Style::kBlueBg());
            fill.setAlpha(50);
            stroke = QColor(Style::kBlueBorder());
            strokeWidth = 0.9;
        }

        painter.setBrush(fill);
        QPen pen(stroke);
        pen.setWidthF(strokeWidth);
        painter.setPen(pen);
        painter.drawPolygon(polygon);

        const QPointF labelPoint = polygon.boundingRect().center();
        painter.setFont(labelFont);
        painter.setPen(QColor(Style::kTextInactive()));
        painter.drawText(QRectF(labelPoint.x() - 20, labelPoint.y() - 8, 40, 16), Qt::AlignCenter, cell.code);
    }
}

void MapWidget::drawRingsLayer(QPainter& painter, const QRectF& area) const
{
    // Dashed at 50 km steps, solid at 100 km steps, per the mockup.
    // rx/ry independent (not a single radius) -- see
    // projectBearingDistance's own comment: a square area still draws
    // perfect circles, a wide/short area (fit-to-window mode) draws
    // true ellipses instead of a circle with wasted margins.
    const QPointF center = area.center();
    const double halfWidth = area.width() / 2.0;
    const double halfHeight = area.height() / 2.0;
    const QColor amber{Style::kAmberText()};

    painter.setBrush(Qt::NoBrush);
    for (double km = 50.0; km <= m_visibleRangeKm + 0.5; km += 50.0) {
        const double frac = km / m_visibleRangeKm;
        const bool solid = std::fmod(km, 100.0) < 0.5;
        QColor color = amber;
        color.setAlpha(solid ? 140 : 85);
        QPen pen(color);
        pen.setWidthF(1.0);
        pen.setStyle(solid ? Qt::SolidLine : Qt::DashLine);
        painter.setPen(pen);
        painter.drawEllipse(center, frac * halfWidth, frac * halfHeight);
    }
}

void MapWidget::drawSpokesLayer(QPainter& painter, const QRectF& area) const
{
    const QPointF center = area.center();
    const QColor amber{Style::kAmberText()};

    // Bold every 45 deg, thin every 15 deg -- RotorWidget::drawTicks()'s
    // own split, reused here for the same reason: this widget is meant
    // to read as a sibling instrument, not a differently-designed one.
    for (int deg = 0; deg < 360; deg += 15) {
        const bool bold = (deg % 45 == 0);
        QColor color = amber;
        color.setAlpha(bold ? 150 : 70);
        QPen pen(color);
        pen.setWidthF(bold ? 1.3 : 0.6);
        painter.setPen(pen);
        const QPointF outer = projectBearingDistance(deg, m_visibleRangeKm, m_visibleRangeKm, area);
        painter.drawLine(center, outer);
    }

    for (int deg = 0; deg < 360; deg += 15) {
        QString text;
        bool bold = false;
        if (deg == 0) {
            text = QStringLiteral("N");
            bold = true;
        } else if (deg == 90) {
            text = QStringLiteral("E");
            bold = true;
        } else if (deg == 180) {
            text = QStringLiteral("S");
            bold = true;
        } else if (deg == 270) {
            text = QStringLiteral("W");
            bold = true;
        } else {
            text = QString::number(deg);
        }
        // The rim point itself (direction-aware, via the same elliptical
        // projectBearingDistance used for the spoke line above), not a
        // uniform-radius offset -- a uniform radius would drift inside
        // a wide ellipse's east/west edge or outside a tall one's
        // north/south edge once the area is no longer square.
        const QPointF rim = projectBearingDistance(deg, m_visibleRangeKm, m_visibleRangeKm, area);
        drawSpokeRimLabel(painter, rim, deg, text, bold);
    }
}

void MapWidget::drawSpokeRimLabel(QPainter& painter, const QPointF& rim, double angleDeg, const QString& text,
                                   bool bold) const
{
    const double rad = qDegreesToRadians(angleDeg);
    const double pad = bold ? 12.0 : 10.0;
    const QPointF p(rim.x() + pad * std::sin(rad), rim.y() - pad * std::cos(rad));
    const QFont f = bold ? Style::monoFont(painter.font(), Style::kFontSmall, QFont::Bold)
                          : Style::monoFont(painter.font(), Style::kFontCaption);
    painter.setFont(f);
    const QFontMetrics fm(f);
    const QRectF box(p.x() - fm.horizontalAdvance(text), p.y() - fm.height(), fm.horizontalAdvance(text) * 2.0,
                      fm.height() * 2.0);
    painter.setPen(bold ? QColor(Style::kInstrumentFace()) : QColor(Style::kTextScale()));
    painter.drawText(box, Qt::AlignCenter, text);
}

void MapWidget::drawRotorHeadingsLayer(QPainter& painter, const QRectF& area) const
{
    const QPointF center = area.center();
    // Medium green, thin line -- operator, 2026-09-14: "ev. mittleres
    // grün, dünne linie" (was 2px amber, competing visually with the
    // amber ring/spokes/ticks already all over this canvas). kGreenText
    // is this palette's one existing "medium green" role (the same
    // worked-station colour markerColor()/gridLabelColor() use above),
    // not a new hue invented for this one layer.
    const QColor green{Style::kGreenText()};

    const auto drawHeading = [&](bool connected, double azimuthDeg, const QString& label) {
        if (!connected) {
            return;
        }
        const QPointF rim = projectBearingDistance(azimuthDeg, m_visibleRangeKm, m_visibleRangeKm, area);

        QPen pen(green);
        pen.setWidthF(1.2);
        painter.setPen(pen);
        painter.drawLine(center, rim);

        // A small filled triangle at the rim, pointing outward along the
        // same bearing -- the same "needle tip" language RotorWidget's
        // own drawNeedle() uses for its target marker, so a heading here
        // reads as the same kind of instrument reading, not a generic
        // arrow.
        const double rad = qDegreesToRadians(azimuthDeg);
        const QPointF dir(std::sin(rad), -std::cos(rad));
        const QPointF normal(-dir.y(), dir.x());
        constexpr double kTipLen = 6.0;
        constexpr double kTipHalfW = 3.0;
        QPolygonF tip;
        tip << rim + dir * kTipLen << rim + normal * kTipHalfW << rim - normal * kTipHalfW;
        painter.setPen(Qt::NoPen);
        painter.setBrush(green);
        painter.drawPolygon(tip);
        painter.setBrush(Qt::NoBrush);

        if (!label.isEmpty()) {
            const QFont f = Style::monoFont(painter.font(), Style::kFontSmall, QFont::Bold);
            painter.setFont(f);
            const QFontMetrics fm(f);
            const QPointF labelPoint = rim + dir * (kTipLen + 4.0);
            const QRectF box(labelPoint.x() - fm.horizontalAdvance(label), labelPoint.y() - fm.height(),
                              fm.horizontalAdvance(label) * 2.0, fm.height() * 2.0);
            painter.setPen(green);
            painter.drawText(box, Qt::AlignCenter, label);
        }
    };

    drawHeading(m_showRotor1Heading && m_rotor1Connected, m_rotor1AzimuthDeg, m_rotor1Label);
    drawHeading(m_showRotor2Heading && m_rotor2Connected, m_rotor2AzimuthDeg, m_rotor2Label);
}

void MapWidget::drawStations(QPainter& painter, const QRectF& area) const
{
    if (!isValidGridSquare(m_ownGrid)) {
        return;
    }

    const QFont gridFont = Style::monoFont(painter.font(), Style::kFontBody, QFont::Bold);
    const QFont secondaryFont = Style::monoFont(painter.font(), Style::kFontCaption);

    for (const Station& station : m_stations) {
        if (!isValidGridSquare(station.grid)) {
            continue;
        }
        // Reused directly from Maidenhead.h -- both grid arguments are
        // real locator strings here (unlike the grid-square-corner
        // projection above, which needed the raw lat/lon overload since
        // a corner is not itself a valid locator).
        const double bearing = calculateBearingInDegrees(m_ownGrid, station.grid);
        const double distance = calculateDistanceKm(m_ownGrid, station.grid);
        const QPointF point = projectBearingDistance(bearing, distance, m_visibleRangeKm, area);
        QColor color = markerColor(station.worked);
        // "Altern" only touches the marker/halo/link colour, never the
        // grid-code/callsign text below -- an aged contact should read
        // as visually quieter, not become harder to actually read.
        if (m_showAging && station.worked && station.workedAtUtc.isValid()) {
            color = agedMarkerColor(color, station.workedAtUtc.secsTo(QDateTime::currentDateTimeUtc()));
        }

        if (station.worked) {
            // A confirmed contact is this canvas's actual achievement
            // data -- a soft glow halo (same instrument-glow language
            // as drawGlow()/drawHomeMarker()'s hub) makes it the most
            // visually prominent marker after home itself, ahead of a
            // plain-ringed open/spotted candidate below.
            QColor haloColor = color;
            haloColor.setAlpha(110);
            QColor haloEdge = color;
            haloEdge.setAlpha(0);
            QRadialGradient halo(point, 6.5);
            halo.setColorAt(0.0, haloColor);
            halo.setColorAt(1.0, haloEdge);
            painter.setPen(Qt::NoPen);
            painter.setBrush(halo);
            painter.drawEllipse(point, 6.5, 6.5);

            painter.setBrush(color);
            painter.drawEllipse(point, 3.2, 3.2);
        } else {
            QPen ringPen(color);
            ringPen.setWidthF(1.3);
            painter.setPen(ringPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(point, 2.8, 2.8);
        }

        // Grid-code label amber/bold (worked) or blue/bold (spotted),
        // station callsign secondary and dimmer -- the plan's own
        // wording ("grid-code label in amber/bold, station name
        // secondary in gray"). No station "name"/city is available in
        // this codebase's data (only callsign, grid, source, timestamp
        // -- see SpotCandidate.h/QsoRecord.h), so the callsign fills
        // that secondary role instead; operationally it is the more
        // useful of the two anyway (who you worked matters more than
        // which town they are in).
        painter.setFont(gridFont);
        painter.setPen(gridLabelColor(station.worked));
        painter.drawText(QRectF(point.x() + 5, point.y() - 12, 60, 14), Qt::AlignLeft | Qt::AlignVCenter,
                          station.grid.left(4).toUpper());

        painter.setFont(secondaryFont);
        painter.setPen(station.worked ? QColor(Style::kTextTertiary())
                                       : QColor(Style::kTextInactive()));
        painter.drawText(QRectF(point.x() + 5, point.y() + 1, 80, 12), Qt::AlignLeft | Qt::AlignVCenter,
                          station.callsign);
    }
}

void MapWidget::drawHomeMarker(QPainter& painter, const QRectF& area) const
{
    if (!isValidGridSquare(m_ownGrid)) {
        return;
    }
    const QPointF center = area.center();
    const QColor amber{Style::kAmberText()};

    // Soft glow halo behind the marker itself -- the same "Hub: soft
    // amber radial glow" treatment RotorWidget::paintFullCompassDial()
    // already gives its own needle hub, so both instruments' "this is
    // your reference point" markers share one visual language.
    QRadialGradient hub(center, 9.0);
    hub.setColorAt(0.0, amber);
    QColor hubEdge = amber;
    hubEdge.setAlpha(0);
    hub.setColorAt(1.0, hubEdge);
    painter.setPen(Qt::NoPen);
    painter.setBrush(hub);
    painter.drawEllipse(center, 9.0, 9.0);

    painter.setBrush(amber);
    constexpr double kSize = 5.0;
    QPolygonF diamond;
    diamond << QPointF(center.x(), center.y() - kSize) << QPointF(center.x() + kSize, center.y())
            << QPointF(center.x(), center.y() + kSize) << QPointF(center.x() - kSize, center.y());
    painter.drawPolygon(diamond);

    // Deliberately no own callsign/grid text label here -- see
    // drawPanelHeader() for the same 2026-09-11 decision. The diamond
    // marker itself stays (it's the neutral "Standort" legend entry).
}

void MapWidget::drawLegend(QPainter& painter, const QRectF& area) const
{
    const QRectF legendRect(area.left(), area.bottom() + 6, area.width(), kLegendHeight - 6);
    const double y = legendRect.top();
    const double lineHeight = legendRect.height();
    const QColor green{Style::kGreenText()};
    const QColor blue{Style::kBlueBg()};
    const QColor amber{Style::kAmberText()};
    const QColor secondary{Style::kTextSecondary()};

    const QFont legendFont = Style::capsFont(painter.font());
    painter.setFont(legendFont);
    const QFontMetrics fm(legendFont);
    double x = legendRect.left();

    painter.setPen(QPen(green, 1.6));
    painter.drawLine(QPointF(x, y + lineHeight / 2.0), QPointF(x + 14, y + lineHeight / 2.0));
    x += 18;
    painter.setPen(secondary);
    const QString workedLabel = QStringLiteral("gearbeitet");
    painter.drawText(QRectF(x, y, fm.horizontalAdvance(workedLabel) + 4, lineHeight), Qt::AlignVCenter | Qt::AlignLeft,
                      workedLabel);
    x += fm.horizontalAdvance(workedLabel) + 18;

    QPen dashPen(blue, 1.2);
    dashPen.setStyle(Qt::DashLine);
    painter.setPen(dashPen);
    painter.drawLine(QPointF(x, y + lineHeight / 2.0), QPointF(x + 14, y + lineHeight / 2.0));
    x += 18;
    painter.setPen(secondary);
    const QString spottedLabel = QStringLiteral("gespottet · offen");
    // Measured via QFontMetrics rather than a fixed guess -- a fixed
    // 106px advance clipped this label's own text box short and let
    // the Standort swatch/label right after it overlap "OFFEN" (found
    // live via the map mockup render, 2026-09-13: this caps-tracked
    // font, see Style::capsFont()'s own .18em letter-spacing, runs
    // noticeably wider than a guessed pixel count).
    painter.drawText(QRectF(x, y, fm.horizontalAdvance(spottedLabel) + 4, lineHeight), Qt::AlignVCenter | Qt::AlignLeft,
                      spottedLabel);
    x += fm.horizontalAdvance(spottedLabel) + 18;

    painter.setPen(Qt::NoPen);
    painter.setBrush(amber);
    painter.drawRect(QRectF(x, y + lineHeight / 2.0 - 4, 8, 8));
    x += 14;
    painter.setPen(secondary);
    painter.drawText(QRectF(x, y, 100, lineHeight), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("Standort"));
}

void MapWidget::drawTerrainSectorWash(QPainter& painter, const QRectF& area) const
{
    // No-op until MainWindow actually has a sweep to give this widget --
    // not an error state, just "Phase 2 terrain data not wired up (yet)
    // for this instance", same guard RotorWidget::drawTerrainSectorWash()
    // uses for the same reason.
    if (m_terrainSectors.size() != 360) {
        return;
    }

    // Same "loop a degree range, find contiguous Blocked/Marginal runs,
    // build a QPainterPath along the rim, stroke once" technique
    // RotorWidget::drawTerrainSectorWash() uses -- projectBearingDistance()
    // at the full visible range is this map's own rim point for a given
    // bearing (elliptical in fit-to-window mode), the direct equivalent
    // of that method's circular pointOnCircle().
    int i = 0;
    while (i < 360) {
        const LineOfSightClass cls = m_terrainSectors.at(i);
        if (cls != LineOfSightClass::Blocked && cls != LineOfSightClass::Marginal) {
            ++i;
            continue;
        }
        int j = i;
        while (j < 360 && m_terrainSectors.at(j) == cls) {
            ++j;
        }

        // Operator, 2026-09-14: "der rote kreise bei karte verbindungen
        // ist auch viel zu markant, ev. unser gelb und viel dünner. dient
        // nur als markierung" -- this rim wash is informational (which
        // bearings are shadowed at the visible range), not a warning, so
        // it no longer reaches for kRedBorder even on Blocked; both
        // classes share the same quiet amber the ring/rate-limit markers
        // elsewhere already use, and the stroke drops from a 5px band to
        // a thin 1.2px line -- a marking, not an alarm.
        QPen washPen{QColor(Style::kAmberWarn())};
        washPen.setWidthF(1.2);
        painter.setPen(washPen);
        painter.setBrush(Qt::NoBrush);

        QPainterPath path;
        bool first = true;
        for (int deg = i; deg < j; ++deg) {
            const QPointF p = projectBearingDistance(static_cast<double>(deg), m_visibleRangeKm, m_visibleRangeKm, area);
            if (first) {
                path.moveTo(p);
                first = false;
            } else {
                path.lineTo(p);
            }
        }
        painter.drawPath(path);
        i = j;
    }
}

void MapWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Panel chrome: rounded background + border, clipped so everything
    // drawn afterwards respects the rounded corners -- same structure
    // as RotorWidget::paintEvent().
    QPainterPath panelPath;
    panelPath.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Style::kPanelRadius, Style::kPanelRadius);
    painter.fillPath(panelPath, QColor(Style::kPanelBg()));
    painter.setPen(QPen(QColor(Style::kBorderSubtle()), 1.0));
    painter.drawPath(panelPath);
    painter.setClipPath(panelPath);

    drawPanelHeader(painter);

    const QRectF area = canvasRect();
    if (area.width() > 4.0 && area.height() > 4.0 && isValidGridSquare(m_ownGrid)) {
        painter.save();
        painter.setClipRect(area);
        painter.fillRect(area, QColor(Style::kInsetBg()));
        drawGlow(painter, area);
        if (m_showBorders) {
            drawBordersLayer(painter, area);
        }
        if (m_showGrid) {
            drawGridLayer(painter, area);
        }
        if (m_showRings) {
            drawRingsLayer(painter, area);
        }
        if (m_showSpokes) {
            drawSpokesLayer(painter, area);
        }
        if (m_showRotor1Heading || m_showRotor2Heading) {
            drawRotorHeadingsLayer(painter, area);
        }
        if (m_showCities) {
            drawCitiesLayer(painter, area);
        }
        drawStations(painter, area);
        drawHomeMarker(painter, area);
        painter.restore();

        QPen borderPen{QColor(Style::kBorderSubtle())};
        borderPen.setWidthF(1.0);
        painter.setPen(borderPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(area);
        drawTerrainSectorWash(painter, area);

        drawLegend(painter, area);
    } else {
        // "Unbekannt ist ein Strich, keine Null" in spirit -- a missing
        // home grid means there is genuinely nothing to project, so the
        // canvas says so in words rather than silently rendering an
        // empty ring.
        painter.setFont(Style::capsFont(painter.font()));
        painter.setPen(QColor(Style::kTextInactive()));
        painter.drawText(QRectF(0, kHeaderHeight + kSettingsRowHeight, width(), height() - kHeaderHeight - kSettingsRowHeight),
                          Qt::AlignCenter, QStringLiteral("KEIN STANDORT-GRID KONFIGURIERT"));
    }
}

void MapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !isValidGridSquare(m_ownGrid)) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QRectF area = canvasRect();
    if (area.width() <= 4.0 || area.height() <= 4.0) {
        QWidget::mousePressEvent(event);
        return;
    }

    // Same projection drawStations() uses -- the closest marker within
    // kStationClickTolerancePx wins; ties (extremely unlikely at this
    // scale) go to whichever m_stations entry is checked first.
    const QPointF clickPos = event->position();
    double bestDistSq = kStationClickTolerancePx * kStationClickTolerancePx;
    const Station* hit = nullptr;
    for (const Station& station : m_stations) {
        if (!isValidGridSquare(station.grid)) {
            continue;
        }
        const double bearing = calculateBearingInDegrees(m_ownGrid, station.grid);
        const double distance = calculateDistanceKm(m_ownGrid, station.grid);
        const QPointF point = projectBearingDistance(bearing, distance, m_visibleRangeKm, area);
        const double dx = point.x() - clickPos.x();
        const double dy = point.y() - clickPos.y();
        const double distSq = dx * dx + dy * dy;
        if (distSq <= bestDistSq) {
            bestDistSq = distSq;
            hit = &station;
        }
    }

    if (hit) {
        emit candidateActivated(hit->callsign, hit->grid, hit->freqHz);
        return;
    }
    QWidget::mousePressEvent(event);
}

} // namespace Contestprogramm
