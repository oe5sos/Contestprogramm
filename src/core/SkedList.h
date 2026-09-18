#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

namespace Contestprogramm {

struct SpotCandidate;

// A sked: "DL0GTH, 144.317, 14:35 UTC, beam 340°". What wtKST and Tucnak
// keep for the ON4KST way of running a VHF contest -- arrange on the
// chat, meet on the frequency at the minute. Kept in the database
// (ContestDatabase's `skeds` table) so a restart mid-contest loses
// none. The rules here are pure functions over a Sked and "now", so
// SkedPanel and MainWindow agree on what "in 4 min", "jetzt", "verpasst"
// mean without either of them owning a clock.
struct Sked {
    enum class State { Suggested, Open, Done, Missed };

    int id = -1;
    QString callsign;
    QString grid;
    QString band;      // "144"/"432"/... derived from freqHz
    qint64 freqHz = 0;
    QDateTime timeUtc; // the agreed minute
    State state = State::Open;
    QString source;    // "manual" | "on4kst"
    QString note;      // for a suggestion: the chat text it came from
    QDateTime createdUtc;
};

namespace SkedRules {

// The window in which a sked counts as "now" and the grace after which
// an untouched one is missed.
constexpr int kDueBeforeSecs = 120;
constexpr int kAlarmBeforeSecs = 120;
constexpr int kMissedAfterSecs = 600;

// "in 4 min" / "jetzt" / "offen" / "verpasst" / "erledigt" /
// "aus KST · übernehmen?" -- the list's Status column.
QString statusText(const Sked& sked, const QDateTime& nowUtc);

// Open and inside [time - kDueBeforeSecs, time + kMissedAfterSecs].
bool isDue(const Sked& sked, const QDateTime& nowUtc);

// Open and past time + kMissedAfterSecs.
bool isOverdue(const Sked& sked, const QDateTime& nowUtc);

// The open sked that comes next (earliest time not yet overdue), or
// nullptr.
const Sked* nextOpen(const QVector<Sked>& skeds, const QDateTime& nowUtc);

// "144.317", "144,317", "144317", "432.220", "1296.200" -> Hz, only on
// a VHF/UHF band this program knows; nothing otherwise.
std::optional<qint64> parseFrequencyHz(const QString& text);

// Manual entry: "14:35" / "1435" (UTC today, or tomorrow if that minute
// is more than 30 min gone), "+5" (minutes from now), "" (now).
std::optional<QDateTime> parseManualTime(const QString& text, const QDateTime& nowUtc);

// A KST chat line addressed to `myCall` (destination field or the call
// in the text) that names a frequency becomes a suggested sked: time
// from "14:35"/"at 1435"/"in 5 min"/"now", else the message's own
// minute. The suggestion keeps the text as its note.
std::optional<Sked> suggestionFromChat(const SpotCandidate& candidate, const QString& myCall, const QDateTime& nowUtc);

} // namespace SkedRules

} // namespace Contestprogramm
