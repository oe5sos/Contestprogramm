#pragma once

#include "core/Cities.h"
#include "core/CountryBorders.h"
#include "core/terrain/LineOfSight.h"

#include <QColor>
#include <QDateTime>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QLabel;
class QMouseEvent;
class QPainter;
class QPushButton;
class QTimer;

namespace Contestprogramm {

// Azimuthal, own-QTH-centred map instrument -- the plan's MapWidget
// ("Karte / Verbindungen"), with Map-A.dc.html as the definitive visual
// reference (see its class-comment-equivalent note in the design
// mockup). Mirrors RotorWidget's architecture: private draw*() helper
// methods called from one paintEvent(), StyleKit colours throughout,
// its own hand-painted panel header rather than a PanelHeaderBar child
// (same reasoning as RotorWidget -- this widget paints its own full
// panel chrome so it can sit directly in MainWindow's rotor row without
// an extra wrapInPanel() frame, feeling like a sibling instrument, not
// a differently-built panel next to two hand-painted compasses).
//
// The one deliberate structural departure from RotorWidget: this widget
// does carry a handful of real child QWidgets (four QCheckBox layer
// toggles + two QPushButton zoom buttons), laid out in a thin row
// beneath the painted header. That is not a break from the "hand-painted
// instrument" idea -- PanelHeaderBar already establishes the same
// pattern in this codebase (a painted background/accent bar with one
// real QLabel child laid out on top) -- and genuine QCheckBox/QPushButton
// widgets are what "real checkboxes/buttons... on the widget itself"
// (the plan's own wording for this requirement) means, versus
// hand-painted+manually-hit-tested fakes.
class MapWidget : public QWidget {
    Q_OBJECT

public:
    // One row worth of plottable data -- either a worked QSO (green) or
    // a spotted-but-unworked candidate (blue), per the plan's colour
    // convention (confirmed against N1MM+'s "Grid Square Map" window in
    // the project plan's UI section -- house colours swapped in for
    // N1MM's blue/red, since red stays reserved for warnings here).
    struct Station {
        QString callsign;
        QString grid;
        bool worked = false; // false == spotted/unworked candidate
        // 0 when not known -- passed straight through to
        // candidateActivated() on a click, the same "0 == unknown"
        // convention SpotCandidate::freqHz already uses, so
        // MainWindow::commandRotorForCandidate() gets a real value when
        // one exists and simply skips any frequency-dependent step
        // otherwise.
        qint64 freqHz = 0;
        // When this station was logged (worked == true only -- always
        // invalid/default for a spotted candidate, which has no "worked
        // at" moment yet). Invalid means "unknown", not "just now" --
        // agedMarkerColor()/drawStations()/computeGridCells() all treat
        // an invalid timestamp as "don't fade", the same "unbekannt
        // zeigt nichts falsches an" rule HAUSSTIL rule 7 applies
        // everywhere else in this codebase (assuming an unknown age is
        // recent would be a fabricated claim, not a neutral default).
        QDateTime workedAtUtc;
    };

    explicit MapWidget(QWidget* parent = nullptr);

    // ContestSettings::ownGrid -- the projection's pole (every bearing/
    // distance on this widget is measured from here). An empty/invalid
    // grid simply yields an empty map (drawHomeMarker/drawGridLayer/
    // drawStations all no-op without a usable home) rather than
    // guessing a location.
    void setOwnGrid(const QString& grid);
    QString ownGrid() const { return m_ownGrid; }
    // ContestSettings::ownCallsign, shown next to the home marker the
    // way Map-A.dc.html shows "JN67 · FEUERKOGEL" in its header.
    void setOwnLabel(const QString& label);

    // Replaces the full worked/spotted station set in one call.
    // MainWindow calls this after every QSO log and on every spot-feed
    // change (see MainWindow::refreshMapWidget) -- a full replace rather
    // than an incremental add/remove API, since contest-scale station
    // counts (tens to low hundreds, not thousands) make a full
    // re-projection on every call a non-issue, the same call already
    // made for ChatFeedModel::addCandidate's full-rebuild reasoning.
    void setStations(const QVector<Station>& stations);
    const QVector<Station>& stations() const { return m_stations; }

