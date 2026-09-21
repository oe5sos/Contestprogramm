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

class QAction;
class QLabel;
class QMenu;
class QMouseEvent;
class QPainter;
class QPushButton;
class QToolButton;
class QTimer;

namespace Contestprogramm {

// "Karte / Verbindungen": the own-QTH-centred instrument for the one
// question a VHF contest asks all night -- where to turn the antenna,
// and who is still open there. Rebuilt 2026-09-20 from two chosen
// design sheets (operator: "baue 1 und 4, die man wechseln kann"):
//
//   View::Radar       a dark scope, range rings, bearing ticks, the
//                     stations as dots (green worked, blue open), the
//                     rotor as a light cone, the terrain horizon as a
//                     dark rim -- thick where mountains block -- and a
//                     column of numbers (beam, open in that direction,
//                     QSOs/points, ODX). No map ballast.
//   View::MapHorizon  a quiet geographic map (borders, a few cities,
//                     locator squares) and, beneath it, the horizon
//                     unrolled 0-360°: the skyline as a profile, every
//                     station a tick at its bearing, the beam a marker.
//
// Every layer the panel had before stays available in both views --
// Grid, Ringe, Speichen, Gearbeitet, Grenzen, Städte, Altern, Füllen,
// Rotor 1/2, plus the new Horizont -- now behind one ⚙ menu instead of
// ten checkboxes, and the choice of view plus all toggles are reported
// through preferencesChanged() so MainWindow can keep them in the
// settings table (preferencesText()/applyPreferencesText()).
//
// Sits in a PanelContainerWidget in header mode: the container's
// PanelHeaderBar carries title, lock and the ⚙ whose menu
// populateOptionsMenu() fills
// -- top right with its own symbol, like every other panel (operator,
// 2026-09-20: "optionen sollen immer rechts oben mit eigenem symbol
// erreichbar sein"). Projection is azimuthal equidistant around the own
// locator: bearing on screen is the bearing to turn the rotor to,
// distance is the distance to log.
class MapWidget : public QWidget {
    Q_OBJECT

public:
    enum class View { Radar, MapHorizon };

    // One plottable row -- a worked QSO or a spotted-but-unworked
    // candidate. freqHz 0 = unknown (passed through candidateActivated
    // unchanged); workedAtUtc invalid = "age unknown, never fade".
    struct Station {
        QString callsign;
        QString grid;
        bool worked = false;
        qint64 freqHz = 0;
        QDateTime workedAtUtc;
    };

    explicit MapWidget(QWidget* parent = nullptr);

    void setOwnGrid(const QString& grid);
    QString ownGrid() const { return m_ownGrid; }
    void setOwnLabel(const QString& label);

    // Full replace on every change -- contest-scale counts make a
    // re-projection per call a non-issue.
    void setStations(const QVector<Station>& stations);
    const QVector<Station>& stations() const { return m_stations; }

    void setView(View view);
    View view() const { return m_view; }

    // Layer toggles. Each also drives the ⚙ menu's checkmark. Grid,
    // cells, borders and cities are kept per view -- the radar starts
    // without map ballast, the map with it, and each remembers its own
    // choice (operator, 2026-09-21: "bei RADAR ein- und ausblenden").
    // The getters/setters below address the current view's set.
    void setGridLayerVisible(bool visible);
    void setRingsLayerVisible(bool visible);
    void setSpokesLayerVisible(bool visible);
    void setWorkedCellsLayerVisible(bool visible);
    void setBordersLayerVisible(bool visible);
    void setCitiesLayerVisible(bool visible);
    void setAgingEnabled(bool enabled);
    void setFitToWindowEnabled(bool enabled);
    void setRotor1HeadingLayerVisible(bool visible);
    void setRotor2HeadingLayerVisible(bool visible);
    void setHorizonLayerVisible(bool visible);
    bool gridLayerVisible() const { return layers().grid; }
    bool ringsLayerVisible() const { return m_showRings; }
    bool spokesLayerVisible() const { return m_showSpokes; }
    bool workedCellsLayerVisible() const { return layers().cells; }
    bool bordersLayerVisible() const { return layers().borders; }
    bool citiesLayerVisible() const { return layers().cities; }
    bool agingEnabled() const { return m_showAging; }
    bool fitToWindowEnabled() const { return m_fitToWindow; }
    bool rotor1HeadingLayerVisible() const { return m_showRotor1Heading; }
    bool rotor2HeadingLayerVisible() const { return m_showRotor2Heading; }
    bool horizonLayerVisible() const { return m_showHorizon; }

