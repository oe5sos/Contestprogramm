// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Contestprogramm - Maidenhead locator geometry. Implementation.
//
// Ported from freedv-gui src/gui/dialogs/freedv_reporter.cpp:2312-2410
// (calculateDistance_ / calculateLatLonFromGridSquare_ /
// calculateBearingInDegrees_ / DegreesToRadians_ / RadiansToDegrees_)
// [@77e793a], by way of Longpath/NereusSDR src/core/Maidenhead.h +
// src/models/FreeDVStationModel.cpp (hoisted there 2026-08-07). See
// Maidenhead.h and NOTICE.md for the full license/attribution chain.
//
// Copyright (C) 2026 Contestprogramm contributors.
// Distance / heading math: derived from freedv-gui source (LGPLv2.1+,
// copyright the freedv-gui contributors / FreeDV project).

#include "Maidenhead.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace Contestprogramm {

namespace {

// From freedv-gui src/gui/dialogs/freedv_reporter.cpp:2401-2404 [@77e793a]
inline double DegreesToRadians(double degrees)
{
    return degrees * (M_PI / 180.0);
}

// From freedv-gui src/gui/dialogs/freedv_reporter.cpp:2406-2410 [@77e793a]
inline double RadiansToDegrees(double radians)
{
    auto result = (radians > 0 ? radians : (2 * M_PI + radians)) * 360 / (2 * M_PI);
    return (result == 360) ? 0 : result;
}

} // namespace

// From freedv-gui src/gui/dialogs/freedv_reporter.cpp:2336-2374 [@77e793a]
void calculateLatLonFromGridSquare(QString gridSquare, double& lat, double& lon)
{
    const char charA = 'A';
    const char char0 = '0';

    // Uppercase grid square for easier processing
    gridSquare = gridSquare.toUpper();

    // Start from antimeridian South Pole (e.g. over the Pacific, not over the UK)
    lon = -180.0;
    lat = -90.0;

    if (gridSquare.size() < 4) {
        return;
    }

    // Process first two characters
    lon += (gridSquare.at(0).toLatin1() - charA) * 20;
    lat += (gridSquare.at(1).toLatin1() - charA) * 10;

    // Then next two
    lon += (gridSquare.at(2).toLatin1() - char0) * 2;
    lat += (gridSquare.at(3).toLatin1() - char0) * 1;

    // If grid square is 6 or more letters, THEN use the next two.
    // Otherwise, optional.
    QString optionalSegment = gridSquare.mid(4, 2);
    const bool sixCharSubSquare =
        gridSquare.size() >= 6
        && optionalSegment.size() == 2
        && optionalSegment.at(0).isLetter()
        && optionalSegment.at(1).isLetter();
    if (sixCharSubSquare) {
        lon += (gridSquare.at(4).toLatin1() - charA) * 5.0 / 60;
        lat += (gridSquare.at(5).toLatin1() - charA) * 2.5 / 60;

        // Center in middle of grid square
        lon += 5.0 / 60 / 2;
        lat += 2.5 / 60 / 2;
    } else {
        lon += 2 / 2;
        lat += 1.0 / 2;
    }
}

// From freedv-gui src/gui/dialogs/freedv_reporter.cpp:2312-2334 [@77e793a],
// factored to delegate to the lat/lon form below -- no maths changed.
double calculateDistanceKm(const QString& gridSquare1, const QString& gridSquare2)
{
    double lat1 = 0;
    double lon1 = 0;
    double lat2 = 0;
    double lon2 = 0;

    // Grab latitudes and longitudes for the two locations.
    calculateLatLonFromGridSquare(gridSquare1, lat1, lon1);
    calculateLatLonFromGridSquare(gridSquare2, lat2, lon2);

    return calculateDistanceKmBetween(lat1, lon1, lat2, lon2);
}

// From freedv-gui src/gui/dialogs/freedv_reporter.cpp:2376-2399 [@77e793a],
// factored to delegate to the lat/lon form below -- no maths changed.
double calculateBearingInDegrees(const QString& gridSquare1, const QString& gridSquare2)
{
    double lat1 = 0;
    double lon1 = 0;
    double lat2 = 0;
    double lon2 = 0;

    // Grab latitudes and longitudes for the two locations.
    calculateLatLonFromGridSquare(gridSquare1, lat1, lon1);
    calculateLatLonFromGridSquare(gridSquare2, lat2, lon2);

    return calculateBearingInDegreesBetween(lat1, lon1, lat2, lon2);
}

