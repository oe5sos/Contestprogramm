#include <QtTest>

#include <QApplication>
#include <QDateTime>
#include <QAction>
#include <QSignalSpy>

#include "core/Maidenhead.h"
#include "core/terrain/LineOfSight.h"
#include "ui/MapWidget.h"
#include "ui/StyleKit.h"

using namespace Contestprogramm;

// MapWidget's projection maths (projectBearingDistance/
// projectGridSquareCorners) and the worked/spotted colour mapping are
// plain static functions -- no QWidget/QPainter needed to exercise them
// -- but TestMapWidgetLive below constructs an actual MapWidget (a
// QWidget subclass), which needs a live QApplication (same reasoning as
// test_rotorwidget.cpp's TestRotorWidgetBandLabel). Both classes run in
// one executable via the custom main() at the bottom, mirroring
// test_rotorwidget.cpp's own structure.
class TestMapWidgetProjection : public QObject
{
    Q_OBJECT

private slots:
    void zeroDistanceProjectsToCenter();
    void dueNorthProjectsAboveCenter();
    void dueEastProjectsRightOfCenter();
    void distanceAtVisibleRangeLandsOnRim();
    void homeSquareCornersSurroundCenter();
    void neighborSquareToNorthProjectsAbove();
    void markerColorMatchesWorkedState();
    void gridLabelColorMatchesWorkedState();
    void nonSquareMapRectScalesAxesIndependently();
    void distanceAtVisibleRangeLandsOnEllipseRim();
    void agedMarkerColorUnchangedBeforeAgingStarts();
    void agedMarkerColorFullyFadedAfterCompletion();
    void agedMarkerColorInterpolatesPartway();
    void agedMarkerColorPreservesAlpha();
    void agedMarkerColorTreatsNonPositiveElapsedAsUnchanged();
};

void TestMapWidgetProjection::zeroDistanceProjectsToCenter()
{
    const QRectF mapRect(0.0, 0.0, 300.0, 300.0);
    const QPointF p = MapWidget::projectBearingDistance(123.0, 0.0, 400.0, mapRect);
    QVERIFY(qAbs(p.x() - mapRect.center().x()) < 0.01);
    QVERIFY(qAbs(p.y() - mapRect.center().y()) < 0.01);
}

void TestMapWidgetProjection::dueNorthProjectsAboveCenter()
{
    const QRectF mapRect(0.0, 0.0, 300.0, 300.0);
    const QPointF p = MapWidget::projectBearingDistance(0.0, 100.0, 400.0, mapRect);
    QVERIFY(p.y() < mapRect.center().y());
    QVERIFY(qAbs(p.x() - mapRect.center().x()) < 0.01);
}

void TestMapWidgetProjection::dueEastProjectsRightOfCenter()
{
    const QRectF mapRect(0.0, 0.0, 300.0, 300.0);
    const QPointF p = MapWidget::projectBearingDistance(90.0, 100.0, 400.0, mapRect);
    QVERIFY(p.x() > mapRect.center().x());
    QVERIFY(qAbs(p.y() - mapRect.center().y()) < 0.01);
}

void TestMapWidgetProjection::distanceAtVisibleRangeLandsOnRim()
{
    const QRectF mapRect(0.0, 0.0, 300.0, 300.0);
    const double radiusPx = std::min(mapRect.width(), mapRect.height()) / 2.0;
    const QPointF p = MapWidget::projectBearingDistance(217.0, 400.0, 400.0, mapRect);
    const double actualRadius = QLineF(mapRect.center(), p).length();
    QVERIFY(qAbs(actualRadius - radiusPx) < 0.05);
}

