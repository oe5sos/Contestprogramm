#include <QtTest>

#include "data/ContestDefinition.h"
#include "data/ContestScoring.h"
#include "data/QsoRecord.h"

using namespace Contestprogramm;

namespace {

QsoRecord makeQso(const QString& call, const QString& band, const QString& grid, double km)
{
    QsoRecord r;
    r.callsign = call;
    r.band = band;
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QStringLiteral("2026-10-03T14:01:00Z");
    r.gridSquare = grid;
    r.distanceKm = km;
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    return r;
}

const QString kOwn = QStringLiteral("JN67UT");

} // namespace

// The IARU-R1/ÖVSV rule as this project applies it -- see
// data/ContestScoring.h. These are the same numbers EdiExporter puts
// in the CQSOs/CQSOP/CWWLs/CODXC header lines (test_ediexporter.cpp
// pins those), so a change here must show up there too.
class TestContestScoring : public QObject
{
    Q_OBJECT

private slots:
    void distancePointsAreWholeKilometresWithAFloorOfOne();
    void dupesAndInvalidQsosNeverScore();
    void sumsPerBandInDefinitionOrderWithOdxAndSquares();
    void qsoCountRuleScoresOnePointPerQso();
    void definitionParsesAndRoundTripsTheScoringKey();
    void definitionRejectsUnknownScoring();
    void definitionSerialScopeDefaultsToBandAndRoundTrips();
};

void TestContestScoring::distancePointsAreWholeKilometresWithAFloorOfOne()
{
    QsoRecord r = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("JN58SD"), 187.4);
    QCOMPARE(qsoDistancePoints(r, kOwn), 187);
    r.distanceKm = 214.6;
    QCOMPARE(qsoDistancePoints(r, kOwn), 215);
    // Same square: 0 km on the map, 1 point in the log.
    r.distanceKm = 0.0;
    QCOMPARE(qsoDistancePoints(r, kOwn), 1);
    // No stored distance but both locators known -> computed on the fly.
    r.distanceKm.reset();
    r.gridSquare = QStringLiteral("JN88TC");
    QVERIFY(qsoDistancePoints(r, kOwn) > 150);
    // No locator at all: nothing to score.
    r.gridSquare.clear();
    QCOMPARE(qsoDistancePoints(r, kOwn), 0);
    // No own locator either: nothing to compute from.
    r.gridSquare = QStringLiteral("JN88TC");
    QCOMPARE(qsoDistancePoints(r, QString()), 0);
}

void TestContestScoring::dupesAndInvalidQsosNeverScore()
{
    QsoRecord dupe = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("JN58SD"), 187.4);
    dupe.isDupe = true;
    QCOMPARE(qsoPoints(dupe, kOwn, QStringLiteral("distance_km")), 0);
    QsoRecord invalid = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("JN58SD"), 187.4);
    invalid.isInvalid = true;
    QCOMPARE(qsoPoints(invalid, kOwn, QStringLiteral("distance_km")), 0);

    const ContestScore score = computeContestScore({dupe, invalid}, kOwn, {QStringLiteral("144")});
    QCOMPARE(score.validQsos, 0);
    QCOMPARE(score.dupes, 1); // the invalid one is not even a dupe -- it is gone
    QCOMPARE(score.points, qint64(0));
    QCOMPARE(score.odxKm, 0);
    QCOMPARE(score.bands.size(), 1);
    QCOMPARE(score.bands.first().dupes, 1);
}

void TestContestScoring::sumsPerBandInDefinitionOrderWithOdxAndSquares()
{
    QVector<QsoRecord> records = {
        makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("JN58SD"), 187.4),
        makeQso(QStringLiteral("OE3XYZ"), QStringLiteral("144"), QStringLiteral("JN88TC"), 214.6),
        makeQso(QStringLiteral("OE3ABC"), QStringLiteral("144"), QStringLiteral("JN88AA"), 200.0), // same large square
        makeQso(QStringLiteral("OE5XYZ"), QStringLiteral("432"), QStringLiteral("JN67UT"), 0.0),
        makeQso(QStringLiteral("HB9ZZZ"), QStringLiteral("1296"), QStringLiteral("JN47PM"), 300.2), // not in the definition
    };
    records[2].isDupe = false;

    const ContestScore score = computeContestScore(records, kOwn, {QStringLiteral("432"), QStringLiteral("144")});

    // Definition order first (432 before 144 here), then the unlisted band.
    QCOMPARE(score.bands.size(), 3);
    QCOMPARE(score.bands.at(0).band, QStringLiteral("432"));
    QCOMPARE(score.bands.at(1).band, QStringLiteral("144"));
    QCOMPARE(score.bands.at(2).band, QStringLiteral("1296"));

    const BandScore* b144 = score.band(QStringLiteral("144"));
    QVERIFY(b144);
    QCOMPARE(b144->validQsos, 3);
    QCOMPARE(b144->points, qint64(187 + 215 + 200));
    QCOMPARE(b144->largeSquares, 2); // JN58 + JN88
    QCOMPARE(b144->odxCall, QStringLiteral("OE3XYZ"));
    QCOMPARE(b144->odxGrid, QStringLiteral("JN88TC"));
    QCOMPARE(b144->odxKm, 215);

    const BandScore* b432 = score.band(QStringLiteral("432"));
    QVERIFY(b432);
    QCOMPARE(b432->validQsos, 1);
    QCOMPARE(b432->points, qint64(1));
    QCOMPARE(b432->largeSquares, 1);

    QCOMPARE(score.validQsos, 5);
    QCOMPARE(score.points, qint64(187 + 215 + 200 + 1 + 300));
    QCOMPARE(score.odxCall, QStringLiteral("HB9ZZZ"));
    QCOMPARE(score.odxBand, QStringLiteral("1296"));
    QCOMPARE(score.odxKm, 300);
    QVERIFY(!score.band(QStringLiteral("70")));
}

