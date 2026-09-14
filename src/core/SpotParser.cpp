#include "core/SpotParser.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QStringList>
#include <QTimeZone>

namespace Contestprogramm {

bool SpotParser::parseDxSpotLine(const QString& line, SpotCandidate& candidateOut)
{
    const QStringList fields = line.split(QLatin1Char('|'));
    // DL|unix_time|dx_utc|spotter|qrg|dx|info|spotter_locator|dx_locator|
    //  0     1        2      3    4   5   6        7              8
    if (fields.size() < 9 || fields.at(0) != QStringLiteral("DL")) {
        return false;
    }

    const QString dxCall = fields.at(5).trimmed();
    if (dxCall.isEmpty()) {
        return false;
    }

    bool timeOk = false;
    const qint64 unixTime = fields.at(1).toLongLong(&timeOk);

    SpotCandidate candidate;
    candidate.callsign = dxCall.toUpper();
    candidate.grid = fields.at(8).trimmed().toUpper();
    candidate.rawLine = line;
    candidate.timestampUtc = (timeOk && unixTime > 0)
        ? QDateTime::fromSecsSinceEpoch(unixTime, QTimeZone(QTimeZone::UTC))
        : QDateTime::currentDateTimeUtc();
    candidate.source = QStringLiteral("on4kst");
    // qrg assumed kHz -- see the header comment.
    candidate.freqHz = static_cast<qint64>(fields.at(4).toDouble() * 1000.0);

    candidateOut = candidate;
    return true;
}

bool SpotParser::parseChatLine(const QString& line, SpotCandidate& candidateOut)
{
    const QStringList fields = line.split(QLatin1Char('|'));
    // CH|chat_id|date|callsign|firstname|destination|msg|highlight|
    //  0    1      2     3         4          5        6      7
    if (fields.size() < 7) {
        return false;
    }
    if (fields.at(0) != QStringLiteral("CH") && fields.at(0) != QStringLiteral("CR")) {
        return false;
    }

    const QString callsign = fields.at(3).trimmed();
    if (callsign.isEmpty()) {
        return false;
    }

    SpotCandidate candidate;
    candidate.callsign = callsign.toUpper();
    candidate.rawLine = line;
    candidate.timestampUtc = QDateTime::currentDateTimeUtc();
    candidate.source = QStringLiteral("on4kst");

    const QString msg = fields.at(6);
    static const QRegularExpression rxGrid(
        QStringLiteral("\\b([A-Ra-r]{2}[0-9]{2}(?:[A-Xa-x]{2})?)\\b"));
    const QRegularExpressionMatch match = rxGrid.match(msg);
    if (match.hasMatch()) {
        candidate.grid = match.captured(1).toUpper();
    }

    candidateOut = candidate;
    return true;
}

} // namespace Contestprogramm