// Contestprogramm addition (2026-09-10): the Haversine distance formula
// above, factored out so a caller holding raw coordinates (not a grid
// square string) can reuse it -- see Maidenhead.h's comment on this
// function for why (MapWidget's grid-square-corner projection). Formula
// itself is unchanged from the freedv-gui-derived body above.
double calculateDistanceKmBetween(double lat1, double lon1, double lat2, double lon2)
{
    const double EARTH_RADIUS = 6371;
    double dLat = DegreesToRadians(lat2 - lat1);
    double dLon = DegreesToRadians(lon2 - lon1);
    double a =
        sin(dLat / 2) * sin(dLat / 2) +
        cos(DegreesToRadians(lat1)) * cos(DegreesToRadians(lat2)) *
        sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return EARTH_RADIUS * c;
}

// Contestprogramm addition (2026-09-10): same rationale as
// calculateDistanceKmBetween() above, for the initial-bearing formula.
double calculateBearingInDegreesBetween(double lat1, double lon1, double lat2, double lon2)
{
    lat1 = DegreesToRadians(lat1);
    lat2 = DegreesToRadians(lat2);
    lon1 = DegreesToRadians(lon1);
    lon2 = DegreesToRadians(lon2);

    double diffLongitude = lon2 - lon1;
    double x = cos(lat2) * sin(diffLongitude);
    double y = (cos(lat1) * sin(lat2)) - (sin(lat1) * cos(lat2) * cos(diffLongitude));
    double radians = atan2(x, y);

    return RadiansToDegrees(radians);
}

// Contestprogramm addition (2026-09-12): Ed Williams' Aviation
// Formulary intermediate-point formula -- see Maidenhead.h's own
// comment. Deliberately does NOT reuse this file's RadiansToDegrees()
// helper above for the lat/lon outputs: that helper wraps negative
// inputs into 0-360 (correct for a BEARING, e.g.
// calculateBearingInDegreesBetween()'s own use), which would silently
// corrupt a legitimately negative latitude/longitude (e.g. -10 deg
// longitude becoming 350). Plain qRadiansToDegrees() (QtMath, already
// included) is the correct, unwrapped conversion for a coordinate.
void intermediatePointOnGreatCircle(double lat1, double lon1, double lat2, double lon2, double fraction,
                                     double& latOut, double& lonOut)
{
    const double lat1Rad = DegreesToRadians(lat1);
    const double lon1Rad = DegreesToRadians(lon1);
    const double lat2Rad = DegreesToRadians(lat2);
    const double lon2Rad = DegreesToRadians(lon2);

    // Central angle between the two points -- the same Haversine core
    // calculateDistanceKmBetween() uses, recomputed here in radians
    // (that function returns kilometres, not the angle itself).
    const double dLat = lat2Rad - lat1Rad;
    const double dLon = lon2Rad - lon1Rad;
    const double a =
        sin(dLat / 2) * sin(dLat / 2) + cos(lat1Rad) * cos(lat2Rad) * sin(dLon / 2) * sin(dLon / 2);
    const double angularDistance = 2 * atan2(sqrt(a), sqrt(1 - a));

    if (angularDistance < 1e-12) {
        // Coincident (or effectively coincident) points -- fraction is
        // meaningless without a real path; point1 is as good an answer
        // as any.
        latOut = lat1;
        lonOut = lon1;
        return;
    }

    const double A = sin((1 - fraction) * angularDistance) / sin(angularDistance);
    const double B = sin(fraction * angularDistance) / sin(angularDistance);
    const double x = A * cos(lat1Rad) * cos(lon1Rad) + B * cos(lat2Rad) * cos(lon2Rad);
    const double y = A * cos(lat1Rad) * sin(lon1Rad) + B * cos(lat2Rad) * sin(lon2Rad);
    const double z = A * sin(lat1Rad) + B * sin(lat2Rad);

    latOut = qRadiansToDegrees(atan2(z, sqrt(x * x + y * y)));
    lonOut = qRadiansToDegrees(atan2(y, x));
}

