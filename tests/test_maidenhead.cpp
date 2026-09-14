// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Tests for the ported Maidenhead locator geometry. See
// src/core/Maidenhead.h / NOTICE.md for provenance.

#include <QtTest>

#include "core/Maidenhead.h"

using namespace Contestprogramm;

class TestMaidenhead : public QObject
{
    Q_OBJECT

private slots:
    void sameGridDistanceIsZero();
    void knownPairDistanceAndBearingEurope();
    void knownPairDistanceAndBearingTransatlantic();
    void gridRoundTrip();
    void gridValidation();
    void intermediatePointAlongGreatCircle();
    void destinationPointFromDistanceAndBearing();
};

void TestMaidenhead::sameGridDistanceIsZero()
{
    QCOMPARE(calculateDistanceKm(QStringLiteral("JN77QT"), QStringLiteral("JN77QT")), 0.0);
}

void TestMaidenhead::knownPairDistanceAndBearingEurope()
{
    // JN77QT and JN88TC are two European VHF grid squares one field-digit
    // apart in both axes (7->8 in both the longitude and latitude square
    // digit), so the true separation is mostly eastward with a small
    // northward component -- a bearing just under due east is expected.
    // Cross-checked by independently re-implementing the same
    // Haversine/initial-bearing formulas in Python (transcribed
    // separately from this C++ file, not derived from it), which gives
    // 170.65 km / 78.21 degrees for this pair.
    const double distance = calculateDistanceKm(QStringLiteral("JN77QT"), QStringLiteral("JN88TC"));
    const double bearing = calculateBearingInDegrees(QStringLiteral("JN77QT"), QStringLiteral("JN88TC"));
    QVERIFY(qAbs(distance - 170.65) < 0.5);
    QVERIFY(bearing > 70.0 && bearing < 85.0);
}

void TestMaidenhead::knownPairDistanceAndBearingTransatlantic()
{
    // FN31pr (ARRL HQ, Newington CT) to IO91 (southern England / London
    // area) is a commonly cited real-world long-path distance reference.
    // The real great-circle distance is on the order of 5500-5700 km
    // with an initial bearing in the low 50s degrees (northeasterly --
    // the great-circle route from New England to the UK bows north). A
    // spherical-earth Haversine calculation (what this code, and
    // freedv-gui's original, both compute) comes in a little short of
    // that because it ignores the earth's oblateness; independently
    // cross-checked in Python at 5414.7 km / 52.2 degrees, so the
    // assertions below use a tolerant band around that.
    const double distance = calculateDistanceKm(QStringLiteral("FN31PR"), QStringLiteral("IO91WM"));
    const double bearing = calculateBearingInDegrees(QStringLiteral("FN31PR"), QStringLiteral("IO91WM"));
    QVERIFY(distance > 5000.0 && distance < 5800.0);
    QVERIFY(bearing > 40.0 && bearing < 65.0);
}

void TestMaidenhead::gridRoundTrip()
{
    struct Case { double lat; double lon; };
    const Case cases[] = {
        {47.8, 15.4},
        {51.5, -0.1},
        {-33.9, 151.2},
        {0.0, 0.0},
    };
    for (const auto& c : cases) {
        const QString grid = gridSquareFromLatLon(c.lat, c.lon);
        QCOMPARE(grid.size(), 6);
        double lat = 0.0;
        double lon = 0.0;
        calculateLatLonFromGridSquare(grid, lat, lon);
        // A 6-character grid square is about 5 x 2.5 km; reconstructing
        // its centre from the original coordinate can be off by up to
        // roughly half a square edge (~0.04 deg lon / 0.02 deg lat).
        QVERIFY(qAbs(lat - c.lat) < 0.1);
        QVERIFY(qAbs(lon - c.lon) < 0.1);
    }
}

void TestMaidenhead::gridValidation()
{
    QVERIFY(isValidGridSquare(QStringLiteral("JN77")));
    QVERIFY(isValidGridSquare(QStringLiteral("jn77qt")));
    QVERIFY(!isValidGridSquare(QStringLiteral("JN7")));
    QVERIFY(!isValidGridSquare(QStringLiteral("")));
    QVERIFY(!isValidGridSquare(QStringLiteral("ZZ77")));
}

