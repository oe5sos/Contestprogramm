#include <QtTest>

#include <QApplication>
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

// The "CW-Makros anzeigen" checkbox lives in MainWindow's filter row
// alongside the existing raw-feed toggle and has no distinguishing
// objectName (matching the existing rawFeedCheck's own local-variable
// style) -- found here by its visible text instead.
QCheckBox* findCheckboxByText(QWidget* root, const QString& text)
{
    for (QCheckBox* box : root->findChildren<QCheckBox*>()) {
        if (box->text() == text) {
            return box;
        }
    }
    return nullptr;
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

    QCheckBox* cwCheck = findCheckboxByText(&window, QStringLiteral("CW-Makros anzeigen"));
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