void TestContestScoring::qsoCountRuleScoresOnePointPerQso()
{
    QVector<QsoRecord> records = {
        makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("JN58SD"), 187.4),
        makeQso(QStringLiteral("OE3XYZ"), QStringLiteral("144"), QStringLiteral("JN88TC"), 214.6),
    };
    records[1].isDupe = true;
    const ContestScore score = computeContestScore(records, kOwn, {QStringLiteral("144")}, QStringLiteral("qso_count"));
    QCOMPARE(score.points, qint64(1));
    QCOMPARE(score.validQsos, 1);
    QCOMPARE(score.dupes, 1);
    // ODX stays a distance whatever the rule.
    QCOMPARE(score.odxKm, 187);
}

void TestContestScoring::definitionParsesAndRoundTripsTheScoringKey()
{
    const char* json = R"JSON({"id":"X","name":"X","bands":["144"],"dupe_scope":["callsign","band","mode"],
        "exchange_fields":[{"key":"grid","label":"Grid","type":"grid6"}]})JSON";
    QString error;
    ContestDefinition def = ContestDefinition::loadFromJson(QByteArray(json), &error);
    QVERIFY2(def.isValid(), qPrintable(error));
    QCOMPARE(def.scoring(), QStringLiteral("distance_km")); // the default when the key is absent

    const char* counted = R"JSON({"id":"X","name":"X","bands":["144"],"dupe_scope":["callsign","band","mode"],
        "scoring":"qso_count","exchange_fields":[{"key":"grid","label":"Grid","type":"grid6"}]})JSON";
    def = ContestDefinition::loadFromJson(QByteArray(counted), &error);
    QVERIFY2(def.isValid(), qPrintable(error));
    QCOMPARE(def.scoring(), QStringLiteral("qso_count"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("x.json"));
    QVERIFY(def.saveToFile(path, &error));
    const ContestDefinition reloaded = ContestDefinition::loadFromFile(path, &error);
    QVERIFY2(reloaded.isValid(), qPrintable(error));
    QCOMPARE(reloaded.scoring(), QStringLiteral("qso_count"));
}

void TestContestScoring::definitionSerialScopeDefaultsToBandAndRoundTrips()
{
    const char* json = R"JSON({"id":"X","name":"X","bands":["144"],"dupe_scope":["callsign","band","mode"],
        "exchange_fields":[{"key":"grid","label":"Grid","type":"grid6"}]})JSON";
    QString error;
    ContestDefinition def = ContestDefinition::loadFromJson(QByteArray(json), &error);
    QVERIFY2(def.isValid(), qPrintable(error));
    QCOMPARE(def.serialScope(), QStringLiteral("band")); // the IARU R1 rule is the default

    const char* whole = R"JSON({"id":"X","name":"X","bands":["144"],"dupe_scope":["callsign","band","mode"],
        "serial_scope":"contest","exchange_fields":[{"key":"grid","label":"Grid","type":"grid6"}]})JSON";
    def = ContestDefinition::loadFromJson(QByteArray(whole), &error);
    QVERIFY2(def.isValid(), qPrintable(error));
    QCOMPARE(def.serialScope(), QStringLiteral("contest"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("x.json"));
    QVERIFY(def.saveToFile(path, &error));
    QCOMPARE(ContestDefinition::loadFromFile(path, &error).serialScope(), QStringLiteral("contest"));

    const char* bad = R"JSON({"id":"X","name":"X","bands":["144"],"dupe_scope":["callsign","band","mode"],
        "serial_scope":"operator","exchange_fields":[{"key":"grid","label":"Grid","type":"grid6"}]})JSON";
    QVERIFY(!ContestDefinition::loadFromJson(QByteArray(bad), &error).isValid());
    QVERIFY(error.contains(QStringLiteral("operator")));
}

void TestContestScoring::definitionRejectsUnknownScoring()
{
    const char* json = R"JSON({"id":"X","name":"X","bands":["144"],"dupe_scope":["callsign","band","mode"],
        "scoring":"bonus_points","exchange_fields":[{"key":"grid","label":"Grid","type":"grid6"}]})JSON";
    QString error;
    const ContestDefinition def = ContestDefinition::loadFromJson(QByteArray(json), &error);
    QVERIFY(!def.isValid());
    QVERIFY(error.contains(QStringLiteral("bonus_points")));
}

QTEST_APPLESS_MAIN(TestContestScoring)
#include "test_contest_scoring.moc"