    // Terrain, two levels of detail. The profile (core/terrain/
    // HorizonProfile.h: elevation angle per bearing degree, 360 values)
    // draws the rim and the skyline; without one, the line-of-sight
    // classes per degree (TerrainDataManager::sectorSweep) still give a
    // coarse rim -- Blocked thick, Marginal thin.
    void setHorizonProfile(const QVector<double>& elevationDegByBearing);
    const QVector<double>& horizonProfile() const { return m_horizonProfile; }
    void setTerrainSectors(const QVector<LineOfSightClass>& sectorsByDegree);

    void setRotor1Heading(bool connected, double azimuthDeg, const QString& label);
    void setRotor2Heading(bool connected, double azimuthDeg, const QString& label);
    // A second antenna on the same rotor (ContestSettings::rotorN
    // SecondAntennaEnabled/-OffsetDeg): its own cone at heading +
    // offset, in a second colour. Operator, 2026-09-20: "auf rotor 1
    // sind 2 antennen, graph sollte dann auch in 2 richtungen sein".
    void setRotor1SecondAntenna(bool enabled, double offsetDeg);
    void setRotor2SecondAntenna(bool enabled, double offsetDeg);
    // Opening angle (full -3 dB beamwidth) of each rotor's antennas --
    // the cone's width. A map preference (⚙ menu), persisted with the
    // layer toggles; 30° unless set.
    void setRotor1BeamwidthDeg(double degrees);
    void setRotor2BeamwidthDeg(double degrees);
    double rotor1BeamwidthDeg() const { return m_beamwidth1Deg; }
    double rotor2BeamwidthDeg() const { return m_beamwidth2Deg; }

    // The radar's number column. Unset (qsos < 0) means "count the
    // worked stations and sum their kilometres" -- MainWindow passes
    // the real contest score so both agree with the Rate panel.
    void setScoreSummary(int qsos, qint64 points, const QString& odxText);

    void setVisibleRangeKm(double rangeKm);
    double visibleRangeKm() const { return m_visibleRangeKm; }
    void zoomIn();
    void zoomOut();

    // Fills `menu` (a fresh QMenu the caller owns and pops up -- MainWindow
    // on PanelHeaderBar::optionsRequested) with the layer toggles, the
    // second-antenna switches and the beamwidth submenus.
    void populateOptionsMenu(QMenu* menu);

    // "view=radar;grid=1;..." -- what MainWindow persists.
    QString preferencesText() const;
    void applyPreferencesText(const QString& text);

    // Pure projection helpers (unit-tested): bearing/distance from the
    // centre of `mapRect` scaled so `visibleRangeKm` lands on its rim,
    // per axis (an ellipse when the rect is not square).
    static QPointF projectBearingDistance(double bearingDeg, double distanceKm, double visibleRangeKm,
                                           const QRectF& mapRect);
    static QVector<QPointF> projectGridSquareCorners(const QString& gridSquare, double homeLat, double homeLon,
                                                       double visibleRangeKm, const QRectF& mapRect);
    static QColor markerColor(bool worked);
    static QColor gridLabelColor(bool worked);
    static QColor agedMarkerColor(const QColor& base, qint64 secondsSinceWorked);

    // The scope (Radar) or map (MapHorizon) rectangle -- its centre is
    // the own station. Tests click there.
    QRectF canvasRectForTest() const { return scopeRect(); }
    QRectF horizonStripRectForTest() const { return horizonStripRect(); }

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    // A click on a station (marker or skyline tick).
    void candidateActivated(const QString& callsign, const QString& grid, qint64 freqHz);
    // The ⚙ menu's "Zweitantenne" entry for rotor 1 or 2 -- the same
    // station setting the settings dialog edits (operator, 2026-09-20:
    // "dies soll eine option sein, dann kann ich es selbst machen");
    // MainWindow stores it and feeds it back via setRotorNSecondAntenna.
    void secondAntennaToggled(int rotor, bool enabled);
    // View or any layer toggle changed by the operator or a setter.
    void preferencesChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    struct GridCell {
        QString code;
        QVector<QPointF> corners; // always 4 points, screen space
        bool isHome = false;
        bool hasWorked = false;
        bool hasSpotted = false;
        QDateTime freshestWorkedAtUtc;
    };
    struct Plotted {
        const Station* station = nullptr;
        double bearingDeg = 0.0;
        double distanceKm = 0.0;
        QPointF point;
    };

    void buildControls();
    void syncControls();
    void notePreferenceChange();
    void refreshZoomLabel() const;

    // Geometry
    QRectF canvasRect() const;       // everything below the controls
    QRectF scopeRect() const;        // the (possibly elliptical) map/scope area
    QRectF numbersRect() const;      // Radar: the column to the right, empty when too narrow
    QRectF horizonStripRect() const; // MapHorizon: the strip below the map, empty when off
    double horizonAngleAt(int bearingDeg) const; // profile, else from sectors, 0 = flat
    bool hasHorizon() const;
    QVector<Plotted> plotStations(const QRectF& area) const;
    QVector<GridCell> computeGridCells(const QRectF& area) const;

