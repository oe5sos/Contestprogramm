#include <QtTest>

#include <QTimeZone>

#include "app/ContestSettings.h"
#include "data/ContestDefinition.h"
#include "data/LogCheck.h"
#include "data/QsoRecord.h"

using namespace Contestprogramm;

namespace {

QDateTime utc(int y, int m, int d, int hh, int mm)
{
    return QDateTime(QDate(y, m, d), QTime(hh, mm), QTimeZone::UTC);
}

int nextId = 1;

QsoRecord qso(const QString& call, const QString& band, int serialSent, const QString& grid,
              const QString& time = QStringLiteral("2026-10-03T15:00:00Z"), const QString& mode = QStringLiteral("SSB"))
{
    QsoRecord r;
    r.id = nextId++;
    r.callsign = call;
    r.band = band;
    r.mode = mode;
    r.timestampUtc = time;
    r.gridSquare = grid;
    r.serialSent = serialSent;
    r.serialRcvd = 7;
    r.rstSent = mode == QStringLiteral("CW") ? QStringLiteral("599") : QStringLiteral("59");
    r.rstRcvd = r.rstSent;
    r.contestId = QStringLiteral("IARU_R1_UHF");
    return r;
}

LogCheckContext context()
{
    LogCheckContext c;
    c.ownCallsign = QStringLiteral("OE5SOS");
    c.ownGrid = QStringLiteral("JN67UT");
    c.bands = {QStringLiteral("144"), QStringLiteral("432")};
    c.dupeScope = {QStringLiteral("callsign"), QStringLiteral("band")};
    c.window.startUtc = utc(2026, 10, 3, 14, 0);
    c.window.endUtc = utc(2026, 10, 4, 14, 0);
    return c;
}

QStringList codes(const LogCheckResult& result, LogCheckIssue::Severity severity)
{
    QStringList out;
    for (const LogCheckIssue& issue : result.issues) {
        if (issue.severity == severity) {
            out << issue.code;
        }
    }
    return out;
}

QStringList allCodes(const LogCheckResult& result)
{
    QStringList out;
    for (const LogCheckIssue& issue : result.issues) {
        out << issue.code;
    }
    return out;
}

const LogCheckIssue* find(const LogCheckResult& result, const QString& code)
{
    for (const LogCheckIssue& issue : result.issues) {
        if (issue.code == code) {
            return &issue;
        }
    }
    return nullptr;
}

} // namespace

// data/LogCheck.h: what the IARU-R1/ÖVSV robot would deduct for,
// found before the EDI file is sent. Errors block "abgabebereit",
// warnings and hints do not.
class TestLogCheck : public QObject
{
    Q_OBJECT

private slots:
    void cleanLogHasNoIssuesAndIsSubmittable();
    void missingOrShortLocatorIsAnError();
    void missingReceivedSerialIsAnError();
    void timeOutsideTheContestIsAnError();
    void unknownWindowSkipsTheTimeTestWithAHint();
    void sentSerialsMustBeUniqueAndAscending();
    void serialGapsAreHintsUnlessAnInvalidQsoExplainsThem();
    void serialScopeContestCountsAcrossBands();
    void forbiddenModeAndOwnCallsignAreErrors();
    void oddCallsignAndRstShapeAreWarnings();
    void sameCallWithTwoLocatorsIsAWarning();
    void implausibleDistanceAndBandMismatchesAreWarnings();
    void unmarkedDupeIsAWarningAndMarkedDupesAreExemptFromExchangeTests();
    void invalidQsosAreCountedNotChecked();
    void issuesAreOrderedBySeverityThenTime();
    void contextComesFromDefinitionAndSettings();
};

void TestLogCheck::cleanLogHasNoIssuesAndIsSubmittable()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"))
        << qso(QStringLiteral("OE3XYZ"), QStringLiteral("144"), 2, QStringLiteral("JN88TC"), QStringLiteral("2026-10-03T15:10:00Z"))
        << qso(QStringLiteral("OE3XYZ"), QStringLiteral("432"), 1, QStringLiteral("JN88TC"), QStringLiteral("2026-10-03T15:12:00Z"));
    const LogCheckResult result = checkLog(log, context());
    QVERIFY2(result.issues.isEmpty(), qPrintable(allCodes(result).join(QLatin1Char(' '))));
    QCOMPARE(result.checkedQsos, 3);
    QVERIFY(result.submittable());
    QCOMPARE(result.countsText(), QStringLiteral("0 Fehler, 0 Warnungen, 0 Hinweise"));
}

