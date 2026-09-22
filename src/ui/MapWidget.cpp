#include "ui/MapWidget.h"

#include "core/Cities.h"
#include "core/CountryBorders.h"
#include "core/Maidenhead.h"
#include "core/SolarPosition.h"
#include "ui/StyleKit.h"

#include <QAction>
#include <QActionGroup>
#include <QFont>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QHelpEvent>
#include <QToolTip>
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
constexpr int kControlsRowHeight = 24;
constexpr int kCanvasMargin = 8;
// Radar: the number column to the right of the scope, and the least
// scope width worth keeping it (below that the numbers go and the scope
// takes the whole canvas).
constexpr int kNumbersColumnWidth = 186;
constexpr int kMinScopeWidthWithNumbers = 240;
// The horizon rim: elevation angles above this count as "blocked".
constexpr double kStripMaxElevationDeg = 8.0;
constexpr double kRimMaxThicknessPx = 22.0;
constexpr double kMinVisibleRangeKm = 25.0;
// Kurzwelle: eine Station auf der anderen Seite der Erde ist gut
// 20 000 km weit weg. Die Obergrenze lag bei 3 200 km -- richtig,
// solange nur UKW im Spiel war.
constexpr double kMaxVisibleRangeKm = 20000.0;
// Operator, 2026-09-14: "mache schritte beim radius bitte alle 250km".
constexpr double kVisibleRangeStepKm = 250.0;

// Was im ⚙-Menü als Sprungweite steht: vom Nahbereich bis zur
// Weltkarte, ohne zwanzigmal auf die Zoomtaste zu drücken.
constexpr double kRangePresetsKm[] = {100.0, 300.0, 1000.0, 3000.0, 10000.0, 20000.0};

// Die Graulinie wandert gut 15 Grad je Stunde, also rund 0,25 Grad je
// Minute -- einmal je Minute neu zeichnen ist mehr, als man sieht.
constexpr int kGreylineRefreshIntervalMs = 60 * 1000;
// Bürgerliche Dämmerung: die Sonne 6 Grad unter dem Horizont. Auf der
// Kugel sind das 6/90 des Viertelumfangs jenseits der Tag-Nacht-Grenze,
// rund 667 km -- so breit ist das Band, das gezeichnet wird.
constexpr double kCivilTwilightDeg = 6.0;

// Der Schritt der beiden Zoomtasten. Unter 3 200 km bleibt es bei
// Martins 250 km (2026-09-14: "mache schritte beim radius bitte alle
// 250km") -- darüber wären das 67 Klicks bis zur Gegenseite der Erde.
double zoomStepKm(double rangeKm)
{
    if (rangeKm < 3200.0) {
        return kVisibleRangeStepKm;
    }
    if (rangeKm < 10000.0) {
        return 1000.0;
    }
    return 2500.0;
}
// Operator, 2026-09-12: "die wichtigsten großen städte ab 150 km".
constexpr double kCityMinDistanceKm = 150.0;
// Operator, 2026-09-13: "alte Kontakte ausgrauen" -- full colour for
// the first 30 minutes, fading to grey by 180.
constexpr int kAgingStartMinutes = 30;
constexpr int kAgingCompleteMinutes = 180;
constexpr int kAgingRefreshIntervalMs = 60000;
// A worked station keeps its callsign label this long; older ones are
// just dots -- the log has the names, the map should show the shape.
constexpr int kFreshLabelMinutes = 60;
constexpr double kStationClickTolerancePx = 9.0;
constexpr double kDefaultBeamwidthDeg = 30.0;
constexpr double kMinBeamwidthDeg = 5.0;
constexpr double kMaxBeamwidthDeg = 120.0;
const int kBeamwidthChoices[] = {10, 15, 20, 25, 30, 40, 50, 60};

double wrap360(double deg)
{
    return std::fmod(std::fmod(deg, 360.0) + 360.0, 360.0);
}

double angularDistance(double a, double b)
{
    const double d = std::fabs(wrap360(a) - wrap360(b));
    return std::min(d, 360.0 - d);
}

QColor withAlpha(const QString& hex, int alpha)
{
    QColor color(hex);
    color.setAlpha(alpha);
    return color;
}

QPointF polar(const QPointF& centre, double bearingDeg, double rx, double ry)
{
    const double a = qDegreesToRadians(bearingDeg);
    return QPointF(centre.x() + rx * std::sin(a), centre.y() - ry * std::cos(a));
}

QString compassLabel(int deg)
{
    switch (deg) {
    case 0: return QStringLiteral("N");
    case 90: return QStringLiteral("O");
    case 180: return QStringLiteral("S");
    case 270: return QStringLiteral("W");
    default: return QString::number(deg);
    }
}

QString groupedKm(qint64 value)
{
    return QLocale(QLocale::German, QLocale::Austria).toString(value);
}

// Label placement that refuses to overprint: right of the point, else
// left, else below; gives up when all three collide with earlier ones.
struct LabelPlacer {
    QVector<QRectF> taken;
    bool place(QPainter& painter, const QPointF& point, const QString& text, const QColor& color)
    {
        const QFontMetricsF fm(painter.font());
        const double w = fm.horizontalAdvance(text);
        const QPointF tries[3] = {point + QPointF(6.0, 4.0), point + QPointF(-6.0 - w, 4.0),
                                  point + QPointF(-w / 2.0, 14.0)};
        for (const QPointF& at : tries) {
            const QRectF box(at.x() - 1.0, at.y() - fm.ascent(), w + 2.0, fm.height());
            bool clash = false;
            for (const QRectF& other : taken) {
                if (other.intersects(box)) {
                    clash = true;
                    break;
                }
            }
            if (clash) {
                continue;
            }
            taken.append(box);
            painter.setPen(color);
            painter.drawText(at, text);
            return true;
        }
        return false;
    }
};
} // namespace

MapWidget::MapWidget(QWidget* parent)
    : QWidget(parent)
    , m_countryBorders(loadCountryBorders())
    , m_cities(loadCities())
{
    buildControls();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_controlsRow);
    layout->addStretch(1);

    setMinimumSize(minimumSizeHint());
    refreshZoomLabel();
    syncControls();

    m_agingRefreshTimer = new QTimer(this);
    m_agingRefreshTimer->setInterval(kAgingRefreshIntervalMs);
    connect(m_agingRefreshTimer, &QTimer::timeout, this, QOverload<>::of(&MapWidget::update));
    if (m_showAging) {
        m_agingRefreshTimer->start();
    }

    m_greylineTimer = new QTimer(this);
    m_greylineTimer->setInterval(kGreylineRefreshIntervalMs);
    connect(m_greylineTimer, &QTimer::timeout, this, QOverload<>::of(&MapWidget::update));
    syncGreylineTimer();
}