void TestMapWidgetProjection::homeSquareCornersSurroundCenter()
{
    // A grid square's own cell, projected using its own centre as home,
    // must surround the projection's pole (the mapRect centre) -- the
    // basic geometric sanity check for the per-corner bearing/distance
    // projection (mirrors test_rotorwidget.cpp's own geometry checks).
    double homeLat = 0.0;
    double homeLon = 0.0;
    calculateLatLonFromGridSquare(QStringLiteral("JN77"), homeLat, homeLon);

    const QRectF mapRect(0.0, 0.0, 300.0, 300.0);
    const QVector<QPointF> corners =
        MapWidget::projectGridSquareCorners(QStringLiteral("JN77"), homeLat, homeLon, 400.0, mapRect);
    QCOMPARE(corners.size(), 4);

    double minX = corners.first().x();
    double maxX = corners.first().x();
    double minY = corners.first().y();
    double maxY = corners.first().y();
    for (const QPointF& corner : corners) {
        minX = std::min(minX, corner.x());
        maxX = std::max(maxX, corner.x());
        minY = std::min(minY, corner.y());
        maxY = std::max(maxY, corner.y());
    }
    QVERIFY(minX < mapRect.center().x());
    QVERIFY(maxX > mapRect.center().x());
    QVERIFY(minY < mapRect.center().y());
    QVERIFY(maxY > mapRect.center().y());
}

void TestMapWidgetProjection::neighborSquareToNorthProjectsAbove()
{
    // JN77 -> JN78 changes only the latitude digit (7 -> 8), i.e. a pure
    // +1 deg latitude step with no longitude change -- so JN78's corners
    // must project, on average, above (smaller y than) JN77's, and at
    // roughly the same x, exercising the per-corner bearing/distance
    // projection against a known, deterministic direction rather than
    // just checking it produces four points.
    double homeLat = 0.0;
    double homeLon = 0.0;
    calculateLatLonFromGridSquare(QStringLiteral("JN77"), homeLat, homeLon);

    const QRectF mapRect(0.0, 0.0, 300.0, 300.0);
    const QVector<QPointF> homeCorners =
        MapWidget::projectGridSquareCorners(QStringLiteral("JN77"), homeLat, homeLon, 400.0, mapRect);
    const QVector<QPointF> northCorners =
        MapWidget::projectGridSquareCorners(QStringLiteral("JN78"), homeLat, homeLon, 400.0, mapRect);

    auto averageY = [](const QVector<QPointF>& points) {
        double sum = 0.0;
        for (const QPointF& p : points) {
            sum += p.y();
        }
        return sum / points.size();
    };
    auto averageX = [](const QVector<QPointF>& points) {
        double sum = 0.0;
        for (const QPointF& p : points) {
            sum += p.x();
        }
        return sum / points.size();
    };

    QVERIFY(averageY(northCorners) < averageY(homeCorners));
    QVERIFY(qAbs(averageX(northCorners) - averageX(homeCorners)) < 5.0);
}

void TestMapWidgetProjection::nonSquareMapRectScalesAxesIndependently()
{
    // MapWidget's "Füllen" (fit-to-window) mode passes a non-square
    // mapRect straight through -- the whole point being that x and y
    // scale by the rect's own width/height independently, not by a
    // shared min(w,h) radius (see projectBearingDistance's own
    // comment). A wide rect (600x300) put a due-east point at 600px of
    // travel from centre at full range and a due-north point at only
    // 300px -- different pixel distances for the same real-world
    // distance, which is exactly what "the rings become elliptical" in
    // Martin's own words means.
    const QRectF mapRect(0.0, 0.0, 600.0, 300.0);
    const QPointF east = MapWidget::projectBearingDistance(90.0, 400.0, 400.0, mapRect);
    const QPointF north = MapWidget::projectBearingDistance(0.0, 400.0, 400.0, mapRect);

    const double eastOffset = east.x() - mapRect.center().x();
    const double northOffset = mapRect.center().y() - north.y();

    QVERIFY(qAbs(eastOffset - 300.0) < 0.05);  // half the 600px width
    QVERIFY(qAbs(northOffset - 150.0) < 0.05); // half the 300px height
}

void TestMapWidgetProjection::distanceAtVisibleRangeLandsOnEllipseRim()
{
    // Same "lands on the rim at full range" check as
    // distanceAtVisibleRangeLandsOnRim() above, but for a non-square
    // rect -- the rim here is an ellipse, so the check is per-axis
    // (x offset == half-width, y offset == half-height) rather than a
    // single radius from centre.
    const QRectF mapRect(0.0, 0.0, 600.0, 300.0);
    const QPointF east = MapWidget::projectBearingDistance(90.0, 400.0, 400.0, mapRect);
    const QPointF south = MapWidget::projectBearingDistance(180.0, 400.0, 400.0, mapRect);

    QVERIFY(qAbs((east.x() - mapRect.center().x()) - 300.0) < 0.05);
    QVERIFY(qAbs((south.y() - mapRect.center().y()) - 150.0) < 0.05);
}