void TestLogCheck::missingOrShortLocatorIsAnError()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QString())
        << qso(QStringLiteral("DL2ABC"), QStringLiteral("144"), 2, QStringLiteral("JN58"))
        << qso(QStringLiteral("DL3ABC"), QStringLiteral("144"), 3, QStringLiteral("XX99XX"));
    const LogCheckResult result = checkLog(log, context());
    QCOMPARE(codes(result, LogCheckIssue::Severity::Error),
             (QStringList{QStringLiteral("no_locator"), QStringLiteral("bad_locator"), QStringLiteral("bad_locator")}));
    QVERIFY(!result.submittable());
    QCOMPARE(find(result, QStringLiteral("no_locator"))->callsign, QStringLiteral("DL1ABC"));
    QCOMPARE(find(result, QStringLiteral("no_locator"))->qsoId, log.at(0).id);
    // A contest without a locator field does not ask for one.
    LogCheckContext noGrid = context();
    noGrid.hasGridField = false;
    QVERIFY(checkLog(log, noGrid).submittable());
}

void TestLogCheck::missingReceivedSerialIsAnError()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"));
    log[0].serialRcvd.reset();
    QCOMPARE(codes(checkLog(log, context()), LogCheckIssue::Severity::Error), QStringList{QStringLiteral("serial_rcvd_missing")});
    LogCheckContext noSerial = context();
    noSerial.hasSerialField = false;
    QVERIFY(checkLog(log, noSerial).submittable());
}

void TestLogCheck::timeOutsideTheContestIsAnError()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T13:59:00Z"))
        << qso(QStringLiteral("DL2ABC"), QStringLiteral("144"), 2, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T14:00:00Z"))
        << qso(QStringLiteral("DL3ABC"), QStringLiteral("144"), 3, QStringLiteral("JN58SD"), QStringLiteral("2026-10-04T14:00:00Z"))
        << qso(QStringLiteral("DL4ABC"), QStringLiteral("144"), 4, QStringLiteral("JN58SD"), QStringLiteral("2026-10-04T14:01:00Z"))
        << qso(QStringLiteral("DL5ABC"), QStringLiteral("144"), 5, QStringLiteral("JN58SD"), QStringLiteral("garbage"));
    const LogCheckResult result = checkLog(log, context());
    QCOMPARE(codes(result, LogCheckIssue::Severity::Error),
             (QStringList{QStringLiteral("outside_window"), QStringLiteral("outside_window"), QStringLiteral("bad_time")}));
    QVERIFY(find(result, QStringLiteral("outside_window"))->message.contains(QStringLiteral("Sa 03.10. 14:00 – So 04.10. 14:00 UTC")));
}

void TestLogCheck::unknownWindowSkipsTheTimeTestWithAHint()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-01-01T00:00:00Z"));
    LogCheckContext c = context();
    c.window = ContestWindow();
    const LogCheckResult result = checkLog(log, c);
    QVERIFY(result.submittable());
    QCOMPARE(codes(result, LogCheckIssue::Severity::Hint), QStringList{QStringLiteral("no_window")});
}

