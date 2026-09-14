#pragma once

// Contestprogramm-original (2026-09-12), for MapWidget's "Städte" layer
// (operator: "auch bitte die wichtigsten großen städte ab 150 km" --
// the most important major cities, from 150 km distance onward, as
// reference points on the map). Loads real city name/position/
// population data from resources/geo/cities_110m.dat -- see that
// file's own header comment for provenance (Natural Earth 1:110m
// populated places, public domain) and exact text format. The 150 km
// distance-from-home cutoff and the "which cities count as important"
// selection are NOT baked into this data (it carries every city
// Natural Earth's own 1:110m-scale curation includes) -- MapWidget's
// drawCitiesLayer() applies the distance filter live, off the
// operator's own current QTH, the same way every other layer here
// computes its own live bearing/distance from ContestSettings::ownGrid
// rather than a precomputed value.

#include <QByteArray>
#include <QString>
#include <QVector>

namespace Contestprogramm {

struct CityPoint {
    QString name;
    double lat = 0.0;
    double lon = 0.0;
    qint64 popMax = 0;
    bool isCapital = false;
};

// Parses the plain-text format resources/geo/cities_110m.dat uses: one
// city per non-comment, non-blank line, tab-separated
// "lon\tlat\tpop_max\tis_capital(0/1)\tname". Pure function, no file
// I/O -- same "pure parse, thin I/O wrapper" split
// core/CountryBorders.h's parseCountryBordersData() already
// established, directly unit-testable with an inline sample. A
// malformed line is skipped rather than aborting the whole file.
QVector<CityPoint> parseCitiesData(const QByteArray& data);

// Finds resources/geo/cities_110m.dat next to the built binary,
// falling back to the source tree in a dev build -- the same
// besideBinary/besideSource search loadCountryBorders() already
// established -- and parses it. Returns an empty vector (not a guess)
// if the file cannot be found/read; MapWidget::drawCitiesLayer() then
// simply draws nothing, the same "no data, no layer" degradation every
// other layer here already has for missing input.
QVector<CityPoint> loadCities();

} // namespace Contestprogramm
