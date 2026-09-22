// core/DxInfo.h: was über die Gegenstation bekannt ist, während ihr
// Rufzeichen getippt wird -- Land, Richtung, Entfernung, langer Weg,
// Ortszeit und Sonnenauf-/untergang dort.
//
// Die Prüfliste unten ist von Hand im cty.dat-Format geschrieben (die
// echte lädt der Bediener selbst); die Koordinaten sind die bekannten
// Mittelpunkte der Gebiete.

#include <QtTest>

#include <QTimeZone>

#include "core/BeamHeading.h"
#include "core/DxInfo.h"

using namespace Contestprogramm;

namespace {

const char* const kMiniCty = R"CTY(
Fed. Rep. of Germany:      14:  28:  EU:   51.00:   -10.00:    -1.0:  DL:
    DA,DB,DC,DD,DJ,DK,DL,DM;
Japan:                     25:  45:  AS:   36.40:  -138.38:    -9.0:  JA:
    JA,JE,JF,JG,JH;
Australia:                 30:  59:  OC:  -23.70:  -132.33:   -10.0:  VK:
    VK,AX;
)CTY";

QDateTime utc(int year, int month, int day, int hour, int minute = 0)
{
    return QDateTime(QDate(year, month, day), QTime(hour, minute), QTimeZone::utc());
}

} // namespace

class TestDxInfo : public QObject
{
    Q_OBJECT

private slots:
    void knowsCountryDirectionAndDistance();
    void anExchangedLocatorBeatsTheCountryCentre();
    void farStationsCarryTheLongPath();
    void localTimeAndSunFollowTheCountry();
    void withoutListOrLocatorNothingIsClaimed();
};

void TestDxInfo::knowsCountryDirectionAndDistance()
{
    CountryPrefixIndex countries;
    QVERIFY(countries.loadFromCty(QByteArray(kMiniCty)));

    const DxInfo info = lookupDxInfo(countries, QStringLiteral("JN67UT"), QStringLiteral("DL1ABC"),
                                     QString(), utc(2026, 9, 22, 18));
    QVERIFY(info.known);
    QCOMPARE(info.countryName, QStringLiteral("Fed. Rep. of Germany"));
    QCOMPARE(info.primaryPrefix, QStringLiteral("DL"));
    QCOMPARE(info.continent, QStringLiteral("EU"));
    QCOMPARE(info.cqZone, 14);
    // Vom Feuerkogel in die Mitte Deutschlands: nordwestlich, ein paar
    // hundert Kilometer.
    QVERIFY2(info.bearingDeg > 300.0 && info.bearingDeg < 350.0, qPrintable(QString::number(info.bearingDeg)));
    QVERIFY2(info.distanceKm > 300.0 && info.distanceKm < 600.0, qPrintable(QString::number(info.distanceKm)));
    // Landesmittelpunkt, kein Locator -- und die Zeile sagt es.
    QVERIFY(info.approximate);
    QVERIFY2(info.statusLine().contains(QStringLiteral("~")), qPrintable(info.statusLine()));
    QVERIFY2(info.statusLine().contains(QStringLiteral("(DL)")), qPrintable(info.statusLine()));
}

void TestDxInfo::anExchangedLocatorBeatsTheCountryCentre()
{
    CountryPrefixIndex countries;
    QVERIFY(countries.loadFromCty(QByteArray(kMiniCty)));

    // Derselbe Anruf, aber mit getauschtem Locator: der zählt, und die
    // Angabe ist nicht mehr ungefähr.
    const DxInfo exact = lookupDxInfo(countries, QStringLiteral("JN67UT"), QStringLiteral("DL1ABC"),
                                      QStringLiteral("JO60AA"), utc(2026, 9, 22, 18));
    QVERIFY(exact.known);
    QVERIFY(!exact.approximate);
    QVERIFY2(!exact.statusLine().contains(QStringLiteral("~")), qPrintable(exact.statusLine()));
    // JO60 liegt östlicher als die Mitte Deutschlands -- die Richtung
    // muss sich messbar unterscheiden.
    const DxInfo centre = lookupDxInfo(countries, QStringLiteral("JN67UT"), QStringLiteral("DL1ABC"),
                                       QString(), utc(2026, 9, 22, 18));
    QVERIFY2(std::abs(exact.bearingDeg - centre.bearingDeg) > 5.0,
             qPrintable(QStringLiteral("%1 vs %2").arg(exact.bearingDeg).arg(centre.bearingDeg)));

    // Ein Locator ohne bekanntes Land reicht auch: Richtung und
    // Entfernung stehen, der Name bleibt leer.
    const DxInfo gridOnly = lookupDxInfo(countries, QStringLiteral("JN67UT"), QStringLiteral("ZZ9ZZZ"),
                                         QStringLiteral("JN88TC"), utc(2026, 9, 22, 18));
    QVERIFY(gridOnly.known);
    QVERIFY(gridOnly.countryName.isEmpty());
    QVERIFY(gridOnly.distanceKm > 100.0);
}

