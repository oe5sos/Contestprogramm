// core/CountryPrefixIndex.h: von welchem Rufzeichen kommt welches Land.
//
// Die Prüfdatei unten ist von Hand geschrieben, im cty.dat-Format, mit
// einer Handvoll Ländern -- die echte Länderliste lädt der Bediener
// selbst (Datei > Länderliste laden), im Quelltext liegt nichts
// Fremdes. Die Koordinaten sind die bekannten Mittelpunkte der Gebiete.

#include <QtTest>

#include "core/CountryPrefixIndex.h"

using namespace Contestprogramm;

namespace {

// Kopfzeile: Name : CQ : ITU : Kontinent : Breite : Länge(West+) : TZ : Hauptpräfix ;
const char* const kMiniCty = R"CTY(
# Eine kleine Liste im cty.dat-Format, von Hand für diesen Prüfstand.
Austria:                   15:  28:  EU:   47.33:   -13.33:    -1.0:  OE:
    OE,OE1,OE2,OE3,OE4,OE5,OE6,OE7,OE8,OE9;
Fed. Rep. of Germany:      14:  28:  EU:   51.00:   -10.00:    -1.0:  DL:
    DA,DB,DC,DD,DE,DF,DG,DH,DJ,DK,DL,DM,DO,DP,DQ,DR,=DL0AAA;
United States:             05:  08:  NA:   37.53:    91.67:     5.0:  K:
    AA,AB,AC,AK,AL,K,N,W,WA,WB,=W1AW;
Japan:                     25:  45:  AS:   36.40:  -138.38:    -9.0:  JA:
    JA,JE,JF,JG,JH,JI,JJ,JK,JL,JM,JN,JO,JP,JQ,JR,JS,7J,7K,7L,7M,7N,8J,8N;
Australia:                 30:  59:  OC:  -23.70:  -132.33:   -10.0:  VK:
    VK,AX;
European Turkey:           20:  39:  EU:   41.00:   -29.00:    -3.0:  *TA1:
    TA1,TB1,TC1;
)CTY";

} // namespace

class TestCountryPrefixIndex : public QObject
{
    Q_OBJECT

private slots:
    void readsTheRecordsAndTheirFields();
    void longestPrefixWins();
    void exactCallsignsBeatPrefixes();
    void portableDesignatorsDecideTheCountry();
    void unknownCallsignStaysUnknown();
    void abrokenFileLeavesTheOldListAlone();
};

void TestCountryPrefixIndex::readsTheRecordsAndTheirFields()
{
    CountryPrefixIndex index;
    QString error;
    QVERIFY2(index.loadFromCty(QByteArray(kMiniCty), &error), qPrintable(error));
    QCOMPARE(index.countryCount(), 6);
    QVERIFY(!index.isEmpty());

    const CountryEntry oe = index.lookup(QStringLiteral("OE5SOS"));
    QVERIFY(oe.isValid());
    QCOMPARE(oe.name, QStringLiteral("Austria"));
    QCOMPARE(oe.primaryPrefix, QStringLiteral("OE"));
    QCOMPARE(oe.continent, QStringLiteral("EU"));
    QCOMPARE(oe.cqZone, 15);
    QCOMPARE(oe.ituZone, 28);
    QVERIFY(std::abs(oe.latitudeDeg - 47.33) < 0.01);
    // cty.dat führt die Länge nach Westen positiv -- hier nach Osten,
    // wie überall sonst im Programm. Österreich liegt östlich.
    QVERIFY2(oe.longitudeDeg > 13.0 && oe.longitudeDeg < 13.5, qPrintable(QString::number(oe.longitudeDeg)));
    // Auch die Zeitverschiebung führt cty.dat andersherum: -1.0 heißt
    // eine Stunde ÖSTLICH von UTC.
    QVERIFY2(std::abs(oe.utcOffsetHours - 1.0) < 0.01, qPrintable(QString::number(oe.utcOffsetHours)));
    QVERIFY2(std::abs(index.lookup(QStringLiteral("W1XYZ")).utcOffsetHours + 5.0) < 0.01,
             qPrintable(QString::number(index.lookup(QStringLiteral("W1XYZ")).utcOffsetHours)));
    QVERIFY2(std::abs(index.lookup(QStringLiteral("JA1QQQ")).utcOffsetHours - 9.0) < 0.01,
             qPrintable(QString::number(index.lookup(QStringLiteral("JA1QQQ")).utcOffsetHours)));

    // Und ein Gebiet mit '*' am Hauptpräfix (kein eigenes DXCC).
    const CountryEntry ta1 = index.lookup(QStringLiteral("TA1ABC"));
    QCOMPARE(ta1.primaryPrefix, QStringLiteral("TA1"));
    QCOMPARE(ta1.continent, QStringLiteral("EU"));
}