    // Layer toggles, per the plan's "Options-Icon je Widget" /
    // "individuell anpassen" requirement -- backed by the real
    // QCheckBox row below the header, not a separate dialog.
    void setGridLayerVisible(bool visible);
    void setRingsLayerVisible(bool visible);
    void setSpokesLayerVisible(bool visible);
    bool gridLayerVisible() const { return m_showGrid; }
    bool ringsLayerVisible() const { return m_showRings; }
    bool spokesLayerVisible() const { return m_showSpokes; }

    // Whether a worked/spotted/home grid square is tinted (green/blue/
    // amber fill+border, see drawGridLayer()) -- split out from the
    // plain "Grid" toggle above (operator, 2026-09-14: "die markierten
    // gearbeiteten grid sollen ein und aus zum blenden sein"), so the
    // grid LINES themselves and the worked/spotted MARKINGS on top of
    // them can be hidden independently. "Grid" off already implies no
    // tinted cells either (drawGridLayer() is skipped entirely), but
    // "Grid" on + this off now shows a plain, untinted grid.
    void setWorkedCellsLayerVisible(bool visible);
    bool workedCellsLayerVisible() const { return m_showWorkedCells; }

    // Country border/coastline outlines (operator, 2026-09-12: "kannst
    // du in die karte/verbindungen auch die umrisse der staaten
    // einzeichen, sodass ich diese ein uns ausblenden kann") -- see
    // core/CountryBorders.h/drawBordersLayer() below. Same real-
    // QCheckBox-toggle convention as the four layers above.
    void setBordersLayerVisible(bool visible);
    bool bordersLayerVisible() const { return m_showBorders; }

    // Major-city reference points (operator, 2026-09-12: "auch bitte
    // die wichtigsten großen städte ab 150 km") -- see
    // core/Cities.h/drawCitiesLayer()/kCityMinDistanceKm below. Same
    // real-QCheckBox-toggle convention as the layers above.
    void setCitiesLayerVisible(bool visible);
    bool citiesLayerVisible() const { return m_showCities; }

    // Fades a worked station's marker and grid-cell tint toward grey the
    // longer ago it was logged (operator, 2026-09-13: "alte Kontakte
    // ausgrauen" -- the plan's own backlog wording, "bereits gearbeitete
    // Quadrate/Verbindungen nach einer Weile auf Grau zurückstufen", so
    // current activity does not visually drown in an evening's worth of
    // old green). A styling mode for already-shown data, not a
    // separate layer -- same real-QCheckBox convention as the layers
    // above regardless ("Füllen" is the same kind of styling toggle,
    // not a data layer, and already lives in this same row). See
    // agedMarkerColor() for the actual fade curve.
    void setAgingEnabled(bool enabled);
    bool agingEnabled() const { return m_showAging; }

    // Terrain line-of-sight sectors around the map's own rim, one entry
    // per integer degree -- the same data (and the same "wash the rim
    // red/amber where Blocked/Marginal, do nothing otherwise, never a
    // restriction" contract) RotorWidget::setTerrainSectors() already
    // documents in full; repeating only what differs here. An empty
    // vector (the default) disables the wash entirely.
    void setTerrainSectors(const QVector<LineOfSightClass>& sectorsByDegree);

    // Current heading of each rotor, drawn as a bold spoke from home to
    // the rim (see drawRotorHeadingsLayer()) -- operator, 2026-09-14,
    // replacing the removed "Links" (station connection-line) toggle:
    // "option rotorenrichtung wäre sinnvoller". `label` is the rotor's
    // own band label (RotorWidget::bandLabel(), e.g. "2m"/"70cm"),
    // printed at the spoke's rim end so two simultaneous headings stay
    // distinguishable. `connected` false (rotor slot disabled, or its
    // RotctldClient not connected) simply omits that one spoke, the same
    // "no data, draw nothing" contract every other live layer here uses.
    void setRotor1Heading(bool connected, double azimuthDeg, const QString& label);
    void setRotor2Heading(bool connected, double azimuthDeg, const QString& label);
    // Separately hideable per rotor -- operator, 2026-09-14: "rotor 1
    // und rotor 2 zum ein und ausblenden" (the single combined toggle
    // this replaces could not show one heading while hiding the other).
    void setRotor1HeadingLayerVisible(bool visible);
    void setRotor2HeadingLayerVisible(bool visible);
    bool rotor1HeadingLayerVisible() const { return m_showRotor1Heading; }
    bool rotor2HeadingLayerVisible() const { return m_showRotor2Heading; }