void TestDxInfo::farStationsCarryTheLongPath()
{
    CountryPrefixIndex countries;
    QVERIFY(countries.loadFromCty(QByteArray(kMiniCty)));

    const DxInfo vk = lookupDxInfo(countries, QStringLiteral("JN67UT"), QStringLiteral("VK3RRR"),
                                   QString(), utc(2026, 9, 22, 18));
    QVERIFY(vk.known);
    QVERIFY2(vk.distanceKm > 14000.0, qPrintable(QString::number(vk.distanceKm)));
    // Gegenrichtung und Restweg um die Erde.
    QVERIFY2(std::abs(BeamHeading::wrap360(vk.bearingDeg + 180.0) - vk.longPathBearingDeg) < 0.01,
             qPrintable(QString::number(vk.longPathBearingDeg)));
    QVERIFY2(std::abs(vk.distanceKm + vk.longPathKm - 40030.17) < 1.0,
             qPrintable(QString::number(vk.longPathKm)));
    QVERIFY2(vk.statusLine().contains(QStringLiteral("lang ")), qPrintable(vk.statusLine()));

    // Ein Nachbarland trägt ihn nicht -- dort wäre er Unsinn.
    const DxInfo dl = lookupDxInfo(countries, QStringLiteral("JN67UT"), QStringLiteral("DL1ABC"),
                                   QString(), utc(2026, 9, 22, 18));
    QVERIFY2(!dl.statusLine().contains(QStringLiteral("lang ")), qPrintable(dl.statusLine()));
}

void TestDxInfo::localTimeAndSunFollowTheCountry()
{
    CountryPrefixIndex countries;
    QVERIFY(countries.loadFromCty(QByteArray(kMiniCty)));

    // 18:00 UTC: in Japan ist es neun Stunden später, also drei Uhr
    // früh am nächsten Tag.
    const DxInfo ja = lookupDxInfo(countries, QStringLiteral("JN67UT"), QStringLiteral("JA1QQQ"),
                                   QString(), utc(2026, 9, 22, 18));
    QVERIFY(ja.localTime.isValid());
    QCOMPARE(ja.localTime.toUTC().time().hour(), 3);
    QCOMPARE(ja.localTime.toUTC().date().day(), 23);
    QVERIFY2(ja.statusLine().contains(QStringLiteral("dort 03:00")), qPrintable(ja.statusLine()));

    // Und die Sonne: Ende September geht sie in Japan gegen 20:30 UTC
    // auf (05:30 Ortszeit) und gegen 09:00 UTC unter.
    QVERIFY(ja.sunriseUtc.isValid());
    QVERIFY(ja.sunsetUtc.isValid());
    QVERIFY2(ja.statusLine().contains(QStringLiteral("Sonne ")), qPrintable(ja.statusLine()));
    QVERIFY(!ja.polarDay);
    QVERIFY(!ja.polarNight);
}

void TestDxInfo::withoutListOrLocatorNothingIsClaimed()
{
    CountryPrefixIndex empty;
    // Keine Liste geladen, kein Locator: nichts behauptet.
    DxInfo none = lookupDxInfo(empty, QStringLiteral("JN67UT"), QStringLiteral("DL1ABC"), QString(),
                               utc(2026, 9, 22, 18));
    QVERIFY(!none.known);
    QVERIFY(none.statusLine().isEmpty());

    // Leeres Rufzeichen ebenso.
    CountryPrefixIndex countries;
    QVERIFY(countries.loadFromCty(QByteArray(kMiniCty)));
    QVERIFY(!lookupDxInfo(countries, QStringLiteral("JN67UT"), QString(), QString(), utc(2026, 9, 22, 18)).known);

    // Ein unbekanntes Rufzeichen bleibt unbekannt -- kein erstbestes Land.
    QVERIFY(!lookupDxInfo(countries, QStringLiteral("JN67UT"), QStringLiteral("ZZ9ZZZ"), QString(),
                          utc(2026, 9, 22, 18)).known);

    // Ohne eigenen Locator: Land und Ortszeit ja, Richtung nein.
    const DxInfo noHome = lookupDxInfo(countries, QString(), QStringLiteral("DL1ABC"), QString(),
                                       utc(2026, 9, 22, 18));
    QVERIFY(noHome.known);
    QCOMPARE(noHome.distanceKm, 0.0);
    QVERIFY2(!noHome.statusLine().contains(QStringLiteral("km")), qPrintable(noHome.statusLine()));
    QVERIFY2(noHome.statusLine().contains(QStringLiteral("dort ")), qPrintable(noHome.statusLine()));
}

QTEST_APPLESS_MAIN(TestDxInfo)
#include "test_dx_info.moc"
