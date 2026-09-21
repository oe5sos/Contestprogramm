#include <QtTest>

#include <QApplication>
#include <QMenu>
#include <QPushButton>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/MapWidget.h"
#include "ui/PanelContainerWidget.h"
#include "ui/PanelHeaderBar.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {

std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& contestId)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(QStringLiteral("corrections.sqlite")))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = contestId;
    settings.esmEnabled = false;
    controller->setSettings(settings);
    return controller;
}

void logQso(UnifiedLogWidget* log, const QString& call, const QString& serial, const QString& grid)
{
    log->setCallsign(call);
    log->setExchangeFieldValue(QStringLiteral("serial"), serial);
    log->setExchangeFieldValue(QStringLiteral("grid"), grid);
    emit log->logRequested();
}

} // namespace

// Corrections in the log through the real MainWindow: a changed
// callsign or an invalidated QSO re-derives the dupe flags
// (data/DupeRescore.h); a single-mode contest starts in its mode; a
// QSO outside the contest period leaves a note in the status bar.
class TestLogCorrections : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void correctedCallsignRescoresTheDupeFlags();
    void invalidatingTheFirstQsoFreesTheSecond();
    void singleModeContestStartsInThatMode();
    void loggingOutsideThePeriodLeavesANote();
    void mapOptionsMenuOpensFromThePanelHeader();
};

