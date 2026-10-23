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
    if (!isValid() || !utc.isValid()) {
        return false;
    }
    // Auf die Minute abgeschnitten, wie das EDI sie fuehrt: ein QSO um
    // 14:00:30 am Sonntag steht dort als 1400 und gilt fuer den Robot
    // noch als im Contest -- hier galt es vorher als "ausserhalb".
    QDateTime minute = utc.toUTC();
    minute.setTime(QTime(minute.time().hour(), minute.time().minute()));
    return minute >= startUtc && minute <= endUtc;
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
    return month >= 1 && month <= 12
        && ((weekend >= 1 && weekend <= kMaxWeekend)
            || (weekend <= -1 && weekend >= -kMaxWeekend))
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

    QDate start;
    if (weekend >= 1) {
        start = first.addDays(toSaturday + 7 * (weekend - 1));
    } else {
        // ── Vom Monatsende her gezaehlt ──────────────────────────────
        //
        // Dazugekommen 2026-10-23 fuer die Kurzwellenconteste. Die
        // grossen rechnen nicht vom Monatsanfang, sondern vom Ende:
        // CQ WW SSB ist „das letzte volle Wochenende im Oktober", CQ
        // WPX CW „das letzte volle Wochenende im Mai". Mit `weekend`
        // ab 1 waere das jedes Jahr eine andere Zahl -- also zaehlt
        // eine NEGATIVE Zahl von hinten: -1 ist das letzte, -2 das
        // vorletzte.
        //
        // „Voll" heisst: Samstag UND Sonntag liegen im Monat. Ein
        // Samstag am 31. ist also nicht das letzte volle Wochenende,
        // sondern der Sonntag faellt schon in den naechsten Monat --
        // genau der Fall, an dem eine Rechnung „letzter Samstag"
        // falsch liegt. CQ WW 2026 ist dafuer das Beispiel: der 31.
        // Oktober ist ein Samstag, der Contest aber am 24./25.
        const QDate last(year, month, first.daysInMonth());
        // Der letzte Sonntag des Monats -- sein Samstag ist der Tag
        // davor und liegt damit immer im selben Monat.
        const int backToSunday = (last.dayOfWeek() - Qt::Sunday + 7) % 7;
        const QDate letzterSonntag = last.addDays(-backToSunday);
        start = letzterSonntag.addDays(-1 - 7 * (-weekend - 1));
        if (start.month() != month) {
            return window;   // so weit zurueck gibt es den Monat nicht
        }
    }
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
        // 0 ist kein gueltiger Wert und damit ein brauchbarer Fehlwert:
        // „weekend": "foo" wird zu 0 und faellt unten durch.
        schedule.weekend = object.value(QStringLiteral("weekend")).toInt(0);
        const int betrag = schedule.weekend < 0 ? -schedule.weekend : schedule.weekend;
        if (schedule.weekend == 0 || betrag > kMaxWeekend) {
            return fail(QStringLiteral("\"schedule\".\"weekend\" must be 1-%1 "
                                       "(or -1..-%1 counting back from the end "
                                       "of the month)").arg(kMaxWeekend));
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
