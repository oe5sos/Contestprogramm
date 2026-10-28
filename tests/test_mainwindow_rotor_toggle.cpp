#include <QtTest>

#include <QApplication>
#include <QMetaObject>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "ui/MainWindow.h"
#include "ui/MapWidget.h"
#include "ui/RotorWidget.h"

#include <memory>

using namespace Contestprogramm;

// End-to-end coverage for the task's mandatory smoke test: "toggling
// rotor 2 off in Settings and confirming its RotorWidget actually
// disappears from MainWindow, not just shows disconnected." Drives the
// exact same two calls MainWindow::openSettingsDialog() makes after a
// real SettingsDialog::exec() (AppController::setSettings() then
// applyRotorWidgetSettings()) -- applyRotorWidgetSettings() is a
// private slot specifically so QMetaObject::invokeMethod can call it
// here without needing to drive a real modal dialog.
class TestMainWindowRotorToggle : public QObject
{
    Q_OBJECT

private slots:
    void bothRotorWidgetsExistByDefault();
    void disablingRotor2RemovesItsWidgetEntirely();
    void reEnablingRotor2RecreatesItsWidget();
    void mapBeamwidthReachesTheRotorDials();
    void mapKnowsWhetherTheRotorLinkIsUp();
    void turningTheDialMovesTheMapsBeam();
    void derKlickInDieKartenscheibeDrehtDenRotor();
};

namespace {

// A fresh AppController + open temp database, with ownCallsign already
// set so MainWindow's constructor does not auto-open the first-run
// SettingsDialog (which would otherwise block this test on a modal
// event loop with nothing to close it).
std::unique_ptr<AppController> makeReadyController(QTemporaryDir& dir, const QString& dbFileName)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(dbFileName))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN77QT");
    controller->setSettings(settings);
    return controller;
}

} // namespace

void TestMainWindowRotorToggle::bothRotorWidgetsExistByDefault()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir, QStringLiteral("mw_default.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    const QList<RotorWidget*> widgets = window.findChildren<RotorWidget*>();
    QCOMPARE(widgets.size(), 2);
}

void TestMainWindowRotorToggle::disablingRotor2RemovesItsWidgetEntirely()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir, QStringLiteral("mw_disable.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    QCOMPARE(window.findChildren<RotorWidget*>().size(), 2);

    // The same two calls MainWindow::openSettingsDialog() makes after a
    // real SettingsDialog accept -- see the class comment.
    ContestSettings updated = controller->settings();
    updated.rotor2Enabled = false;
    controller->setSettings(updated);
    QMetaObject::invokeMethod(&window, "applyRotorWidgetSettings");

    // applyRotorSlot() hides + removeWidget()s synchronously but
    // deleteLater()s the actual QObject -- process the event loop so
    // the deferred deletion actually runs before checking the object
    // tree, otherwise the widget would still technically be a child
    // for one more event-loop turn.
    QCoreApplication::processEvents();
    QTest::qWait(10);
    QCoreApplication::processEvents();

    const QList<RotorWidget*> remaining = window.findChildren<RotorWidget*>();
    QCOMPARE(remaining.size(), 1);
    // Confirms specifically the disabled slot's widget is gone (not an
    // arbitrary one) -- the survivor must be slot 1's "2m" widget.
    QCOMPARE(remaining.first()->bandLabel(), QStringLiteral("2m"));
}

void TestMainWindowRotorToggle::reEnablingRotor2RecreatesItsWidget()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir, QStringLiteral("mw_reenable.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);

    ContestSettings disabled = controller->settings();
    disabled.rotor2Enabled = false;
    controller->setSettings(disabled);
    QMetaObject::invokeMethod(&window, "applyRotorWidgetSettings");
    QCoreApplication::processEvents();
    QTest::qWait(10);
    QCoreApplication::processEvents();
    QCOMPARE(window.findChildren<RotorWidget*>().size(), 1);

    ContestSettings reEnabled = controller->settings();
    reEnabled.rotor2Enabled = true;
    reEnabled.rotor2Label = QStringLiteral("70cm");
    controller->setSettings(reEnabled);
    QMetaObject::invokeMethod(&window, "applyRotorWidgetSettings");
    QCoreApplication::processEvents();

    const QList<RotorWidget*> widgets = window.findChildren<RotorWidget*>();
    QCOMPARE(widgets.size(), 2);
    QStringList labels;
    for (RotorWidget* widget : widgets) {
        labels << widget->bandLabel();
    }
    QVERIFY(labels.contains(QStringLiteral("2m")));
    QVERIFY(labels.contains(QStringLiteral("70cm")));
}