    // Whether the projection fills the whole panel (true, the default --
    // rings/grid squares scale independently in x and y, so a non-square
    // panel draws true ellipses) or stays locked to a centred circle
    // letterboxed into the panel's shorter dimension (false). Martin's
    // own call after seeing this panel resized to a wide, non-square
    // shape: "sollte sich vielleicht nach dem Fenster anpassen, auch
    // wenn die Kreise dann elliptisch werden -- diese [Anpassung] kann
    // man ja auch deaktivieren" -- fit-to-window by default, with the
    // "Füllen" checkbox below to opt back into fixed circles.
    void setFitToWindowEnabled(bool enabled);
    bool fitToWindowEnabled() const { return m_fitToWindow; }

    // Visible half-width of the map, in kilometres -- the outermost
    // distance ring's radius. Halved/doubled by the zoom buttons;
    // also settable directly so MainWindow can seed it from
    // ContestSettings::radiusKm.
    void setVisibleRangeKm(double rangeKm);
    double visibleRangeKm() const { return m_visibleRangeKm; }
    void zoomIn();
    void zoomOut();

    // bearing/distance -> screen point within `mapRect`, home at its
    // centre, 0 deg = up, increasing clockwise -- the exact convention
    // RotorWidget::pointOnCircle() already uses, so a bearing means the
    // same thing on both instruments. Exposed as a standalone static
    // (like RotorWidget::secondAntennaBearing()) so the projection
    // maths is directly unit-testable without a QPainter.
    static QPointF projectBearingDistance(double bearingDeg, double distanceKm, double visibleRangeKm,
                                           const QRectF& mapRect);

    // The four corners of one 4-character Maidenhead square, each
    // projected individually via its own great-circle bearing/distance
    // from (homeLat, homeLon) -- an azimuthal-equidistant projection,
    // not a uniform pixel-per-degree grid, so a square far from home
    // renders as a (correctly) skewed quadrilateral, not a rectangle.
    // `gridSquare` must be a valid 4- or 6-character locator (only the
    // first 4 characters are used -- a 4-character square's fixed
    // 2 deg-lon x 1 deg-lat size, per the Maidenhead spec Maidenhead.h
    // already encodes, is applied around calculateLatLonFromGridSquare()'s
    // returned centre rather than re-deriving the encoding here).
    static QVector<QPointF> projectGridSquareCorners(const QString& gridSquare, double homeLat, double homeLon,
                                                       double visibleRangeKm, const QRectF& mapRect);

    // The worked/spotted colour pair used for a station's marker/line
    // and its grid-code label -- exposed as standalone statics (same
    // testability rationale as projectBearingDistance() above) so the
    // categorisation rule is verifiable without constructing a widget.
    static QColor markerColor(bool worked);
    static QColor gridLabelColor(bool worked);

    // `base`'s fade toward Style::kTextInactive() as `secondsSinceWorked`
    // grows -- unchanged for the first kAgingStartMinutes, linearly
    // interpolated to fully grey by kAgingCompleteMinutes, clamped
    // beyond that. A standalone static (same testability rationale as
    // markerColor()/gridLabelColor() above) so the fade curve itself is
    // verifiable without constructing a widget or waiting real time.
    static QColor agedMarkerColor(const QColor& base, qint64 secondsSinceWorked);

