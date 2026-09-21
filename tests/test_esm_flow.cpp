#include <QtTest>

#include <QApplication>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/RigctldClient.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {

std::unique_ptr<AppController> makeReadyController(QTemporaryDir& dir)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(QStringLiteral("esm.sqlite")))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.esmEnabled = true;
    settings.operatingMode = ContestSettings::OperatingMode::Run;
    controller->setSettings(settings);
    return controller;
}

} // namespace

// Enter Sends Message through the real MainWindow: with ESM on and the
// rig in CW, Enter keys and only logs once the exchange is complete;
// in SSB Enter keeps its plain log meaning.
class TestEsmFlow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void enterWalksTheRunSequenceAndLogsOnlyWhenComplete();
    void ssbAndEsmOffKeepPlainLogging();
};

void TestEsmFlow::enterWalksTheRunSequenceAndLogsOnlyWhenComplete()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir);
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    const QString contestId = controller->settings().activeContestId;
    QVERIFY(!contestId.isEmpty());

    // The radio reports CW -- the one place m_currentMode comes from.
    emit controller->rigctldClient().modeChanged(QStringLiteral("CW"), 500);

    // Empty call: CQ, nothing logged.
    emit log->logRequested();
    QCOMPARE(controller->database().qsoCountForContest(contestId), 0);
    QVERIFY2(window.statusBar()->currentMessage().startsWith(QStringLiteral("ESM: CQ TEST OE5SOS OE5SOS TEST")),
             qPrintable(window.statusBar()->currentMessage()));

    // Call typed, exchange still empty: his call + our exchange, no log,
    // cursor moves on to the received exchange.
    log->setCallsign(QStringLiteral("DL1ABC"));
    emit log->logRequested();
    QCOMPARE(controller->database().qsoCountForContest(contestId), 0);
    QVERIFY2(window.statusBar()->currentMessage().startsWith(QStringLiteral("ESM: DL1ABC 599 001 JN67UT")),
             qPrintable(window.statusBar()->currentMessage()));

    // Exchange complete: TU is keyed and the QSO is in the log (the
    // log path's own status-bar refresh replaces the "ESM: TU" note
    // right away, so only the log is checked here).
    log->setExchangeFieldValue(QStringLiteral("serial"), QStringLiteral("7"));
    log->setExchangeFieldValue(QStringLiteral("grid"), QStringLiteral("JN58SD"));
    emit log->logRequested();
    QCOMPARE(controller->database().qsoCountForContest(contestId), 1);
    const QVector<QsoRecord> logged = controller->database().qsosForContest(contestId);
    QCOMPARE(logged.first().callsign, QStringLiteral("DL1ABC"));
    QCOMPARE(logged.first().gridSquare, QStringLiteral("JN58SD"));
    QCOMPARE(logged.first().mode, QStringLiteral("CW"));
}

void TestEsmFlow::ssbAndEsmOffKeepPlainLogging()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir);
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    const QString contestId = controller->settings().activeContestId;

    // SSB (the default mode, no rig report): Enter with a call and a
    // complete exchange logs at once -- ESM does not apply without a
    // keyer. (An incomplete exchange asks once first, see
    // TestLogCorrections::enterOnAnIncompleteExchangeAsksOnceThenLogs.)
    log->setCallsign(QStringLiteral("DL1ABC"));
    log->setExchangeFieldValue(QStringLiteral("serial"), QStringLiteral("1"));
    log->setExchangeFieldValue(QStringLiteral("grid"), QStringLiteral("JN58SD"));
    emit log->logRequested();
    QCOMPARE(controller->database().qsoCountForContest(contestId), 1);

    // CW but ESM switched off: the same plain behaviour.
    ContestSettings settings = controller->settings();
    settings.esmEnabled = false;
    controller->setSettings(settings);
    emit controller->rigctldClient().modeChanged(QStringLiteral("CW"), 500);
    log->setCallsign(QStringLiteral("OE3XYZ"));
    log->setExchangeFieldValue(QStringLiteral("serial"), QStringLiteral("2"));
    log->setExchangeFieldValue(QStringLiteral("grid"), QStringLiteral("JN77QT"));
    emit log->logRequested();
    QCOMPARE(controller->database().qsoCountForContest(contestId), 2);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestEsmFlow tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_esm_flow.moc"