void MapWidget::buildControls()
{
    m_controlsRow = new QWidget(this);
    m_controlsRow->setFixedHeight(kControlsRowHeight);
    auto* row = new QHBoxLayout(m_controlsRow);
    row->setContentsMargins(10, 2, 8, 2);
    row->setSpacing(6);

    // Only the zoom lives here, on the right; the view switch that
    // used to sit on the left went with the second view.
    row->addStretch(1);

    // The ⚙ menu's entries. The QActions live here, parented to the
    // widget; the QMenu itself is built fresh for every click
    // (populateOptionsMenu) -- a QMenu kept as a child of a widget that
    // is later re-parented into its panel container crashed inside
    // Cocoa's popup path on the second start (stale platform window),
    // the Log panel's per-click menu never did.
    const auto addToggle = [this](const QString& text, const QString& tip, void (MapWidget::*setter)(bool)) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setToolTip(tip);
        connect(action, &QAction::toggled, this, [this, setter](bool on) {
            if (!m_syncingControls) {
                (this->*setter)(on);
            }
        });
        return action;
    };
    m_ringsAction = addToggle(QStringLiteral("Entfernungsringe"), QStringLiteral("Ringe alle 100 km"), &MapWidget::setRingsLayerVisible);
    m_greylineAction = addToggle(QStringLiteral("Graulinie"),
                                 QStringLiteral("Wo gerade Dämmerung ist -- auf Kurzwelle die Zone, in der die "
                                                "unteren Bänder aufmachen"),
                                 &MapWidget::setGreylineLayerVisible);
    m_spokesAction = addToggle(QStringLiteral("Peilung"), QStringLiteral("Gradteilung am Rand, in der Karte auch Speichen"), &MapWidget::setSpokesLayerVisible);
    m_horizonAction = addToggle(QStringLiteral("Horizont"), QStringLiteral("Berge als Rand des Radars bzw. als Skyline unter der Karte"), &MapWidget::setHorizonLayerVisible);
    m_rotor1Action = addToggle(QStringLiteral("Rotor 1"), QStringLiteral("Peilung von Rotor 1 als Lichtkegel"), &MapWidget::setRotor1HeadingLayerVisible);
    m_rotor2Action = addToggle(QStringLiteral("Rotor 2"), QStringLiteral("Peilung von Rotor 2"), &MapWidget::setRotor2HeadingLayerVisible);
    // Whether a rotor carries a second antenna: a station setting (the
    // settings dialog has it too), switchable here where the cones are.
    // One or two antennas per rotor (operator, 2026-09-21: "pro rotor
    // ein oder zwei antennen auswählen können … bei rotor 2 öfters nur
    // eine antenne, stack"): a pair of exclusive entries per rotor, the
    // station setting the settings dialog edits too.
    const auto addAntennaChoice = [this](int rotor, bool two) {
        auto* action = new QAction(this);
        action->setCheckable(true);
        connect(action, &QAction::triggered, this, [this, rotor, two] {
            if (m_syncingControls) {
                return;
            }
            bool& enabled = rotor == 1 ? m_rotor1SecondEnabled : m_rotor2SecondEnabled;
            if (enabled == two) {
                syncControls();
                return;
            }
            enabled = two;
            syncControls();
            update();
            emit secondAntennaToggled(rotor, two);
        });
        return action;
    };
    m_rotor1OneAction = addAntennaChoice(1, false);
    m_rotor1SecondAction = addAntennaChoice(1, true);
    m_rotor2OneAction = addAntennaChoice(2, false);
    m_rotor2SecondAction = addAntennaChoice(2, true);
    m_agingAction = addToggle(QStringLiteral("Alte Kontakte verblassen"), QStringLiteral("Gearbeitete Stationen werden nach 30 min langsam grau"), &MapWidget::setAgingEnabled);
    m_fitAction = addToggle(QStringLiteral("Fläche füllen"), QStringLiteral("Scheibe füllt die Fläche (elliptisch); aus: Kreis"), &MapWidget::setFitToWindowEnabled);
    m_bordersAction = addToggle(QStringLiteral("Grenzen"), QStringLiteral("Staatsgrenzen/Küstenlinien (Natural Earth 1:110m)"), &MapWidget::setBordersLayerVisible);
    m_citiesAction = addToggle(QStringLiteral("Städte"), QStringLiteral("Wichtigste Großstädte ab 150 km (Natural Earth 1:110m)"), &MapWidget::setCitiesLayerVisible);
    m_gridAction = addToggle(QStringLiteral("Locator-Raster"), QStringLiteral("Großfelder als Raster mit Beschriftung"), &MapWidget::setGridLayerVisible);
    m_workedCellsAction = addToggle(QStringLiteral("Gearbeitete Felder färben"), QStringLiteral("Gearbeitete/gespottete Großfelder im Raster tönen"), &MapWidget::setWorkedCellsLayerVisible);

    m_zoomOutButton = new QPushButton(QStringLiteral("−"), m_controlsRow);
    m_zoomInButton = new QPushButton(QStringLiteral("+"), m_controlsRow);
    for (QPushButton* button : {m_zoomOutButton, m_zoomInButton}) {
        button->setFixedSize(20, 18);
        button->setFont(Style::monoFont(font(), Style::kFontBody, QFont::Bold));
        button->setFocusPolicy(Qt::NoFocus);
        button->setStyleSheet(Style::iconButtonStyle());
    }
    m_zoomRangeLabel = new QLabel(m_controlsRow);
    m_zoomRangeLabel->setFont(Style::monoFont(font(), Style::kFontCaption));
    m_zoomRangeLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextTertiary()));
    m_zoomRangeLabel->setAlignment(Qt::AlignCenter);
    m_zoomRangeLabel->setFixedWidth(52);
    row->addWidget(m_zoomOutButton);
    row->addWidget(m_zoomRangeLabel);
    row->addWidget(m_zoomInButton);
    connect(m_zoomOutButton, &QPushButton::clicked, this, &MapWidget::zoomOut);
    connect(m_zoomInButton, &QPushButton::clicked, this, &MapWidget::zoomIn);
}

void MapWidget::syncControls()
{
    m_syncingControls = true;
    const auto sync = [](QAction* action, bool on) {
        if (action && action->isChecked() != on) {
            action->setChecked(on);
        }
    };
    sync(m_gridAction, m_layers.grid);
    sync(m_ringsAction, m_showRings);
    sync(m_greylineAction, m_layers.greyline);
    sync(m_spokesAction, m_showSpokes);
    sync(m_workedCellsAction, m_layers.cells);
    sync(m_bordersAction, m_layers.borders);
    sync(m_citiesAction, m_layers.cities);
    sync(m_agingAction, m_showAging);
    sync(m_fitAction, m_fitToWindow);
    sync(m_rotor1Action, m_showRotor1Heading);
    sync(m_rotor2Action, m_showRotor2Heading);
    sync(m_horizonAction, m_showHorizon);
    if (m_rotor1SecondAction) {
        const auto twoText = [](double offset) {
            return QStringLiteral("Zwei Antennen (%1%2°)").arg(offset >= 0 ? QStringLiteral("+") : QString()).arg(offset, 0, 'f', 0);
        };
        m_rotor1OneAction->setText(QStringLiteral("Eine Antenne (Stack)"));
        m_rotor1SecondAction->setText(twoText(m_rotor1SecondOffsetDeg));
        m_rotor2OneAction->setText(QStringLiteral("Eine Antenne (Stack)"));
        m_rotor2SecondAction->setText(twoText(m_rotor2SecondOffsetDeg));
        sync(m_rotor1OneAction, !m_rotor1SecondEnabled);
        sync(m_rotor1SecondAction, m_rotor1SecondEnabled);
        sync(m_rotor2OneAction, !m_rotor2SecondEnabled);
        sync(m_rotor2SecondAction, m_rotor2SecondEnabled);
    }
    m_syncingControls = false;
}

void MapWidget::populateOptionsMenu(QMenu* menu)
{
    if (!menu) {
        return;
    }
    syncControls();
    menu->addAction(m_ringsAction);
    menu->addAction(m_greylineAction);
    menu->addAction(m_spokesAction);
    menu->addAction(m_horizonAction);
    menu->addAction(m_rotor1Action);
    menu->addAction(m_rotor2Action);
    QMenu* antennas1 = menu->addMenu(QStringLiteral("Antennen Rotor 1"));
    antennas1->addAction(m_rotor1OneAction);
    antennas1->addAction(m_rotor1SecondAction);
    QMenu* antennas2 = menu->addMenu(QStringLiteral("Antennen Rotor 2"));
    antennas2->addAction(m_rotor2OneAction);
    antennas2->addAction(m_rotor2SecondAction);
    // Opening angle of each rotor's antennas -- the width of its cone;
    // the submenus are the menu's own, built per click.
    const auto beamwidthMenu = [this, menu](const QString& title, double current, void (MapWidget::*setter)(double)) {
        QMenu* sub = menu->addMenu(title);
        auto* group = new QActionGroup(sub);
        group->setExclusive(true);
        for (int deg : kBeamwidthChoices) {
            QAction* action = sub->addAction(QStringLiteral("%1°").arg(deg));
            action->setCheckable(true);
            action->setChecked(std::fabs(current - deg) < 0.5);
            group->addAction(action);
            connect(action, &QAction::triggered, this, [this, setter, deg] { (this->*setter)(deg); });
        }
    };
    beamwidthMenu(QStringLiteral("Öffnungswinkel Rotor 1"), m_beamwidth1Deg, &MapWidget::setRotor1BeamwidthDeg);
    beamwidthMenu(QStringLiteral("Öffnungswinkel Rotor 2"), m_beamwidth2Deg, &MapWidget::setRotor2BeamwidthDeg);
    // Sprungweiten statt Klicken: von 300 km auf die Weltkarte wären es
    // mit den beiden Zoomtasten zwanzig Klicks.
    QMenu* rangeMenu = menu->addMenu(QStringLiteral("Reichweite"));
    auto* rangeGroup = new QActionGroup(rangeMenu);
    rangeGroup->setExclusive(true);
    for (double km : kRangePresetsKm) {
        QAction* action = rangeMenu->addAction(QStringLiteral("%1 km").arg(groupedKm(static_cast<qint64>(km))));
        action->setCheckable(true);
        action->setChecked(std::fabs(m_visibleRangeKm - km) < 0.5);
        rangeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, km] { setVisibleRangeKm(km); });
    }
    menu->addAction(m_agingAction);
    menu->addAction(m_fitAction);
    menu->addSeparator();
    menu->addAction(m_bordersAction);
    menu->addAction(m_citiesAction);
    menu->addAction(m_gridAction);
    menu->addAction(m_workedCellsAction);
}

