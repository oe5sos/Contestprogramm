#include "core/SolarPosition.h"

#include <QTimeZone>
#include <QtMath>   // definiert M_PI auch dort, wo <cmath> das nicht tut (MSVC)

#include <cmath>

namespace Contestprogramm {

namespace {

constexpr double kDegToRad = M_PI / 180.0;
constexpr double kRadToDeg = 180.0 / M_PI;
// Derselbe Erdradius wie in core/Maidenhead.cpp -- zwei verschiedene
// Erden in einem Programm wären eine Fehlerquelle ohne Gewinn.
constexpr double kEarthRadiusKm = 6371.0;

double normalizeDegrees(double degrees, double lower, double upper)
{
    const double span = upper - lower;
    double value = std::fmod(degrees - lower, span);
    if (value < 0.0) {
        value += span;
    }
    return value + lower;
}

} // namespace

SolarPoint subsolarPoint(const QDateTime& utc)
{
    const QDateTime t = utc.toUTC();
    // Julianische Tage seit J2000,0 (2000-01-01 12:00 UTC).
    const double julianDay = static_cast<double>(t.toMSecsSinceEpoch()) / 86400000.0 + 2440587.5;
    const double n = julianDay - 2451545.0;

    const double meanLongitude = normalizeDegrees(280.460 + 0.9856474 * n, 0.0, 360.0);
    const double meanAnomaly = normalizeDegrees(357.528 + 0.9856003 * n, 0.0, 360.0) * kDegToRad;
    // Mittelpunktsgleichung: aus der mittleren wird die wahre Länge.
    const double eclipticLongitude =
        (meanLongitude + 1.915 * std::sin(meanAnomaly) + 0.020 * std::sin(2.0 * meanAnomaly)) * kDegToRad;
    const double obliquity = (23.439 - 0.0000004 * n) * kDegToRad;

    const double declination = std::asin(std::sin(obliquity) * std::sin(eclipticLongitude));
    const double rightAscension =
        std::atan2(std::cos(obliquity) * std::sin(eclipticLongitude), std::cos(eclipticLongitude));

    // Sternzeit von Greenwich in Stunden, daraus die Länge, über der
    // die Sonne steht.
    const double gmstHours = normalizeDegrees(18.697374558 + 24.06570982441908 * n, 0.0, 24.0);
    const double longitude = normalizeDegrees(rightAscension * kRadToDeg - gmstHours * 15.0, -180.0, 180.0);

    SolarPoint point;
    point.latitudeDeg = declination * kRadToDeg;
    point.longitudeDeg = longitude;
    return point;
}

double terminatorRadiusKm()
{
    return kEarthRadiusKm * M_PI / 2.0;
}

SunTimes sunTimes(const QDateTime& day, double latitudeDeg, double longitudeDeg)
{
    SunTimes times;
    const QDate date = day.toUTC().date();

    // Wahrer Mittag: der Zeitpunkt, an dem die Sonne über DIESER Länge
    // steht. Start beim mittleren Mittag, dann zwei Näherungsschritte --
    // die Sonne wandert 15 Grad je Stunde, die Zeitgleichung macht
    // höchstens gut vier Grad aus.
    QDateTime noon(date, QTime(12, 0), QTimeZone::utc());
    noon = noon.addSecs(static_cast<qint64>(std::llround(-longitudeDeg / 15.0 * 3600.0)));
    for (int step = 0; step < 2; ++step) {
        const double subsolarLon = subsolarPoint(noon).longitudeDeg;
        double error = subsolarLon - longitudeDeg;   // Grad, die die Sonne noch braucht
        error = normalizeDegrees(error, -180.0, 180.0);
        noon = noon.addSecs(static_cast<qint64>(std::llround(error / 15.0 * 3600.0)));
    }
    times.noonUtc = noon;

    // Stundenwinkel für den Horizont: -0,833 Grad deckt Refraktion und
    // den halben Sonnendurchmesser ab, so rechnen es die Ephemeriden.
    const double declination = subsolarPoint(noon).latitudeDeg * kDegToRad;
    const double latitude = latitudeDeg * kDegToRad;
    const double zenith = -0.833 * kDegToRad;
    const double cosHourAngle = (std::sin(zenith) - std::sin(latitude) * std::sin(declination))
        / (std::cos(latitude) * std::cos(declination));
    if (cosHourAngle < -1.0) {
        times.kind = SunTimes::Kind::AlwaysUp;
        return times;
    }
    if (cosHourAngle > 1.0) {
        times.kind = SunTimes::Kind::AlwaysDown;
        return times;
    }
    const double hourAngleDeg = std::acos(cosHourAngle) * kRadToDeg;
    const qint64 halfDaySecs = static_cast<qint64>(std::llround(hourAngleDeg / 15.0 * 3600.0));
    times.riseUtc = noon.addSecs(-halfDaySecs);
    times.setUtc = noon.addSecs(halfDaySecs);
    return times;
}

} // namespace Contestprogramm
