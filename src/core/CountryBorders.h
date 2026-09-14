#pragma once

// Contestprogramm-original (2026-09-12), for MapWidget's new "Grenzen"
// layer (operator: "kannst du in die karte/verbindungen auch die
// umrisse der staaten einzeichen, sodass ich diese ein uns ausblenden
// kann"). Loads real country border/coastline polylines from
// resources/geo/country_borders_110m.dat -- see that file's own header
// comment for provenance (Natural Earth 1:110m, public domain) and
// exact text format. No coordinates in this file are invented: every
// point traces back to that dataset, simplified only for point count,
// never approximated in shape (HAUSSTIL rule 7 applies to map data the
// same as any other displayed value).
//
// Per-ring country name (2026-09-13, operator: "würde aber schon die
// ländergrenzen richtig einzeichnen und diese dann auch pro land
// markieren"): rather than a blind worldwide re-fetch-and-relabel of
// all 288 rings (most of which never appear on a VHF/UHF contest map
// anyway), only the rings for the countries actually reachable within
// a realistic contest range of Central Europe were identified -- each
// by verified bounding-box match against that country's real known
// extent (see resources/geo/country_borders_110m.dat's own header for
// the exact line numbers and the reasoning per match). A ring with no
// verified name stays unnamed rather than guessed; MapWidget draws its
// outline the same as before, just without a label.

#include <QByteArray>
#include <QPointF>
#include <QString>
#include <QVector>

namespace Contestprogramm {

// One border/coastline ring, optionally attributed to a country.
// `name` is a German exonym (matches core/Cities.h's NAME_DE
// convention) when verified, empty when not (an unlabeled coastline
// segment or a ring too far away to have been worth identifying).
struct CountryBorderRing {
    QString name;
    QVector<QPointF> points;
};

// Parses the plain-text ring format resources/geo/country_borders_110m.dat
// uses: one closed ring (a country border segment or coastline) per
// non-comment, non-blank line, optionally "Name\tlon,lat lon,lat ..."
// (tab-separated name prefix) or plain "lon,lat lon,lat ..." when no
// name was verified for that ring. Pure function, no file I/O -- same
// "pure parse, thin I/O wrapper" split WeatherClient::parseOpenMeteoResponse
// already established, so this is directly unit-testable with an inline
// sample. Points come back as (lon, lat) -- QPointF::x() is longitude,
// y() is latitude, matching the source GeoJSON's own [lon, lat] axis
// order, NOT the (lat, lon) order Maidenhead.h's own functions take. A
// malformed line/pair is skipped rather than aborting the whole file
// (one bad ring should not blank the entire layer); a ring left with
// fewer than 2 points after skipping is dropped entirely (its name, if
// any, is dropped with it).
QVector<CountryBorderRing> parseCountryBordersData(const QByteArray& data);

// Finds resources/geo/country_borders_110m.dat next to the built
// binary, falling back to the source tree in a dev build -- the same
// besideBinary/besideSource search AppController::findContestDefinitionsDir()
// already established for resources/contest_definitions/*.json -- and
// parses it. Returns an empty vector (not a guess/placeholder shape) if
// the file cannot be found or read; MapWidget::drawBordersLayer() then
// simply draws nothing, the same "no data, no layer" degradation its
// other layers already have for a missing own-grid.
QVector<CountryBorderRing> loadCountryBorders();

} // namespace Contestprogramm
