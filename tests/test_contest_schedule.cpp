#include <QtTest>

#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>

#include "data/ContestDefinition.h"
#include "data/ContestSchedule.h"
#include "ui/StyleKit.h"
#include "ui/UtcClockWidget.h"

using namespace Contestprogramm;

namespace {

QDateTime utc(int y, int m, int d, int hh, int mm)
{
    return QDateTime(QDate(y, m, d), QTime(hh, mm), QTimeZone::UTC);
}

ContestSchedule october()
{
    ContestSchedule s;
    s.month = 10;
    return s;
}

} // namespace

// The IARU Region 1 "first full weekend of <month>, Saturday 14:00 to
// Sunday 14:00 UTC" rule as data/ContestSchedule.h turns it into dates,
// plus the readout UtcClockWidget makes of a window with a start.
class TestContestSchedule : public QObject
{
    Q_OBJECT

private slots:
    void firstFullWeekendIsTheFirstSaturdayOfTheMonth();
    void laterWeekendsSundayStartAndShortContests();
    void nearestWindowLooksAcrossTheYearBoundary();
    void upcomingWindowSkipsAPastContest();
    void manualEndWinsOverTheSchedule();
    void windowContainsBothEndsAndDescribesItself();
    void jsonParsesDefaultsAndRejectsNonsense();
    void definitionRoundTripsScheduleAndModes();
    void countdownReadoutBeforeDuringAndAfter();
    void dasLetzteVolleWochenendeGegenVeroeffentlichteDaten();
};

void TestContestSchedule::firstFullWeekendIsTheFirstSaturdayOfTheMonth()
{
    // 2026: VHF 5/6 September, UHF 3/4 October, Marconi 7/8 November --
    // November 2026 starts on a Sunday, so 1 November is not a "full"
    // weekend and the contest is a week later.
    ContestSchedule s;
    s.month = 9;
    QCOMPARE(s.windowIn(2026).startUtc, utc(2026, 9, 5, 14, 0));
    QCOMPARE(s.windowIn(2026).endUtc, utc(2026, 9, 6, 14, 0));
    s.month = 10;
    QCOMPARE(s.windowIn(2026).startUtc, utc(2026, 10, 3, 14, 0));
    s.month = 11;
    QCOMPARE(s.windowIn(2026).startUtc, utc(2026, 11, 7, 14, 0));
    QCOMPARE(s.windowIn(2026).endUtc, utc(2026, 11, 8, 14, 0));
    // 2027 for good measure, and a month that itself starts on a Saturday.
    s.month = 9;
    QCOMPARE(s.windowIn(2027).startUtc, utc(2027, 9, 4, 14, 0));
    s.month = 3;
    QCOMPARE(s.windowIn(2025).startUtc, utc(2025, 3, 1, 14, 0));
    QCOMPARE(s.windowIn(2025).startUtc.timeSpec(), Qt::UTC);
}

void TestContestSchedule::laterWeekendsSundayStartAndShortContests()
{
    ContestSchedule s;
    s.month = 6;
    s.weekend = 3;
    s.startTimeUtc = QTime(14, 0);
    // June 2026: Saturdays are the 6th, 13th, 20th.
    QCOMPARE(s.windowIn(2026).startUtc, utc(2026, 6, 20, 14, 0));
    s.weekend = 1;
    s.startDay = Qt::Sunday;
    s.startTimeUtc = QTime(7, 0);
    s.hours = 6;
    QCOMPARE(s.windowIn(2026).startUtc, utc(2026, 6, 7, 7, 0));
    QCOMPARE(s.windowIn(2026).endUtc, utc(2026, 6, 7, 13, 0));
    // No schedule: no window, and no crash.
    QVERIFY(!ContestSchedule().isValid());
    QVERIFY(!ContestSchedule().windowIn(2026).isValid());
}

void TestContestSchedule::nearestWindowLooksAcrossTheYearBoundary()
{
    ContestSchedule marconi;
    marconi.month = 11;
    // A check run on 10 January 2027 of the November 2026 log measures
    // against November 2026, not November 2027.
    QCOMPARE(marconi.windowNearest(utc(2027, 1, 10, 12, 0)).startUtc, utc(2026, 11, 7, 14, 0));
    // Inside the contest: that contest.
    QCOMPARE(marconi.windowNearest(utc(2026, 11, 8, 2, 0)).startUtc, utc(2026, 11, 7, 14, 0));
    // Well before: this year's.
    QCOMPARE(october().windowNearest(utc(2026, 9, 20, 12, 0)).startUtc, utc(2026, 10, 3, 14, 0));
}

