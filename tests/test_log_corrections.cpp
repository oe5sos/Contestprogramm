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
    void editingNrGridIsTypeAwareNotPositional();
    void aKnownCallsignPrefillsTheGridButNeverTheSerial();
    void enterOnAnIncompleteExchangeAsksOnceThenLogs();
    void aQsoInTheOwnSquareHasNoBearing();
    void invalidQsosLeaveTheCounts();
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

    // And a second time (a fresh menu each).
    QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, inWindow);
    QTRY_COMPARE(visibleMenus(), 1);
    for (QMenu* menu : window.findChildren<QMenu*>()) {
        if (menu->isVisible()) {
            menu->close();
        }
    }
    QTRY_COMPARE(visibleMenus(), 0);
}

// The Nr./Grid cell of a logged row is one text ("12 JN58SD"); a hand
// edit must be read by what each token IS, not by its position -- found
// 2026-09-21 in the operator's own log: "JN67UT" typed alone was taken
// as a failed serial and the locator vanished, "59003" on a row without
// RST became the RST. The RST itself is the record's own (that column is
// not editable), and the stored exchange text is recomposed the way a
// fresh log composes it (zero-padded serial).
void TestLogCorrections::editingNrGridIsTypeAwareNotPositional()
{
    QTemporaryDir dir;
    auto controller = makeController(dir, QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    const QString contestId = controller->settings().activeContestId;

    logQso(log, QStringLiteral("OE5XYZ"), QStringLiteral("3"), QStringLiteral("JN78CD"));
    QVector<QsoRecord> qsos = controller->database().qsosForContest(contestId);
    QCOMPARE(qsos.size(), 1);
    const int id = qsos.at(0).id;
    QCOMPARE(qsos.at(0).rstRcvd, QStringLiteral("59"));

    // Grid only (as UnifiedFeedModel::setData composes it: RST first).
    emit log->historyExchangeRcvdEditRequested(id, QStringLiteral("59 JN67UT"));
    auto q = controller->database().qsoById(id);
    QVERIFY(q);
    QCOMPARE(q->gridSquare, QStringLiteral("JN67UT"));
    QVERIFY(!q->serialRcvd.has_value());
    QCOMPARE(q->rstRcvd, QStringLiteral("59"));
    QCOMPARE(q->exchangeRcvd, QStringLiteral("59 JN67UT"));
    QVERIFY(q->distanceKm.has_value());

    // Serial only: the grid the operator removed is gone, nothing else.
    emit log->historyExchangeRcvdEditRequested(id, QStringLiteral("59 7"));
    q = controller->database().qsoById(id);
    QCOMPARE(*q->serialRcvd, 7);
    QVERIFY(q->gridSquare.isEmpty());
    QCOMPARE(q->exchangeRcvd, QStringLiteral("59 007"));
    QVERIFY(!q->distanceKm.has_value());

    // Both, in either order.
    emit log->historyExchangeRcvdEditRequested(id, QStringLiteral("59 jn58sd 12"));
    q = controller->database().qsoById(id);
    QCOMPARE(*q->serialRcvd, 12);
    QCOMPARE(q->gridSquare, QStringLiteral("JN58SD"));
    QCOMPARE(q->rstRcvd, QStringLiteral("59"));
    QCOMPARE(q->exchangeRcvd, QStringLiteral("59 012 JN58SD"));

    // A row logged without RST: the first token is NOT an RST.
    QsoRecord noRst;
    noRst.callsign = QStringLiteral("DL9ZZZ");
    noRst.band = QStringLiteral("144");
    noRst.mode = QStringLiteral("SSB");
    noRst.timestampUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    noRst.contestId = contestId;
    QVERIFY(controller->database().insertQso(noRst));
    emit log->historyExchangeRcvdEditRequested(noRst.id, QStringLiteral("45 JN47AB"));
    q = controller->database().qsoById(noRst.id);
    QVERIFY(q->rstRcvd.isEmpty());
    QCOMPARE(*q->serialRcvd, 45);
    QCOMPARE(q->gridSquare, QStringLiteral("JN47AB"));
}

// Typing a callsign already in the log prefills the locator (the station
// has not moved) but never the received serial: on the other band it
// sends a fresh number, on the same band it is a dupe -- and a prefilled
// wrong number gets logged with one Enter (2026-09-21, seen live).
void TestLogCorrections::aKnownCallsignPrefillsTheGridButNeverTheSerial()
{
    QTemporaryDir dir;
    auto controller = makeController(dir, QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    logQso(log, QStringLiteral("DL1ABC"), QStringLiteral("12"), QStringLiteral("JN58SD"));
    QCOMPARE(controller->database().qsosForContest(controller->settings().activeContestId).size(), 1);

    log->setCallsign(QStringLiteral("DL1ABC"));
    QTRY_COMPARE_WITH_TIMEOUT(log->exchangeReceived().value(QStringLiteral("grid")), QStringLiteral("JN58SD"), 2000);
    QVERIFY(log->exchangeReceived().value(QStringLiteral("serial")).isEmpty());
}

// Enter with the received number/locator still missing: the first Enter
// jumps to the missing field and does not log (the operator's own log
// had such rows, 2026-09-21); a second Enter with nothing typed in
// between logs anyway -- a station that faded before the locator came
// must not block the serial sequence.
void TestLogCorrections::enterOnAnIncompleteExchangeAsksOnceThenLogs()
{
    QTemporaryDir dir;
    auto controller = makeController(dir, QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    const QString contestId = controller->settings().activeContestId;

    log->setCallsign(QStringLiteral("OE1XYZ"));
    emit log->logRequested();
    QCOMPARE(controller->database().qsosForContest(contestId).size(), 0);
    QVERIFY2(window.statusBar()->currentMessage().contains(QStringLiteral("unvollständig")),
             qPrintable(window.statusBar()->currentMessage()));

    emit log->logRequested();
    QCOMPARE(controller->database().qsosForContest(contestId).size(), 1);

    // Typing in between re-arms the check.
    log->setCallsign(QStringLiteral("OE1ABC"));
    emit log->logRequested();
    QCOMPARE(controller->database().qsosForContest(contestId).size(), 1);
    log->setExchangeFieldValue(QStringLiteral("serial"), QStringLiteral("4"));
    log->setExchangeFieldValue(QStringLiteral("grid"), QStringLiteral("JN77QT"));
    emit log->logRequested();
    QCOMPARE(controller->database().qsosForContest(contestId).size(), 2);
}

// Two stations in the same locator square: 0 km centre to centre and no
// bearing at all (the maths returned "180" for the operator's own-square
// test QSO, 2026-09-21) -- unknown is a dash, not a fabricated heading.
void TestLogCorrections::aQsoInTheOwnSquareHasNoBearing()
{
    QTemporaryDir dir;
    auto controller = makeController(dir, QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    logQso(log, QStringLiteral("OE5ASD"), QStringLiteral("2"), controller->settings().ownGrid);
    const QVector<QsoRecord> qsos = controller->database().qsosForContest(controller->settings().activeContestId);
    QCOMPARE(qsos.size(), 1);
    QVERIFY(qsos.at(0).distanceKm.has_value());
    QVERIFY(*qsos.at(0).distanceKm < 0.5);
    QVERIFY(!qsos.at(0).bearingDeg.has_value());
}

// An invalidated QSO is out of the log for every count -- status bar,
// Rate panel -- not just out of the score (2026-09-21: "3 QSOs" with one
// struck through).
void TestLogCorrections::invalidQsosLeaveTheCounts()
{
    QTemporaryDir dir;
    auto controller = makeController(dir, QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    const QString contestId = controller->settings().activeContestId;

    logQso(log, QStringLiteral("DL1ABC"), QStringLiteral("1"), QStringLiteral("JN58SD"));
    logQso(log, QStringLiteral("DL1ABD"), QStringLiteral("2"), QStringLiteral("JN58SD"));
    QCOMPARE(controller->database().qsoCountForContest(contestId), 2);
    QVERIFY2(window.statusBar()->currentMessage().contains(QStringLiteral("2 QSOs")),
             qPrintable(window.statusBar()->currentMessage()));

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(contestId);
    emit log->historyInvalidToggleRequested(qsos.at(1).id);
    QCOMPARE(controller->database().qsoCountForContest(contestId), 1);
    QCOMPARE(controller->database().qsoCountSince(contestId, QDateTime::currentDateTimeUtc().addSecs(-600)), 1);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestLogCorrections tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_log_corrections.moc"