void MapWidget::notePreferenceChange()
{
    syncControls();
    update();
    emit preferencesChanged();
}

void MapWidget::refreshZoomLabel() const
{
    if (m_zoomRangeLabel) {
        m_zoomRangeLabel->setText(
            QStringLiteral("%1 km").arg(groupedKm(static_cast<qint64>(std::llround(m_visibleRangeKm)))));
    }
}

// --- data ------------------------------------------------------------------

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

#define MAPWIDGET_TOGGLE(Setter, member)                                                                            \
    void MapWidget::Setter(bool on)                                                                                 \
    {                                                                                                               \
        if (member == on) {                                                                                         \
            syncControls();                                                                                         \
            return;                                                                                                 \
        }                                                                                                           \
        member = on;                                                                                                \
        notePreferenceChange();                                                                                     \
    }

MAPWIDGET_TOGGLE(setGridLayerVisible, m_layers.grid)
MAPWIDGET_TOGGLE(setRingsLayerVisible, m_showRings)
MAPWIDGET_TOGGLE(setSpokesLayerVisible, m_showSpokes)
MAPWIDGET_TOGGLE(setWorkedCellsLayerVisible, m_layers.cells)
MAPWIDGET_TOGGLE(setBordersLayerVisible, m_layers.borders)
MAPWIDGET_TOGGLE(setCitiesLayerVisible, m_layers.cities)
MAPWIDGET_TOGGLE(setFitToWindowEnabled, m_fitToWindow)
MAPWIDGET_TOGGLE(setRotor1HeadingLayerVisible, m_showRotor1Heading)
MAPWIDGET_TOGGLE(setRotor2HeadingLayerVisible, m_showRotor2Heading)
MAPWIDGET_TOGGLE(setHorizonLayerVisible, m_showHorizon)
#undef MAPWIDGET_TOGGLE

// Nicht über das Makro: diese Schicht hat einen Zeitgeber, der mit ihr
// an- und ausgeht.
void MapWidget::setGreylineLayerVisible(bool on)
{
    if (m_layers.greyline == on) {
        syncControls();
        return;
    }
    m_layers.greyline = on;
    syncGreylineTimer();
    notePreferenceChange();
}

void MapWidget::syncGreylineTimer()
{
    if (!m_greylineTimer) {
        return;
    }
    if (m_layers.greyline) {
        m_greylineTimer->start();
    } else {
        m_greylineTimer->stop();
    }
}

void MapWidget::setAgingEnabled(bool enabled)
{
    if (m_showAging != enabled) {
        m_showAging = enabled;
        if (m_agingRefreshTimer) {
            if (enabled) {
                m_agingRefreshTimer->start();
            } else {
                m_agingRefreshTimer->stop();
            }
        }
        notePreferenceChange();
    } else {
        syncControls();
    }
}

void MapWidget::setHorizonProfile(const QVector<double>& elevationDegByBearing)
{
    m_horizonProfile = elevationDegByBearing.size() == 360 ? elevationDegByBearing : QVector<double>();
    update();
}

void MapWidget::setTerrainSectors(const QVector<LineOfSightClass>& sectorsByDegree)
{
    m_terrainSectors = sectorsByDegree;
    update();
}

void MapWidget::setRotor1Heading(bool connected, double azimuthDeg, const QString& label)
{
    m_rotor1Connected = connected;
    m_rotor1AzimuthDeg = azimuthDeg;
    m_rotor1Label = label;
    update();
}

void MapWidget::setRotorLinkLive(int rotor, bool live)
{
    bool& member = rotor == 2 ? m_rotor2Live : m_rotor1Live;
    if (member == live) {
        return;
    }
    member = live;
    update();
}

void MapWidget::setRotor2Heading(bool connected, double azimuthDeg, const QString& label)
{
    m_rotor2Connected = connected;
    m_rotor2AzimuthDeg = azimuthDeg;
    m_rotor2Label = label;
    update();
}

void MapWidget::setRotor1SecondAntenna(bool enabled, double offsetDeg)
{
    m_rotor1SecondEnabled = enabled;
    m_rotor1SecondOffsetDeg = offsetDeg;
    syncControls();
    update();
}

void MapWidget::setRotor2SecondAntenna(bool enabled, double offsetDeg)
{
    m_rotor2SecondEnabled = enabled;
    m_rotor2SecondOffsetDeg = offsetDeg;
    syncControls();
    update();
}

void MapWidget::setRotor1BeamwidthDeg(double degrees)
{
    const double clamped = std::clamp(degrees, kMinBeamwidthDeg, kMaxBeamwidthDeg);
    if (qFuzzyCompare(m_beamwidth1Deg, clamped)) {
        syncControls();
        return;
    }
    m_beamwidth1Deg = clamped;
    notePreferenceChange();
}

void MapWidget::setRotor2BeamwidthDeg(double degrees)
{
    const double clamped = std::clamp(degrees, kMinBeamwidthDeg, kMaxBeamwidthDeg);
    if (qFuzzyCompare(m_beamwidth2Deg, clamped)) {
        syncControls();
        return;
    }
    m_beamwidth2Deg = clamped;
    notePreferenceChange();
}

void MapWidget::setScoreSummary(int qsos, qint64 points, const QString& odxText)
{
    m_scoreQsos = qsos;
    m_scorePoints = points;
    m_scoreOdx = odxText;
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
    // Der Schritt der Stufe, in die es hineingeht, nicht der, aus der
    // es kommt -- sonst springt ein Klick bei 3 200 km um 1 000 km.
    setVisibleRangeKm(m_visibleRangeKm - zoomStepKm(m_visibleRangeKm - 1.0));
}

void MapWidget::zoomOut()
{
    setVisibleRangeKm(m_visibleRangeKm + zoomStepKm(m_visibleRangeKm));
}

QString MapWidget::preferencesText() const
{
    const auto flag = [](bool on) { return on ? QStringLiteral("1") : QStringLiteral("0"); };
    // The layer keys keep their "r" prefix from the days of two views,
    // so a stored preference string still reads the same.
    return QStringLiteral("rings=%1;spokes=%2;aging=%3;fit=%4;rotor1=%5;rotor2=%6;horizon=%7;bw1=%8;bw2=%9;"
                          "rgrid=%10;rcells=%11;rborders=%12;rcities=%13;greyline=%14")
        .arg(flag(m_showRings), flag(m_showSpokes), flag(m_showAging), flag(m_fitToWindow), flag(m_showRotor1Heading),
             flag(m_showRotor2Heading), flag(m_showHorizon))
        .arg(m_beamwidth1Deg, 0, 'f', 0).arg(m_beamwidth2Deg, 0, 'f', 0)
        .arg(flag(m_layers.grid), flag(m_layers.cells), flag(m_layers.borders), flag(m_layers.cities),
             flag(m_layers.greyline));
}