void TestMapWidgetProjection::markerColorMatchesWorkedState()
{
    QCOMPARE(MapWidget::markerColor(true), QColor(Style::kGreenText()));
    QCOMPARE(MapWidget::markerColor(false), QColor(Style::kBlueBg()));
}

void TestMapWidgetProjection::gridLabelColorMatchesWorkedState()
{
    QCOMPARE(MapWidget::gridLabelColor(true), QColor(Style::kAmberText()));
    QCOMPARE(MapWidget::gridLabelColor(false), QColor(Style::kBlueBg()));
}

void TestMapWidgetProjection::agedMarkerColorUnchangedBeforeAgingStarts()
{
    const QColor base(Style::kGreenText());
    // 10 minutes -- well under the 30-minute "still fresh" threshold.
    QCOMPARE(MapWidget::agedMarkerColor(base, 10 * 60), base);
}

void TestMapWidgetProjection::agedMarkerColorFullyFadedAfterCompletion()
{
    const QColor base(Style::kGreenText());
    const QColor faded(Style::kTextInactive());
    // 4 hours -- well past the 180-minute "fully faded" threshold.
    const QColor result = MapWidget::agedMarkerColor(base, 4 * 3600);
    QCOMPARE(result.red(), faded.red());
    QCOMPARE(result.green(), faded.green());
    QCOMPARE(result.blue(), faded.blue());
    QCOMPARE(result.alpha(), base.alpha());
}

void TestMapWidgetProjection::agedMarkerColorInterpolatesPartway()
{
    const QColor base(Style::kGreenText());
    const QColor faded(Style::kTextInactive());
    // 105 minutes -- exactly halfway between the 30-minute start and the
    // 180-minute completion of the fade.
    const QColor result = MapWidget::agedMarkerColor(base, 105 * 60);
    // Strictly between the two endpoints on any channel that actually
    // differs between base and faded -- a robust way to check "really
    // interpolated", not just returning one endpoint verbatim.
    bool checkedAnyChannel = false;
    if (base.red() != faded.red()) {
        checkedAnyChannel = true;
        QVERIFY(result.red() > std::min(base.red(), faded.red()));
        QVERIFY(result.red() < std::max(base.red(), faded.red()));
    }
    if (base.green() != faded.green()) {
        checkedAnyChannel = true;
        QVERIFY(result.green() > std::min(base.green(), faded.green()));
        QVERIFY(result.green() < std::max(base.green(), faded.green()));
    }
    if (base.blue() != faded.blue()) {
        checkedAnyChannel = true;
        QVERIFY(result.blue() > std::min(base.blue(), faded.blue()));
        QVERIFY(result.blue() < std::max(base.blue(), faded.blue()));
    }
    // kGreenText and kTextInactive are two genuinely different named
    // colours in every current theme -- if this ever fired it would
    // mean the two happened to match exactly, making the checks above
    // vacuously skipped rather than actually exercised.
    QVERIFY(checkedAnyChannel);
}

void TestMapWidgetProjection::agedMarkerColorPreservesAlpha()
{
    QColor base(Style::kBlueBg());
    base.setAlpha(50); // matches the spotted grid-cell fill's own setAlpha(50)
    QCOMPARE(MapWidget::agedMarkerColor(base, 10 * 60).alpha(), 50);
    QCOMPARE(MapWidget::agedMarkerColor(base, 105 * 60).alpha(), 50);
    QCOMPARE(MapWidget::agedMarkerColor(base, 4 * 3600).alpha(), 50);
}

void TestMapWidgetProjection::agedMarkerColorTreatsNonPositiveElapsedAsUnchanged()
{
    const QColor base(Style::kGreenText());
    QCOMPARE(MapWidget::agedMarkerColor(base, 0), base);
    QCOMPARE(MapWidget::agedMarkerColor(base, -5), base);
}