    // Test seam -- same idea as On4kstClient's own *ForTest() methods:
    // exposes canvasRect() (otherwise a private implementation detail)
    // so a click-hit-test can target a station placed exactly at home
    // (which always projects to this rect's own centre -- see
    // projectBearingDistance()'s zero-distance case) without a test
    // needing to duplicate this widget's internal header/margin/legend
    // layout constants just to predict a screen point.
    QRectF canvasRectForTest() const { return canvasRect(); }

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    // A station marker (worked or spotted) was clicked -- same signal
    // shape (and the same MainWindow::handleCandidateActivated consumer,
    // which also commands the rotor when one is configured) ChatFeedView/
    // UnifiedLogWidget's own candidateActivated already uses, per the
    // plan's own "Turn Antenna"/N1MM+ precedent for this exact map.
    void candidateActivated(const QString& callsign, const QString& grid, qint64 freqHz);

protected:
    void paintEvent(QPaintEvent* event) override;
    // Hit-tests m_stations' own projected marker points (same projection
    // paintEvent()'s drawStations() uses) against the click position,
    // picking the closest marker within a small tolerance radius --
    // generous enough for an easy click without requiring pixel-perfect
    // accuracy on a small marker. Left button only; emits
    // candidateActivated() on a hit, otherwise a no-op (clicking empty
    // map area does nothing, same as clicking empty space in
    // ChatFeedView's own table does nothing).
    void mousePressEvent(QMouseEvent* event) override;

private:
    struct GridCell {
        QString code;
        QVector<QPointF> corners; // always 4 points, screen space
        bool isHome = false;
        bool hasWorked = false;
        bool hasSpotted = false;
        // The most recent workedAtUtc among every worked station this
        // cell matched -- one recent contact keeps the whole cell
        // "fresh" even if an older one shares the same 4-character
        // square, matching how a single active grid should read
        // (recency of the newest evidence, not the oldest). Invalid
        // when no matching worked station carried a known timestamp.
        QDateTime freshestWorkedAtUtc;
    };

    void buildSettingsRow();
    void refreshZoomLabel() const;
    void drawPanelHeader(QPainter& painter) const;
    QRectF canvasRect() const;
    QVector<GridCell> computeGridCells(const QRectF& area) const;
    // Soft radial "instrument glow" centred on home, under every other
    // layer -- operator, 2026-09-12: "die grafik sollte eine augenweide
    // sein". The same "Bogeninstrumente glimmen" language
    // RotorWidget::drawGlow() already gives its own compass dial
    // (kInstrumentGlowHi/Lo, HAUSSTIL.md), elliptical here (via a
    // scaled painter transform) rather than circular so it still fits a
    // non-square fit-to-window area exactly like every other layer's
    // own independent x/y scaling (see projectBearingDistance()).
    void drawGlow(QPainter& painter, const QRectF& area) const;
    // Drawn first, beneath every other layer -- a basemap, not an
    // instrument overlay (see paintEvent()'s own draw order). Fills
    // each cached ring as land (a hair lighter than the canvas's own
    // sea/unknown fill, via Style::shiftL()) and strokes its outline;
    // a handful of real-world enclaves (San Marino, Vatican, Lesotho...)
    // double-paint a few pixels where their host country's own ring
    // technically includes the enclave's area too (this data has no
    // exterior/hole distinction -- see core/CountryBorders.h) -- both
    // fills are the same land colour, so the result is still the
    // correct land shape, just not perfectly seamless at that pixel
    // scale; not worth the extra bookkeeping to avoid.
    void drawBordersLayer(QPainter& painter, const QRectF& area) const;
    // Drawn after the border basemap but before the grid/rings/spokes/
    // station layers -- reference points, not contest data, so real
    // worked/spotted station markers must still read as the visually
    // dominant layer on top. Only a city at least kCityMinDistanceKm
    // from home AND within the currently visible range is drawn --
    // both checked live against m_ownGrid/m_visibleRangeKm, never
    // baked into the cached data (see core/Cities.h's own doc comment).
    void drawCitiesLayer(QPainter& painter, const QRectF& area) const;
    void drawGridLayer(QPainter& painter, const QRectF& area) const;
    void drawRingsLayer(QPainter& painter, const QRectF& area) const;
    void drawSpokesLayer(QPainter& painter, const QRectF& area) const;
    // `rim` is the already-projected point on the rim itself (from
    // projectBearingDistance at the visible range) -- direction-aware,
    // so the label lands just outside the true rim at this bearing
    // whether that rim is circular or elliptical, rather than a
    // uniform-radius offset that would drift inside a wide ellipse's
    // east/west edge.
    void drawSpokeRimLabel(QPainter& painter, const QPointF& rim, double angleDeg, const QString& text,
                            bool bold) const;
    // Drawn after the generic Speichen layer, in the rotor needle's own
    // amber (Style::kAmberText) rather than the Speichen layer's dimmer
    // reference-grid amber -- a live reading, not a static ruler, so it
    // needs to read as the more prominent of the two when both are on.
    void drawRotorHeadingsLayer(QPainter& painter, const QRectF& area) const;
    void drawStations(QPainter& painter, const QRectF& area) const;
    void drawHomeMarker(QPainter& painter, const QRectF& area) const;
    void drawLegend(QPainter& painter, const QRectF& area) const;
    // Washes the map's own outer rim (area's bounding ellipse) red
    // (Blocked) / amber (Marginal) where m_terrainSectors marks it --
    // called right after the plain boundary ellipse in paintEvent() so
    // the wash sits on top of it, the same "plain ring first, coloured
    // wash on top" order RotorWidget::paintFullCompassDial() uses for
    // its own ring + drawTerrainSectorWash(). No-op when m_terrainSectors
    // is not exactly 360 entries (see setTerrainSectors()'s own comment).
    void drawTerrainSectorWash(QPainter& painter, const QRectF& area) const;