void TestContestSchedule::upcomingWindowSkipsAPastContest()
{
    ContestSchedule september;
    september.month = 9;
    // 20 September 2026: this year's VHF contest is over -> 2027's.
    QCOMPARE(september.windowUpcoming(utc(2026, 9, 20, 12, 0)).startUtc, utc(2027, 9, 4, 14, 0));
    // The UHF contest is still ahead.
    QCOMPARE(october().windowUpcoming(utc(2026, 9, 20, 12, 0)).startUtc, utc(2026, 10, 3, 14, 0));
    // During the contest it is the running one, up to and including its last second.
    QCOMPARE(october().windowUpcoming(utc(2026, 10, 4, 13, 59)).startUtc, utc(2026, 10, 3, 14, 0));
    QCOMPARE(october().windowUpcoming(utc(2026, 10, 4, 14, 1)).startUtc, utc(2027, 10, 2, 14, 0));
}

void TestContestSchedule::manualEndWinsOverTheSchedule()
{
    const ContestWindow manual = effectiveContestWindow(october(), QStringLiteral("2026-10-11T12:00:00Z"),
                                                        utc(2026, 9, 20, 12, 0));
    QCOMPARE(manual.endUtc, utc(2026, 10, 11, 12, 0));
    QCOMPARE(manual.startUtc, utc(2026, 10, 10, 12, 0));
    // A manual end without any schedule assumes the usual 24 hours; a
    // zone-less string is read as UTC.
    const ContestWindow bare = effectiveContestWindow(ContestSchedule(), QStringLiteral("2026-10-11T12:00:00"),
                                                      utc(2026, 9, 20, 12, 0));
    QCOMPARE(bare.startUtc, utc(2026, 10, 10, 12, 0));
    QCOMPARE(bare.endUtc, utc(2026, 10, 11, 12, 0));
    // Nothing set: the schedule's nearest occurrence.
    QCOMPARE(effectiveContestWindow(october(), QString(), utc(2026, 9, 20, 12, 0)).startUtc, utc(2026, 10, 3, 14, 0));
    // Neither: unknown.
    QVERIFY(!effectiveContestWindow(ContestSchedule(), QString(), utc(2026, 9, 20, 12, 0)).isValid());
}

void TestContestSchedule::windowContainsBothEndsAndDescribesItself()
{
    const ContestWindow w = october().windowIn(2026);
    QVERIFY(w.contains(utc(2026, 10, 3, 14, 0)));
    QVERIFY(w.contains(utc(2026, 10, 4, 14, 0)));
    QVERIFY(!w.contains(utc(2026, 10, 3, 13, 59)));
    QVERIFY(!w.contains(utc(2026, 10, 4, 14, 1)));
    QCOMPARE(w.distanceSecs(utc(2026, 10, 3, 13, 0)), qint64(3600));
    QCOMPARE(w.distanceSecs(utc(2026, 10, 4, 15, 0)), qint64(3600));
    QCOMPARE(w.distanceSecs(utc(2026, 10, 4, 1, 0)), qint64(0));
    QCOMPARE(w.describe(), QStringLiteral("Sa 03.10. 14:00 – So 04.10. 14:00 UTC"));
    QVERIFY(ContestWindow().describe().isEmpty());
}

void TestContestSchedule::jsonParsesDefaultsAndRejectsNonsense()
{
    QString error;
    ContestSchedule s = ContestSchedule::fromJson(QJsonDocument::fromJson("{\"month\": 10}").object(), &error);
    QVERIFY2(s.isValid(), qPrintable(error));
    QCOMPARE(s.weekend, 1);
    QCOMPARE(s.startDay, int(Qt::Saturday));
    QCOMPARE(s.startTimeUtc, QTime(14, 0));
    QCOMPARE(s.hours, 24);

    s = ContestSchedule::fromJson(
        QJsonDocument::fromJson("{\"month\": 6, \"weekend\": 3, \"day\": \"sun\", \"start\": \"07:00\", \"hours\": 6}").object(),
        &error);
    QVERIFY2(s.isValid(), qPrintable(error));
    QCOMPARE(s.weekend, 3);
    QCOMPARE(s.startDay, int(Qt::Sunday));
    QCOMPARE(s.startTimeUtc, QTime(7, 0));
    QCOMPARE(s.hours, 6);
    // toJson() is what fromJson() reads.
    const ContestSchedule again = ContestSchedule::fromJson(s.toJson(), &error);
    QCOMPARE(again.windowIn(2026).startUtc, s.windowIn(2026).startUtc);
    QCOMPARE(again.windowIn(2026).endUtc, s.windowIn(2026).endUtc);

    QVERIFY(!ContestSchedule::fromJson(QJsonDocument::fromJson("{}").object(), &error).isValid());
    QVERIFY(error.contains(QStringLiteral("month")));
    QVERIFY(!ContestSchedule::fromJson(QJsonDocument::fromJson("{\"month\": 13}").object(), &error).isValid());
    QVERIFY(!ContestSchedule::fromJson(QJsonDocument::fromJson("{\"month\": 10, \"day\": \"mon\"}").object(), &error).isValid());
    QVERIFY(!ContestSchedule::fromJson(QJsonDocument::fromJson("{\"month\": 10, \"start\": \"25:00\"}").object(), &error).isValid());
    QVERIFY(!ContestSchedule::fromJson(QJsonDocument::fromJson("{\"month\": 10, \"hours\": 0}").object(), &error).isValid());
}

