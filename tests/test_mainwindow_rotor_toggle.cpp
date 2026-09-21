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

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestMainWindowRotorToggle tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_mainwindow_rotor_toggle.moc"
