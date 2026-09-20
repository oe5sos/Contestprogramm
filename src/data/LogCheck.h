#pragma once

#include "data/ContestSchedule.h"

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Contestprogramm {

class ContestDefinition;
struct ContestSettings;
struct QsoRecord;

// The pre-submission log check DXLog.net has as "Check log" and N1MM+
// folds into "Rescore": everything the IARU-R1/ÖVSV robots deduct for
// or reject, found before the EDI file leaves the house rather than in
// the results list four weeks later. A pure function over the records
// (like computeContestScore), so it is unit-testable and the same run
// feeds Datei > Log prüfen and the EDI export's "N Fehler -- trotzdem?"
// question.
//
// Severity: Error = the robot scores this QSO 0 or rejects the log
// (no locator, no received serial, time outside the contest, forbidden
// mode, own callsign, a sent serial used twice); Warning = probably a
// typo worth a look (RST shape, odd callsign, same call logged with
// two locators, implausible distance, frequency not on the logged band,
// unmarked duplicate, serial running backwards); Hint = informational
// (dupes and invalid QSOs in the log, serial gaps, no contest period
// known). Only errors make a log not submittable.
struct LogCheckIssue {
    enum class Severity { Error, Warning, Hint };
    Severity severity = Severity::Hint;
    int qsoId = -1;          // -1: about the log as a whole
    QString timestampUtc;    // the QSO's, empty for log-wide issues
    QString callsign;
    QString band;
    QString code;            // stable identifier, e.g. "no_locator"
    QString message;         // German, ready to show
};

struct LogCheckResult {
    QVector<LogCheckIssue> issues; // errors first, then warnings, then hints, each in log order
    int checkedQsos = 0;           // records that are not marked invalid
    int errors = 0;
    int warnings = 0;
    int hints = 0;

    bool submittable() const { return errors == 0; }
    // "0 Fehler, 2 Warnungen, 1 Hinweis"
    QString countsText() const;
};

// What the check needs to know about the contest and the station.
struct LogCheckContext {
    QString ownCallsign;
    QString ownGrid;
    QStringList bands;              // the definition's bands
    QStringList modes;              // allowed modes, empty = any
    QStringList dupeScope;          // "callsign" + "band" [+ "mode"]
    QString serialScope = QStringLiteral("band");
    ContestWindow window;           // invalid = unknown, time test skipped
    bool hasSerialField = true;
    bool hasGridField = true;
    bool hasRstField = true;
    double implausibleKm = 1500.0;  // beyond this a locator is questioned
};

// Context for the active contest: bands/modes/scopes/fields from the
// definition, own call/grid from the settings, the contest period via
// effectiveContestWindow() -- nearest to the first QSO's time (a check
// run in November on the October log), or to `nowUtc` for an empty log.
LogCheckContext logCheckContextFor(const ContestDefinition& definition,
                                   const ContestSettings& settings,
                                   const QVector<QsoRecord>& records,
                                   const QDateTime& nowUtc);

LogCheckResult checkLog(const QVector<QsoRecord>& records, const LogCheckContext& context);

} // namespace Contestprogramm
