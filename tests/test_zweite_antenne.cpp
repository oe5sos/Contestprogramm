// Martin, 2026-09-27: "ich möchte bei den rotoren eine option haben,
// ob und in welche richtung die zweite antenne versetzt steht. es kann
// auch sein, dass ich pro rotor nur eine antennenrichtung habe. dies
// soll in der taskleiste einzustellen sein." Und auf die Rückfrage,
// was "Richtung 1" heißt: "wenn es nur eine richtung gibt, dann kein
// versatz, wenn es 2 richtungen gibt, dann versatz."
//
// Richtung 1 ist also immer die Rotorstellung selbst, nur die zweite
// hat einen Versatz. Die Einstellung gab es schon -- aber nur im
// Einstellungsfenster. Jetzt liegt sie im ⚙ des jeweiligen Rotors.

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/BeamHeading.h"
#include "ui/RotorWidget.h"

#include <memory>

using namespace Contestprogramm;

class TestZweiteAntenne : public QObject
{
    Q_OBJECT

private slots:
    void withoutASecondAntennaThereIsNoOffset();
    void theOffsetRidesOnTheRotorPosition();
    void theWidgetAsksForBothAndAppliesThemToItself();
};

// Eine Antenne: die Richtung ist die Rotorstellung, fertig.
void TestZweiteAntenne::withoutASecondAntennaThereIsNoOffset()
{
    RotorWidget widget(QStringLiteral("2m"));
    widget.setAzimuthDeg(120.0);
    QVERIFY(!widget.secondAntennaEnabled());
    QCOMPARE(widget.azimuthDeg(), 120.0);
    // Die Vorgabe trägt keinen Versatz -- eine Station mit einer
    // Antenne je Rotor soll nichts einstellen müssen.
    QCOMPARE(widget.secondAntennaOffsetDeg(), 0.0);
}

// Zwei Richtungen: die zweite steht um den Versatz versetzt und dreht
// mit -- beide hängen am selben Mast.
void TestZweiteAntenne::theOffsetRidesOnTheRotorPosition()
{
    QCOMPARE(RotorWidget::secondAntennaBearing(0.0, 45.0), 45.0);
    QCOMPARE(RotorWidget::secondAntennaBearing(120.0, 45.0), 165.0);
    // Über Nord hinaus wird gewickelt, nicht 380°.
    QCOMPARE(RotorWidget::secondAntennaBearing(350.0, 45.0), 35.0);
    // Und ein Versatz nach links geht genauso.
    QCOMPARE(RotorWidget::secondAntennaBearing(10.0, -45.0), 325.0);
}

void TestZweiteAntenne::theWidgetAsksForBothAndAppliesThemToItself()
{
    RotorWidget widget(QStringLiteral("2m"));
    QSignalSpy spy(&widget, &RotorWidget::secondAntennaRequested);

    // Was das ⚙ tut: erst auf sich selbst anwenden, dann melden --
    // dieselbe Reihenfolge wie beim Anzeigestil, damit die Scheibe
    // stimmt, bevor MainWindows Runde zurückkommt.
    widget.setSecondAntenna(true, 45.0);
    QVERIFY(widget.secondAntennaEnabled());
    QCOMPARE(widget.secondAntennaOffsetDeg(), 45.0);
    emit widget.secondAntennaRequested(true, 45.0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toBool(), true);
    QCOMPARE(spy.first().at(1).toDouble(), 45.0);

    // Und wieder aus: der Versatz bleibt gemerkt, er gilt nur nicht.
    widget.setSecondAntenna(false, 45.0);
    QVERIFY(!widget.secondAntennaEnabled());
    QCOMPARE(widget.secondAntennaOffsetDeg(), 45.0);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestZweiteAntenne tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_zweite_antenne.moc"
