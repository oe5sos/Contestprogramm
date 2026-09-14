// Tests for core/Cities.h's pure parse function -- see that header's
// own doc comment for the plain-text city format
// resources/geo/cities_110m.dat uses.

#include <QtTest>

#include "core/Cities.h"

using namespace Contestprogramm;

class TestCities : public QObject
{
    Q_OBJECT

private slots:
    void parsesOneCityPerLine();
    void skipsCommentAndBlankLines();
    void skipsLinesWithWrongFieldCount();
    void skipsLinesWithUnparsableCoordinates();
    void skipsLinesWithAnEmptyName();
    void emptyInputYieldsNoCities();
};

void TestCities::parsesOneCityPerLine()
{
    const QByteArray data = "16.365\t48.202\t2400000\t1\tWien\n"
                             "13.041\t47.799\t150000\t0\tSalzburg\n";
    const QVector<CityPoint> cities = parseCitiesData(data);
    QCOMPARE(cities.size(), 2);

    QCOMPARE(cities.at(0).name, QStringLiteral("Wien"));
    QCOMPARE(cities.at(0).lon, 16.365);
    QCOMPARE(cities.at(0).lat, 48.202);
    QCOMPARE(cities.at(0).popMax, static_cast<qint64>(2400000));
    QVERIFY(cities.at(0).isCapital);

    QCOMPARE(cities.at(1).name, QStringLiteral("Salzburg"));
    QVERIFY(!cities.at(1).isCapital);
}

void TestCities::skipsCommentAndBlankLines()
{
    const QByteArray data = "# a header comment\n"
                             "\n"
                             "   \n"
                             "16.365\t48.202\t2400000\t1\tWien\n"
                             "# another comment\n";
    const QVector<CityPoint> cities = parseCitiesData(data);
    QCOMPARE(cities.size(), 1);
    QCOMPARE(cities.at(0).name, QStringLiteral("Wien"));
}

void TestCities::skipsLinesWithWrongFieldCount()
{
    const QByteArray data = "16.365\t48.202\t2400000\tWien\n" // only 4 fields
                             "13.041\t47.799\t150000\t0\tSalzburg\n";
    const QVector<CityPoint> cities = parseCitiesData(data);
    QCOMPARE(cities.size(), 1);
    QCOMPARE(cities.at(0).name, QStringLiteral("Salzburg"));
}

void TestCities::skipsLinesWithUnparsableCoordinates()
{
    const QByteArray data = "not-a-lon\t48.202\t2400000\t1\tWien\n"
                             "13.041\t47.799\t150000\t0\tSalzburg\n";
    const QVector<CityPoint> cities = parseCitiesData(data);
    QCOMPARE(cities.size(), 1);
    QCOMPARE(cities.at(0).name, QStringLiteral("Salzburg"));
}

void TestCities::skipsLinesWithAnEmptyName()
{
    const QByteArray data = "16.365\t48.202\t2400000\t1\t\n"
                             "13.041\t47.799\t150000\t0\tSalzburg\n";
    const QVector<CityPoint> cities = parseCitiesData(data);
    QCOMPARE(cities.size(), 1);
    QCOMPARE(cities.at(0).name, QStringLiteral("Salzburg"));
}

void TestCities::emptyInputYieldsNoCities()
{
    QVERIFY(parseCitiesData(QByteArray()).isEmpty());
}

QTEST_APPLESS_MAIN(TestCities)
#include "test_cities.moc"