void TestCountryPrefixIndex::longestPrefixWins()
{
    CountryPrefixIndex index;
    QVERIFY(index.loadFromCty(QByteArray(kMiniCty)));
    // "W" ist USA, "VK" Australien -- und ein Rufzeichen, das mit einem
    // längeren Präfix anfängt, darf nicht beim kürzeren landen.
    QCOMPARE(index.lookup(QStringLiteral("W1XYZ")).name, QStringLiteral("United States"));
    QCOMPARE(index.lookup(QStringLiteral("VK3RRR")).name, QStringLiteral("Australia"));
    QCOMPARE(index.lookup(QStringLiteral("JA1QQQ")).name, QStringLiteral("Japan"));
    QCOMPARE(index.lookup(QStringLiteral("DL1ABC")).name, QStringLiteral("Fed. Rep. of Germany"));
    // Kleinschreibung und Leerzeichen kommen auch an.
    QCOMPARE(index.lookup(QStringLiteral("  dl1abc ")).name, QStringLiteral("Fed. Rep. of Germany"));
}

void TestCountryPrefixIndex::exactCallsignsBeatPrefixes()
{
    CountryPrefixIndex index;
    QVERIFY(index.loadFromCty(QByteArray(kMiniCty)));
    // =W1AW steht als ganzes Rufzeichen in der Liste; der Präfix W
    // führte hier zum selben Land, aber die Reihenfolge muss stimmen --
    // in der echten Liste stehen so die Ausnahmen.
    QCOMPARE(index.lookup(QStringLiteral("W1AW")).name, QStringLiteral("United States"));
    QCOMPARE(index.lookup(QStringLiteral("DL0AAA")).name, QStringLiteral("Fed. Rep. of Germany"));
}

void TestCountryPrefixIndex::portableDesignatorsDecideTheCountry()
{
    CountryPrefixIndex index;
    QVERIFY(index.loadFromCty(QByteArray(kMiniCty)));
    // Ein Betriebszusatz ändert nichts.
    QCOMPARE(index.lookup(QStringLiteral("OE5SOS/P")).name, QStringLiteral("Austria"));
    QCOMPARE(index.lookup(QStringLiteral("OE5SOS/MM")).name, QStringLiteral("Austria"));
    // Ein Länderzusatz schon -- vor wie hinter dem Schrägstrich.
    QCOMPARE(index.lookup(QStringLiteral("DL/OE5SOS")).name, QStringLiteral("Fed. Rep. of Germany"));
    QCOMPARE(index.lookup(QStringLiteral("OE5SOS/DL")).name, QStringLiteral("Fed. Rep. of Germany"));
    // Eine einzelne Ziffer verschiebt nur den Bezirk, nicht das Land.
    QCOMPARE(index.lookup(QStringLiteral("W1AW/4")).name, QStringLiteral("United States"));
    QCOMPARE(CountryPrefixIndex::lookupKey(QStringLiteral("DL/OE5SOS/P")), QStringLiteral("DL"));
}

void TestCountryPrefixIndex::unknownCallsignStaysUnknown()
{
    CountryPrefixIndex index;
    QVERIFY(index.loadFromCty(QByteArray(kMiniCty)));
    // Kein Treffer heißt kein Treffer -- nicht das erstbeste Land.
    QVERIFY(!index.lookup(QStringLiteral("ZZ9ZZZ")).isValid());
    QVERIFY(!index.lookup(QString()).isValid());
    QVERIFY(!index.lookup(QStringLiteral("///")).isValid());
}

void TestCountryPrefixIndex::abrokenFileLeavesTheOldListAlone()
{
    CountryPrefixIndex index;
    QVERIFY(index.loadFromCty(QByteArray(kMiniCty)));
    QCOMPARE(index.countryCount(), 6);

    QString error;
    QVERIFY(!index.loadFromCty(QByteArrayLiteral("das ist keine Laenderliste"), &error));
    QVERIFY(!error.isEmpty());
    // Mitten im Contest darf eine kaputte Datei nicht alles wegnehmen.
    QCOMPARE(index.countryCount(), 6);
    QCOMPARE(index.lookup(QStringLiteral("OE5SOS")).name, QStringLiteral("Austria"));
}

QTEST_APPLESS_MAIN(TestCountryPrefixIndex)
#include "test_country_prefix_index.moc"