void MapWidget::applyPreferencesText(const QString& text)
{
    const QStringList parts = text.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    bool changed = false;
    for (const QString& part : parts) {
        const int eq = part.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        const QString key = part.left(eq).trimmed();
        const QString value = part.mid(eq + 1).trimmed();
        const bool on = value == QStringLiteral("1");
        const auto apply = [&](bool& member) {
            if (member != on) {
                member = on;
                changed = true;
            }
        };
        // "view", "grid", "cells", "borders", "cities" were the second
        // view's keys; a stored string may still carry them -- ignored.
        if (key == QStringLiteral("rings")) {
            apply(m_showRings);
        } else if (key == QStringLiteral("spokes")) {
            apply(m_showSpokes);
        } else if (key == QStringLiteral("rgrid")) {
            apply(m_layers.grid);
        } else if (key == QStringLiteral("rcells")) {
            apply(m_layers.cells);
        } else if (key == QStringLiteral("rborders")) {
            apply(m_layers.borders);
        } else if (key == QStringLiteral("rcities")) {
            apply(m_layers.cities);
        } else if (key == QStringLiteral("greyline")) {
            apply(m_layers.greyline);
            syncGreylineTimer();
        } else if (key == QStringLiteral("aging")) {
            if (m_showAging != on) {
                changed = true;
            }
            m_showAging = on;
            if (m_agingRefreshTimer) {
                if (on) {
                    m_agingRefreshTimer->start();
                } else {
                    m_agingRefreshTimer->stop();
                }
            }
        } else if (key == QStringLiteral("fit")) {
            apply(m_fitToWindow);
        } else if (key == QStringLiteral("rotor1")) {
            apply(m_showRotor1Heading);
        } else if (key == QStringLiteral("rotor2")) {
            apply(m_showRotor2Heading);
        } else if (key == QStringLiteral("horizon")) {
            apply(m_showHorizon);
        } else if (key == QStringLiteral("bw1") || key == QStringLiteral("bw2")) {
            bool ok = false;
            const double deg = value.toDouble(&ok);
            if (ok) {
                double& member = key == QStringLiteral("bw1") ? m_beamwidth1Deg : m_beamwidth2Deg;
                const double clamped = std::clamp(deg, kMinBeamwidthDeg, kMaxBeamwidthDeg);
                if (!qFuzzyCompare(member, clamped)) {
                    member = clamped;
                    changed = true;
                }
            }
        }
    }
    syncControls();
    if (changed) {
        update();
    }
}

// --- pure helpers ----------------------------------------------------------

QPointF MapWidget::projectBearingDistance(double bearingDeg, double distanceKm, double visibleRangeKm,
                                           const QRectF& mapRect)
{
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
    constexpr double kHalfLonDeg = 1.0;
    constexpr double kHalfLatDeg = 0.5;
    const double lonOffsets[4] = {-kHalfLonDeg, kHalfLonDeg, kHalfLonDeg, -kHalfLonDeg};
    const double latOffsets[4] = {-kHalfLatDeg, -kHalfLatDeg, kHalfLatDeg, kHalfLatDeg};
    QVector<QPointF> corners;
    corners.reserve(4);
    for (int i = 0; i < 4; ++i) {
        const double cornerLat = centerLat + latOffsets[i];
        const double cornerLon = centerLon + lonOffsets[i];
        const double bearing = calculateBearingInDegreesBetween(homeLat, homeLon, cornerLat, cornerLon);
        const double distance = calculateDistanceKmBetween(homeLat, homeLon, cornerLat, cornerLon);
        corners.append(projectBearingDistance(bearing, distance, visibleRangeKm, mapRect));
    }
    return corners;
}

QColor MapWidget::markerColor(bool worked)
{
    // Green = worked, blue = spotted/open (blue is what can be clicked);
    // red stays reserved for warnings.
    return worked ? QColor(Style::kGreenText()) : QColor(Style::kBlueBg());
}

QColor MapWidget::gridLabelColor(bool worked)
{
    return worked ? QColor(Style::kAmberText()) : QColor(Style::kBlueBg());
}