// intermediatePointOnGreatCircle() -- Contestprogramm addition
// (2026-09-12) for core/terrain/PathProfile's own need to sample
// elevation along the great-circle line between two stations.
void TestMaidenhead::intermediatePointAlongGreatCircle()
{
    double lat = 0.0;
    double lon = 0.0;

    // Points on the equator: the equator itself is a great circle, so
    // the intermediate point is trivially verifiable by inspection (the
    // midpoint of an equatorial arc is just the average longitude, same
    // latitude) -- a real independent-of-this-code geometric fact, not
    // a value derived by running this function.
    intermediatePointOnGreatCircle(0.0, 0.0, 0.0, 10.0, 0.5, lat, lon);
    QVERIFY(qAbs(lat - 0.0) < 1e-6);
    QVERIFY(qAbs(lon - 5.0) < 1e-6);

    // Points on the same meridian: same reasoning -- the midpoint is
    // the average latitude, same longitude.
    intermediatePointOnGreatCircle(10.0, 20.0, 30.0, 20.0, 0.5, lat, lon);
    QVERIFY(qAbs(lat - 20.0) < 1e-6);
    QVERIFY(qAbs(lon - 20.0) < 1e-6);

    // fraction=0 and fraction=1 must return the endpoints exactly (up
    // to floating-point rounding), for an arbitrary (non-degenerate)
    // pair.
    intermediatePointOnGreatCircle(47.7, 13.7, 51.5, -0.1, 0.0, lat, lon);
    QVERIFY(qAbs(lat - 47.7) < 1e-6);
    QVERIFY(qAbs(lon - 13.7) < 1e-6);
    intermediatePointOnGreatCircle(47.7, 13.7, 51.5, -0.1, 1.0, lat, lon);
    QVERIFY(qAbs(lat - 51.5) < 1e-6);
    QVERIFY(qAbs(lon - (-0.1)) < 1e-6);

    // Coincident points: fraction is meaningless without a real path --
    // must not divide by zero or crash, and returns point1 (see the
    // function's own doc comment).
    intermediatePointOnGreatCircle(47.7, 13.7, 47.7, 13.7, 0.5, lat, lon);
    QVERIFY(qAbs(lat - 47.7) < 1e-6);
    QVERIFY(qAbs(lon - 13.7) < 1e-6);
}

// destinationPoint() -- Contestprogramm addition (2026-09-12) for the
// rotor compass's own terrain-sector sweep.
void TestMaidenhead::destinationPointFromDistanceAndBearing()
{
    double lat = 0.0;
    double lon = 0.0;

    // Due north from the equator by ~111.19 km (1 degree of latitude
    // for a sphere of radius 6371 km) should land at approximately
    // (1.0, 0.0) -- a real, independently-verifiable geometric fact,
    // not a value derived by running this function.
    destinationPoint(0.0, 0.0, 0.0, 111.19, lat, lon);
    QVERIFY(qAbs(lat - 1.0) < 0.01);
    QVERIFY(qAbs(lon - 0.0) < 0.01);

    // Due east from the equator by the same distance should land at
    // approximately (0.0, 1.0) -- same reasoning, the equator is
    // itself a great circle of the same radius.
    destinationPoint(0.0, 0.0, 90.0, 111.19, lat, lon);
    QVERIFY(qAbs(lat - 0.0) < 0.01);
    QVERIFY(qAbs(lon - 1.0) < 0.01);

    // Round-trip consistency for an arbitrary point/bearing/distance:
    // travelling out and re-measuring the distance/bearing back to the
    // start must reproduce what was travelled -- this exercises the
    // formula independently of any single hand-picked geometric special
    // case above.
    const double startLat = 47.7;
    const double startLon = 13.7;
    const double bearingDeg = 205.0;
    const double distanceKm = 83.0;
    destinationPoint(startLat, startLon, bearingDeg, distanceKm, lat, lon);
    const double measuredDistance = calculateDistanceKmBetween(startLat, startLon, lat, lon);
    const double measuredBearing = calculateBearingInDegreesBetween(startLat, startLon, lat, lon);
    QVERIFY(qAbs(measuredDistance - distanceKm) < 0.1);
    QVERIFY(qAbs(measuredBearing - bearingDeg) < 0.1);
}

QTEST_APPLESS_MAIN(TestMaidenhead)
#include "test_maidenhead.moc"
