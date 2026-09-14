#include <QtTest>

#include <QApplication>
#include <QDateTime>
#include <QTemporaryDir>
#include <QTimeZone>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "ui/SettingsDialog.h"
#include "ui/StyleKit.h"
#include "ui/UtcClockWidget.h"

using namespace Contestprogramm;

// Covers the operator's "real UTC clock + contest countdown" request:
// UtcClockWidget::formatRemaining()'s dash-vs-real-value behavior (a
// pure, fixed-reference-QDateTime function per the task's own guidance,
// so this cannot flake against real wall-clock time), ContestSettings::
// contestEndUtc/countdownVisible persistence, and SettingsDialog's own
// "Contest-Ende (UTC)" checkbox+field round trip.
class TestUtcCountdown : public QObject
{
    Q_OBJECT

private slots:
    void formatRemainingIsDashForUnsetEnd();
    void formatRemainingComputesCorrectDuration();
    void formatRemainingClampsToZeroOncePassed();
    void formatRemainingIsExactlyZeroAtEnd();
    void settingsRoundTripUnsetContestEnd();
    void settingsRoundTripSetContestEndAndCountdownVisible();
    void settingsDialogRoundTripsUnsetContestEnd();
    void settingsDialogRoundTripsSetContestEnd();
};

void TestUtcCountdown::formatRemainingIsDashForUnsetEnd()
{
    const QDateTime now = QDateTime(QDate(2026, 9, 10), QTime(12, 0, 0), QTimeZone::utc());
    QCOMPARE(UtcClockWidget::formatRemaining(now, QDateTime()), Style::unknownDash());
}

void TestUtcCountdown::formatRemainingComputesCorrectDuration()
{
    const QDateTime now = QDateTime(QDate(2026, 9, 10), QTime(12, 0, 0), QTimeZone::utc());
    const QDateTime end = QDateTime(QDate(2026, 9, 11), QTime(14, 30, 15), QTimeZone::utc());
    // 26h30m15s remaining -- crosses a day boundary, exercising the
    // hours-can-exceed-24 path (a contest countdown is not a clock face).
    QCOMPARE(UtcClockWidget::formatRemaining(now, end), QStringLiteral("26:30:15"));
}

void TestUtcCountdown::formatRemainingClampsToZeroOncePassed()
{
    const QDateTime now = QDateTime(QDate(2026, 9, 10), QTime(12, 0, 0), QTimeZone::utc());
    const QDateTime end = QDateTime(QDate(2026, 9, 10), QTime(11, 0, 0), QTimeZone::utc()); // 1h in the past
    // A genuine zero once reached/passed -- not "unknown" (HAUSSTIL rule
    // 7) and not a negative countdown running past the contest's end.
    QCOMPARE(UtcClockWidget::formatRemaining(now, end), QStringLiteral("00:00:00"));
}

void TestUtcCountdown::formatRemainingIsExactlyZeroAtEnd()
{
    const QDateTime now = QDateTime(QDate(2026, 9, 10), QTime(12, 0, 0), QTimeZone::utc());
    QCOMPARE(UtcClockWidget::formatRemaining(now, now), QStringLiteral("00:00:00"));
}

void TestUtcCountdown::settingsRoundTripUnsetContestEnd()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("countdown_unset.sqlite")), QStringLiteral("countdown_unset")));

    ContestSettings settings;
    QVERIFY(settings.contestEndUtc.isEmpty()); // unset by default
    QVERIFY(!settings.countdownVisible);       // hidden by default ("nicht fix sichtbar")
    settings.saveTo(db);

    ContestSettings reloaded;
    reloaded.loadFrom(db);
    QVERIFY(reloaded.contestEndUtc.isEmpty());
    QVERIFY(!reloaded.countdownVisible);
}

void TestUtcCountdown::settingsRoundTripSetContestEndAndCountdownVisible()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("countdown_set.sqlite")), QStringLiteral("countdown_set")));

    const QDateTime end = QDateTime(QDate(2026, 9, 13), QTime(12, 0, 0), QTimeZone::utc());
    ContestSettings settings;
    settings.contestEndUtc = end.toString(Qt::ISODate);
    settings.countdownVisible = true;
    settings.saveTo(db);

    ContestSettings reloaded;
    reloaded.loadFrom(db);
    QCOMPARE(QDateTime::fromString(reloaded.contestEndUtc, Qt::ISODate), end);
    QVERIFY(reloaded.countdownVisible);
}

void TestUtcCountdown::settingsDialogRoundTripsUnsetContestEnd()
{
    ContestSettings initial;
    initial.ownCallsign = QStringLiteral("OE5SOS");
    QVERIFY(initial.contestEndUtc.isEmpty());

    SettingsDialog dialog(initial, {});
    QCOMPARE(dialog.settings().contestEndUtc, QString());
}

void TestUtcCountdown::settingsDialogRoundTripsSetContestEnd()
{
    const QDateTime end = QDateTime(QDate(2026, 9, 13), QTime(18, 45, 0), QTimeZone::utc());
    ContestSettings initial;
    initial.ownCallsign = QStringLiteral("OE5SOS");
    initial.contestEndUtc = end.toString(Qt::ISODate);

    SettingsDialog dialog(initial, {});
    const QDateTime roundTripped = QDateTime::fromString(dialog.settings().contestEndUtc, Qt::ISODate);
    QVERIFY(roundTripped.isValid());
    QCOMPARE(roundTripped, end);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestUtcCountdown tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_utc_countdown.moc"