void TestLogCorrections::correctedCallsignRescoresTheDupeFlags()
{
    QTemporaryDir dir;
    auto controller = makeController(dir, QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    const QString contestId = controller->settings().activeContestId;

    logQso(log, QStringLiteral("DL1ABC"), QStringLiteral("1"), QStringLiteral("JN58SD"));
    logQso(log, QStringLiteral("DL1ABX"), QStringLiteral("2"), QStringLiteral("JN58SD"));
    QVector<QsoRecord> qsos = controller->database().qsosForContest(contestId);
    QCOMPARE(qsos.size(), 2);
    QVERIFY(!qsos.at(1).isDupe);

    // The typo corrected: now the second DL1ABC on the band -> dupe.
    emit log->historyCallsignEditRequested(qsos.at(1).id, QStringLiteral("DL1ABC"));
    qsos = controller->database().qsosForContest(contestId);
    QCOMPARE(qsos.at(1).callsign, QStringLiteral("DL1ABC"));
    QVERIFY(qsos.at(1).isDupe);
    QVERIFY(!qsos.at(0).isDupe);
    QVERIFY2(window.statusBar()->currentMessage().startsWith(QStringLiteral("Dupe-Status von 1 QSO")),
             qPrintable(window.statusBar()->currentMessage()));

    // And back to a different station: the flag goes again.
    emit log->historyCallsignEditRequested(qsos.at(1).id, QStringLiteral("DL1ABD"));
    qsos = controller->database().qsosForContest(contestId);
    QVERIFY(!qsos.at(1).isDupe);
}

void TestLogCorrections::invalidatingTheFirstQsoFreesTheSecond()
{
    QTemporaryDir dir;
    auto controller = makeController(dir, QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    const QString contestId = controller->settings().activeContestId;

    logQso(log, QStringLiteral("DL1ABC"), QStringLiteral("1"), QStringLiteral("JN58SD"));
    logQso(log, QStringLiteral("DL1ABC"), QStringLiteral("2"), QStringLiteral("JN58SD"));
    QVector<QsoRecord> qsos = controller->database().qsosForContest(contestId);
    QCOMPARE(qsos.size(), 2);
    QVERIFY(qsos.at(1).isDupe);

    emit log->historyInvalidToggleRequested(qsos.at(0).id);
    qsos = controller->database().qsosForContest(contestId);
    QVERIFY(qsos.at(0).isInvalid);
    QVERIFY(!qsos.at(1).isDupe);

    // Un-invalidated: the first is the valid one again, the second a dupe.
    emit log->historyInvalidToggleRequested(qsos.at(0).id);
    qsos = controller->database().qsosForContest(contestId);
    QVERIFY(!qsos.at(0).isInvalid);
    QVERIFY(qsos.at(1).isDupe);
}

void TestLogCorrections::singleModeContestStartsInThatMode()
{
    QTemporaryDir dir;
    auto controller = makeController(dir, QStringLiteral("IARU_R1_MARCONI"));
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    // No rig ever reported a mode: the CW-only contest still logs CW,
    // with the CW report default, not SSB/59.
    logQso(log, QStringLiteral("DL1ABC"), QStringLiteral("1"), QStringLiteral("JN58SD"));
    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_MARCONI"));
    QCOMPARE(qsos.size(), 1);
    QCOMPARE(qsos.first().mode, QStringLiteral("CW"));
    QCOMPARE(qsos.first().rstSent, QStringLiteral("599"));
    QCOMPARE(qsos.first().band, QStringLiteral("144"));
}

void TestLogCorrections::loggingOutsideThePeriodLeavesANote()
{
    QTemporaryDir dir;
    auto controller = makeController(dir, QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY(controller);
    // A manual contest end long past: every QSO logged now is outside.
    ContestSettings settings = controller->settings();
    settings.contestEndUtc = QStringLiteral("2020-01-02T00:00:00Z");
    controller->setSettings(settings);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    logQso(log, QStringLiteral("DL1ABC"), QStringLiteral("1"), QStringLiteral("JN58SD"));
    QCOMPARE(controller->database().qsoCountForContest(controller->settings().activeContestId), 1); // logged anyway
    QVERIFY2(window.statusBar()->currentMessage().startsWith(QStringLiteral("Hinweis: QSO außerhalb des Contestzeitraums")),
             qPrintable(window.statusBar()->currentMessage()));
    QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("Mi 01.01. 00:00 – Do 02.01. 00:00 UTC")));
}

void TestLogCorrections::mapOptionsMenuOpensFromThePanelHeader()
{
    QTemporaryDir dir;
    auto controller = makeController(dir, QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY(controller);
    MainWindow window(*controller);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* map = window.findChild<MapWidget*>();
    QVERIFY(map);
    // The ⚙ of the map panel's header bar: the PanelHeaderBar of the
    // container the map widget lives in.
    QWidget* container = map->parentWidget();
    while (container && !qobject_cast<PanelContainerWidget*>(container)) {
        container = container->parentWidget();
    }
    auto* panel = qobject_cast<PanelContainerWidget*>(container);
    QVERIFY(panel);
    QVERIFY(panel->headerBar());
    auto* button = panel->headerBar()->findChild<QPushButton*>(QStringLiteral("panelHeaderOptionsButton"));
    QVERIFY(button);
    QVERIFY(button->isVisible());

    const auto visibleMenus = [&window] {
        int n = 0;
        for (QMenu* menu : window.findChildren<QMenu*>()) {
            n += menu->isVisible() ? 1 : 0;
        }
        return n;
    };
    QCOMPARE(visibleMenus(), 0);
    // Through the window system: delivered like a real click, hit-tested
    // to the widget under the point.
    const QPoint inWindow = window.mapFromGlobal(button->mapToGlobal(button->rect().center()));
    QCOMPARE(window.childAt(inWindow), button);
    QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, inWindow);
    QTRY_COMPARE(visibleMenus(), 1);
    QMenu* shown = nullptr;
    for (QMenu* menu : window.findChildren<QMenu*>()) {
        if (menu->isVisible()) {
            shown = menu;
        }
    }
    QVERIFY(shown);
    QStringList texts;
    for (QAction* action : shown->actions()) {
        texts << action->text();
    }
    QVERIFY2(texts.contains(QStringLiteral("Entfernungsringe")), qPrintable(texts.join(QStringLiteral(" | "))));
    QVERIFY(texts.contains(QStringLiteral("Öffnungswinkel Rotor 1")));
    QVERIFY(texts.contains(QStringLiteral("Grenzen")));
    shown->close();
    QTRY_COMPARE(visibleMenus(), 0);

    // The same in the other view, and a second time (a fresh menu each).
    map->setView(MapWidget::View::Radar);
    QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, inWindow);
    QTRY_COMPARE(visibleMenus(), 1);
    for (QMenu* menu : window.findChildren<QMenu*>()) {
        if (menu->isVisible()) {
            menu->close();
        }
    }
    QTRY_COMPARE(visibleMenus(), 0);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestLogCorrections tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_log_corrections.moc"
