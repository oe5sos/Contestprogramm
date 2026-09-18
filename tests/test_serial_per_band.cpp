#include <QtTest>

#include <QApplication>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/RigctldClient.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {

std::unique_ptr<AppController> makeReadyController(QTemporaryDir& dir)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(QStringLiteral("serial.sqlite")))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    controller->setSettings(settings);
    return controller;
}

void logCall(UnifiedLogWidget* log, const QString& call, const QString& grid)
{
    log->setCallsign(call);
    log->setExchangeFieldValue(QStringLiteral("serial"), QStringLiteral("5"));
    log->setExchangeFieldValue(QStringLiteral("grid"), grid);
    emit log->logRequested();
}

} // namespace

// The shipped IARU definition says serial_scope "band": through the
// real MainWindow, the sent serial restarts at 001 when the rig moves
// to the other band, and continues where it was on return.
class TestSerialPerBand : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void serialsRestartPerBand();
};

void TestSerialPerBand::serialsRestartPerBand()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir);
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    emit controller->rigctldClient().frequencyChanged(144300000);
    logCall(log, QStringLiteral("DL1ABC"), QStringLiteral("JN58SD"));
    logCall(log, QStringLiteral("OE3XYZ"), QStringLiteral("JN88TC"));
    emit controller->rigctldClient().frequencyChanged(432200000);
    logCall(log, QStringLiteral("HB9ZZZ"), QStringLiteral("JN47PM"));
    emit controller->rigctldClient().frequencyChanged(144300000);
    logCall(log, QStringLiteral("OK1KIM"), QStringLiteral("JO60RN"));

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(qsos.size(), 4);
    QCOMPARE(qsos.at(0).band, QStringLiteral("144"));
    QCOMPARE(qsos.at(0).serialSent.value_or(-1), 1);
    QCOMPARE(qsos.at(1).serialSent.value_or(-1), 2);
    QCOMPARE(qsos.at(2).band, QStringLiteral("432"));
    QCOMPARE(qsos.at(2).serialSent.value_or(-1), 1); // 001 again on the new band
    QCOMPARE(qsos.at(3).band, QStringLiteral("144"));
    QCOMPARE(qsos.at(3).serialSent.value_or(-1), 3); // back on 144: continues at 003
    QVERIFY(qsos.at(2).exchangeSent.contains(QStringLiteral(" 001 ")));
    QVERIFY(qsos.at(3).exchangeSent.contains(QStringLiteral(" 003 ")));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestSerialPerBand tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_serial_per_band.moc"