    // Painting
    void drawScopeFace(QPainter& painter, const QRectF& area) const;
    void drawBordersLayer(QPainter& painter, const QRectF& area) const;
    void drawCitiesLayer(QPainter& painter, const QRectF& area) const;
    void drawGridLayer(QPainter& painter, const QRectF& area) const;
    void drawRingsLayer(QPainter& painter, const QRectF& area) const;
    void drawSpokesLayer(QPainter& painter, const QRectF& area) const;
    void drawHorizonRim(QPainter& painter, const QRectF& area) const;
    void drawRotorHeadingsLayer(QPainter& painter, const QRectF& area) const;
    struct Beam {
        double azimuthDeg = 0.0;
        double halfWidthDeg = 15.0;
        QColor color;
        Qt::PenStyle lineStyle = Qt::SolidLine;
        QString label;
        int labelPx = 11;
    };
    QVector<Beam> beams() const; // every antenna direction currently to draw
    void drawBeam(QPainter& painter, const QRectF& area, const Beam& beam, bool labelsOnly,
                  QVector<QRectF>* takenLabels = nullptr) const;
    void drawStations(QPainter& painter, const QRectF& area) const;
    void drawHomeMarker(QPainter& painter, const QRectF& area) const;
    void drawNumbersColumn(QPainter& painter, const QRectF& column) const;
    void drawHorizonStrip(QPainter& painter, const QRectF& strip) const;
    void drawLegend(QPainter& painter, const QPointF& bottomLeft) const;

    QString m_ownGrid;
    QString m_ownLabel;
    QVector<Station> m_stations;
    View m_view = View::Radar;
    struct ViewLayers {
        bool grid = true;
        bool cells = true;
        bool borders = true;
        bool cities = true;
    };
    ViewLayers& layers() { return m_view == View::Radar ? m_radarLayers : m_mapLayers; }
    const ViewLayers& layers() const { return m_view == View::Radar ? m_radarLayers : m_mapLayers; }
    ViewLayers m_radarLayers{false, true, false, false};
    ViewLayers m_mapLayers;
    bool m_showRings = true;
    bool m_showSpokes = true;
    bool m_showAging = true;
    bool m_fitToWindow = true;
    bool m_showRotor1Heading = true;
    bool m_showRotor2Heading = true;
    bool m_showHorizon = true;
    double m_visibleRangeKm = 400.0;
    QVector<double> m_horizonProfile;
    QVector<LineOfSightClass> m_terrainSectors;
    bool m_rotor1Connected = false;
    double m_rotor1AzimuthDeg = 0.0;
    QString m_rotor1Label;
    bool m_rotor2Connected = false;
    double m_rotor2AzimuthDeg = 0.0;
    QString m_rotor2Label;
    bool m_rotor1SecondEnabled = false;
    double m_rotor1SecondOffsetDeg = 0.0;
    bool m_rotor2SecondEnabled = false;
    double m_rotor2SecondOffsetDeg = 0.0;
    double m_beamwidth1Deg = 30.0;
    double m_beamwidth2Deg = 30.0;
    int m_scoreQsos = -1;
    qint64 m_scorePoints = 0;
    QString m_scoreOdx;
    QVector<CountryBorderRing> m_countryBorders;
    QVector<CityPoint> m_cities;

    QWidget* m_controlsRow = nullptr;
    QPushButton* m_radarButton = nullptr;
    QPushButton* m_mapButton = nullptr;
    QAction* m_gridAction = nullptr;
    QAction* m_ringsAction = nullptr;
    QAction* m_spokesAction = nullptr;
    QAction* m_workedCellsAction = nullptr;
    QAction* m_bordersAction = nullptr;
    QAction* m_citiesAction = nullptr;
    QAction* m_agingAction = nullptr;
    QAction* m_fitAction = nullptr;
    QAction* m_rotor1Action = nullptr;
    QAction* m_rotor2Action = nullptr;
    QAction* m_horizonAction = nullptr;
    QAction* m_rotor1OneAction = nullptr;
    QAction* m_rotor1SecondAction = nullptr;
    QAction* m_rotor2OneAction = nullptr;
    QAction* m_rotor2SecondAction = nullptr;
    QPushButton* m_zoomOutButton = nullptr;
    QPushButton* m_zoomInButton = nullptr;
    QLabel* m_zoomRangeLabel = nullptr;
    QTimer* m_agingRefreshTimer = nullptr;
    bool m_syncingControls = false;
};

} // namespace Contestprogramm