// MapWidget is a QWidget subclass, so exercising its live state needs a
// QApplication (same reasoning as TestRotorWidgetBandLabel in
// test_rotorwidget.cpp) -- combined into one executable via the custom
// main() below rather than a second test binary.
class TestMapWidgetLive : public QObject
{
    Q_OBJECT

private slots:
    void setStationsPreservesWorkedFlagAndOrder();
    void layerTogglesRoundTrip();
    void zoomInHalvesVisibleRange();
    void zoomOutDoublesVisibleRange();
    void visibleRangeIsClamped();
    void fitToWindowDefaultsTrueAndRoundTrips();
    void agingEnabledDefaultsTrueAndRoundTrips();
    void clickingStationMarkerEmitsCandidateActivated();
    void clickingEmptyAreaDoesNotEmitCandidateActivated();
    void terrainSectorWashPaintsCleanlyWhenSet();
    void agingPaintsCleanlyForAnAgedWorkedStation();
    void viewSwitchRoundTripsAndReportsPreferences();
    void preferencesTextRoundTrips();
    void horizonProfileAndSecondAntennasPaintCleanlyInBothViews();
    void clickingASkylineTickActivatesTheStation();
    void secondAntennaMenuEntryReportsTheStationSetting();
};

void TestMapWidgetLive::setStationsPreservesWorkedFlagAndOrder()
{
    MapWidget widget;
    QVector<MapWidget::Station> stations;
    stations.append(MapWidget::Station{QStringLiteral("OE5SOS"), QStringLiteral("JN77QT"), true});
    stations.append(MapWidget::Station{QStringLiteral("OE1XYZ"), QStringLiteral("JN88TC"), false});
    widget.setStations(stations);

    QCOMPARE(widget.stations().size(), 2);
    QCOMPARE(widget.stations().at(0).callsign, QStringLiteral("OE5SOS"));
    QVERIFY(widget.stations().at(0).worked);
    QCOMPARE(widget.stations().at(1).callsign, QStringLiteral("OE1XYZ"));
    QVERIFY(!widget.stations().at(1).worked);
}

void TestMapWidgetLive::layerTogglesRoundTrip()
{
    MapWidget widget;
    QVERIFY(widget.gridLayerVisible());
    QVERIFY(widget.ringsLayerVisible());
    QVERIFY(widget.spokesLayerVisible());
    QVERIFY(widget.workedCellsLayerVisible());
    QVERIFY(widget.rotor1HeadingLayerVisible());
    QVERIFY(widget.rotor2HeadingLayerVisible());
    QVERIFY(widget.bordersLayerVisible());
    QVERIFY(widget.citiesLayerVisible());

    widget.setGridLayerVisible(false);
    widget.setRingsLayerVisible(false);
    widget.setSpokesLayerVisible(false);
    widget.setWorkedCellsLayerVisible(false);
    widget.setRotor1HeadingLayerVisible(false);
    widget.setRotor2HeadingLayerVisible(false);
    widget.setBordersLayerVisible(false);
    widget.setCitiesLayerVisible(false);

    QVERIFY(!widget.gridLayerVisible());
    QVERIFY(!widget.ringsLayerVisible());
    QVERIFY(!widget.spokesLayerVisible());
    QVERIFY(!widget.workedCellsLayerVisible());
    QVERIFY(!widget.rotor1HeadingLayerVisible());
    QVERIFY(!widget.rotor2HeadingLayerVisible());
    QVERIFY(!widget.bordersLayerVisible());
    QVERIFY(!widget.citiesLayerVisible());
}

void TestMapWidgetLive::zoomInHalvesVisibleRange()
{
    // Name unchanged (see test_mapwidget.h) -- behaviour is now a fixed
    // 250 km step, not a halving (operator, 2026-09-14: "mache schritte
    // beim radius bitte alle 250km").
    MapWidget widget;
    widget.setVisibleRangeKm(400.0);
    widget.zoomIn();
    QCOMPARE(widget.visibleRangeKm(), 150.0);
}

void TestMapWidgetLive::zoomOutDoublesVisibleRange()
{
    MapWidget widget;
    widget.setVisibleRangeKm(400.0);
    widget.zoomOut();
    QCOMPARE(widget.visibleRangeKm(), 650.0);
}

