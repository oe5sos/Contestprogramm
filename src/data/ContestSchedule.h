#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QTime>

namespace Contestprogramm {

// One contest period: [startUtc, endUtc], both UTC. Invalid (default)
// means "unknown" -- then nothing that depends on it (the countdown,
// the log check's time test) pretends to know.
struct ContestWindow {
    QDateTime startUtc;
    QDateTime endUtc;

    bool isValid() const { return startUtc.isValid() && endUtc.isValid() && startUtc < endUtc; }
    // Inclusive at both ends: EDI times have minute resolution, and a
    // QSO stamped exactly 14:00 on the closing minute is still one the
    // robots accept.
    bool contains(const QDateTime& utc) const;
    // Seconds from `utc` to the window: 0 inside, positive outside.
    qint64 distanceSecs(const QDateTime& utc) const;
    // "Sa 03.10. 14:00 – So 04.10. 14:00 UTC" for messages.
    QString describe() const;
};

// When a contest runs, as its rules state it. Every IARU Region 1
// VHF/UHF contest is fixed to "the first full weekend of <month>,
// Saturday 14:00 to Sunday 14:00 UTC" (VHF: September, UHF/Microwaves:
// October, Marconi Memorial: November), so a definition carries the
// rule, not a date, and the program works out the dates for any year --
// the operator never has to type a contest end again (though the
// manual ContestSettings::contestEndUtc still wins when set, see
// effectiveContestWindow()). JSON, under the definition's "schedule"
// key, all but "month" optional:
//   { "month": 10, "weekend": 1, "day": "sat", "start": "14:00", "hours": 24 }
// "weekend" counts full weekends (Saturday+Sunday both inside the
// month): November 2026 starts on a Sunday, so its first full weekend
// is 7/8 November, not 1 November.
struct ContestSchedule {
    int month = 0;                // 1-12; 0 = no schedule known
    int weekend = 1;              // 1 = first full weekend of the month
    int startDay = Qt::Saturday;  // Qt::DayOfWeek the window opens on
    QTime startTimeUtc{14, 0};
    int hours = 24;

    bool isValid() const;

    ContestWindow windowIn(int year) const;
    // The occurrence closest to `utc` -- this year's, or a neighbouring
    // year's when that lies nearer (a check run in January on a
    // November log).
    ContestWindow windowNearest(const QDateTime& utc) const;
    // The next occurrence still running or ahead of `nowUtc` (for the
    // countdown: after this year's contest ends, next year's is the one
    // to count towards).
    ContestWindow windowUpcoming(const QDateTime& nowUtc) const;

    // Parses the JSON shape above; an invalid or out-of-range field is
    // an error (reported via `errorOut`, result isValid() == false),
    // not silently defaulted.
    static ContestSchedule fromJson(const QJsonObject& object, QString* errorOut = nullptr);
    QJsonObject toJson() const;
};

// The window the program actually uses: an operator-set contest end
// (ContestSettings::contestEndUtc, ISO-8601) wins, with `schedule.hours`
// (24 without a schedule) before it as the start; else the schedule's
// occurrence nearest to `referenceUtc`; else an invalid window.
ContestWindow effectiveContestWindow(const ContestSchedule& schedule,
                                     const QString& manualEndUtcIso,
                                     const QDateTime& referenceUtc);

} // namespace Contestprogramm
