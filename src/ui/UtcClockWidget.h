#pragma once

#include <QDateTime>
#include <QWidget>

class QLabel;
class QTimer;

namespace Contestprogramm {

// Top-bar UTC clock + contest-end countdown, per the plan's "UTC-Uhr +
// Countdown oben in der Titelzeile" UI direction. Visually a direct port
// of the clock Martin actually sees every day in Longpath -- NOT gui/
// meters/ClockItem.cpp (an optional meter item nobody has necessarily
// ever dragged into a container), but gui/TitleBar.cpp's own always-on
// UTC label (Task A7, "single-row UTC clock ... top-right"), read after
// Martin's live correction ("die utc schaut aber nicht nach longpath
// aus"): one plain, small, single-line QLabel --
// `Style::kTextSecondary()` (a quiet grey, NOT this app's own kAmberText
// "Warm = gemessen" convention -- TitleBar's clock is not styled as a
// measured value, just a quiet fact), 11px Menlo, not bold, no caption
// row above it, no box, no border. An earlier version of this widget
// tried the ClockItem stacked-caption-plus-big-digits treatment instead
// -- wrong reference, corrected here.
//
// The UTC readout always ticks and always shows (QDateTime::
// currentDateTimeUtc(), once a second via an internal QTimer). The
// countdown-to-contest-end readout (TitleBar has no equivalent -- this
// is Contestprogramm's own real requirement) sits right after it in the
// same plain single-line style, independently show/hideable
// (setCountdownVisible) per the operator's explicit request -- this
// widget has no opinion on the default state, it only renders whatever
// MainWindow tells it (see ContestSettings::countdownVisible).
class UtcClockWidget : public QWidget {
    Q_OBJECT

public:
    explicit UtcClockWidget(QWidget* parent = nullptr);

    // ISO-8601 (see ContestSettings::contestEndUtc); an empty or
    // unparseable string means "unset" -- the countdown then shows
    // Style::unknownDash() rather than a fake 00:00:00 or a wrong guess
    // (see formatRemaining below).
    void setContestEndUtc(const QString& iso8601);
    // Both ends of the contest (data/ContestSchedule.h's ContestWindow,
    // from the definition's schedule or the manual end): with a start
    // the readout also counts down to it before the contest and says
    // "Beendet" after it. An invalid start means end-only, as above.
    void setContestWindow(const QDateTime& startUtc, const QDateTime& endUtc);
    void setCountdownVisible(bool visible);

    // Pure, testable formatting -- HH:MM:SS remaining from `nowUtc` to
    // `endUtc`, zero-padded, clamped at "00:00:00" once reached/passed (a
    // genuine zero, not "unknown" -- HAUSSTIL rule 7's own
    // zero-is-a-real-value case, "0 QSOs in the last 10 minutes" is
    // RateMeterWidget's equivalent) or Style::unknownDash() if `endUtc`
    // is not valid at all.
    static QString formatRemaining(const QDateTime& nowUtc, const QDateTime& endUtc);

    // The whole readout: "Noch " + formatRemaining() while the contest
    // runs (or without a known start); before a known start "Start in
    // HH:MM:SS", or "Start Sa 03.10. 14:00 UTC" when that is more than a
    // week away; "Beendet" once the end has passed with a known start.
    static QString formatCountdown(const QDateTime& nowUtc, const QDateTime& startUtc, const QDateTime& endUtc);

private slots:
    void tick();

private:
    QLabel* m_utcLabel;
    QLabel* m_countdownLabel;
    QDateTime m_contestStartUtc; // invalid == unknown
    QDateTime m_contestEndUtc;   // invalid == unset
    QTimer* m_timer;
};

} // namespace Contestprogramm