QColor MapWidget::agedMarkerColor(const QColor& base, qint64 secondsSinceWorked)
{
    if (secondsSinceWorked <= 0) {
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
    const auto lerp = [t](int fromChannel, int toChannel) {
        return fromChannel + static_cast<int>((toChannel - fromChannel) * t);
    };
    QColor result(lerp(base.red(), faded.red()), lerp(base.green(), faded.green()), lerp(base.blue(), faded.blue()));
    result.setAlpha(base.alpha());
    return result;
}

QSize MapWidget::minimumSizeHint() const
{
    return QSize(300, kControlsRowHeight + 220 + 2 * kCanvasMargin);
}

QSize MapWidget::sizeHint() const
{
    return QSize(340, kControlsRowHeight + 300 + 2 * kCanvasMargin);
}

// --- geometry --------------------------------------------------------------

QRectF MapWidget::canvasRect() const
{
    const int controlsBottom = m_controlsRow ? m_controlsRow->geometry().bottom() + 1 : kControlsRowHeight;
    const int top = std::max(controlsBottom, kControlsRowHeight) + kCanvasMargin;
    const int bottom = height() - kCanvasMargin;
    return QRectF(kCanvasMargin, top, std::max(0, width() - 2 * kCanvasMargin), std::max(0, bottom - top));
}

QRectF MapWidget::numbersRect() const
{
    const QRectF canvas = canvasRect();
    if (canvas.width() - kNumbersColumnWidth < kMinScopeWidthWithNumbers) {
        return QRectF();
    }
    return QRectF(canvas.right() - kNumbersColumnWidth + 12, canvas.top(), kNumbersColumnWidth - 12, canvas.height());
}

QRectF MapWidget::scopeRect() const
{
    QRectF area = canvasRect();
    const QRectF numbers = numbersRect();
    if (!numbers.isEmpty()) {
        area.setRight(numbers.left() - 8.0);
    }
    // The rim labels and the beam labels outside them need air.
    area.adjust(32.0, 32.0, -32.0, -32.0);
    if (area.width() <= 0.0 || area.height() <= 0.0) {
        return QRectF();
    }
    if (m_fitToWindow) {
        return area;
    }
    const double side = std::min(area.width(), area.height());
    return QRectF(area.center().x() - side / 2.0, area.center().y() - side / 2.0, side, side);
}

bool MapWidget::hasHorizon() const
{
    return m_horizonProfile.size() == 360 || m_terrainSectors.size() == 360;
}

double MapWidget::horizonAngleAt(int bearingDeg) const
{
    const int deg = ((bearingDeg % 360) + 360) % 360;
    if (m_horizonProfile.size() == 360) {
        return std::clamp(m_horizonProfile.at(deg), 0.0, kStripMaxElevationDeg);
    }
    if (m_terrainSectors.size() == 360) {
        switch (m_terrainSectors.at(deg)) {
        case LineOfSightClass::Blocked: return 6.0;
        case LineOfSightClass::Marginal: return 3.0;
        default: return 0.0;
        }
    }
    return 0.0;
}

QVector<MapWidget::Plotted> MapWidget::plotStations(const QRectF& area) const
{
    QVector<Plotted> out;
    if (!isValidGridSquare(m_ownGrid) || area.isEmpty()) {
        return out;
    }
    out.reserve(m_stations.size());
    for (const Station& station : m_stations) {
        if (!isValidGridSquare(station.grid)) {
            continue;
        }
        Plotted p;
        p.station = &station;
        p.bearingDeg = calculateBearingInDegrees(m_ownGrid, station.grid);
        p.distanceKm = calculateDistanceKm(m_ownGrid, station.grid);
        p.point = projectBearingDistance(p.bearingDeg, p.distanceKm, m_visibleRangeKm, area);
        out.append(p);
    }
    return out;
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

// --- painting --------------------------------------------------------------

void MapWidget::drawScopeFace(QPainter& painter, const QRectF& area) const
{
    // Sunk, black, a touch lighter at the centre -- the instrument face
    // the dots sit on; the ellipse is the scope's rim.
    QRadialGradient face(area.center(), std::max(area.width(), area.height()) / 2.0);
    face.setColorAt(0.0, QColor(Style::shiftL(QColor(Style::kInsetBg()), 4)));
    face.setColorAt(1.0, QColor(Style::kInsetBg()));
    painter.setPen(QPen(QColor(Style::kBorder()), 1.0));
    painter.setBrush(face);
    painter.drawEllipse(area);
}

void MapWidget::drawBordersLayer(QPainter& painter, const QRectF& area) const
{
    if (m_countryBorders.isEmpty() || !isValidGridSquare(m_ownGrid)) {
        return;
    }
    double homeLat = 0.0;
    double homeLon = 0.0;
    calculateLatLonFromGridSquare(m_ownGrid, homeLat, homeLon);
    // Quiet lines, no land fill, no country names: orientation, not a
    // school atlas. The radar dims them further.
    // Quieter still on the radar, where the scope is the point.
    // Je weiter die Karte reicht, desto kleiner und blasser werden die
    // Umrisse -- auf der Weltkarte blieben von den Küstenlinien bei
    // Alpha 90 nur Andeutungen übrig. Im Nahbereich bleibt es bei der
    // Zurückhaltung von 2026-09-12 ("Orientierung, kein Schulatlas").
    QColor line{Style::kTextInactive()};
    const double reach = std::clamp((m_visibleRangeKm - 1000.0) / (20000.0 - 1000.0), 0.0, 1.0);
    line.setAlpha(static_cast<int>(std::lround(90.0 + reach * 70.0)));
    painter.setPen(QPen(line, 1.0));
    painter.setBrush(Qt::NoBrush);
    const QRectF keep = area.adjusted(-2000, -2000, 2000, 2000);
    for (const CountryBorderRing& ring : m_countryBorders) {
        QPainterPath path;
        bool first = true;
        for (const QPointF& lonLat : ring.points) {
            const double bearing = calculateBearingInDegreesBetween(homeLat, homeLon, lonLat.y(), lonLat.x());
            const double distance = calculateDistanceKmBetween(homeLat, homeLon, lonLat.y(), lonLat.x());
            const QPointF p = projectBearingDistance(bearing, distance, m_visibleRangeKm, area);
            if (!keep.contains(p)) {
                first = true;
                continue;
            }
            if (first) {
                path.moveTo(p);
                first = false;
            } else {
                path.lineTo(p);
            }
        }
        painter.drawPath(path);
    }
}

// Die Dämmerungszone als Band, nicht als ausgemalte Nachtseite: die
// Karte ist dunkel, eine halb zugedeckte Scheibe würde mit den
// anderen Schichten streiten. Dazu die Sonne als kleiner Ring, damit
// zu sehen ist, welche Seite Tag ist.
//
// Gezeichnet wird der Kreis mit 10 008 km Abstand um den GEGENPUNKT
// der Sonne -- in dieser Darstellung (Richtung und Entfernung vom
// eigenen Standort) ist das kein Kreis mehr, sondern ein Vieleck aus
// 360 gerechneten Punkten. Wo es den Gegenpunkt des eigenen Standorts
// streift, läuft die Linie über den Rand der Scheibe; dort wird sie
// abgesetzt statt quer durchs Bild gezogen.
void MapWidget::drawGreylineLayer(QPainter& painter, const QRectF& area) const
{
    if (!isValidGridSquare(m_ownGrid)) {
        return;
    }
    double homeLat = 0.0;
    double homeLon = 0.0;
    calculateLatLonFromGridSquare(m_ownGrid, homeLat, homeLon);

    const SolarPoint sun = subsolarPoint(QDateTime::currentDateTimeUtc());
    const double antiLat = -sun.latitudeDeg;
    const double antiLon = sun.longitudeDeg > 0.0 ? sun.longitudeDeg - 180.0 : sun.longitudeDeg + 180.0;

    const auto plot = [&](double lat, double lon, bool& nearRim) {
        const double bearing = calculateBearingInDegreesBetween(homeLat, homeLon, lat, lon);
        const double distance = calculateDistanceKmBetween(homeLat, homeLon, lat, lon);
        nearRim = distance > 0.97 * m_visibleRangeKm;
        return projectBearingDistance(bearing, distance, m_visibleRangeKm, area);
    };

    const auto ringPath = [&](double radiusKm) {
        QPainterPath path;
        bool started = false;
        bool previousNearRim = false;
        QPointF previous;
        for (int azimuth = 0; azimuth <= 360; ++azimuth) {
            double lat = 0.0;
            double lon = 0.0;
            destinationPoint(antiLat, antiLon, azimuth % 360, radiusKm, lat, lon);
            bool nearRim = false;
            const QPointF point = plot(lat, lon, nearRim);
            const bool jump = started
                && QLineF(previous, point).length() > area.width() / 4.0
                && (nearRim || previousNearRim);
            if (!started || jump) {
                path.moveTo(point);
                started = true;
            } else {
                path.lineTo(point);
            }
            previous = point;
            previousNearRim = nearRim;
        }
        return path;
    };

    painter.setBrush(Qt::NoBrush);
    // Das Band zuerst, breit und leise; die Linie selbst darüber.
    const double radius = terminatorRadiusKm();
    const double twilight = radius * kCivilTwilightDeg / 90.0;
    QColor band{Style::kTextSecondary()};
    band.setAlpha(45);
    painter.setPen(QPen(band, 1.0, Qt::DotLine));
    painter.drawPath(ringPath(radius - twilight));
    painter.drawPath(ringPath(radius + twilight));
    QColor line{Style::kTextPrimary()};
    line.setAlpha(110);
    painter.setPen(QPen(line, 1.6));
    painter.drawPath(ringPath(radius));

    // Die Sonne, wenn sie im Bild liegt: ein Ring in Bernstein, kein
    // ausgefüllter Punkt -- ausgefüllte Punkte sind hier Stationen.
    bool sunNearRim = false;
    const double sunDistance = calculateDistanceKmBetween(homeLat, homeLon, sun.latitudeDeg, sun.longitudeDeg);
    if (sunDistance <= m_visibleRangeKm) {
        const QPointF point = plot(sun.latitudeDeg, sun.longitudeDeg, sunNearRim);
        QColor sunColor{Style::kAmberText()};
        sunColor.setAlpha(150);
        painter.setPen(QPen(sunColor, 1.4));
        painter.drawEllipse(point, 5.0, 5.0);
        painter.drawEllipse(point, 9.0, 9.0);
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
    const QFont nameFont = Style::monoFont(font(), Style::kFontCaption);
    const QColor color{Style::kTextInactive()};
    painter.setFont(nameFont);
    for (const CityPoint& city : m_cities) {
        const double bearing = calculateBearingInDegreesBetween(homeLat, homeLon, city.lat, city.lon);
        const double distance = calculateDistanceKmBetween(homeLat, homeLon, city.lat, city.lon);
        if (distance < kCityMinDistanceKm || distance > m_visibleRangeKm) {
            continue;
        }
        const QPointF point = projectBearingDistance(bearing, distance, m_visibleRangeKm, area);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(point, city.isCapital ? 2.0 : 1.5, city.isCapital ? 2.0 : 1.5);
        painter.setPen(color);
        painter.drawText(QRectF(point.x() + 5, point.y() - 7, 100, 14), Qt::AlignLeft | Qt::AlignVCenter, city.name);
    }
}

void MapWidget::drawGridLayer(QPainter& painter, const QRectF& area) const
{
    const QVector<GridCell> cells = computeGridCells(area);
    const QFont labelFont = Style::capsFont(font(), Style::kFontCaption);
    for (const GridCell& cell : cells) {
        if (cell.corners.size() != 4) {
            continue;
        }
        const QPolygonF polygon(cell.corners);
        QColor fill(Qt::transparent);
        QColor stroke{Style::kBorderSubtle()};
        qreal strokeWidth = 0.6;
        // Tints, not blocks: the squares are a background hint, the
        // dots on top are the information.
        if (m_layers.cells && cell.hasWorked) {
            fill = QColor(Style::kGreenText());
            fill.setAlpha(34);
            stroke = QColor(Style::kGreenBorder());
            strokeWidth = 0.9;
            if (m_showAging && cell.freshestWorkedAtUtc.isValid()) {
                const qint64 secs = cell.freshestWorkedAtUtc.secsTo(QDateTime::currentDateTimeUtc());
                fill = agedMarkerColor(fill, secs);
                stroke = agedMarkerColor(stroke, secs);
            }
        } else if (m_layers.cells && cell.hasSpotted) {
            fill = QColor(Style::kBlueBg());
            fill.setAlpha(28);
            stroke = QColor(Style::kBlueBorder());
            stroke.setAlpha(160);
            strokeWidth = 0.9;
        }
        painter.setBrush(fill);
        QPen pen(stroke);
        pen.setWidthF(strokeWidth);
        painter.setPen(pen);
        painter.drawPolygon(polygon);
        const QPointF labelPoint = polygon.boundingRect().center();
        painter.setFont(labelFont);
        painter.setPen(cell.isHome ? QColor(Style::kAmberDim()) : QColor(Style::kTextInactive()));
        painter.drawText(QRectF(labelPoint.x() - 20, labelPoint.y() - 8, 40, 16), Qt::AlignCenter, cell.code);
    }
}

void MapWidget::drawRingsLayer(QPainter& painter, const QRectF& area) const
{
    const QPointF center = area.center();
    const double halfWidth = area.width() / 2.0;
    const double halfHeight = area.height() / 2.0;
    painter.setBrush(Qt::NoBrush);
    painter.setFont(Style::monoFont(font(), Style::kFontCaption));
    // Bis 3 200 km unverändert (50 km im Nahbereich, sonst 100). Darüber
    // wären 100-km-Ringe bei 20 000 km zweihundert Kreise.
    double step = 100.0;
    if (m_visibleRangeKm <= 300.0) {
        step = 50.0;
    } else if (m_visibleRangeKm > 12000.0) {
        step = 2500.0;
    } else if (m_visibleRangeKm > 3200.0) {
        step = 1000.0;
    }
    const double majorEvery = step <= 100.0 ? 200.0 : step * 5.0;
    for (double km = step; km < m_visibleRangeKm - 0.5; km += step) {
        const double frac = km / m_visibleRangeKm;
        const bool major = std::fmod(km, majorEvery) < 0.5;
        QColor color{Style::kBorder()};
        color.setAlpha(major ? 255 : 150);
        QPen pen(color, 1.0, major ? Qt::SolidLine : Qt::DotLine);
        painter.setPen(pen);
        painter.drawEllipse(center, frac * halfWidth, frac * halfHeight);
        painter.setPen(QColor(Style::kTextInactive()));
        painter.drawText(polar(center, 135.0, frac * halfWidth, frac * halfHeight) + QPointF(3.0, 10.0),
                         QString::number(static_cast<int>(km)));
    }
}

void MapWidget::drawSpokesLayer(QPainter& painter, const QRectF& area) const
{
    const QPointF center = area.center();
    const double rx = area.width() / 2.0;
    const double ry = area.height() / 2.0;
    // Ticks on the rim every 10°, labels every 30°.
    for (int deg = 0; deg < 360; deg += 10) {
        const bool major = deg % 30 == 0;
        QColor tick{Style::kTextScale()};
        tick.setAlpha(major ? 200 : 110);
        painter.setPen(QPen(tick, major ? 1.2 : 0.8));
        const double inset = major ? 7.0 : 4.0;
        painter.drawLine(polar(center, deg, rx - inset, ry - inset * ry / std::max(1.0, rx)), polar(center, deg, rx, ry));
        if (major) {
            painter.setFont(Style::capsFont(font(), Style::kFontCaption));
            painter.setPen(QColor(Style::kTextScale()));
            const QPointF p = polar(center, deg, rx + 11.0, ry + 11.0);
            painter.drawText(QRectF(p.x() - 14, p.y() - 6, 28, 12), Qt::AlignCenter, compassLabel(deg));
        }
    }
}

void MapWidget::drawHorizonRim(QPainter& painter, const QRectF& area) const
{
    if (!hasHorizon()) {
        return;
    }
    const QPointF center = area.center();
    const double rx = area.width() / 2.0;
    const double ry = area.height() / 2.0;
    QPainterPath inner;
    for (int deg = 0; deg <= 360; ++deg) {
        const double frac = horizonAngleAt(deg) / kStripMaxElevationDeg;
        const QPointF p = polar(center, deg, rx - frac * kRimMaxThicknessPx, ry - frac * kRimMaxThicknessPx * ry / std::max(1.0, rx));
        if (deg == 0) {
            inner.moveTo(p);
        } else {
            inner.lineTo(p);
        }
    }
    QPainterPath rim;
    rim.addEllipse(area);
    painter.setPen(Qt::NoPen);
    painter.setBrush(withAlpha(Style::kAmberDim(), 110));
    painter.drawPath(rim.subtracted(inner));
    painter.setPen(QPen(withAlpha(Style::kAmberWarn(), 150), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(inner);
}

QVector<MapWidget::Beam> MapWidget::beams() const
{
    // Rotor 1's main antenna in amber, its second antenna in the darker
    // amber (same rotor, other direction), rotor 2 in a pale grey so the
    // two rotors never read as one; each cone as wide as its beamwidth.
    QVector<Beam> out;
    const auto degrees = [](double az) { return QStringLiteral("%1°").arg(wrap360(az), 0, 'f', 0); };
    if (m_showRotor1Heading && m_rotor1Connected) {
        Beam main;
        main.azimuthDeg = wrap360(m_rotor1AzimuthDeg);
        main.halfWidthDeg = m_beamwidth1Deg / 2.0;
        main.color = QColor(Style::kAmberText());
        main.label = degrees(m_rotor1AzimuthDeg);
        main.labelPx = Style::kFontSmall;
        if (!m_rotor1Live) {
            // Kein Draht zum Rotor: die Richtung ist das, was der
            // Steckplatz verfolgt, keine gemessene Peilung.
            main.color.setAlpha(110);
            main.lineStyle = Qt::DashLine;
        }
        out.append(main);
        if (m_rotor1SecondEnabled) {
            Beam second = main;
            second.azimuthDeg = wrap360(m_rotor1AzimuthDeg + m_rotor1SecondOffsetDeg);
            second.color = QColor(Style::kAmberWarn());
            second.lineStyle = Qt::DashLine;
            second.label = degrees(second.azimuthDeg);
            second.labelPx = Style::kFontCaption;
            out.append(second);
        }
    }
    if (m_showRotor2Heading && m_rotor2Connected) {
        Beam main;
        main.azimuthDeg = wrap360(m_rotor2AzimuthDeg);
        main.halfWidthDeg = m_beamwidth2Deg / 2.0;
        main.color = QColor(Style::kTextSecondary());
        main.lineStyle = Qt::DashLine;
        if (!m_rotor2Live) {
            main.color.setAlpha(110);
        }
        main.label = (m_rotor2Label.isEmpty() ? QString() : m_rotor2Label + QLatin1Char(' ')) + degrees(m_rotor2AzimuthDeg);
        main.labelPx = Style::kFontCaption;
        out.append(main);
        if (m_rotor2SecondEnabled) {
            Beam second = main;
            second.azimuthDeg = wrap360(m_rotor2AzimuthDeg + m_rotor2SecondOffsetDeg);
            second.color = QColor(Style::kTextTertiary());
            second.label = degrees(second.azimuthDeg);
            out.append(second);
        }
    }
    return out;
}

void MapWidget::drawBeam(QPainter& painter, const QRectF& area, const Beam& beam, bool labelsOnly,
                         QVector<QRectF>* takenLabels) const
{
    const QPointF center = area.center();
    const double rx = area.width() / 2.0;
    const double ry = area.height() / 2.0;
    if (!labelsOnly) {
        QPainterPath wedge;
        wedge.moveTo(center);
        wedge.arcTo(area, 90.0 - (beam.azimuthDeg - beam.halfWidthDeg), -2.0 * beam.halfWidthDeg);
        wedge.closeSubpath();
        QColor inner = beam.color;
        inner.setAlpha(60);
        QColor outer = beam.color;
        outer.setAlpha(0);
        QRadialGradient grad(center, std::max(rx, ry));
        grad.setColorAt(0.0, inner);
        grad.setColorAt(1.0, outer);
        painter.setPen(Qt::NoPen);
        painter.setBrush(grad);
        painter.drawPath(wedge);
        QColor line = beam.color;
        line.setAlpha(200);
        painter.setPen(QPen(line, 1.2, beam.lineStyle));
        painter.drawLine(center, polar(center, beam.azimuthDeg, rx, ry));
        return;
    }
    if (beam.label.isEmpty()) {
        return;
    }
    // On a dark backing (it may sit on a compass label), and pushed
    // one step further out for every earlier label it would cover --
    // two antennas at the same heading read as two lines, not a blot.
    painter.setFont(Style::monoFont(font(), beam.labelPx, QFont::DemiBold));
    const QFontMetricsF fm(painter.font());
    const double w = fm.horizontalAdvance(beam.label) + 6.0;
    const double h = fm.height() + 2.0;
    QRectF box;
    // First outside the rim; a second antenna on the same heading goes
    // just inside it, a third further in still.
    for (int step = 0; step < 4; ++step) {
        const double out = step == 0 ? 26.0 : -(18.0 + (step - 1) * (h + 4.0));
        const QPointF tip = polar(center, beam.azimuthDeg, rx + out, ry + out);
        box = QRectF(tip.x() - w / 2.0, tip.y() - h / 2.0, w, h);
        bool clash = false;
        if (takenLabels) {
            for (const QRectF& other : *takenLabels) {
                clash = clash || other.adjusted(-6.0, -4.0, 6.0, 4.0).intersects(box);
            }
        }
        if (!clash) {
            break;
        }
    }
    if (takenLabels) {
        takenLabels->append(box);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(withAlpha(Style::kPanelBg(), 220));
    painter.drawRoundedRect(box, 3.0, 3.0);
    painter.setPen(beam.color);
    painter.drawText(box, Qt::AlignCenter, beam.label);
}

void MapWidget::drawRotorHeadingsLayer(QPainter& painter, const QRectF& area) const
{
    // Cones and lines inside the rim; the labels are painted by the
    // caller's second, outside-the-rim pass (see paintEvent).
    for (const Beam& beam : beams()) {
        drawBeam(painter, area, beam, false);
    }
}

void MapWidget::drawStations(QPainter& painter, const QRectF& area) const
{
    const QVector<Plotted> plotted = plotStations(area);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    LabelPlacer labels;
    painter.setFont(Style::monoFont(font(), Style::kFontCaption));
    // Worked dots first, open rings and their labels on top.
    for (int pass = 0; pass < 2; ++pass) {
        for (const Plotted& p : plotted) {
            const Station& station = *p.station;
            if ((pass == 0) != station.worked) {
                continue;
            }
            if (p.distanceKm > m_visibleRangeKm) {
                continue;
            }
            QColor color = markerColor(station.worked);
            const qint64 ageSecs = station.workedAtUtc.isValid() ? station.workedAtUtc.secsTo(now) : -1;
            if (m_showAging && station.worked && ageSecs >= 0) {
                color = agedMarkerColor(color, ageSecs);
            }
            if (station.worked) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                painter.drawEllipse(p.point, station.approximate ? 2.0 : 3.0, station.approximate ? 2.0 : 3.0);
                const bool fresh = ageSecs >= 0 && ageSecs < kFreshLabelMinutes * 60;
                if (fresh) {
                    labels.place(painter, p.point, station.callsign, QColor(Style::kTextTertiary()));
                }
            } else {
                painter.setPen(QPen(color, 1.5));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(p.point, 4.0, 4.0);
                labels.place(painter, p.point, station.callsign, QColor(Style::kTextPrimary()));
            }
            // Nur der Mittelpunkt eines Landes, kein getauschter
            // Locator: ein gepunkteter Hof sagt, dass die Station
            // irgendwo dort drin sitzt und nicht genau da.
            if (station.approximate) {
                QColor halo = color;
                halo.setAlpha(120);
                painter.setPen(QPen(halo, 1.0, Qt::DotLine));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(p.point, 7.0, 7.0);
            }
        }
    }
}

void MapWidget::drawHomeMarker(QPainter& painter, const QRectF& area) const
{
    if (!isValidGridSquare(m_ownGrid)) {
        return;
    }
    const QPointF center = area.center();
    const QColor amber{Style::kAmberText()};
    painter.setPen(Qt::NoPen);
    painter.setBrush(amber);
    painter.drawEllipse(center, 3.5, 3.5);
    painter.setPen(QPen(withAlpha(Style::kAmberText(), 120), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, 7.0, 7.0);
}

void MapWidget::drawNumbersColumn(QPainter& painter, const QRectF& column) const
{
    const double x = column.left();
    double y = column.top() + 10.0;
    const auto caption = [&](const QString& text) {
        painter.setFont(Style::capsFont(font(), Style::kFontCaption));
        painter.setPen(QColor(Style::kTextScale()));
        painter.drawText(QPointF(x, y), text.toUpper());
    };
    const auto value = [&](const QString& text, const QColor& color, int px) {
        painter.setFont(Style::monoFont(font(), px, QFont::DemiBold));
        painter.setPen(color);
        painter.drawText(QPointF(x, y + px + 4), text);
        y += px + 26;
    };

    // Beam: rotor 1's heading, its second antenna's after a dot.
    caption(m_rotor1Label.isEmpty() ? QStringLiteral("Beam · Rotor 1") : QStringLiteral("Beam · Rotor 1 · %1").arg(m_rotor1Label));
    QString beamText = Style::unknownDash();
    if (m_rotor1Connected) {
        beamText = QStringLiteral("%1°").arg(wrap360(m_rotor1AzimuthDeg), 0, 'f', 0);
        if (!m_rotor1Live) {
            beamText += QStringLiteral(" · getrennt");
        }
        if (m_rotor1SecondEnabled) {
            beamText += QStringLiteral(" · %1°").arg(wrap360(m_rotor1AzimuthDeg + m_rotor1SecondOffsetDeg), 0, 'f', 0);
        }
    }
    value(beamText, m_rotor1Live ? QColor(Style::kAmberText()) : QColor(Style::kTextInactive()),
          Style::kFontReading);
    if (m_rotor2Connected && m_showRotor2Heading) {
        caption(m_rotor2Label.isEmpty() ? QStringLiteral("Rotor 2") : QStringLiteral("Rotor 2 · %1").arg(m_rotor2Label));
        QString text = QStringLiteral("%1°").arg(wrap360(m_rotor2AzimuthDeg), 0, 'f', 0);
        if (m_rotor2SecondEnabled) {
            text += QStringLiteral(" · %1°").arg(wrap360(m_rotor2AzimuthDeg + m_rotor2SecondOffsetDeg), 0, 'f', 0);
        }
        if (!m_rotor2Live) {
            text += QStringLiteral(" · getrennt");
        }
        value(text, m_rotor2Live ? QColor(Style::kTextSecondary()) : QColor(Style::kTextInactive()),
              Style::kFontBody);
    }

    // Open stations inside any of rotor 1's cones -- blue, because a
    // click on it works them: each click hands the next one (farthest
    // first) to the log as a candidate, like a click on its marker.
    caption(QStringLiteral("Offen in Richtung"));
    QString openText = Style::unknownDash();
    m_openInBeamRect = QRectF();
    if (m_rotor1Connected) {
        const QVector<const Station*> open = openStationsInBeam();
        if (open.isEmpty()) {
            openText = QStringLiteral("0");
        } else {
            const Station* farthest = open.first();
            openText = QStringLiteral("%1 · %2 %3 km")
                           .arg(open.size())
                           .arg(farthest->callsign)
                           .arg(calculateDistanceKm(m_ownGrid, farthest->grid), 0, 'f', 0);
            m_openInBeamRect = QRectF(x - 4.0, y - 2.0, column.width() + 4.0, Style::kFontBody + 12.0);
        }
    }
    value(openText, QColor(Style::kBlueBg()), Style::kFontBody);

    // QSOs · points
    caption(QStringLiteral("QSOs · Punkte"));
    int qsos = m_scoreQsos;
    qint64 points = m_scorePoints;
    QString odx = m_scoreOdx;
    if (qsos < 0) {
        qsos = 0;
        points = 0;
        double bestKm = -1.0;
        for (const Station& station : m_stations) {
            if (!station.worked || !isValidGridSquare(station.grid) || !isValidGridSquare(m_ownGrid)) {
                continue;
            }
            ++qsos;
            const double km = calculateDistanceKm(m_ownGrid, station.grid);
            points += static_cast<qint64>(std::floor(km)) + 1;
            if (km > bestKm) {
                bestKm = km;
                odx = QStringLiteral("%1 %2 km").arg(station.callsign).arg(km, 0, 'f', 0);
            }
        }
    }
    value(QStringLiteral("%1 · %2").arg(qsos).arg(groupedKm(points)), QColor(Style::kTextPrimary()), Style::kFontSub);

    caption(QStringLiteral("ODX"));
    value(odx.isEmpty() ? Style::unknownDash() : odx, QColor(Style::kTextPrimary()), Style::kFontBody);

    // Wer die Graulinie eingeschaltet hat, will auch wissen, wann sie
    // über den eigenen Standort läuft -- sonst steht sie nur im Bild.
    if (m_layers.greyline && isValidGridSquare(m_ownGrid)) {
        double homeLat = 0.0;
        double homeLon = 0.0;
        calculateLatLonFromGridSquare(m_ownGrid, homeLat, homeLon);
        const SunTimes sun = sunTimes(QDateTime::currentDateTimeUtc(), homeLat, homeLon);
        QString sunText = Style::unknownDash();
        switch (sun.kind) {
        case SunTimes::Kind::AlwaysUp:
            sunText = QStringLiteral("geht nicht unter");
            break;
        case SunTimes::Kind::AlwaysDown:
            sunText = QStringLiteral("geht nicht auf");
            break;
        case SunTimes::Kind::RiseAndSet:
            sunText = QStringLiteral("%1–%2Z")
                          .arg(sun.riseUtc.toString(QStringLiteral("HH:mm")),
                               sun.setUtc.toString(QStringLiteral("HH:mm")));
            break;
        }
        caption(QStringLiteral("Sonne hier"));
        value(sunText, QColor(Style::kTextSecondary()), Style::kFontBody);
    }

    drawLegend(painter, QPointF(x, column.bottom() - 4.0));
}

void MapWidget::drawLegend(QPainter& painter, const QPointF& bottomLeft) const
{
    painter.setFont(Style::capsFont(font(), Style::kFontCaption));
    const QColor text{Style::kTextTertiary()};
    double y = bottomLeft.y();
    const double x = bottomLeft.x();
    if (hasHorizon() && m_showHorizon) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(withAlpha(Style::kAmberDim(), 160));
        painter.drawRect(QRectF(x, y - 9.0, 9.0, 8.0));
        painter.setPen(text);
        painter.drawText(QPointF(x + 14.0, y - 1.0), QStringLiteral("BERGE VERDECKEN"));
        y -= 16.0;
    }
    painter.setPen(QPen(markerColor(false), 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(x + 4.0, y - 5.0), 4.0, 4.0);
    painter.setPen(text);
    painter.drawText(QPointF(x + 14.0, y - 1.0), QStringLiteral("GESPOTTET · OFFEN"));
    y -= 16.0;
    painter.setPen(Qt::NoPen);
    painter.setBrush(markerColor(true));
    painter.drawEllipse(QPointF(x + 4.0, y - 5.0), 3.0, 3.0);
    painter.setPen(text);
    painter.drawText(QPointF(x + 14.0, y - 1.0), QStringLiteral("GEARBEITET"));
}

void MapWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    // The container draws the frame and header; this is the inside.
    painter.fillRect(rect(), QColor(Style::kPanelBg()));

    const QRectF area = scopeRect();
    if (area.width() <= 4.0 || area.height() <= 4.0 || !isValidGridSquare(m_ownGrid)) {
        painter.setFont(Style::capsFont(font()));
        painter.setPen(QColor(Style::kTextInactive()));
        painter.drawText(canvasRect(), Qt::AlignCenter,
                         isValidGridSquare(m_ownGrid) ? QStringLiteral("ZU KLEIN") : QStringLiteral("KEIN STANDORT-GRID KONFIGURIERT"));
        return;
    }

    // The scope/map disc, everything geographic clipped to it.
    drawScopeFace(painter, area);
    painter.save();
    QPainterPath clip;
    clip.addEllipse(area);
    painter.setClipPath(clip, Qt::IntersectClip);
    if (m_layers.borders) {
        drawBordersLayer(painter, area);
    }
    if (m_layers.greyline) {
        drawGreylineLayer(painter, area);
    }
    if (m_layers.grid) {
        drawGridLayer(painter, area);
    }
    if (m_showRings) {
        drawRingsLayer(painter, area);
    }
    if (m_layers.cities) {
        drawCitiesLayer(painter, area);
    }
    if (m_showHorizon) {
        drawHorizonRim(painter, area);
    }
    drawRotorHeadingsLayer(painter, area);
    drawStations(painter, area);
    drawHomeMarker(painter, area);
    painter.restore();
    if (m_showSpokes) {
        drawSpokesLayer(painter, area);
    } else {
        painter.setPen(QPen(QColor(Style::kBorder()), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(area);
    }
    // Beam labels: outside the rim (or just inside it when headings
    // coincide), on top of everything, clipped to the canvas only.
    painter.save();
    painter.setClipRect(canvasRect(), Qt::IntersectClip);
    QVector<QRectF> beamLabels;
    for (const Beam& beam : beams()) {
        drawBeam(painter, area, beam, true, &beamLabels);
    }
    painter.restore();

    const QRectF numbers = numbersRect();
    if (!numbers.isEmpty()) {
        drawNumbersColumn(painter, numbers);
    }
}

QVector<const MapWidget::Station*> MapWidget::openStationsInBeam() const
{
    QVector<const Station*> open;
    if (!m_rotor1Connected || !isValidGridSquare(m_ownGrid)) {
        return open;
    }
    QVector<double> directions{m_rotor1AzimuthDeg};
    if (m_rotor1SecondEnabled) {
        directions.append(m_rotor1AzimuthDeg + m_rotor1SecondOffsetDeg);
    }
    for (const Station& station : m_stations) {
        if (station.worked || !isValidGridSquare(station.grid)) {
            continue;
        }
        const double bearing = calculateBearingInDegrees(m_ownGrid, station.grid);
        bool inside = false;
        for (double direction : directions) {
            inside = inside || angularDistance(bearing, direction) <= m_beamwidth1Deg / 2.0;
        }
        if (inside) {
            open.append(&station);
        }
    }
    std::stable_sort(open.begin(), open.end(), [this](const Station* a, const Station* b) {
        return calculateDistanceKm(m_ownGrid, a->grid) > calculateDistanceKm(m_ownGrid, b->grid);
    });
    return open;
}

bool MapWidget::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        if (!m_openInBeamRect.isEmpty() && m_openInBeamRect.contains(help->pos())) {
            QToolTip::showText(help->globalPos(),
                               QStringLiteral("Klick: die nächste offene Station in Beamrichtung anfunken (weiteste zuerst)"),
                               this, m_openInBeamRect.toRect());
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(event);
}

void MapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !isValidGridSquare(m_ownGrid)) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QPointF clickPos = event->position();
    // "Offen in Richtung": the next open station in the beam.
    if (!m_openInBeamRect.isEmpty() && m_openInBeamRect.contains(clickPos)) {
        const QVector<const Station*> open = openStationsInBeam();
        if (!open.isEmpty()) {
            const Station* next = open.at(m_openInBeamCursor % open.size());
            m_openInBeamCursor = (m_openInBeamCursor + 1) % open.size();
            emit candidateActivated(next->callsign, next->grid, next->freqHz);
        }
        return;
    }
    const Station* hit = nullptr;
    double bestDistSq = kStationClickTolerancePx * kStationClickTolerancePx;
    const QRectF area = scopeRect();
    for (const Plotted& p : plotStations(area)) {
        const double dx = p.point.x() - clickPos.x();
        const double dy = p.point.y() - clickPos.y();
        const double distSq = dx * dx + dy * dy;
        if (distSq <= bestDistSq) {
            bestDistSq = distSq;
            hit = p.station;
        }
    }
    if (hit) {
        emit candidateActivated(hit->callsign, hit->grid, hit->freqHz);
        return;
    }
    QWidget::mousePressEvent(event);
}

} // namespace Contestprogramm
