// Tests for core/CountryBorders.h's pure parse function -- see that
// header's own doc comment for the plain-text ring format
// resources/geo/country_borders_110m.dat uses.

#include <QtTest>

#include "core/CountryBorders.h"

using namespace Contestprogramm;

class TestCountryBorders : public QObject
{
    Q_OBJECT

private slots:
    void parsesOneRingPerLine();
    void skipsCommentAndBlankLines();
    void dropsRingsWithFewerThanTwoPoints();
    void skipsMalformedPairsWithinARingRatherThanDroppingTheWholeRing();
    void emptyInputYieldsNoRings();
    void parsesOptionalNamePrefix();
    void plainLineWithNoTabHasEmptyName();
    void nameIsDroppedIfItsRingIsDropped();
};

void TestCountryBorders::parsesOneRingPerLine()
{
    const QByteArray data = "13.500,47.000 14.000,47.500 13.800,47.200\n"
                             "10.000,50.000 11.000,50.500\n";
    const QVector<CountryBorderRing> rings = parseCountryBordersData(data);
    QCOMPARE(rings.size(), 2);
    QCOMPARE(rings.at(0).points.size(), 3);
    // (lon, lat) order -- x() is longitude, y() is latitude.
    QCOMPARE(rings.at(0).points.at(0), QPointF(13.5, 47.0));
    QCOMPARE(rings.at(0).points.at(2), QPointF(13.8, 47.2));
    QCOMPARE(rings.at(1).points.at(1), QPointF(11.0, 50.5));
}

void TestCountryBorders::skipsCommentAndBlankLines()
{
    const QByteArray data = "# a header comment\n"
                             "\n"
                             "   \n"
                             "13.500,47.000 14.000,47.500\n"
                             "# another comment\n";
    const QVector<CountryBorderRing> rings = parseCountryBordersData(data);
    QCOMPARE(rings.size(), 1);
    QCOMPARE(rings.at(0).points.size(), 2);
}

void TestCountryBorders::dropsRingsWithFewerThanTwoPoints()
{
    const QByteArray data = "13.500,47.000\n"
                             "10.000,50.000 11.000,50.500\n";
    const QVector<CountryBorderRing> rings = parseCountryBordersData(data);
    QCOMPARE(rings.size(), 1);
    QCOMPARE(rings.at(0).points.at(0), QPointF(10.0, 50.0));
}

void TestCountryBorders::skipsMalformedPairsWithinARingRatherThanDroppingTheWholeRing()
{
    const QByteArray data = "13.500,47.000 not-a-pair 14.000,47.500 13.5;47 14.500,48.000\n";
    const QVector<CountryBorderRing> rings = parseCountryBordersData(data);
    QCOMPARE(rings.size(), 1);
    // Three real pairs survive; the two malformed tokens are skipped.
    QCOMPARE(rings.at(0).points.size(), 3);
    QCOMPARE(rings.at(0).points.at(1), QPointF(14.0, 47.5));
}

void TestCountryBorders::emptyInputYieldsNoRings()
{
    QVERIFY(parseCountryBordersData(QByteArray()).isEmpty());
}

void TestCountryBorders::parsesOptionalNamePrefix()
{
    const QByteArray data = "Österreich\t13.500,47.000 14.000,47.500 13.800,47.200\n"
                             "10.000,50.000 11.000,50.500\n";
    const QVector<CountryBorderRing> rings = parseCountryBordersData(data);
    QCOMPARE(rings.size(), 2);
    QCOMPARE(rings.at(0).name, QStringLiteral("Österreich"));
    QCOMPARE(rings.at(0).points.size(), 3);
    QCOMPARE(rings.at(0).points.at(0), QPointF(13.5, 47.0));
}

void TestCountryBorders::plainLineWithNoTabHasEmptyName()
{
    const QByteArray data = "13.500,47.000 14.000,47.500\n";
    const QVector<CountryBorderRing> rings = parseCountryBordersData(data);
    QCOMPARE(rings.size(), 1);
    QVERIFY(rings.at(0).name.isEmpty());
}

void TestCountryBorders::nameIsDroppedIfItsRingIsDropped()
{
    // A named ring with fewer than 2 points after skipping still drops
    // entirely -- the name doesn't rescue an otherwise-invalid ring.
    const QByteArray data = "Nirgendwo\t13.500,47.000\n"
                             "10.000,50.000 11.000,50.500\n";
    const QVector<CountryBorderRing> rings = parseCountryBordersData(data);
    QCOMPARE(rings.size(), 1);
    QVERIFY(rings.at(0).name.isEmpty());
    QCOMPARE(rings.at(0).points.at(0), QPointF(10.0, 50.0));
}

QTEST_APPLESS_MAIN(TestCountryBorders)
#include "test_country_borders.moc"