void TestMainWindowRotorToggle::mapBeamwidthReachesTheRotorDials()
{
    // The "Öffnungswinkel Rotor N" preference of the map is the one
    // beamwidth: the dials' cones start on it and follow every change.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir, QStringLiteral("mw_beamwidth.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* map = window.findChild<MapWidget*>();
    QVERIFY(map);
    const QList<RotorWidget*> widgets = window.findChildren<RotorWidget*>();
    QCOMPARE(widgets.size(), 2);
    QCOMPARE(widgets.at(0)->beamwidthDeg(), map->rotor1BeamwidthDeg());
    QCOMPARE(widgets.at(1)->beamwidthDeg(), map->rotor2BeamwidthDeg());

    map->setRotor1BeamwidthDeg(45.0);
    map->setRotor2BeamwidthDeg(20.0);
    QCOMPARE(widgets.at(0)->beamwidthDeg(), 45.0);
    QCOMPARE(widgets.at(1)->beamwidthDeg(), 20.0);

    // A rotor widget recreated later (slot toggled off and on) gets it
    // too.
    ContestSettings updated = controller->settings();
    updated.rotor2Enabled = false;
    controller->setSettings(updated);
    QMetaObject::invokeMethod(&window, "applyRotorWidgetSettings");
    updated.rotor2Enabled = true;
    controller->setSettings(updated);
    QMetaObject::invokeMethod(&window, "applyRotorWidgetSettings");
    QCoreApplication::processEvents();
    QTest::qWait(10);
    QCoreApplication::processEvents();
    const QList<RotorWidget*> recreated = window.findChildren<RotorWidget*>();
    QCOMPARE(recreated.size(), 2);
    QCOMPARE(recreated.at(1)->beamwidthDeg(), 20.0);
}

// Die Karte zeichnet die Speiche weiter, auch ohne Draht zum Rotor
// (Martin, 2026-09-14: sie zeigt, was der Steckplatz verfolgt) -- aber
// sie muss wissen, dass die Verbindung nicht steht, sonst sieht die
// Richtung aus wie eine gemessene Peilung.
void TestMainWindowRotorToggle::mapKnowsWhetherTheRotorLinkIsUp()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir, QStringLiteral("mw_link.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* map = window.findChild<MapWidget*>();
    QVERIFY(map);
    // Kein rotctld in diesem Prüfstand: beide Steckplätze haben ein
    // Bedienfeld, aber keine Verbindung.
    QVERIFY(!controller->rotor1Client().isConnected());
    QVERIFY(!map->rotorLinkLive(1));
    QVERIFY(!map->rotorLinkLive(2));
}

// Martin, 2026-09-28: "ich habe den rotor-zeiger geändert, aber in der
// karte war dieser auf null grad." Der Zeiger und der Lichtkegel
// müssen dieselbe Richtung zeigen, auch ohne Draht zum Rotor -- und
// gerade dann, denn ohne Gerät ist der Zeiger das Einzige, was man von
// Hand stellt.
void TestMainWindowRotorToggle::turningTheDialMovesTheMapsBeam()
{
    QTemporaryDir dir;
    auto controller = makeReadyController(dir, QStringLiteral("mw_beam.sqlite"));
    QVERIFY(controller);
    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* karte = window.findChild<MapWidget*>();
    QVERIFY(karte);
    const QList<RotorWidget*> rotoren = window.findChildren<RotorWidget*>();
    QVERIFY2(rotoren.size() >= 1, "kein Rotor-Bedienfeld");

    rotoren.first()->setAzimuthDeg(31.0);
    QCoreApplication::processEvents();
    qInfo().noquote() << "Zeiger:" << rotoren.first()->azimuthDeg() << "-- Karte:" << karte->rotorHeadingForTest(1);
    QCOMPARE(karte->rotorHeadingForTest(1), 31.0);

    if (rotoren.size() >= 2) {
        rotoren.at(1)->setAzimuthDeg(39.0);
        QCoreApplication::processEvents();
        qInfo().noquote() << "Zeiger 2:" << rotoren.at(1)->azimuthDeg()
                          << "-- Karte:" << karte->rotorHeadingForTest(2);
        QCOMPARE(karte->rotorHeadingForTest(2), 39.0);
    }
}