    QString m_ownGrid;
    QString m_ownLabel;
    QVector<Station> m_stations;

    bool m_showGrid = true;
    bool m_showRings = true;
    bool m_showSpokes = true;
    bool m_showWorkedCells = true;
    bool m_showBorders = true;
    bool m_showCities = true;
    bool m_showAging = true;
    bool m_fitToWindow = true;
    bool m_showRotor1Heading = true;
    bool m_showRotor2Heading = true;
    double m_visibleRangeKm = 400.0;

    QVector<LineOfSightClass> m_terrainSectors;

    bool m_rotor1Connected = false;
    double m_rotor1AzimuthDeg = 0.0;
    QString m_rotor1Label;
    bool m_rotor2Connected = false;
    double m_rotor2AzimuthDeg = 0.0;
    QString m_rotor2Label;

    // Loaded once at construction (core/CountryBorders.h) -- the raw
    // (lon, lat) ring data never changes at runtime, only its
    // projection to screen space does (every paint, via
    // drawBordersLayer()), so caching it here avoids re-reading and
    // re-parsing the resource file on every repaint.
    QVector<CountryBorderRing> m_countryBorders;
    // Same caching rationale as m_countryBorders above, for
    // core/Cities.h's data -- see drawCitiesLayer()'s own doc comment.
    QVector<CityPoint> m_cities;

    QWidget* m_settingsRow = nullptr;
    QCheckBox* m_gridCheck = nullptr;
    QCheckBox* m_ringsCheck = nullptr;
    QCheckBox* m_spokesCheck = nullptr;
    QCheckBox* m_workedCellsCheck = nullptr;
    QCheckBox* m_bordersCheck = nullptr;
    QCheckBox* m_citiesCheck = nullptr;
    QCheckBox* m_agingCheck = nullptr;
    QCheckBox* m_fitCheck = nullptr;
    QCheckBox* m_rotor1HeadingCheck = nullptr;
    QCheckBox* m_rotor2HeadingCheck = nullptr;
    QPushButton* m_zoomOutButton = nullptr;
    QPushButton* m_zoomInButton = nullptr;
    QLabel* m_zoomRangeLabel = nullptr;
    // Aging is the one layer whose own visual state (how far a marker
    // has faded) changes purely with wall-clock time, not with any
    // other signal this widget already reacts to (a new QSO, a spot-feed
    // change, a resize) -- everything else here only ever needs to
    // repaint in response to one of those. Ticks update() once a
    // minute, cheap enough for a small hand-painted panel, and only
    // while m_showAging is on (see setAgingEnabled()).
    QTimer* m_agingRefreshTimer = nullptr;
};

} // namespace Contestprogramm
