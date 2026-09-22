#include <QtTest>

#include <cmath>

#include "core/BeamHeading.h"

using namespace Contestprogramm;

class TestBeamHeading : public QObject
{
    Q_OBJECT

private slots:
    void wrap360NormalizesAnyAngle();
    void longPathAddsHalfTurn();
    void longPathDistanceIsTheRestOfTheWayAround();
    void planWithNoStopTakesShorterDirection();
    void planWithNoStopHandlesWrapAcrossZero();
    void planWithNorthStopCannotCrossZero();
    void planWithSouthStopCannotCross180();
    void adviceIsEmptyForAlreadyPointingThere();
    void adviceNamesDirection();
};

void TestBeamHeading::wrap360NormalizesAnyAngle()
{
    QCOMPARE(BeamHeading::wrap360(0.0), 0.0);
    QCOMPARE(BeamHeading::wrap360(360.0), 0.0);
    QCOMPARE(BeamHeading::wrap360(370.0), 10.0);
    QCOMPARE(BeamHeading::wrap360(-10.0), 350.0);
    QCOMPARE(BeamHeading::wrap360(-370.0), 350.0);
}

void TestBeamHeading::longPathAddsHalfTurn()
{
    QCOMPARE(BeamHeading::longPath(0.0), 180.0);
    QCOMPARE(BeamHeading::longPath(90.0), 270.0);
    QCOMPARE(BeamHeading::longPath(270.0), 90.0);
}

// Einmal um die Erde, minus dem kurzen Weg -- auf Kurzwelle die zweite
// Hälfte der Frage "wohin drehe ich?".
void TestBeamHeading::longPathDistanceIsTheRestOfTheWayAround()
{
    // 40 030 km Umfang (Erdradius 6371, wie überall im Programm).
    QVERIFY(std::abs(BeamHeading::longPathDistanceKm(0.0) - 40030.17) < 0.5);
    QVERIFY(std::abs(BeamHeading::longPathDistanceKm(16280.0) - 23750.17) < 0.5);
    // Der Gegenpunkt liegt in der Mitte: beide Wege gleich lang.
    const double half = BeamHeading::longPathDistanceKm(0.0) / 2.0;
    QVERIFY(std::abs(BeamHeading::longPathDistanceKm(half) - half) < 0.5);
    // Nie negativ, auch wenn die Zahl nicht passt.
    QCOMPARE(BeamHeading::longPathDistanceKm(50000.0), 0.0);
}

void TestBeamHeading::planWithNoStopTakesShorterDirection()
{
    // 10 -> 30: 20 degrees clockwise is shorter than 340 the other way.
    const BeamHeading::Move m = BeamHeading::plan(10.0, 30.0, BeamHeading::Stop::None);
    QVERIFY(m.reachable);
    QCOMPARE(m.targetDeg, 30.0);
    QCOMPARE(m.travelDeg, 20.0);
}

void TestBeamHeading::planWithNoStopHandlesWrapAcrossZero()
{
    // 350 -> 10: going forward through 360/0 (20 degrees) is shorter
    // than going the other way (340 degrees) -- Stop::None means the
    // rotor is free to take the short way.
    const BeamHeading::Move m = BeamHeading::plan(350.0, 10.0, BeamHeading::Stop::None);
    QVERIFY(m.reachable);
    QCOMPARE(m.targetDeg, 10.0);
    QCOMPARE(m.travelDeg, 20.0);

    // The reverse move is the same 20 degrees, the other sign.
    const BeamHeading::Move back = BeamHeading::plan(10.0, 350.0, BeamHeading::Stop::None);
    QVERIFY(back.reachable);
    QCOMPARE(back.travelDeg, -20.0);
}

void TestBeamHeading::planWithNorthStopCannotCrossZero()
{
    // A north-stop rotor cannot pass through 0/360 -- 350 -> 10 must go
    // the long way round (340 degrees, the opposite direction from the
    // Stop::None case's 20-degree move through zero above), not the
    // short way through zero. Magnitude only, per the class comment's
    // own worked example ("a three-hundred-and-forty-degree move if it
    // cannot") -- the sign falls out of plan()'s unwrapped b-a and is
    // an implementation detail, not part of the documented contract.
    const BeamHeading::Move m = BeamHeading::plan(350.0, 10.0, BeamHeading::Stop::North);
    QVERIFY(m.reachable);
    QCOMPARE(m.targetDeg, 10.0);
    QCOMPARE(std::abs(m.travelDeg), 340.0);
    QVERIFY(!m.note.isEmpty());
}

void TestBeamHeading::planWithSouthStopCannotCross180()
{
    // A south-stop rotor cannot pass through 180 -- 170 -> 190 must go
    // the long way round.
    const BeamHeading::Move m = BeamHeading::plan(170.0, 190.0, BeamHeading::Stop::South);
    QVERIFY(m.reachable);
    QCOMPARE(m.targetDeg, 190.0);
    QCOMPARE(std::abs(m.travelDeg), 340.0);
}

void TestBeamHeading::adviceIsEmptyForAlreadyPointingThere()
{
    const BeamHeading::Move m = BeamHeading::plan(100.0, 100.5, BeamHeading::Stop::None);
    QCOMPARE(BeamHeading::advice(m), QStringLiteral("Already pointing there."));
}

void TestBeamHeading::adviceNamesDirection()
{
    const BeamHeading::Move cw = BeamHeading::plan(0.0, 90.0, BeamHeading::Stop::None);
    QVERIFY(BeamHeading::advice(cw).contains(QStringLiteral("clockwise")));
    QVERIFY(!BeamHeading::advice(cw).contains(QStringLiteral("anticlockwise")));

    const BeamHeading::Move ccw = BeamHeading::plan(90.0, 0.0, BeamHeading::Stop::None);
    QVERIFY(BeamHeading::advice(ccw).contains(QStringLiteral("anticlockwise")));
}

QTEST_APPLESS_MAIN(TestBeamHeading)
#include "test_beamheading.moc"