void TestMapWidgetLive::visibleRangeIsClamped()
{
    MapWidget widget;
    widget.setVisibleRangeKm(1.0);
    QVERIFY(widget.visibleRangeKm() >= 25.0);
    widget.setVisibleRangeKm(100000.0);
    QVERIFY(widget.visibleRangeKm() <= 3200.0);
}

void TestMapWidgetLive::fitToWindowDefaultsTrueAndRoundTrips()
{
    // Default true -- fit-to-window (ellipses allowed) is the default
    // behaviour per Martin's own request, not an opt-in.
    MapWidget widget;
    QVERIFY(widget.fitToWindowEnabled());

    widget.setFitToWindowEnabled(false);
    QVERIFY(!widget.fitToWindowEnabled());

    widget.setFitToWindowEnabled(true);
    QVERIFY(widget.fitToWindowEnabled());
}

void TestMapWidgetLive::agingEnabledDefaultsTrueAndRoundTrips()
{
    MapWidget widget;
    QVERIFY(widget.agingEnabled());

    widget.setAgingEnabled(false);
    QVERIFY(!widget.agingEnabled());

    widget.setAgingEnabled(true);
    QVERIFY(widget.agingEnabled());
}

void TestMapWidgetLive::clickingStationMarkerEmitsCandidateActivated()
{
    MapWidget widget;
    widget.resize(500, 500);
    widget.setOwnGrid(QStringLiteral("JN67VV"));
    widget.setVisibleRangeKm(400.0);

    // Placed exactly at home -- projectBearingDistance's zero-distance
    // case (see zeroDistanceProjectsToCenter() above) guarantees this
    // always lands at canvasRectForTest().center(), regardless of this
    // widget's own header/margin/legend layout constants.
    MapWidget::Station station;
    station.callsign = QStringLiteral("OE5DEMO");
    station.grid = QStringLiteral("JN67VV");
    station.worked = true;
    station.freqHz = 144300000;
    widget.setStations({station});

    QSignalSpy spy(&widget, &MapWidget::candidateActivated);
    const QPoint clickPoint = widget.canvasRectForTest().center().toPoint();
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, clickPoint);

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), QStringLiteral("OE5DEMO"));
    QCOMPARE(args.at(1).toString(), QStringLiteral("JN67VV"));
    QCOMPARE(args.at(2).toLongLong(), static_cast<qint64>(144300000));
}

void TestMapWidgetLive::clickingEmptyAreaDoesNotEmitCandidateActivated()
{
    MapWidget widget;
    widget.resize(500, 500);
    widget.setOwnGrid(QStringLiteral("JN67VV"));
    widget.setVisibleRangeKm(400.0);
    // No stations at all -- even a click dead centre (where a station
    // WOULD project to, per the test above) must stay a no-op.

    QSignalSpy spy(&widget, &MapWidget::candidateActivated);
    const QPoint clickPoint = widget.canvasRectForTest().center().toPoint();
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, clickPoint);

    QCOMPARE(spy.count(), 0);
}

void TestMapWidgetLive::terrainSectorWashPaintsCleanlyWhenSet()
{
    // Same "real 360-entry sweep with a Blocked and a Marginal run, must
    // still paint cleanly" smoke test
    // test_rotorwidget.cpp's own terrainSectorWashPaintsCleanlyAcrossDialStyles()
    // already established for RotorWidget's identical contract.
    MapWidget widget;
    widget.resize(400, 400);
    widget.setOwnGrid(QStringLiteral("JN67VV"));

    QVector<LineOfSightClass> sectors(360, LineOfSightClass::Clear);
    for (int deg = 80; deg < 100; ++deg) {
        sectors[deg] = LineOfSightClass::Blocked;
    }
    for (int deg = 200; deg < 220; ++deg) {
        sectors[deg] = LineOfSightClass::Marginal;
    }
    widget.setTerrainSectors(sectors);

    QVERIFY(!widget.grab().isNull());
}

