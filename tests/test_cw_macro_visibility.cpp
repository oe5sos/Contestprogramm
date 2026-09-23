#include <QtTest>

#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "ui/MainWindow.h"

#include <memory>

using namespace Contestprogramm;

// Covers the operator's "CW macro row hidden by default, toggleable"
// request: ContestSettings::cwMacroPanelVisible's persistence (default
// false), and -- mirroring test_mainwindow_rotor_toggle.cpp's approach of
// driving a real MainWindow -- that the row is actually hidden by
// default and that the "CW-Makros anzeigen" checkbox in the filter row
// both flips its visibility and persists the choice back through
// AppController::setSettings(). Does not touch RigctldClient::
// sendMorse() or the macro templates themselves -- this is purely about
// the row's default visibility, per the task's own scope note.
class TestCwMacroVisibility : public QObject
{
    Q_OBJECT

private slots:
    void defaultSettingRoundTripsHidden();
    void settingRoundTripsVisibleWhenSetTrue();
    void mainWindowHidesCwRowByDefault();
    void mainWindowShowsCwRowWhenSettingIsTrue();
    void checkboxTogglePersistsSettingAndFlipsRowVisibility();
};

namespace {

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

// "CW-Makros anzeigen" hängt seit 2026-09-23 im ⚙-Menü der obersten
// Zeile (vorher ein Kästchen in einer eigenen Filterzeile) -- Martins
// Regel: Optionen gehören rechts oben unter das Zahnrad. Über den
// objectName gesucht, nicht über den sichtbaren Text.
QAction* findOption(QWidget* root, const QString& objectName)
{
    return root->findChild<QAction*>(objectName);
}

} // namespace

void TestCwMacroVisibility::defaultSettingRoundTripsHidden()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("cw_default.sqlite")), QStringLiteral("cw_visibility_default")));

    ContestSettings settings;
    QVERIFY(!settings.cwMacroPanelVisible); // hidden by default, per the task
    settings.saveTo(db);

    ContestSettings reloaded;
    reloaded.loadFrom(db);
    QVERIFY(!reloaded.cwMacroPanelVisible);
}

void TestCwMacroVisibility::settingRoundTripsVisibleWhenSetTrue()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("cw_visible.sqlite")), QStringLiteral("cw_visibility_true")));

    ContestSettings settings;
    settings.cwMacroPanelVisible = true;
    settings.saveTo(db);

    ContestSettings reloaded;
    reloaded.loadFrom(db);
    QVERIFY(reloaded.cwMacroPanelVisible);
}

void TestCwMacroVisibility::mainWindowHidesCwRowByDefault()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir, QStringLiteral("mw_cw_default.sqlite"));
    QVERIFY(controller);
    QVERIFY(!controller->settings().cwMacroPanelVisible);

    MainWindow window(*controller);
    auto* cwRow = window.findChild<QWidget*>(QStringLiteral("cwMacroRow"));
    QVERIFY(cwRow);
    QVERIFY(cwRow->isHidden());
}

void TestCwMacroVisibility::mainWindowShowsCwRowWhenSettingIsTrue()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir, QStringLiteral("mw_cw_visible.sqlite"));
    QVERIFY(controller);
    ContestSettings settings = controller->settings();
    settings.cwMacroPanelVisible = true;
    controller->setSettings(settings);

    MainWindow window(*controller);
    auto* cwRow = window.findChild<QWidget*>(QStringLiteral("cwMacroRow"));
    QVERIFY(cwRow);
    QVERIFY(!cwRow->isHidden());
}

void TestCwMacroVisibility::checkboxTogglePersistsSettingAndFlipsRowVisibility()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir, QStringLiteral("mw_cw_toggle.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* cwRow = window.findChild<QWidget*>(QStringLiteral("cwMacroRow"));
    QVERIFY(cwRow);
    QVERIFY(cwRow->isHidden());

    QAction* cwCheck = findOption(&window, QStringLiteral("optionCwMacros"));
    QVERIFY(cwCheck);
    QVERIFY(!cwCheck->isChecked());

    cwCheck->setChecked(true);
    QVERIFY(!cwRow->isHidden());
    QVERIFY(controller->settings().cwMacroPanelVisible);

    cwCheck->setChecked(false);
    QVERIFY(cwRow->isHidden());
    QVERIFY(!controller->settings().cwMacroPanelVisible);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestCwMacroVisibility tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_cw_macro_visibility.moc"