void TestContestSchedule::definitionRoundTripsScheduleAndModes()
{
    const QByteArray json = R"JSON({
      "id": "IARU_R1_MARCONI", "name": "Marconi", "bands": ["144"],
      "dupe_scope": ["callsign", "band"], "modes": ["cw"],
      "schedule": { "month": 11 },
      "exchange_fields": [ { "key": "grid", "label": "Grid", "type": "grid6" } ]
    })JSON";
    QString error;
    const ContestDefinition def = ContestDefinition::loadFromJson(json, &error);
    QVERIFY2(def.isValid(), qPrintable(error));
    QVERIFY(def.schedule().isValid());
    QCOMPARE(def.schedule().windowIn(2026).startUtc, utc(2026, 11, 7, 14, 0));
    QCOMPARE(def.modes(), QStringList{QStringLiteral("CW")}); // upper-cased

    // An override saved by the rules editor must keep both keys, or the
    // countdown and the CW-only check would silently vanish after an
    // exchange-field edit.
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("marconi.json"));
    QVERIFY(def.withExchangeFields(def.exchangeFields()).saveToFile(path, &error));
    const ContestDefinition back = ContestDefinition::loadFromFile(path, &error);
    QVERIFY2(back.isValid(), qPrintable(error));
    QCOMPARE(back.schedule().windowIn(2026).startUtc, utc(2026, 11, 7, 14, 0));
    QCOMPARE(back.modes(), QStringList{QStringLiteral("CW")});

    // Both keys absent: no schedule, any mode -- the shipped VHF/UHF
    // file stays as it is.
    const ContestDefinition plain = ContestDefinition::loadFromJson(R"JSON({
      "id": "X", "name": "X", "bands": ["144"], "dupe_scope": ["callsign"], "exchange_fields": []
    })JSON", &error);
    QVERIFY(plain.isValid());
    QVERIFY(!plain.schedule().isValid());
    QVERIFY(plain.modes().isEmpty());
    // A broken schedule is a broken definition, not a silently ignored one.
    QVERIFY(!ContestDefinition::loadFromJson(R"JSON({
      "id": "X", "name": "X", "bands": ["144"], "dupe_scope": ["callsign"], "exchange_fields": [],
      "schedule": { "month": 14 }
    })JSON", &error).isValid());
    QVERIFY(error.contains(QStringLiteral("month")));
}

void TestContestSchedule::countdownReadoutBeforeDuringAndAfter()
{
    const ContestWindow w = october().windowIn(2026);
    // More than a week out: the date, not a five-digit hour count.
    QCOMPARE(UtcClockWidget::formatCountdown(utc(2026, 9, 20, 12, 0), w.startUtc, w.endUtc),
             QStringLiteral("Start Sa 03.10. 14:00 UTC"));
    // Inside the week: a countdown to the start.
    QCOMPARE(UtcClockWidget::formatCountdown(utc(2026, 10, 1, 14, 0), w.startUtc, w.endUtc),
             QStringLiteral("Start in 48:00:00"));
    // Running: the familiar "Noch".
    QCOMPARE(UtcClockWidget::formatCountdown(utc(2026, 10, 3, 14, 0), w.startUtc, w.endUtc),
             QStringLiteral("Noch 24:00:00"));
    QCOMPARE(UtcClockWidget::formatCountdown(utc(2026, 10, 4, 13, 59), w.startUtc, w.endUtc),
             QStringLiteral("Noch 00:01:00"));
    // Over.
    QCOMPARE(UtcClockWidget::formatCountdown(utc(2026, 10, 4, 14, 1), w.startUtc, w.endUtc), QStringLiteral("Beendet"));
    // End only (the manual setting of old): exactly the old readout,
    // clamped at zero rather than "Beendet".
    QCOMPARE(UtcClockWidget::formatCountdown(utc(2026, 10, 4, 14, 1), QDateTime(), w.endUtc),
             QStringLiteral("Noch 00:00:00"));
    // Nothing known.
    QCOMPARE(UtcClockWidget::formatCountdown(utc(2026, 10, 4, 14, 1), QDateTime(), QDateTime()),
             QStringLiteral("Noch ") + Style::unknownDash());
}

