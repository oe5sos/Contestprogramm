#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QMenu>
#include <QMenuBar>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "ui/MainWindow.h"

#include <memory>

using namespace Contestprogramm;

namespace {

std::unique_ptr<AppController> makeController(QTemporaryDir& dir)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(QStringLiteral("menu.sqlite")))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    controller->setSettings(settings);
    return controller;
}

QAction* findAction(QWidget* window, const QString& text)
{
    const auto actions = window->findChildren<QAction*>();
    for (QAction* action : actions) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

// Triggers `action` and counts how many modal dialogs that one click
// puts on screen, closing each as it appears. The dialogs here are
// exec()-modal, so the trigger() call below does not return until they
// are gone -- which is exactly why a double connection is invisible in
// any test that calls the handler directly instead of the menu entry.
int modalDialogsOpenedBy(QAction* action)
{
    int opened = 0;
    QTimer closer;
    closer.setInterval(5);
    QObject::connect(&closer, &QTimer::timeout, [&opened]() {
        if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
            ++opened;
            dialog->reject();
        }
    });
    closer.start();
    action->trigger();
    closer.stop();
    return opened;
}

} // namespace

// A menu entry has to do its one thing, once. Both halves of this were
// one typo: "Datei › Einstellungen..." was created and the connect()
// under it named a DIFFERENT action (the gear menu's own settings
// entry, one near-identical variable name away). So the file menu's
// entry did nothing at all, and the gear menu's entry was wired twice
// and put the modal dialog up two times in a row -- close it, and it
// comes straight back.
//
// Checked by WIRING? No: a QAction inside a real QMenu always carries
// Qt's own internal connections to triggered(), so "is anything
// connected" answers yes even for a dead entry (measured, 2026-10-15).
// The only honest question is what a click actually does.
class TestMenuActions : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void settingsEntriesOpenTheDialogExactlyOnce();
};

void TestMenuActions::settingsEntriesOpenTheDialogExactlyOnce()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir);
    QVERIFY(controller);
    MainWindow window(*controller);

    // The gear menu's entry (ui/MainWindow.cpp: "optionSettings").
    QAction* gearSettings = window.findChild<QAction*>(QStringLiteral("optionSettings"));
    QVERIFY(gearSettings);
    QCOMPARE(modalDialogsOpenedBy(gearSettings), 1);

    // And the file menu's own entry, which has no objectName -- found
    // by the text the operator reads.
    QAction* fileSettings = findAction(&window, QStringLiteral("&Einstellungen..."));
    QVERIFY2(fileSettings, "Datei › Einstellungen... gibt es nicht mehr");
    QCOMPARE(modalDialogsOpenedBy(fileSettings), 1);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestMenuActions tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_menu_actions.moc"