void TestLogCheck::sentSerialsMustBeUniqueAndAscending()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:00:00Z"))
        << qso(QStringLiteral("DL2ABC"), QStringLiteral("144"), 2, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:01:00Z"))
        << qso(QStringLiteral("DL3ABC"), QStringLiteral("144"), 2, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:02:00Z"))
        // Logged later but stamped earlier (a hand-edited time): its
        // number 3 comes after 2 in log order, fine -- but the one
        // below runs backwards.
        << qso(QStringLiteral("DL4ABC"), QStringLiteral("144"), 4, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:05:00Z"))
        << qso(QStringLiteral("DL5ABC"), QStringLiteral("144"), 3, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:06:00Z"));
    const LogCheckResult result = checkLog(log, context());
    QCOMPARE(codes(result, LogCheckIssue::Severity::Error), QStringList{QStringLiteral("serial_sent_duplicate")});
    const LogCheckIssue* dup = find(result, QStringLiteral("serial_sent_duplicate"));
    QCOMPARE(dup->callsign, QStringLiteral("DL3ABC"));
    QVERIFY(dup->message.contains(QStringLiteral("002")));
    QVERIFY(dup->message.contains(QStringLiteral("DL2ABC")));
    QCOMPARE(codes(result, LogCheckIssue::Severity::Warning), QStringList{QStringLiteral("serial_sent_order")});
    QCOMPARE(find(result, QStringLiteral("serial_sent_order"))->callsign, QStringLiteral("DL5ABC"));
    // One QSO with a mistyped time that lands before all the others is
    // reported once -- not every QSO that follows it.
    QVector<QsoRecord> early;
    early << qso(QStringLiteral("OK1XYZ"), QStringLiteral("144"), 6, QStringLiteral("JO70EC"), QStringLiteral("2026-10-03T14:05:00Z"))
          << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:00:00Z"))
          << qso(QStringLiteral("DL2ABC"), QStringLiteral("144"), 2, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:01:00Z"))
          << qso(QStringLiteral("DL3ABC"), QStringLiteral("144"), 3, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:02:00Z"))
          << qso(QStringLiteral("DL4ABC"), QStringLiteral("144"), 4, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:03:00Z"))
          << qso(QStringLiteral("DL5ABC"), QStringLiteral("144"), 5, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:04:00Z"))
          << qso(QStringLiteral("DL7ABC"), QStringLiteral("144"), 7, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:06:00Z"));
    const LogCheckResult earlyResult = checkLog(early, context());
    QCOMPARE(codes(earlyResult, LogCheckIssue::Severity::Warning), QStringList{QStringLiteral("serial_sent_order")});
    QCOMPARE(find(earlyResult, QStringLiteral("serial_sent_order"))->callsign, QStringLiteral("OK1XYZ"));
    // A mistyped number in the middle: that one, once.
    QVector<QsoRecord> typo;
    typo << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:00:00Z"))
         << qso(QStringLiteral("DL2ABC"), QStringLiteral("144"), 2, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:01:00Z"))
         << qso(QStringLiteral("DL3ABC"), QStringLiteral("144"), 30, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:02:00Z"))
         << qso(QStringLiteral("DL4ABC"), QStringLiteral("144"), 4, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:03:00Z"))
         << qso(QStringLiteral("DL5ABC"), QStringLiteral("144"), 5, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:04:00Z"));
    const LogCheckResult typoResult = checkLog(typo, context());
    QCOMPARE(codes(typoResult, LogCheckIssue::Severity::Warning), QStringList{QStringLiteral("serial_sent_order")});
    QCOMPARE(find(typoResult, QStringLiteral("serial_sent_order"))->callsign, QStringLiteral("DL3ABC"));
    // A QSO without a sent number at all.
    log[1].serialSent.reset();
    QVERIFY(codes(checkLog(log, context()), LogCheckIssue::Severity::Error).contains(QStringLiteral("serial_sent_missing")));
}

void TestLogCheck::serialGapsAreHintsUnlessAnInvalidQsoExplainsThem()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"))
        << qso(QStringLiteral("DL2ABC"), QStringLiteral("144"), 2, QStringLiteral("JN58SD"))
        << qso(QStringLiteral("DL4ABC"), QStringLiteral("144"), 4, QStringLiteral("JN58SD"))
        << qso(QStringLiteral("DL6ABC"), QStringLiteral("144"), 6, QStringLiteral("JN58SD"));
    LogCheckResult result = checkLog(log, context());
    QVERIFY(result.submittable());
    const LogCheckIssue* gap = find(result, QStringLiteral("serial_sent_gap"));
    QVERIFY(gap);
    QCOMPARE(gap->qsoId, -1);
    QCOMPARE(gap->band, QStringLiteral("144"));
    QVERIFY(gap->message.contains(QStringLiteral("003, 005")));
    // Number 3 went to a QSO later marked invalid: that gap is explained.
    QsoRecord voided = qso(QStringLiteral("DL3ABC"), QStringLiteral("144"), 3, QStringLiteral("JN58SD"));
    voided.isInvalid = true;
    log << voided;
    result = checkLog(log, context());
    gap = find(result, QStringLiteral("serial_sent_gap"));
    QVERIFY(gap);
    QVERIFY(gap->message.contains(QStringLiteral("005")));
    QVERIFY(!gap->message.contains(QStringLiteral("003")));
    // Serials count per band: 432 starting at 001 is not a gap on 144.
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("432"), 1, QStringLiteral("JN58SD"));
    result = checkLog(log, context());
    QCOMPARE(codes(result, LogCheckIssue::Severity::Hint).count(QStringLiteral("serial_sent_gap")), 1);
}

void TestLogCheck::serialScopeContestCountsAcrossBands()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:00:00Z"))
        << qso(QStringLiteral("DL1ABC"), QStringLiteral("432"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:01:00Z"));
    QVERIFY(checkLog(log, context()).submittable()); // per band: two 001s are right
    LogCheckContext c = context();
    c.serialScope = QStringLiteral("contest");
    QCOMPARE(codes(checkLog(log, c), LogCheckIssue::Severity::Error), QStringList{QStringLiteral("serial_sent_duplicate")});
}

void TestLogCheck::forbiddenModeAndOwnCallsignAreErrors()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:00:00Z"), QStringLiteral("CW"))
        << qso(QStringLiteral("DL2ABC"), QStringLiteral("144"), 2, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:01:00Z"), QStringLiteral("SSB"))
        << qso(QStringLiteral("oe5sos"), QStringLiteral("144"), 3, QStringLiteral("JN67UT"), QStringLiteral("2026-10-03T15:02:00Z"), QStringLiteral("CW"));
    LogCheckContext marconi = context();
    marconi.modes = {QStringLiteral("CW")};
    const LogCheckResult result = checkLog(log, marconi);
    QCOMPARE(codes(result, LogCheckIssue::Severity::Error), (QStringList{QStringLiteral("mode_not_allowed"), QStringLiteral("own_call")}));
    QCOMPARE(find(result, QStringLiteral("mode_not_allowed"))->callsign, QStringLiteral("DL2ABC"));
    QVERIFY(find(result, QStringLiteral("mode_not_allowed"))->message.contains(QStringLiteral("SSB")));
    QCOMPARE(find(result, QStringLiteral("own_call"))->callsign, QStringLiteral("OE5SOS"));
    // Without a mode list any mode passes.
    QVERIFY(!codes(checkLog(log, context()), LogCheckIssue::Severity::Error).contains(QStringLiteral("mode_not_allowed")));
}

void TestLogCheck::oddCallsignAndRstShapeAreWarnings()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DLABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:00:00Z"))       // no digit
        << qso(QStringLiteral("D"), QStringLiteral("144"), 2, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:01:00Z"))           // too short
        << qso(QStringLiteral("DL1ABC/P"), QStringLiteral("144"), 3, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:02:00Z"))    // fine
        << qso(QStringLiteral("DL2ABC"), QStringLiteral("144"), 4, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:03:00Z"), QStringLiteral("CW"))
        << qso(QStringLiteral("DL3ABC"), QStringLiteral("144"), 5, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:04:00Z"))
        << qso(QStringLiteral("DL4ABC"), QStringLiteral("144"), 6, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:05:00Z"));
    log[3].rstRcvd = QStringLiteral("59");   // two digits in CW
    log[4].rstRcvd = QStringLiteral("599");  // three in SSB
    log[5].rstRcvd.clear();
    const LogCheckResult result = checkLog(log, context());
    QVERIFY(result.submittable());
    QCOMPARE(codes(result, LogCheckIssue::Severity::Warning),
             (QStringList{QStringLiteral("callsign_odd"), QStringLiteral("callsign_odd"), QStringLiteral("rst_odd"),
                          QStringLiteral("rst_odd"), QStringLiteral("rst_missing")}));
    LogCheckContext noRst = context();
    noRst.hasRstField = false;
    QCOMPARE(codes(checkLog(log, noRst), LogCheckIssue::Severity::Warning).size(), 2);
}

void TestLogCheck::sameCallWithTwoLocatorsIsAWarning()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:00:00Z"))
        << qso(QStringLiteral("DL1ABC"), QStringLiteral("432"), 1, QStringLiteral("JN58SE"), QStringLiteral("2026-10-03T15:01:00Z"))
        << qso(QStringLiteral("DL2ABC"), QStringLiteral("144"), 2, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:02:00Z"))
        << qso(QStringLiteral("DL2ABC"), QStringLiteral("432"), 2, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:03:00Z"));
    const LogCheckResult result = checkLog(log, context());
    QCOMPARE(codes(result, LogCheckIssue::Severity::Warning), QStringList{QStringLiteral("locator_inconsistent")});
    const LogCheckIssue* issue = find(result, QStringLiteral("locator_inconsistent"));
    // On a tie the earlier QSO is presumed right, the later one flagged.
    QCOMPARE(issue->band, QStringLiteral("432"));
    QVERIFY(issue->message.contains(QStringLiteral("JN58SD")));
    QVERIFY(issue->message.contains(QStringLiteral("JN58SE")));
}

void TestLogCheck::implausibleDistanceAndBandMismatchesAreWarnings()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("KB2UKA"), QStringLiteral("144"), 1, QStringLiteral("FN20XA"), QStringLiteral("2026-10-03T15:00:00Z"))
        << qso(QStringLiteral("DL1ABC"), QStringLiteral("70"), 2, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:01:00Z"))
        << qso(QStringLiteral("DL2ABC"), QStringLiteral("432"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:02:00Z"));
    log[2].freqHz = 144300000;
    const LogCheckResult result = checkLog(log, context());
    QVERIFY(result.submittable());
    QCOMPARE(codes(result, LogCheckIssue::Severity::Warning),
             (QStringList{QStringLiteral("distance_implausible"), QStringLiteral("band_not_in_contest"),
                          QStringLiteral("freq_band_mismatch")}));
    QVERIFY(find(result, QStringLiteral("distance_implausible"))->message.contains(QStringLiteral("km")));
    QVERIFY(find(result, QStringLiteral("freq_band_mismatch"))->message.contains(QStringLiteral("144.300")));
}

void TestLogCheck::unmarkedDupeIsAWarningAndMarkedDupesAreExemptFromExchangeTests()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:00:00Z"))
        << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 2, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:01:00Z"))
        << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 3, QString(), QStringLiteral("2026-10-03T15:02:00Z"));
    log[2].isDupe = true;
    log[2].serialRcvd.reset();
    log[2].rstRcvd.clear();
    const LogCheckResult result = checkLog(log, context());
    QVERIFY(result.submittable()); // the marked dupe's empty exchange is no error
    QCOMPARE(codes(result, LogCheckIssue::Severity::Warning), QStringList{QStringLiteral("unmarked_dupe")});
    QCOMPARE(find(result, QStringLiteral("unmarked_dupe"))->qsoId, log.at(1).id);
    QCOMPARE(codes(result, LogCheckIssue::Severity::Hint), QStringList{QStringLiteral("dupes")});
    QVERIFY(find(result, QStringLiteral("dupes"))->message.startsWith(QStringLiteral("1 Dupe ")));
}

void TestLogCheck::invalidQsosAreCountedNotChecked()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"));
    QsoRecord voided = qso(QStringLiteral("???"), QStringLiteral("144"), 2, QString(), QStringLiteral("2000-01-01T00:00:00Z"));
    voided.isInvalid = true;
    log << voided;
    const LogCheckResult result = checkLog(log, context());
    QCOMPARE(result.checkedQsos, 1);
    QVERIFY(result.submittable());
    QCOMPARE(allCodes(result), QStringList{QStringLiteral("invalid")});
    QVERIFY(find(result, QStringLiteral("invalid"))->message.startsWith(QStringLiteral("1 ungültig markiertes QSO")));
    // An empty log says so.
    const LogCheckResult empty = checkLog({}, context());
    QCOMPARE(allCodes(empty), QStringList{QStringLiteral("empty")});
}

void TestLogCheck::issuesAreOrderedBySeverityThenTime()
{
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:00:00Z"))
        << qso(QStringLiteral("DL2ABC"), QStringLiteral("144"), 2, QStringLiteral("JN58SD"), QStringLiteral("2026-10-03T15:01:00Z"))
        << qso(QStringLiteral("DL3ABC"), QStringLiteral("144"), 4, QString(), QStringLiteral("2026-10-03T15:02:00Z"));
    log[0].rstRcvd = QStringLiteral("5");  // warning, earliest
    log[1].isDupe = true;                  // hint (log-wide)
    // DL3ABC: error (no locator); serial 3 missing: hint (log-wide)
    const LogCheckResult result = checkLog(log, context());
    QCOMPARE(allCodes(result), (QStringList{QStringLiteral("no_locator"), QStringLiteral("rst_odd"),
                                            QStringLiteral("serial_sent_gap"), QStringLiteral("dupes")}));
    QCOMPARE(result.errors, 1);
    QCOMPARE(result.warnings, 1);
    QCOMPARE(result.hints, 2);
    QCOMPARE(result.countsText(), QStringLiteral("1 Fehler, 1 Warnung, 2 Hinweise"));
}

void TestLogCheck::contextComesFromDefinitionAndSettings()
{
    QString error;
    const ContestDefinition def = ContestDefinition::loadFromJson(R"JSON({
      "id": "IARU_R1_MARCONI", "name": "Marconi", "bands": ["144"],
      "dupe_scope": ["callsign", "band"], "modes": ["CW"], "serial_scope": "contest",
      "schedule": { "month": 11 },
      "exchange_fields": [
        { "key": "rst", "label": "RST", "type": "rst" },
        { "key": "serial", "label": "Nr.", "type": "int", "auto_increment": true },
        { "key": "grid", "label": "Grid", "type": "grid6" } ]
    })JSON", &error);
    QVERIFY2(def.isValid(), qPrintable(error));
    ContestSettings settings;
    settings.ownCallsign = QStringLiteral("oe5sos ");
    settings.ownGrid = QStringLiteral("jn67ut");

    // The window nearest to the earliest QSO: a January check of the
    // November log still measures against that November.
    QVector<QsoRecord> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("144"), 2, QStringLiteral("JN58SD"), QStringLiteral("2026-11-07T20:00:00Z"), QStringLiteral("CW"))
        << qso(QStringLiteral("DL2ABC"), QStringLiteral("144"), 1, QStringLiteral("JN58SD"), QStringLiteral("2026-11-07T15:00:00Z"), QStringLiteral("CW"));
    LogCheckContext c = logCheckContextFor(def, settings, log, utc(2027, 1, 10, 12, 0));
    QCOMPARE(c.ownCallsign, QStringLiteral("OE5SOS"));
    QCOMPARE(c.ownGrid, QStringLiteral("JN67UT"));
    QCOMPARE(c.bands, QStringList{QStringLiteral("144")});
    QCOMPARE(c.modes, QStringList{QStringLiteral("CW")});
    QCOMPARE(c.serialScope, QStringLiteral("contest"));
    QVERIFY(c.hasSerialField && c.hasGridField && c.hasRstField);
    QCOMPARE(c.window.startUtc, utc(2026, 11, 7, 14, 0));
    QVERIFY(checkLog(log, c).submittable());

    // Empty log: nearest to "now".
    c = logCheckContextFor(def, settings, {}, utc(2026, 10, 20, 12, 0));
    QCOMPARE(c.window.startUtc, utc(2026, 11, 7, 14, 0));

    // A manual contest end wins, with the schedule's hours before it.
    settings.contestEndUtc = QStringLiteral("2026-11-15T12:00:00Z");
    c = logCheckContextFor(def, settings, log, utc(2026, 11, 20, 12, 0));
    QCOMPARE(c.window.startUtc, utc(2026, 11, 14, 12, 0));
    QCOMPARE(c.window.endUtc, utc(2026, 11, 15, 12, 0));
    QVERIFY(!checkLog(log, c).submittable()); // the QSOs are now outside it

    // No schedule, no manual end: unknown window, fields off when absent.
    const ContestDefinition bare = ContestDefinition::loadFromJson(R"JSON({
      "id": "X", "name": "X", "bands": ["144"], "dupe_scope": ["callsign"], "exchange_fields": []
    })JSON", &error);
    settings.contestEndUtc.clear();
    c = logCheckContextFor(bare, settings, log, utc(2026, 11, 20, 12, 0));
    QVERIFY(!c.window.isValid());
    QVERIFY(!c.hasSerialField && !c.hasGridField && !c.hasRstField);
}

QTEST_GUILESS_MAIN(TestLogCheck)
#include "test_log_check.moc"