QTEST_MAIN(TestContestSchedule)
// ── Das letzte volle Wochenende, gegen veroeffentlichte Daten ────────
//
// Die grossen Kurzwellenconteste rechnen vom Monatsende her. Der
// Fallstrick ist das Wort VOLL: faellt der letzte Samstag auf den 31.,
// liegt sein Sonntag schon im naechsten Monat, und das Wochenende
// zaehlt nicht mehr. Genau so war CQ WW SSB 2026 -- der 31. Oktober
// ist ein Samstag, der Contest lief aber am 24./25.
//
// Gepruft wird deshalb nicht gegen eine eigene Rechnung, sondern
// gegen Daten, die die Veranstalter selbst veroeffentlicht haben
// (cqww.com, cqwpx.com, abgerufen 2026-10-23):
//
//   CQ WW SSB 2026   24./25. Oktober     (letzter Sa waere der 31.)
//   CQ WW SSB 2027   30./31. Oktober
//   CQ WW CW  2027   27./28. November
//   CQ WPX SSB 2025  29./30. Maerz
//   CQ WPX CW  2025  24./25. Mai
void TestContestSchedule::dasLetzteVolleWochenendeGegenVeroeffentlichteDaten()
{
    const auto beginnt = [](int month, int year, int stundeUtc, int dauer) {
        ContestSchedule s;
        s.month = month;
        s.weekend = -1;                 // das letzte volle Wochenende
        s.startDay = Qt::Saturday;
        s.startTimeUtc = QTime(stundeUtc, 0);
        s.hours = dauer;
        return s.windowIn(year).startUtc.date();
    };

    QCOMPARE(beginnt(10, 2026, 0, 48), QDate(2026, 10, 24));
    QCOMPARE(beginnt(10, 2027, 0, 48), QDate(2027, 10, 30));
    QCOMPARE(beginnt(11, 2027, 0, 48), QDate(2027, 11, 27));
    QCOMPARE(beginnt(3,  2025, 0, 48), QDate(2025, 3, 29));
    QCOMPARE(beginnt(5,  2025, 0, 48), QDate(2025, 5, 24));

    // -2 ist das vorletzte.
    ContestSchedule vorletztes;
    vorletztes.month = 10;
    vorletztes.weekend = -2;
    vorletztes.startTimeUtc = QTime(0, 0);
    vorletztes.hours = 48;
    QCOMPARE(vorletztes.windowIn(2026).startUtc.date(), QDate(2026, 10, 17));

    // Und 48 Stunden sind 48 Stunden.
    ContestSchedule cqww;
    cqww.month = 10;
    cqww.weekend = -1;
    cqww.startTimeUtc = QTime(0, 0);
    cqww.hours = 48;
    QCOMPARE(cqww.windowIn(2026).endUtc,
             QDateTime(QDate(2026, 10, 26), QTime(0, 0), QTimeZone::UTC));

    // Aus JSON, in beide Richtungen.
    QString fehler;
    const ContestSchedule ausJson = ContestSchedule::fromJson(
        QJsonObject{{QStringLiteral("month"), 10},
                    {QStringLiteral("weekend"), -1},
                    {QStringLiteral("start"), QStringLiteral("00:00")},
                    {QStringLiteral("hours"), 48}},
        &fehler);
    QVERIFY2(ausJson.isValid(), qPrintable(fehler));
    QCOMPARE(ausJson.weekend, -1);
    QCOMPARE(ausJson.windowIn(2026).startUtc.date(), QDate(2026, 10, 24));

    // Null bleibt Unsinn, und zu weit zurueck ebenfalls.
    QVERIFY(!ContestSchedule::fromJson(
                 QJsonObject{{QStringLiteral("month"), 10},
                             {QStringLiteral("weekend"), 0}}).isValid());
    QVERIFY(!ContestSchedule::fromJson(
                 QJsonObject{{QStringLiteral("month"), 10},
                             {QStringLiteral("weekend"), -9}}).isValid());
}

#include "test_contest_schedule.moc"
