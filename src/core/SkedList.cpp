#include "core/SkedList.h"

#include "core/BandUtils.h"
#include "core/SpotCandidate.h"

#include <QRegularExpression>
#include <QTimeZone>

namespace Contestprogramm {

namespace SkedRules {

QString statusText(const Sked& sked, const QDateTime& nowUtc)
{
    switch (sked.state) {
    case Sked::State::Suggested: return QStringLiteral("aus KST · übernehmen?");
    case Sked::State::Done: return QStringLiteral("erledigt");
    case Sked::State::Missed: return QStringLiteral("verpasst");
    case Sked::State::Open: break;
    }
    if (!sked.timeUtc.isValid()) {
        return QStringLiteral("offen");
    }
    const qint64 secs = nowUtc.secsTo(sked.timeUtc);
    if (secs > kDueBeforeSecs) {
        const qint64 minutes = (secs + 30) / 60;
        return minutes <= 90 ? QStringLiteral("in %1 min").arg(minutes) : QStringLiteral("offen");
    }
    if (secs >= -kMissedAfterSecs) {
        return QStringLiteral("jetzt");
    }
    return QStringLiteral("verpasst");
}

bool isDue(const Sked& sked, const QDateTime& nowUtc)
{
    if (sked.state != Sked::State::Open || !sked.timeUtc.isValid()) {
        return false;
    }
    const qint64 secs = nowUtc.secsTo(sked.timeUtc);
    return secs <= kDueBeforeSecs && secs >= -kMissedAfterSecs;
}

bool isOverdue(const Sked& sked, const QDateTime& nowUtc)
{
    return sked.state == Sked::State::Open && sked.timeUtc.isValid() && nowUtc.secsTo(sked.timeUtc) < -kMissedAfterSecs;
}

const Sked* nextOpen(const QVector<Sked>& skeds, const QDateTime& nowUtc)
{
    const Sked* best = nullptr;
    for (const Sked& sked : skeds) {
        if (sked.state != Sked::State::Open || !sked.timeUtc.isValid() || isOverdue(sked, nowUtc)) {
            continue;
        }
        if (!best || sked.timeUtc < best->timeUtc) {
            best = &sked;
        }
    }
    return best;
}

std::optional<qint64> parseFrequencyHz(const QString& text)
{
    // 144.317 / 144,317 / 144317 / 432.2 / 1296.200 -- MHz with up to
    // three decimals, separator optional when three digits follow.
    static const QRegularExpression rx(
        QStringLiteral("(?<![\\d.,])(1296|144|145|146|43[0-9]|50|70)(?:[.,](\\d{1,3})|(\\d{3}))(?![\\d.,])"));
    QRegularExpressionMatchIterator it = rx.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const int mhz = m.captured(1).toInt();
        QString frac = m.captured(2).isEmpty() ? m.captured(3) : m.captured(2);
        while (frac.size() < 3) {
            frac += QLatin1Char('0');
        }
        const qint64 hz = qint64(mhz) * 1000000 + frac.toInt() * 1000;
        if (!bandLabelForFrequencyHz(hz).isEmpty()) {
            return hz;
        }
    }
    return std::nullopt;
}

namespace {

std::optional<QTime> clockTimeIn(const QString& text)
{
    static const QRegularExpression rxColon(QStringLiteral("\\b([01]?\\d|2[0-3])[:.]([0-5]\\d)\\b"));
    const QRegularExpressionMatch m = rxColon.match(text);
    if (m.hasMatch()) {
        return QTime(m.captured(1).toInt(), m.captured(2).toInt());
    }
    // "at 1435" / "um 1435" / "@1435": four digits only with a marker,
    // a bare 4-digit number is too often something else.
    static const QRegularExpression rxMarked(QStringLiteral("(?:\\bat|\\bum|@)\\s*([01]\\d|2[0-3])([0-5]\\d)\\b"),
                                             QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch mm = rxMarked.match(text);
    if (mm.hasMatch()) {
        return QTime(mm.captured(1).toInt(), mm.captured(2).toInt());
    }
    return std::nullopt;
}

QDateTime todayOrTomorrow(const QTime& time, const QDateTime& nowUtc)
{
    QDateTime candidate(nowUtc.date(), time, QTimeZone::UTC);
    // A minute more than 30 minutes gone means the next day (a sked
    // arranged at 23:50 for 00:05).
    if (candidate.secsTo(nowUtc) > 30 * 60) {
        candidate = candidate.addDays(1);
    }
    return candidate;
}

} // namespace

std::optional<QDateTime> parseManualTime(const QString& text, const QDateTime& nowUtc)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) {
        return nowUtc;
    }
    static const QRegularExpression rxPlus(QStringLiteral("^\\+\\s*(\\d{1,3})$"));
    const QRegularExpressionMatch plus = rxPlus.match(t);
    if (plus.hasMatch()) {
        return nowUtc.addSecs(plus.captured(1).toInt() * 60);
    }
    static const QRegularExpression rxFour(QStringLiteral("^([01]\\d|2[0-3])([0-5]\\d)$"));
    const QRegularExpressionMatch four = rxFour.match(t);
    if (four.hasMatch()) {
        return todayOrTomorrow(QTime(four.captured(1).toInt(), four.captured(2).toInt()), nowUtc);
    }
    if (const auto clock = clockTimeIn(t)) {
        return todayOrTomorrow(*clock, nowUtc);
    }
    return std::nullopt;
}

std::optional<Sked> suggestionFromChat(const SpotCandidate& candidate, const QString& myCall, const QDateTime& nowUtc)
{
    const QString me = myCall.trimmed().toUpper();
    if (me.isEmpty() || candidate.callsign.trimmed().isEmpty()) {
        return std::nullopt;
    }
    // CH|chat_id|date|callsign|firstname|destination|msg|highlight|
    const QStringList fields = candidate.rawLine.split(QLatin1Char('|'));
    if (fields.size() < 7) {
        return std::nullopt;
    }
    const QString destination = fields.at(5).trimmed().toUpper();
    const QString msg = fields.at(6);
    const bool addressed = destination == me
        || msg.contains(QRegularExpression(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(me)),
                                           QRegularExpression::CaseInsensitiveOption));
    if (!addressed || candidate.callsign.trimmed().toUpper() == me) {
        return std::nullopt;
    }
    const auto hz = parseFrequencyHz(msg);
    if (!hz) {
        return std::nullopt;
    }

    Sked sked;
    sked.callsign = candidate.callsign.trimmed().toUpper();
    sked.grid = candidate.grid.trimmed().toUpper();
    sked.freqHz = *hz;
    sked.band = bandLabelForFrequencyHz(*hz);
    sked.state = Sked::State::Suggested;
    sked.source = QStringLiteral("on4kst");
    sked.note = msg.simplified();
    sked.createdUtc = nowUtc;

    static const QRegularExpression rxIn(QStringLiteral("\\bin\\s*(\\d{1,3})\\s*min"), QRegularExpression::CaseInsensitiveOption);
    if (const auto clock = clockTimeIn(msg)) {
        sked.timeUtc = todayOrTomorrow(*clock, nowUtc);
    } else if (const QRegularExpressionMatch in = rxIn.match(msg); in.hasMatch()) {
        sked.timeUtc = nowUtc.addSecs(in.captured(1).toInt() * 60);
    } else {
        // No time named: the message's own minute -- "144.317?" means
        // "come now".
        sked.timeUtc = nowUtc;
    }
    return sked;
}

} // namespace SkedRules

} // namespace Contestprogramm
