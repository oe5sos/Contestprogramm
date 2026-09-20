#include <QtTest>

#include <QApplication>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
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

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestLogCorrections tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_log_corrections.moc"