// ── Der Klick in die Kartenscheibe dreht wirklich ───────────────────
//
// Betreiber am 2026-10-28, mit Bild: „die rotorsteuerung sollte auch in
// KARTE/Verbindungen funktionieren!"
//
// Die Scheibe meldete den Winkel schon immer (test_karte_kurzwelle
// prueft das Signal), aber MainWindow nahm ihn auf einem eigenen Weg
// entgegen, der ohne verbundenen Rotor still aufgab. Hier haengt
// nichts an 172.30.30.16 -- genau der Fall, in dem es vorher nichts
// tat und im Rotorfenster sehr wohl etwas tut.
void TestMainWindowRotorToggle::derKlickInDieKartenscheibeDrehtDenRotor()
{
    QTemporaryDir dir;
    auto controller = makeReadyController(dir, QStringLiteral("mw_kartenscheibe.sqlite"));
    QVERIFY(controller);
    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* karte = window.findChild<MapWidget*>();
    QVERIFY(karte);
    const QList<RotorWidget*> rotoren = window.findChildren<RotorWidget*>();
    QVERIFY2(!rotoren.isEmpty(), "kein Rotor-Bedienfeld");
    RotorWidget* rotor1 = rotoren.first();

    // Die Scheiben sind von Haus aus aus -- erst einschalten, dann
    // zeichnen lassen, denn ihre Lage merkt sich die Karte beim Malen.
    karte->setRotorDialsVisible(true);
    rotor1->setAzimuthDeg(180.0);
    QCoreApplication::processEvents();
    // Offscreen bekommt das Kartenfeld nur 664x260 Punkte -- darin ist
    // fuer keine Scheibe Platz, die Schranke in drawNumbersColumn()
    // bricht ab (siehe auch die Fensterbreite unter xvfb). Also die
    // Karte selbst auf eine Groesse bringen, wie sie am Schirm hat;
    // der Klick geht ohnehin direkt an dieses Widget.
    karte->resize(560, 760);
    karte->grab();

    const QRectF scheibe = karte->rotorDialRectForTest(1);
    QVERIFY2(!scheibe.isEmpty(), "die Scheibe hat keine Flaeche");
    QVERIFY2(!rotor1->hasTargetBearing(), "da steht schon ein Ziel, bevor geklickt wurde");

    // Oben in der Scheibe sind 0 Grad.
    const QPoint oben(int(scheibe.center().x()), int(scheibe.top() + 4.0));
    QTest::mouseClick(karte, Qt::LeftButton, Qt::NoModifier, oben);
    QCoreApplication::processEvents();

    qInfo().noquote() << "nach dem Klick: Ziel" << rotor1->targetBearingDeg()
                      << "gesetzt:" << rotor1->hasTargetBearing()
                      << "Nadel:" << rotor1->azimuthDeg();
    QVERIFY2(rotor1->hasTargetBearing(), "der Klick in die Kartenscheibe setzte kein Ziel");
    const double ziel = rotor1->targetBearingDeg();
    QVERIFY2(ziel < 5.0 || ziel > 355.0,
             qPrintable(QStringLiteral("oben sind 0 Grad, als Ziel steht %1").arg(ziel)));

    // Und die Nadel laeuft los, auch ohne Draht zum Rotor
    // (RotorWidget::startSimulatedTurn, 2,5 Grad je 100 ms).
    QTRY_VERIFY_WITH_TIMEOUT(rotor1->azimuthDeg() != 180.0, 3000);
    qInfo().noquote() << "die Nadel laeuft:" << rotor1->azimuthDeg();

    // Und die Karte dreht mit -- sie ist dasselbe Instrument.
    QTRY_COMPARE_WITH_TIMEOUT(karte->rotorHeadingForTest(1), rotor1->azimuthDeg(), 3000);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestMainWindowRotorToggle tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_mainwindow_rotor_toggle.moc"