// Contestprogramm addition (2026-09-12): the standard great-circle
// "destination point given distance and bearing from start point"
// formula -- see Maidenhead.h's own comment.
void destinationPoint(double lat1, double lon1, double bearingDeg, double distanceKm, double& latOut, double& lonOut)
{
    constexpr double kEarthRadiusKm = 6371.0;
    const double lat1Rad = DegreesToRadians(lat1);
    const double lon1Rad = DegreesToRadians(lon1);
    const double bearingRad = DegreesToRadians(bearingDeg);
    const double angularDistance = distanceKm / kEarthRadiusKm;

    const double lat2Rad = std::asin(std::sin(lat1Rad) * std::cos(angularDistance)
                                      + std::cos(lat1Rad) * std::sin(angularDistance) * std::cos(bearingRad));
    const double lon2Rad =
        lon1Rad + std::atan2(std::sin(bearingRad) * std::sin(angularDistance) * std::cos(lat1Rad),
                              std::cos(angularDistance) - std::sin(lat1Rad) * std::sin(lat2Rad));

    latOut = qRadiansToDegrees(lat2Rad);
    // Normalize longitude to -180..180 -- lon1Rad + atan2(...) can drift
    // outside that range for a bearing/distance that crosses the
    // antimeridian.
    double lonDeg = qRadiansToDegrees(lon2Rad);
    lonDeg = std::fmod(lonDeg + 180.0, 360.0);
    if (lonDeg < 0.0) {
        lonDeg += 360.0;
    }
    lonOut = lonDeg - 180.0;
}

// NereusSDR addition (2026-08-07), carried forward unchanged: the inverse
// of calculateLatLonFromGridSquare, so callers holding raw coordinates
// can reuse the grid-based distance/bearing helpers instead of growing a
// second haversine.
QString gridSquareFromLatLon(double lat, double lon)
{
    // Shift into the all-positive space the encoding assumes.
    double la = std::clamp(lat, -90.0, 90.0) + 90.0;    // 0..180
    double lo = std::fmod(lon + 180.0, 360.0);          // 0..360
    if (lo < 0.0) { lo += 360.0; }

    QString out;
    // Field: 20 deg lat, 20 deg lon, letters A..R
    const int fieldLon = static_cast<int>(lo / 20.0);
    const int fieldLat = static_cast<int>(la / 10.0);
    out += QChar(QLatin1Char('A').unicode() + std::min(fieldLon, 17));
    out += QChar(QLatin1Char('A').unicode() + std::min(fieldLat, 17));

    lo -= fieldLon * 20.0;
    la -= fieldLat * 10.0;

    // Square: 2 deg lon, 1 deg lat, digits 0..9
    const int sqLon = static_cast<int>(lo / 2.0);
    const int sqLat = static_cast<int>(la / 1.0);
    out += QChar(QLatin1Char('0').unicode() + std::min(sqLon, 9));
    out += QChar(QLatin1Char('0').unicode() + std::min(sqLat, 9));

    lo -= sqLon * 2.0;
    la -= sqLat * 1.0;

    // Sub-square: 5 min lon, 2.5 min lat, letters A..X
    const int subLon = static_cast<int>(lo / (2.0 / 24.0));
    const int subLat = static_cast<int>(la / (1.0 / 24.0));
    out += QChar(QLatin1Char('A').unicode() + std::min(subLon, 23));
    out += QChar(QLatin1Char('A').unicode() + std::min(subLat, 23));

    return out;
}

// NereusSDR addition (2026-08-07), carried forward unchanged: the callers
// of the helpers above need a cheap "is this even a locator" check
// before spending the maths.
bool isValidGridSquare(const QString& gridSquare)
{
    const QString g = gridSquare.trimmed().toUpper();
    if (g.size() != 4 && g.size() != 6) { return false; }
    if (!g.at(0).isLetter() || !g.at(1).isLetter()) { return false; }
    if (g.at(0) > QLatin1Char('R') || g.at(1) > QLatin1Char('R')) { return false; }
    if (!g.at(2).isDigit() || !g.at(3).isDigit()) { return false; }
    if (g.size() == 6) {
        if (!g.at(4).isLetter() || !g.at(5).isLetter()) { return false; }
        if (g.at(4) > QLatin1Char('X') || g.at(5) > QLatin1Char('X')) { return false; }
    }
    return true;
}

} // namespace Contestprogramm
