// core/SolarPosition.h: der Unterpunkt der Sonne, aus dem die
// Graulinie folgt. Geprüft gegen das, was jeder Sonnenstandsrechner
// für diese Zeitpunkte liefert -- Sonnenwenden, Tagundnachtgleiche und
// der Gang der Länge über den Tag.

#include <QtTest>

#include <QTimeZone>

#include "core/Maidenhead.h"
#include "core/SolarPosition.h"

using namespace Contestprogramm;

namespace {
QDateTime utc(int year, int month, int day, int hour, int minute = 0)
{
    return QDateTime(QDate(year, month, day), QTime(hour, minute), QTimeZone::utc());
}
} // namespace

class TestSolarPosition : public QObject
{
    Q_OBJECT

private slots:
    void declinationFollowsTheSeasons();
    void longitudeFollowsTheClock();
    void terminatorIsAQuarterOfTheEarth();
    void dayAndNightFallWhereTheyShould();
    void sunriseAndSunsetMatchTheAlmanac();
};

void TestSolarPosition::declinationFollowsTheSeasons()
{
    // Sommersonnenwende: die Sonne steht über dem Wendekreis des
    // Krebses, 23,4 Grad Nord.
    QVERIFY2(std::abs(subsolarPoint(utc(2026, 6, 21, 12)).latitudeDeg - 23.44) < 0.2,
             qPrintable(QString::number(subsolarPoint(utc(2026, 6, 21, 12)).latitudeDeg)));
    // Wintersonnenwende: dieselbe Zahl nach Süden.
    QVERIFY2(std::abs(subsolarPoint(utc(2026, 12, 21, 12)).latitudeDeg + 23.44) < 0.2,
             qPrintable(QString::number(subsolarPoint(utc(2026, 12, 21, 12)).latitudeDeg)));
    // Tagundnachtgleiche: über dem Äquator.
    QVERIFY2(std::abs(subsolarPoint(utc(2026, 3, 20, 12)).latitudeDeg) < 0.6,
             qPrintable(QString::number(subsolarPoint(utc(2026, 3, 20, 12)).latitudeDeg)));
    QVERIFY2(std::abs(subsolarPoint(utc(2026, 9, 23, 12)).latitudeDeg) < 0.6,
             qPrintable(QString::number(subsolarPoint(utc(2026, 9, 23, 12)).latitudeDeg)));
}

void TestSolarPosition::longitudeFollowsTheClock()
{
    // Um 12 UTC steht die Sonne über Greenwich -- bis auf die
    // Zeitgleichung, die höchstens gut vier Grad ausmacht.
    const double noon = subsolarPoint(utc(2026, 9, 22, 12)).longitudeDeg;
    QVERIFY2(std::abs(noon) < 4.5, qPrintable(QString::number(noon)));
    // Um Mitternacht UTC auf der Gegenseite.
    const double midnight = subsolarPoint(utc(2026, 9, 22, 0)).longitudeDeg;
    QVERIFY2(std::abs(std::abs(midnight) - 180.0) < 4.5, qPrintable(QString::number(midnight)));

    // Und dazwischen wandert sie 15 Grad je Stunde nach Westen.
    const double at06 = subsolarPoint(utc(2026, 9, 22, 6)).longitudeDeg;
    const double at09 = subsolarPoint(utc(2026, 9, 22, 9)).longitudeDeg;
    QVERIFY2(std::abs((at06 - at09) - 45.0) < 0.5,
             qPrintable(QStringLiteral("%1 -> %2").arg(at06).arg(at09)));

    // Die Länge bleibt im Bereich, in dem der Rest des Programms
    // rechnet.
    for (int hour = 0; hour < 24; ++hour) {
        const SolarPoint p = subsolarPoint(utc(2026, 9, 22, hour));
        QVERIFY(p.longitudeDeg >= -180.0 && p.longitudeDeg <= 180.0);
        QVERIFY(p.latitudeDeg >= -23.5 && p.latitudeDeg <= 23.5);
    }
}

void TestSolarPosition::terminatorIsAQuarterOfTheEarth()
{
    // 6371 km * pi/2 -- ein Viertel des Umfangs.
    QVERIFY(std::abs(terminatorRadiusKm() - 10007.5) < 1.0);
}

