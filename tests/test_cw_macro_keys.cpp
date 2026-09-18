#include <QtTest>

#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "ui/CwMacroPanel.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {

std::unique_ptr<AppController> makeReadyController(QTemporaryDir& dir)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(QStringLiteral("keys.sqlite")))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    controller->setSettings(settings);
    return controller;
}

} // namespace

// F1..F6 fire the macro row's templates and Esc requests a keying stop
// -- through the real MainWindow, as key presses.
class TestCwMacroKeys : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void panelActivatesByIndexAndOffersStop();
    void functionKeysKeyTheMacroAndEscStops();
};

void TestCwMacroKeys::panelActivatesByIndexAndOffersStop()
{
    CwMacroPanel panel;
    panel.setMacroTemplates({QStringLiteral("{call}"), QStringLiteral("TU")});
    QSignalSpy activated(&panel, &CwMacroPanel::macroActivated);
    panel.activateMacro(1);
    QCOMPARE(activated.size(), 1);
    QCOMPARE(activated.first().at(0).toString(), QStringLiteral("TU"));
    panel.activateMacro(2); // no such template
    panel.activateMacro(-1);
    QCOMPARE(activated.size(), 1);

    QSignalSpy stop(&panel, &CwMacroPanel::stopRequested);
    QPushButton* stopButton = nullptr;
    for (QPushButton* button : panel.findChildren<QPushButton*>()) {
        if (button->text().contains(QStringLiteral("Esc"))) {
            stopButton = button;
        }
    }
    QVERIFY(stopButton);
    stopButton->click();
    QCOMPARE(stop.size(), 1);
}

void TestCwMacroKeys::functionKeysKeyTheMacroAndEscStops()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir);
    QVERIFY(controller);
    MainWindow window(*controller);
    window.show();
    // QShortcut matching needs an active window (QShortcutMap::
    // correctContext returns false without one); a headless test run
    // does not get one from show() alone.
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(&window);
    QT_WARNING_POP
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    log->setCallsign(QStringLiteral("DL1ABC"));

    // F2 is the second default template, "{call} 5NN {exchange}".
    QTest::keyClick(&window, Qt::Key_F2);
    QVERIFY2(window.statusBar()->currentMessage().startsWith(QStringLiteral("CW: DL1ABC 5NN ")),
             qPrintable(window.statusBar()->currentMessage()));

    QTest::keyClick(&window, Qt::Key_Escape);
    QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("CW gestoppt"));

    // Alt+W wipes the entry row.
    QCOMPARE(log->callsign(), QStringLiteral("DL1ABC"));
    QTest::keyClick(&window, Qt::Key_W, Qt::AltModifier);
    QVERIFY(log->callsign().isEmpty());

    // PgUp/PgDn move the keyer speed by 2 WpM and remember it.
    QCOMPARE(controller->settings().cwSpeedWpm, 24);
    QTest::keyClick(&window, Qt::Key_PageUp);
    QCOMPARE(controller->settings().cwSpeedWpm, 26);
    QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("CW 26 WpM"));
    QTest::keyClick(&window, Qt::Key_PageDown);
    QTest::keyClick(&window, Qt::Key_PageDown);
    QCOMPARE(controller->settings().cwSpeedWpm, 22);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestCwMacroKeys tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_cw_macro_keys.moc"
