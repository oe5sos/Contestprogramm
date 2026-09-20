#include "data/ContestSchedule.h"

#include <QJsonValue>
#include <QTimeZone>

#include <array>
#include <limits>

namespace Contestprogramm {

namespace {

constexpr int kMaxWeekend = 5;
constexpr int kMaxHours = 7 * 24;

QString dayAbbrev(const QDate& date)
{
    static const std::array<const char*, 8> names = {"", "Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"};
    const int day = date.dayOfWeek();
    return QString::fromLatin1(day >= 1 && day <= 7 ? names[static_cast<size_t>(day)] : "");
}

QString stamp(const QDateTime& utc)
{
    return QStringLiteral("%1 %2 %3")
        .arg(dayAbbrev(utc.date()), utc.date().toString(QStringLiteral("dd.MM.")),
             utc.time().toString(QStringLiteral("HH:mm")));
}

} // namespace

bool ContestWindow::contains(const QDateTime& utc) const
{
    return isValid() && utc.isValid() && utc >= startUtc && utc <= endUtc;
}

qint64 ContestWindow::distanceSecs(const QDateTime& utc) const
{
    if (!isValid() || !utc.isValid()) {
        return std::numeric_limits<qint64>::max();
    }
    if (utc < startUtc) {
        return utc.secsTo(startUtc);
    }
    if (utc > endUtc) {
        return endUtc.secsTo(utc);
    }
    return 0;
}

QString ContestWindow::describe() const
{
    if (!isValid()) {
        return QString();
    }
    return QStringLiteral("%1 – %2 UTC").arg(stamp(startUtc), stamp(endUtc));
}

bool ContestSchedule::isValid() const
{
    return month >= 1 && month <= 12 && weekend >= 1 && weekend <= kMaxWeekend
        && (startDay == Qt::Saturday || startDay == Qt::Sunday) && startTimeUtc.isValid() && hours >= 1
        && hours <= kMaxHours;
}

ContestWindow ContestSchedule::windowIn(int year) const
{
    ContestWindow window;
    if (!isValid()) {
        return window;
    }
    // The first full weekend is the one whose Saturday is the first
    // Saturday of the month; the nth full weekend is 7 (n-1) days on.
    const QDate first(year, month, 1);
    if (!first.isValid()) {
        return window;
    }
    const int toSaturday = (Qt::Saturday - first.dayOfWeek() + 7) % 7;
    QDate start = first.addDays(toSaturday + 7 * (weekend - 1));
    if (startDay == Qt::Sunday) {
        start = start.addDays(1);
    }
    window.startUtc = QDateTime(start, startTimeUtc, QTimeZone::UTC);
    window.endUtc = window.startUtc.addSecs(static_cast<qint64>(hours) * 3600);
    return window;
}

ContestWindow ContestSchedule::windowNearest(const QDateTime& utc) const
{
    if (!isValid() || !utc.isValid()) {
        return ContestWindow();
    }
    const int year = utc.toUTC().date().year();
    ContestWindow best;
    qint64 bestDistance = std::numeric_limits<qint64>::max();
    for (int candidate = year - 1; candidate <= year + 1; ++candidate) {
        const ContestWindow window = windowIn(candidate);
        const qint64 distance = window.distanceSecs(utc);
        if (distance < bestDistance) {
            best = window;
            bestDistance = distance;
        }
    }
    return best;
}

ContestWindow ContestSchedule::windowUpcoming(const QDateTime& nowUtc) const
{
    if (!isValid() || !nowUtc.isValid()) {
        return ContestWindow();
    }
    const int year = nowUtc.toUTC().date().year();
    for (int candidate = year; candidate <= year + 1; ++candidate) {
        const ContestWindow window = windowIn(candidate);
        if (window.isValid() && nowUtc <= window.endUtc) {
            return window;
        }
    }
    return ContestWindow();
}

ContestSchedule ContestSchedule::fromJson(const QJsonObject& object, QString* errorOut)
{
    ContestSchedule schedule;
    const auto fail = [&](const QString& why) {
        if (errorOut) {
            *errorOut = why;
        }
        return ContestSchedule();
    };

    if (!object.value(QStringLiteral("month")).isDouble()) {
        return fail(QStringLiteral("\"schedule\" needs a numeric \"month\""));
    }
    schedule.month = object.value(QStringLiteral("month")).toInt();
    if (schedule.month < 1 || schedule.month > 12) {
        return fail(QStringLiteral("\"schedule\".\"month\" must be 1-12"));
    }
    if (object.contains(QStringLiteral("weekend"))) {
        schedule.weekend = object.value(QStringLiteral("weekend")).toInt(-1);
        if (schedule.weekend < 1 || schedule.weekend > kMaxWeekend) {
            return fail(QStringLiteral("\"schedule\".\"weekend\" must be 1-%1").arg(kMaxWeekend));
        }
    }
    if (object.contains(QStringLiteral("day"))) {
        const QString day = object.value(QStringLiteral("day")).toString().trimmed().toLower();
        if (day == QStringLiteral("sat")) {
            schedule.startDay = Qt::Saturday;
        } else if (day == QStringLiteral("sun")) {
            schedule.startDay = Qt::Sunday;
        } else {
            return fail(QStringLiteral("\"schedule\".\"day\" must be \"sat\" or \"sun\""));
        }
    }
    if (object.contains(QStringLiteral("start"))) {
        const QTime start = QTime::fromString(object.value(QStringLiteral("start")).toString(), QStringLiteral("HH:mm"));
        if (!start.isValid()) {
            return fail(QStringLiteral("\"schedule\".\"start\" must be HH:MM (UTC)"));
        }
        schedule.startTimeUtc = start;
    }
    if (object.contains(QStringLiteral("hours"))) {
        schedule.hours = object.value(QStringLiteral("hours")).toInt(-1);
        if (schedule.hours < 1 || schedule.hours > kMaxHours) {
            return fail(QStringLiteral("\"schedule\".\"hours\" must be 1-%1").arg(kMaxHours));
        }
    }
    if (errorOut) {
        errorOut->clear();
    }
    return schedule;
}

QJsonObject ContestSchedule::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("month"), month);
    object.insert(QStringLiteral("weekend"), weekend);
    object.insert(QStringLiteral("day"), startDay == Qt::Sunday ? QStringLiteral("sun") : QStringLiteral("sat"));
    object.insert(QStringLiteral("start"), startTimeUtc.toString(QStringLiteral("HH:mm")));
    object.insert(QStringLiteral("hours"), hours);
    return object;
}

ContestWindow effectiveContestWindow(const ContestSchedule& schedule,
                                     const QString& manualEndUtcIso,
                                     const QDateTime& referenceUtc)
{
    QDateTime manualEnd = QDateTime::fromString(manualEndUtcIso, Qt::ISODate);
    if (manualEnd.isValid()) {
        // Always meant as UTC, whatever fromString() inferred -- same
        // rule as UtcClockWidget::setContestEndUtc.
        manualEnd.setTimeZone(QTimeZone::UTC);
        ContestWindow window;
        const int hours = schedule.isValid() ? schedule.hours : 24;
        window.endUtc = manualEnd;
        window.startUtc = manualEnd.addSecs(-static_cast<qint64>(hours) * 3600);
        return window;
    }
    return schedule.windowNearest(referenceUtc);
}

} // namespace Contestprogramm
