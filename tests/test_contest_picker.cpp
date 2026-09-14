#include <QtTest>

#include <QApplication>
#include <QFile>
#include <QMetaObject>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDefinition.h"
#include "ui/ContestPickerDialog.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

// Covers the operator's "dedicated Contest wählen... picker window"
// request: ContestPickerDialog's own row-selection behavior, and --
// mirroring test_contestrules_override.cpp's approach of driving real
// AppController/MainWindow objects -- that picking a different contest
// actually reaches UnifiedLogWidget's dynamic exchange-field row through
// the exact mechanism MainWindow::openContestPicker() uses
// (AppController::setSettings() + the private-slot
// MainWindow::applyActiveContestDefinition(), driven here via
// QMetaObject::invokeMethod since a real modal ContestPickerDialog::
// exec() cannot be driven headlessly -- same caveat/approach
// test_mainwindow_rotor_toggle.cpp already established for
// SettingsDialog).
class TestContestPicker : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void dialogPreselectsInitialContest();
    void dialogSelectedContestIdFollowsRowSelection();
    void pickingDifferentContestRebuildsUnifiedLogExchangeFields();
};

namespace {

// Same helper shape as test_mainwindow_rotor_toggle.cpp's
// makeReadyController: ownCallsign already set so MainWindow's
// constructor does not auto-open the first-run SettingsDialog (which
// would otherwise block the test on a modal event loop with nothing to
// close it).
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

void TestContestPicker::initTestCase()
{
    // ContestDefinition::overrideDirectory() is built from
    // QStandardPaths::AppDataLocation -- sandbox it for this test
    // process the same way test_contestrules_override.cpp does, so the
    // override this test writes below does not land in (or read stale
    // state back out of) the real operator's app-data directory.
    QStandardPaths::setTestModeEnabled(true);
}

void TestContestPicker::dialogPreselectsInitialContest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir, QStringLiteral("picker_preselect.sqlite"));
    QVERIFY(controller);
    QVERIFY(controller->availableContestDefinitions().size() >= 2);

    ContestPickerDialog dialog(controller->availableContestDefinitions(), QStringLiteral("OE_VHF_UHF"));
    QCOMPARE(dialog.selectedContestId(), QStringLiteral("OE_VHF_UHF"));
}

void TestContestPicker::dialogSelectedContestIdFollowsRowSelection()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir, QStringLiteral("picker_select.sqlite"));
    QVERIFY(controller);

    ContestPickerDialog dialog(controller->availableContestDefinitions(), QStringLiteral("OE_VHF_UHF"));
    auto* table = dialog.findChild<QTableWidget*>();
    QVERIFY(table);

    int iaruRow = -1;
    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem* idItem = table->item(row, 1); // column 1 == ID, see ContestPickerDialog.cpp
        if (idItem && idItem->text() == QStringLiteral("IARU_R1_VHF_UHF")) {
            iaruRow = row;
            break;
        }
    }
    QVERIFY(iaruRow >= 0);

    table->selectRow(iaruRow);
    QCOMPARE(dialog.selectedContestId(), QStringLiteral("IARU_R1_VHF_UHF"));
}

void TestContestPicker::pickingDifferentContestRebuildsUnifiedLogExchangeFields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir, QStringLiteral("picker_rebuild.sqlite"));
    QVERIFY(controller);

    // Both shipped contest_definitions/*.json declare the exact same
    // three exchange fields (rst+serial+grid) -- switching between them
    // as-shipped would not observably prove a rebuild actually
    // happened. Give IARU_R1_VHF_UHF a one-field override instead, same
    // technique test_contestrules_override.cpp uses.
    const ContestDefinition* iaruOriginal = controller->findContestDefinition(QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY(iaruOriginal);
    ContestDefinition::ExchangeField onlyField;
    onlyField.key = QStringLiteral("serial");
    onlyField.label = QStringLiteral("Serial");
    onlyField.type = QStringLiteral("int");
    onlyField.autoIncrement = true;
    const ContestDefinition overridden = iaruOriginal->withExchangeFields({onlyField});
    const QString overridePath = ContestDefinition::overrideFilePath(QStringLiteral("IARU_R1_VHF_UHF"));
    QString error;
    QVERIFY2(overridden.saveToFile(overridePath, &error), qPrintable(error));
    controller->reloadContestDefinitions();

    ContestSettings settings = controller->settings();
    settings.activeContestId = QStringLiteral("OE_VHF_UHF");
    controller->setSettings(settings);

    MainWindow window(*controller);
    auto* unifiedLog = window.findChild<UnifiedLogWidget*>();
    QVERIFY(unifiedLog);
    QCOMPARE(unifiedLog->exchangeReceived().size(), 3); // OE_VHF_UHF, as shipped: rst + serial + grid

    // Exactly the two calls MainWindow::openContestPicker() makes after
    // a real ContestPickerDialog::exec() == Accepted (see
    // ui/MainWindow.cpp): AppController::setSettings() with the picked
    // id, then applyActiveContestDefinition() to rebuild the entry row.
    ContestSettings picked = controller->settings();
    picked.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    controller->setSettings(picked);
    QMetaObject::invokeMethod(&window, "applyActiveContestDefinition");

    QCOMPARE(unifiedLog->exchangeReceived().size(), 1); // overridden IARU_R1_VHF_UHF: serial only
    QVERIFY(unifiedLog->exchangeReceived().contains(QStringLiteral("serial")));

    // Clean up: setTestModeEnabled's sandbox is per-process, not
    // per-test, so leaving this behind could leak into a test that runs
    // later in the same process.
    QFile::remove(overridePath);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestContestPicker tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_contest_picker.moc"