void TestMapWidgetLive::agingPaintsCleanlyForAnAgedWorkedStation()
{
    MapWidget widget;
    widget.resize(400, 400);
    widget.setOwnGrid(QStringLiteral("JN67VV"));

    MapWidget::Station station;
    station.callsign = QStringLiteral("OE5DEMO");
    station.grid = QStringLiteral("JN78CD");
    station.worked = true;
    station.workedAtUtc = QDateTime::currentDateTimeUtc().addSecs(-2 * 3600); // 2h ago -- mid-fade
    widget.setStations({station});

    QVERIFY(widget.agingEnabled());
    QVERIFY(!widget.grab().isNull());
}

void TestMapWidgetLive::viewSwitchRoundTripsAndReportsPreferences()
{
    MapWidget widget;
    QCOMPARE(widget.view(), MapWidget::View::Radar);
    QSignalSpy changed(&widget, &MapWidget::preferencesChanged);
    widget.setView(MapWidget::View::MapHorizon);
    QCOMPARE(widget.view(), MapWidget::View::MapHorizon);
    QCOMPARE(changed.count(), 1);
    widget.setView(MapWidget::View::MapHorizon); // no change, no signal
    QCOMPARE(changed.count(), 1);
    widget.setRingsLayerVisible(false);
    QCOMPARE(changed.count(), 2);
    widget.setRotor1BeamwidthDeg(20.0);
    QCOMPARE(widget.rotor1BeamwidthDeg(), 20.0);
    QCOMPARE(changed.count(), 3);
    // Out-of-range beamwidths are clamped, not refused.
    widget.setRotor2BeamwidthDeg(500.0);
    QVERIFY(widget.rotor2BeamwidthDeg() <= 120.0);
}

void TestMapWidgetLive::preferencesTextRoundTrips()
{
    MapWidget widget;
    widget.setView(MapWidget::View::MapHorizon);
    widget.setGridLayerVisible(false);
    widget.setCitiesLayerVisible(false);
    widget.setHorizonLayerVisible(false);
    widget.setAgingEnabled(false);
    widget.setRotor1BeamwidthDeg(25.0);
    widget.setRotor2BeamwidthDeg(40.0);
    const QString text = widget.preferencesText();
    QVERIFY(text.contains(QStringLiteral("view=map")));
    QVERIFY(text.contains(QStringLiteral("grid=0")));
    QVERIFY(text.contains(QStringLiteral("bw1=25")));

    MapWidget other;
    QSignalSpy changed(&other, &MapWidget::preferencesChanged);
    other.applyPreferencesText(text);
    QCOMPARE(other.view(), MapWidget::View::MapHorizon);
    QVERIFY(!other.gridLayerVisible());
    QVERIFY(!other.citiesLayerVisible());
    QVERIFY(!other.horizonLayerVisible());
    QVERIFY(!other.agingEnabled());
    QVERIFY(other.ringsLayerVisible()); // untouched keys keep their default
    QCOMPARE(other.rotor1BeamwidthDeg(), 25.0);
    QCOMPARE(other.rotor2BeamwidthDeg(), 40.0);
    QCOMPARE(changed.count(), 0); // applying stored preferences is not a change to store again
    // Garbage is ignored, a fresh-install default line works.
    other.applyPreferencesText(QStringLiteral("nonsense;=;view=;bw1=abc"));
    QCOMPARE(other.rotor1BeamwidthDeg(), 25.0);
    other.applyPreferencesText(QStringLiteral("view=radar;grid=0;borders=0;cities=0"));
    QCOMPARE(other.view(), MapWidget::View::Radar);
    QVERIFY(!other.bordersLayerVisible());
}

