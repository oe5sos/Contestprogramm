#include <QtTest>

#include <QApplication>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimeZone>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/On4kstClient.h"
#include "core/RigctldClient.h"
#include "core/SpotCandidate.h"
#include "data/ContestDatabase.h"
#include "ui/MainWindow.h"
#include "ui/SkedPanel.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {

std::unique_ptr<AppController> makeReadyController(QTemporaryDir& dir)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(QStringLiteral("sked.sqlite")))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    controller->setSettings(settings);
    return controller;
}

QString cell(QTableWidget* table, int row, int col)
{
    return table->item(row, col) ? table->item(row, col)->text() : QString();
}

} // namespace

// The Skeds panel through the real MainWindow: entry, chat suggestion,
// acceptance, activation (entry row filled), and the log closing it.
class TestSkedPanel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void entrySuggestionActivationAndAutoDone();
    void databaseKeepsSkedsAcrossReload();
};

void TestSkedPanel::entrySuggestionActivationAndAutoDone()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeReadyController(dir);
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* panel = window.findChild<SkedPanel*>();
    QVERIFY(panel);
    auto* table = panel->findChild<QTableWidget*>();
    QVERIFY(table);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    // Manual entry: call, locator, QRG, "+5".
    emit panel->addRequested(QStringLiteral("dl0gth"), QStringLiteral("jo50jp"), QStringLiteral("144.317"), QStringLiteral("+5"));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(cell(table, 0, 1), QStringLiteral("DL0GTH"));
    QCOMPARE(cell(table, 0, 2), QStringLiteral("JO50JP"));
    QCOMPARE(cell(table, 0, 3), QStringLiteral("144.317"));
    QCOMPARE(cell(table, 0, 6), QStringLiteral("in 5 min"));
    QVERIFY(!cell(table, 0, 5).isEmpty() && cell(table, 0, 5) != QStringLiteral("—")); // km from the two locators

    // A bad time is refused, nothing added.
    emit panel->addRequested(QStringLiteral("OK1KIM"), QString(), QStringLiteral("144.322"), QStringLiteral("soon"));
    QCOMPARE(table->rowCount(), 1);
    QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("Sked-Zeit nicht verstanden")));

    // A chat line for us with a frequency: a suggestion row.
    SpotCandidate chat;
    chat.callsign = QStringLiteral("S59DEM");
    chat.rawLine = QStringLiteral("CH|1|20261003|S59DEM|Miha|OE5SOS|432.220 at %1?|0|")
                       .arg(QDateTime::currentDateTimeUtc().addSecs(20 * 60).time().toString(QStringLiteral("HH:mm")));
    chat.timestampUtc = QDateTime::currentDateTimeUtc();
    chat.source = QStringLiteral("on4kst");
    emit controller->on4kstClient().chatLineReceived(chat);
    QCOMPARE(table->rowCount(), 2);
    int suggestionRow = cell(table, 0, 1) == QStringLiteral("S59DEM") ? 0 : 1;
    QCOMPARE(cell(table, suggestionRow, 6), QStringLiteral("aus KST · übernehmen?"));
    // The same proposal again is not duplicated.
    emit controller->on4kstClient().chatLineReceived(chat);
    QCOMPARE(table->rowCount(), 2);

    // Accepting it makes it an open sked.
    const QVector<Sked> stored = controller->database().skedsForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(stored.size(), 2);
    int suggestionId = -1;
    for (const Sked& s : stored) {
        if (s.state == Sked::State::Suggested) {
            suggestionId = s.id;
        }
    }
    QVERIFY(suggestionId > 0);
    emit panel->suggestionAccepted(suggestionId);
    suggestionRow = cell(table, 0, 1) == QStringLiteral("S59DEM") ? 0 : 1;
    QVERIFY2(cell(table, suggestionRow, 6).startsWith(QStringLiteral("in ")), qPrintable(cell(table, suggestionRow, 6)));

    // Activating the DL0GTH sked fills the entry row with call + grid.
    int dlId = -1;
    for (const Sked& s : stored) {
        if (s.callsign == QStringLiteral("DL0GTH")) {
            dlId = s.id;
        }
    }
    QVERIFY(dlId > 0);
    emit panel->skedActivated(dlId);
    QCOMPARE(log->callsign(), QStringLiteral("DL0GTH"));
    QCOMPARE(log->exchangeReceived().value(QStringLiteral("grid")), QStringLiteral("JO50JP"));

    // Logging the QSO on that band closes the sked by itself.
    emit controller->rigctldClient().frequencyChanged(144317000);
    log->setExchangeFieldValue(QStringLiteral("serial"), QStringLiteral("12"));
    emit log->logRequested();
    QCOMPARE(controller->database().qsoCountForContest(QStringLiteral("IARU_R1_VHF_UHF")), 1);
    const int dlRow = cell(table, 0, 1) == QStringLiteral("DL0GTH") ? 0 : 1;
    QCOMPARE(cell(table, dlRow, 6), QStringLiteral("erledigt"));

    // Delete removes it from panel and database.
    emit panel->deleteRequested(dlId);
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(controller->database().skedsForContest(QStringLiteral("IARU_R1_VHF_UHF")).size(), 1);
}

void TestSkedPanel::databaseKeepsSkedsAcrossReload()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("skeds.sqlite")), QStringLiteral("skeds_db")));
    Sked a;
    a.callsign = QStringLiteral("dl0gth");
    a.grid = QStringLiteral("jo50jp");
    a.band = QStringLiteral("144");
    a.freqHz = 144317000;
    a.timeUtc = QDateTime(QDate(2026, 10, 3), QTime(14, 35), QTimeZone::UTC);
    a.source = QStringLiteral("manual");
    QVERIFY(db.insertSked(QStringLiteral("C"), a));
    QVERIFY(a.id > 0);
    Sked b = a;
    b.callsign = QStringLiteral("OK1KIM");
    b.timeUtc = a.timeUtc.addSecs(-600);
    b.state = Sked::State::Suggested;
    b.note = QStringLiteral("144.322 at 14:25?");
    QVERIFY(db.insertSked(QStringLiteral("C"), b));

    QVector<Sked> loaded = db.skedsForContest(QStringLiteral("C"));
    QCOMPARE(loaded.size(), 2);
    QCOMPARE(loaded.at(0).callsign, QStringLiteral("OK1KIM")); // earlier time first
    QCOMPARE(loaded.at(0).state, Sked::State::Suggested);
    QCOMPARE(loaded.at(0).note, b.note);
    QCOMPARE(loaded.at(1).callsign, QStringLiteral("DL0GTH"));
    QCOMPARE(loaded.at(1).grid, QStringLiteral("JO50JP"));
    QCOMPARE(loaded.at(1).timeUtc, a.timeUtc);
    QCOMPARE(loaded.at(1).freqHz, qint64(144317000));

    QVERIFY(db.updateSkedState(a.id, Sked::State::Done));
    loaded = db.skedsForContest(QStringLiteral("C"));
    QCOMPARE(loaded.at(1).state, Sked::State::Done);
    QVERIFY(db.deleteSked(b.id));
    QCOMPARE(db.skedsForContest(QStringLiteral("C")).size(), 1);
    QVERIFY(db.skedsForContest(QStringLiteral("other")).isEmpty());
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestSkedPanel tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_sked_panel.moc"
