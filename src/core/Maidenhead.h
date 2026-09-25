// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Contestprogramm - Maidenhead locator geometry: grid square to lat/lon,
// great-circle distance, initial bearing.
//
// Ported from freedv-gui src/gui/dialogs/freedv_reporter.cpp:2312-2410
// (calculateDistance_ / calculateLatLonFromGridSquare_ /
// calculateBearingInDegrees_ / DegreesToRadians_ / RadiansToDegrees_)
// [@77e793a]. Haversine great-circle distance + initial bearing.
//
// License (upstream): freedv-gui carries an LGPLv2.1+ root license
// (`freedv-gui/COPYING`); the specific `freedv_reporter.cpp` file has no
// per-file Copyright header, so the project root license applies.
//
// Copyright (C) 2026 Contestprogramm contributors.
// Distance / heading math: derived from freedv-gui source (LGPLv2.1+,
// copyright the freedv-gui contributors / FreeDV project).
//
// This port passed through an intermediate stop: Longpath (fork of
// NereusSDR, itself © J.J. Boyd / KG4VCF), whose src/core/Maidenhead.h
// hoisted the same freedv-gui-derived functions out of
// FreeDVStationModel.cpp on 2026-08-07 with no change to the math.
// Contestprogramm's copy is taken from that already-hoisted form. See
// NOTICE.md for the full attribution chain.
//
// Modification history
//   2026-09-09  Ralph Martin Fischer  Ported into Contestprogramm from
//                                     Longpath/NereusSDR src/core/Maidenhead.{h,cpp}.
//                                     Namespace changed Longpath -> Contestprogramm;
//                                     no maths changed. AI tooling: Anthropic
//                                     Claude Code.

#pragma once

#include <QString>

namespace Contestprogramm {

// Centre of the given Maidenhead square. Accepts 4- or 6-character
// locators; anything shorter leaves the outputs untouched. Takes the
// locator by value because the implementation upper-cases it in place
// (freedv-gui's `gridSquare.MakeUpper()`).
void calculateLatLonFromGridSquare(QString gridSquare,
                                   double& lat, double& lon);

// Great-circle distance between two Maidenhead squares, in kilometres.
double calculateDistanceKm(const QString& gridSquare1,
                           const QString& gridSquare2);

// Initial bearing from square 1 to square 2, in degrees true (0-360).
double calculateBearingInDegrees(const QString& gridSquare1,
                                 const QString& gridSquare2);

// A locator is usable for distance/bearing from four characters on:
// two letters, two digits, optionally two more letters.
bool isValidGridSquare(const QString& gridSquare);

// Ein vollstaendiger, sechsstelliger Locator (JN67VV). Die IARU-R1-
// Regeln verlangen ihn im Austausch (GC 2023, 1.9.1: "the complete QTH
// locator (6 digit)"); ein vierstelliger reicht fuer die Wertung nicht.
bool isFullLocator(const QString& gridSquare);

// Die Entfernung FUER DIE WERTUNG, nach IARU R1 VHF+ (GC 2023) 1.10.1:
// Mittelpunkt jedes Locatorfelds, Kugelgeometrie, und "for the
// conversion from degrees to kilometres a factor of 111.2 should be
// used". Nicht dasselbe wie calculateDistanceKm (Erdradius 6371 km, fuer
// Karte und Anzeige) -- der Unterschied ist klein, reicht aber, damit
// jede ~15. Verbindung einen Punkt zu wenig beansprucht. -1 ohne zwei
// gueltige Locatoren.
double iaruQrbKm(const QString& grid1, const QString& grid2);

// Inverse of calculateLatLonFromGridSquare: the 6-character square
// containing the given position. Longitude is positive east.
QString gridSquareFromLatLon(double lat, double lon);

// Great-circle distance between two raw lat/lon points, in kilometres --
// the same Haversine formula calculateDistanceKm() uses, factored out
// for callers that already hold coordinates rather than a grid square
// string (e.g. MapWidget's per-grid-square-corner projection, which
// needs a square corner's distance/bearing from home, and a corner is
// not itself a valid grid locator). Contestprogramm addition
// (2026-09-10), not part of the original freedv-gui port -- see
// calculateDistanceKm()'s own header comment for the ported formula
// this delegates to; no maths changed, only factored so it is callable
// without a grid square string on either end.
double calculateDistanceKmBetween(double lat1, double lon1, double lat2, double lon2);

// Initial bearing between two raw lat/lon points, in degrees true
// (0-360). Same rationale as calculateDistanceKmBetween() above.
double calculateBearingInDegreesBetween(double lat1, double lon1, double lat2, double lon2);

// The point at `fraction` (0 = point1, 1 = point2, clamp not enforced --
// callers pass values already known to be in [0,1]) along the
// great-circle path between two raw lat/lon points. Ed Williams'
// Aviation Formulary "intermediate points" formula
// (edwilliams.org/avform147.htm), the standard reference most GPS/nav
// software uses for this -- not part of the original freedv-gui port.
// Contestprogramm addition (2026-09-12), for core/terrain/PathProfile's
// own need to sample elevation along the great-circle line between two
// stations, not just at its two endpoints.
void intermediatePointOnGreatCircle(double lat1, double lon1, double lat2, double lon2, double fraction,
                                     double& latOut, double& lonOut);

// The destination point reached by travelling `distanceKm` along
// initial bearing `bearingDeg` (degrees true, 0-360) from (lat1, lon1)
// -- the "direct"/"destination point given distance and bearing"
// problem, the geometric inverse of calculateBearingInDegreesBetween()
// + calculateDistanceKmBetween() together (those two answer "where is
// point 2, given point 1 and point 2"; this answers "where do I end up,
// given point 1, a bearing, and a distance"). Standard great-circle
// destination-point formula (Ed Williams' Aviation Formulary /
// Movable Type's "destination point given distance and bearing from
// start point" -- the same family of reference as
// intermediatePointOnGreatCircle()'s own formula). Contestprogramm
// addition (2026-09-12), for the rotor compass's own terrain-sector
// sweep (core/terrain/TerrainDataManager::sectorSweep()): there is no
// real station at a bare bearing, so a sweep needs to synthesize a
// point in that direction at a fixed reference distance to classify.
void destinationPoint(double lat1, double lon1, double bearingDeg, double distanceKm, double& latOut,
                       double& lonOut);

} // namespace Contestprogramm
