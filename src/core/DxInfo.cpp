#include "core/DxInfo.h"

#include "core/BeamHeading.h"
#include "core/Maidenhead.h"
#include "core/SolarPosition.h"

#include <QLocale>
#include <QStringList>
#include <QTimeZone>

#include <cmath>

namespace Contestprogramm {

namespace {

// Ab hier lohnt der lange Weg als zweite Möglichkeit -- dieselbe
// Schwelle wie an der Rotorscheibe (ui/RotorWidget.cpp).
constexpr double kLongPathFromKm = 5000.0;

QString hhmm(const QDateTime& time)
{
    return time.toString(QStringLiteral("HH:mm"));
}

// Dieselbe Gruppierung wie auf der Karte: 14 047 statt 14047 -- bei
// vierstelligen Entfernungen liest man sie sonst falsch.
QString groupedKm(double km)
{
    return QLocale(QLocale::German, QLocale::Austria).toString(static_cast<qint64>(std::llround(km)));
}

} // namespace

DxInfo lookupDxInfo(const CountryPrefixIndex& countries,
                    const QString& ownGrid,
                    const QString& callsign,
                    const QString& grid,
                    const QDateTime& nowUtc)
{
    DxInfo info;
    if (callsign.trimmed().isEmpty()) {
        return info;
    }

    const CountryEntry country = countries.lookup(callsign);
    // Der Ort: der getauschte Locator, wenn es einen gibt, sonst der
    // Mittelpunkt des Landes. Genau in der Reihenfolge -- ein Locator
    // schlägt immer einen Landesmittelpunkt.
    double dxLat = 0.0;
    double dxLon = 0.0;
    bool havePosition = false;
    if (isValidGridSquare(grid)) {
        calculateLatLonFromGridSquare(grid, dxLat, dxLon);
        havePosition = true;
    } else if (country.isValid()) {
        dxLat = country.latitudeDeg;
        dxLon = country.longitudeDeg;
        havePosition = true;
        info.approximate = true;
    }
    if (!country.isValid() && !havePosition) {
        return info;
    }

    info.known = true;
    info.countryName = country.name;
    info.primaryPrefix = country.primaryPrefix;
    info.continent = country.continent;
    info.cqZone = country.cqZone;

    if (havePosition && isValidGridSquare(ownGrid)) {
        double homeLat = 0.0;
        double homeLon = 0.0;
        calculateLatLonFromGridSquare(ownGrid, homeLat, homeLon);
        info.bearingDeg = calculateBearingInDegreesBetween(homeLat, homeLon, dxLat, dxLon);
        info.distanceKm = calculateDistanceKmBetween(homeLat, homeLon, dxLat, dxLon);
        info.longPathBearingDeg = BeamHeading::longPath(info.bearingDeg);
        info.longPathKm = BeamHeading::longPathDistanceKm(info.distanceKm);
    }

    if (havePosition) {
        const SunTimes sun = sunTimes(nowUtc, dxLat, dxLon);
        info.polarDay = sun.kind == SunTimes::Kind::AlwaysUp;
        info.polarNight = sun.kind == SunTimes::Kind::AlwaysDown;
        info.sunriseUtc = sun.riseUtc;
        info.sunsetUtc = sun.setUtc;
    }
    if (country.isValid()) {
        info.localTime = nowUtc.toUTC().addSecs(static_cast<qint64>(country.utcOffsetHours * 3600.0));
    }
    return info;
}

QString DxInfo::statusLine() const
{
    if (!known) {
        return QString();
    }
    QStringList parts;
    if (!countryName.isEmpty()) {
        parts << (primaryPrefix.isEmpty() ? countryName
                                          : QStringLiteral("%1 (%2)").arg(countryName, primaryPrefix));
    }
    if (distanceKm > 0.0) {
        // Die Tilde sagt, dass der Ort der Mittelpunkt eines Landes ist
        // und nicht ein getauschter Locator -- dieselbe Ehrlichkeit wie
        // der gepunktete Hof auf der Karte.
        parts << QStringLiteral("%1° · %2%3 km")
                     .arg(bearingDeg, 0, 'f', 0)
                     .arg(approximate ? QStringLiteral("~") : QString(), groupedKm(distanceKm));
        if (distanceKm >= kLongPathFromKm) {
            parts << QStringLiteral("lang %1° · %2 km")
                         .arg(longPathBearingDeg, 0, 'f', 0)
                         .arg(groupedKm(longPathKm));
        }
    }
    if (localTime.isValid()) {
        parts << QStringLiteral("dort %1").arg(hhmm(localTime));
    }
    if (polarDay) {
        parts << QStringLiteral("Sonne geht nicht unter");
    } else if (polarNight) {
        parts << QStringLiteral("Sonne geht nicht auf");
    } else if (sunriseUtc.isValid() && sunsetUtc.isValid()) {
        parts << QStringLiteral("Sonne %1–%2Z").arg(hhmm(sunriseUtc), hhmm(sunsetUtc));
    }
    return parts.join(QStringLiteral("  ·  "));
}

} // namespace Contestprogramm