// Die Probe aufs Exempel: ein Ort ist im Tag, wenn er weiter als ein
// Viertel des Erdumfangs vom GEGENPUNKT der Sonne entfernt ist. Genau
// so entscheidet die Graulinie in MapWidget, welche Seite hell ist.
void TestSolarPosition::dayAndNightFallWhereTheyShould()
{
    const auto isDaylight = [](const QDateTime& when, double lat, double lon) {
        const SolarPoint sun = subsolarPoint(when);
        const double antiLat = -sun.latitudeDeg;
        const double antiLon = sun.longitudeDeg > 0.0 ? sun.longitudeDeg - 180.0 : sun.longitudeDeg + 180.0;
        return calculateDistanceKmBetween(antiLat, antiLon, lat, lon) > terminatorRadiusKm();
    };

    // Feuerkogel, JN67UT.
    const double oeLat = 47.81;
    const double oeLon = 13.73;
    QVERIFY(isDaylight(utc(2026, 6, 21, 12), oeLat, oeLon));   // Mittag im Sommer
    QVERIFY(!isDaylight(utc(2026, 6, 21, 0), oeLat, oeLon));   // Mitternacht
    QVERIFY(isDaylight(utc(2026, 12, 21, 12), oeLat, oeLon));  // Mittag im Winter
    QVERIFY(!isDaylight(utc(2026, 12, 21, 0), oeLat, oeLon));

    // Neuseeland liegt fast auf der Gegenseite: dort ist es umgekehrt.
    const double zlLat = -41.3;
    const double zlLon = 174.8;
    QVERIFY(!isDaylight(utc(2026, 6, 21, 12), zlLat, zlLon));
    QVERIFY(isDaylight(utc(2026, 6, 21, 0), zlLat, zlLon));

    // Im Polarsommer geht die Sonne nicht unter.
    const double northPoleLat = 89.0;
    for (int hour = 0; hour < 24; hour += 4) {
        QVERIFY2(isDaylight(utc(2026, 6, 21, hour), northPoleLat, 0.0), qPrintable(QString::number(hour)));
        QVERIFY2(!isDaylight(utc(2026, 12, 21, hour), northPoleLat, 0.0), qPrintable(QString::number(hour)));
    }
}

// Gegen den Kalender geprüft: Wien zur Sommersonnenwende geht die
// Sonne um 04:54 MESZ auf und um 20:59 MESZ unter -- also 02:54 und
// 18:59 UTC. Toleranz drei Minuten; genauer braucht es niemand, der
// auf die Graulinie wartet.
void TestSolarPosition::sunriseAndSunsetMatchTheAlmanac()
{
    const auto minutesBetween = [](const QDateTime& a, const QDateTime& b) {
        return std::abs(a.secsTo(b)) / 60.0;
    };
    const double viennaLat = 48.21;
    const double viennaLon = 16.37;

    SunTimes summer = sunTimes(utc(2026, 6, 21, 12), viennaLat, viennaLon);
    QCOMPARE(summer.kind, SunTimes::Kind::RiseAndSet);
    QVERIFY2(minutesBetween(summer.riseUtc, utc(2026, 6, 21, 2, 54)) < 3.0,
             qPrintable(summer.riseUtc.toString(Qt::ISODate)));
    QVERIFY2(minutesBetween(summer.setUtc, utc(2026, 6, 21, 18, 59)) < 3.0,
             qPrintable(summer.setUtc.toString(Qt::ISODate)));
    // Wahrer Mittag liegt in der Mitte -- und in Wien (16,4 Grad Ost)
    // rund eine Stunde vor 12 UTC.
    QVERIFY2(minutesBetween(summer.noonUtc, utc(2026, 6, 21, 10, 57)) < 3.0,
             qPrintable(summer.noonUtc.toString(Qt::ISODate)));

    // Winter: kurzer Tag, gut acht Stunden.
    SunTimes winter = sunTimes(utc(2026, 12, 21, 12), viennaLat, viennaLon);
    QCOMPARE(winter.kind, SunTimes::Kind::RiseAndSet);
    const double winterDayHours = winter.riseUtc.secsTo(winter.setUtc) / 3600.0;
    QVERIFY2(winterDayHours > 8.0 && winterDayHours < 8.6, qPrintable(QString::number(winterDayHours)));

    // Am Äquator ist der Tag immer rund zwölf Stunden lang.
    SunTimes equator = sunTimes(utc(2026, 6, 21, 12), 0.0, 0.0);
    const double equatorDayHours = equator.riseUtc.secsTo(equator.setUtc) / 3600.0;
    QVERIFY2(std::abs(equatorDayHours - 12.1) < 0.2, qPrintable(QString::number(equatorDayHours)));

    // Und nördlich des Polarkreises hört das Auf und Unter auf.
    QCOMPARE(sunTimes(utc(2026, 6, 21, 12), 78.0, 15.0).kind, SunTimes::Kind::AlwaysUp);
    QCOMPARE(sunTimes(utc(2026, 12, 21, 12), 78.0, 15.0).kind, SunTimes::Kind::AlwaysDown);

    // Die Länge verschiebt den Tag: Tokio geht früher auf als Wien.
    SunTimes tokyo = sunTimes(utc(2026, 6, 21, 12), 35.68, 139.69);
    QVERIFY(tokyo.riseUtc < summer.riseUtc);
}

QTEST_APPLESS_MAIN(TestSolarPosition)
#include "test_solar_position.moc"
