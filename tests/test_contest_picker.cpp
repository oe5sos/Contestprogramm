#include <QtTest>
#include <limits>
#include <algorithm>
#include <QDateTime>
#include <QPushButton>
#include <QLineEdit>
#include <QDialogButtonBox>

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
    void dialogAsksForTheOwnLocatorAndOffersTheExactSquare();
    void derNaechsteTerminStehtObenUndIstVorgewaehlt();
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

    // Die Kennung wird in ALLEN Spalten gesucht, nicht in einer fest
    // genannten. Hier stand „column 1 == ID", und als am 2026-10-23
    // die Spalte „Nächster Termin" nach vorn kam, fiel dieser Lauf an
    // einer Stelle, an der sich nichts Falsches geaendert hatte.
    int iaruRow = -1;
    for (int row = 0; row < table->rowCount() && iaruRow < 0; ++row) {
        for (int col = 0; col < table->columnCount(); ++col) {
            QTableWidgetItem* item = table->item(row, col);
            if (item && item->text() == QStringLiteral("IARU_R1_VHF_UHF")) {
                iaruRow = row;
                break;
            }
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

void TestContestPicker::dialogAsksForTheOwnLocatorAndOffersTheExactSquare()
{
    // The locator is asked every time a contest is picked (operator,
    // 2026-09-21): prefilled, OK only with a valid one, the exact
    // position's square offered when it differs.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir, QStringLiteral("picker_grid.sqlite"));
    QVERIFY(controller);
    ContestPickerDialog dialog(controller->availableContestDefinitions(), QStringLiteral("IARU_R1_UHF"));
    auto* edit = dialog.findChild<QLineEdit*>(QStringLiteral("contestPickerGridEdit"));
    auto* exactButton = dialog.findChild<QPushButton*>(QStringLiteral("contestPickerExactGridButton"));
    QVERIFY(edit);
    QVERIFY(exactButton);
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    QVERIFY(buttons);
    QPushButton* ok = buttons->button(QDialogButtonBox::Ok);
    QVERIFY(ok);

    // Nothing set yet: no valid locator, no OK.
    QVERIFY(!ok->isEnabled());

    // The home locator prefilled; the exact position (Feuerkogel) in
    // another square: offered.
    dialog.setOwnGrid(QStringLiteral("jn67vv"), QStringLiteral("JN67UT"));
    QCOMPARE(dialog.ownGrid(), QStringLiteral("JN67VV"));
    QVERIFY(ok->isEnabled());
    QVERIFY(!exactButton->isHidden());
    QVERIFY(exactButton->text().contains(QStringLiteral("JN67UT")));
    exactButton->click();
    QCOMPARE(dialog.ownGrid(), QStringLiteral("JN67UT"));
    QVERIFY(exactButton->isHidden()); // now the same square

    // A typo disables OK until it is a locator again.
    edit->setText(QStringLiteral("JN6"));
    QVERIFY(!ok->isEnabled());
    edit->setText(QStringLiteral("JN67UT"));
    QVERIFY(ok->isEnabled());

    // No exact position known: nothing offered.
    dialog.setOwnGrid(QStringLiteral("JN67VV"));
    QVERIFY(exactButton->isHidden());
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestContestPicker tc;
    return QTest::qExec(&tc, argc, argv);
}
// ── Der nächste Termin steht oben und ist vorgewählt ─────────────────
//
// Betreiber am 2026-10-23: „toll wäre es, immer die conteste der
// nächsten 4 wochen zu sehen, sodass man nicht lange suchen muss.
// diese sollten dann automatisch vorgeschlagen werden."
//
// Mit dreizehn ausgelieferten Regeln ist „welcher ist am Wochenende"
// aus einer alphabetischen Liste nicht mehr abzulesen.
void TestContestPicker::derNaechsteTerminStehtObenUndIstVorgewaehlt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir, QStringLiteral("picker_termin.sqlite"));
    QVERIFY(controller);

    const QVector<ContestDefinition> regeln = controller->availableContestDefinitions();
    QVERIFY2(regeln.size() >= 10,
             "zu wenige Regeln -- dieser Lauf sagt dann nichts über Sortierung");

    // Ohne vorgegebenen Contest: die Vorauswahl ist der nächste
    // anstehende, nicht einfach die erste Zeile einer Dateiliste.
    ContestPickerDialog dialog(regeln, QString());
    auto* table = dialog.findChild<QTableWidget*>();
    QVERIFY(table);
    QCOMPARE(table->columnCount(), 5);
    QCOMPARE(table->horizontalHeaderItem(0)->text(), QStringLiteral("Nächster Termin"));

    // Die Spalte ist nach Frist sortiert: jede Zeile ist höchstens so
    // weit weg wie die darunter. Geprüft wird gegen die Regeln selbst,
    // nicht gegen eine im Prüflauf nachgebaute Reihenfolge.
    const QDateTime jetzt = QDateTime::currentDateTimeUtc();
    qint64 vorige = std::numeric_limits<qint64>::min();
    for (int row = 0; row < table->rowCount(); ++row) {
        QString id;
        for (int col = 0; col < table->columnCount(); ++col) {
            QTableWidgetItem* it = table->item(row, col);
            if (it && regeln.cend() != std::find_if(regeln.cbegin(), regeln.cend(),
                    [&](const ContestDefinition& d) { return d.id() == it->text(); })) {
                id = it->text();
                break;
            }
        }
        QVERIFY2(!id.isEmpty(), qPrintable(QStringLiteral("Zeile %1 ohne Kennung").arg(row)));

        const auto* def = controller->findContestDefinition(id);
        QVERIFY(def);
        qint64 frist = std::numeric_limits<qint64>::max();
        if (def->schedule().isValid()) {
            const ContestWindow w = def->schedule().windowUpcoming(jetzt);
            if (w.isValid()) {
                frist = w.contains(jetzt) ? -1 : jetzt.secsTo(w.startUtc);
            }
        }
        QVERIFY2(frist >= vorige,
                 qPrintable(QStringLiteral("Zeile %1 (%2) steht nach einem "
                                           "späteren Termin").arg(row).arg(id)));
        vorige = frist;
    }

    // Und die Vorauswahl liegt auf der obersten Zeile mit einem Termin
    // -- das ist der „automatisch vorgeschlagen" aus dem Wunsch.
    QVERIFY(table->currentRow() >= 0);
    QVERIFY2(table->currentRow() <= 1,
             qPrintable(QStringLiteral("vorgewählt ist Zeile %1, nicht der "
                                       "nächste Termin").arg(table->currentRow())));

    // Ein ausdrücklich genannter Contest schlägt den Vorschlag -- sonst
    // überginge die Automatik die Wahl des Betreibers.
    ContestPickerDialog mitVorgabe(regeln, QStringLiteral("IARU_R1_MARCONI"));
    QCOMPARE(mitVorgabe.selectedContestId(), QStringLiteral("IARU_R1_MARCONI"));
}

#include "test_contest_picker.moc"