void TestMapWidgetLive::horizonProfileAndSecondAntennasPaintCleanlyInBothViews()
{
    MapWidget widget;
    widget.resize(774, 510);
    widget.setOwnGrid(QStringLiteral("JN67VV"));
    widget.setOwnLabel(QStringLiteral("OE5SOS"));
    QVector<double> profile(360, 0.0);
    for (int deg = 120; deg < 200; ++deg) {
        profile[deg] = 5.0;
    }
    widget.setHorizonProfile(profile);
    QCOMPARE(widget.horizonProfile().size(), 360);
    widget.setHorizonProfile(QVector<double>(10, 1.0)); // wrong length: rejected
    QVERIFY(widget.horizonProfile().isEmpty());
    widget.setHorizonProfile(profile);
    widget.setRotor1Heading(true, 322.0, QStringLiteral("2m"));
    widget.setRotor1SecondAntenna(true, 45.0);
    widget.setRotor2Heading(true, 140.0, QStringLiteral("70cm"));
    widget.setScoreSummary(47, 9812, QStringLiteral("DJ5AR 469 km"));
    MapWidget::Station open;
    open.callsign = QStringLiteral("DL0GTH");
    open.grid = QStringLiteral("JO50JP");
    MapWidget::Station worked;
    worked.callsign = QStringLiteral("OE3XYZ");
    worked.grid = QStringLiteral("JN88TC");
    worked.worked = true;
    worked.workedAtUtc = QDateTime::currentDateTimeUtc().addSecs(-600);
    widget.setStations({open, worked});
    QVERIFY(!widget.grab().isNull());
    widget.setView(MapWidget::View::MapHorizon);
    QVERIFY(!widget.horizonStripRectForTest().isEmpty());
    QVERIFY(!widget.grab().isNull());
    widget.setHorizonLayerVisible(false);
    QVERIFY(widget.horizonStripRectForTest().isEmpty());
    QVERIFY(!widget.grab().isNull());
    widget.setFitToWindowEnabled(false);
    QVERIFY(!widget.grab().isNull());
}

void TestMapWidgetLive::clickingASkylineTickActivatesTheStation()
{
    MapWidget widget;
    widget.resize(774, 510);
    widget.setOwnGrid(QStringLiteral("JN67VV"));
    widget.setView(MapWidget::View::MapHorizon);
    widget.setVisibleRangeKm(400.0);
    MapWidget::Station station;
    station.callsign = QStringLiteral("DL0GTH");
    station.grid = QStringLiteral("JO50JP"); // roughly north-west
    station.freqHz = 144300000;
    widget.setStations({station});
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    const QRectF strip = widget.horizonStripRectForTest();
    QVERIFY(!strip.isEmpty());
    // The tick's x follows the bearing across the plot (30 px in from
    // the strip's left, 36 px narrower than the strip -- see
    // MapWidget::drawHorizonStrip).
    const double bearing = calculateBearingInDegrees(QStringLiteral("JN67VV"), QStringLiteral("JO50JP"));
    const QRectF plot(strip.left() + 30.0, strip.top() + 22.0, strip.width() - 36.0, strip.height() - 40.0);
    const QPoint at(static_cast<int>(plot.left() + plot.width() * bearing / 360.0), static_cast<int>(plot.center().y()));
    QSignalSpy spy(&widget, &MapWidget::candidateActivated);
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, at);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("DL0GTH"));
    // Far from any tick: nothing.
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(at.x() + 60, at.y()));
    QCOMPARE(spy.count(), 0);
}

void TestMapWidgetLive::secondAntennaMenuEntryReportsTheStationSetting()
{
    MapWidget widget;
    QSignalSpy toggled(&widget, &MapWidget::secondAntennaToggled);
    // Fed from the settings: the menu entry follows, no signal.
    widget.setRotor2SecondAntenna(true, 45.0);
    QCOMPARE(toggled.count(), 0);
    QAction* rotor2 = nullptr;
    for (QAction* action : widget.findChildren<QAction*>()) {
        if (action->text().startsWith(QStringLiteral("Rotor 2: Zweitantenne"))) {
            rotor2 = action;
        }
    }
    QVERIFY(rotor2);
    QVERIFY(rotor2->isChecked());
    QVERIFY(rotor2->text().contains(QStringLiteral("+45°")));
    // Operator unticks it: reported once, so MainWindow can store it.
    rotor2->setChecked(false);
    QCOMPARE(toggled.count(), 1);
    QCOMPARE(toggled.first().at(0).toInt(), 2);
    QCOMPARE(toggled.first().at(1).toBool(), false);
    // Not a map preference: the preferences text does not carry it.
    QVERIFY(!widget.preferencesText().contains(QStringLiteral("second")));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int status = 0;

    TestMapWidgetProjection projectionTest;
    status |= QTest::qExec(&projectionTest, argc, argv);

    TestMapWidgetLive liveTest;
    status |= QTest::qExec(&liveTest, argc, argv);

    return status;
}
#include "test_mapwidget.moc"
