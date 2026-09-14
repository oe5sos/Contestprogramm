// Tests for ChatFeedModel::refreshWorkedAndScores() -- found live,
// 2026-09-12: a candidate's `worked` flag (and therefore `score`) used
// to be computed once, at the moment it first arrived via
// addCandidate(), and never touched again -- so logging a QSO for a
// station already sitting in the feed left it showing as "not worked"
// (and, if it was a needed multiplier, kept scoring as needed) for the
// rest of the contest. This file locks in the fix: an explicit refresh
// must pick up a QSO logged after the candidate arrived.

#include <QtTest>

#include <QCoreApplication>
#include <QDateTime>
#include <QTemporaryDir>

#include "core/GeoFilter.h"
#include "core/SpotCandidate.h"
#include "data/ContestDatabase.h"
#include "data/DupeChecker.h"
#include "data/MultiplierTracker.h"
#include "data/QsoRecord.h"
#include "models/ChatFeedModel.h"

using namespace Contestprogramm;

class TestChatFeedModelRefresh : public QObject
{
    Q_OBJECT

private slots:
    void refreshPicksUpAFreshlyLoggedDupe();
};

void TestChatFeedModelRefresh::refreshPicksUpAFreshlyLoggedDupe()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("refresh.sqlite")), QStringLiteral("chatfeed_refresh")));

    GeoFilter geoFilter;
    geoFilter.setOwnGrid(QStringLiteral("JN67VV"));
    geoFilter.setRadiusKm(1000.0);
    DupeChecker dupeChecker(db);

    ChatFeedModel model(geoFilter, dupeChecker, nullptr);
    model.setActiveContest(QStringLiteral("OE_VHF_UHF"));

    SpotCandidate candidate;
    candidate.callsign = QStringLiteral("OE1ABC");
    candidate.grid = QStringLiteral("JN77XX");
    candidate.timestampUtc = QDateTime::currentDateTimeUtc();
    candidate.source = QStringLiteral("on4kst");
    model.addCandidate(candidate);

    QCOMPARE(model.rowCount(), 1);
    // Not worked yet -- nothing logged for this callsign.
    QVERIFY(!model.data(model.index(0, ChatFeedModel::ColumnCallsign), ChatFeedModel::DupeRole).toBool());

    // The operator logs a QSO with this exact station -- via
    // ContestDatabase directly, the same effect MainWindow::
    // handleLogRequested()'s insertQso() call has; ChatFeedModel has no
    // way to know this happened on its own.
    QsoRecord logged;
    logged.callsign = candidate.callsign;
    logged.band = QStringLiteral("144");
    logged.mode = QStringLiteral("SSB");
    logged.timestampUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    logged.contestId = QStringLiteral("OE_VHF_UHF");
    QVERIFY(db.insertQso(logged));

    // Still stale before the refresh -- proves the test is actually
    // exercising the fix, not a coincidence of some other path already
    // updating it.
    QVERIFY(!model.data(model.index(0, ChatFeedModel::ColumnCallsign), ChatFeedModel::DupeRole).toBool());

    model.refreshWorkedAndScores();

    // The now-worked candidate drops out of the default filtered view
    // entirely (hard exclusion) -- switch to raw mode to inspect its
    // updated worked flag directly.
    model.setShowRawFeed(true);
    QCOMPARE(model.rowCount(), 1);
    QVERIFY(model.data(model.index(0, ChatFeedModel::ColumnCallsign), ChatFeedModel::DupeRole).toBool());
}

// Not QTEST_APPLESS_MAIN: QSqlDatabase requires a live QCoreApplication
// instance (thread-affinity bookkeeping in the SQLite driver).
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestChatFeedModelRefresh tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_chatfeedmodel_refresh.moc"
