#include "core/assistant/MessageDrafter.h"

namespace Contestprogramm {
namespace MessageDrafter {

QString draftDirectedCall(const QString& targetCallsign, const QString& ownCallsign, const QString& exchange)
{
    QString text = targetCallsign + QStringLiteral(" DE ") + ownCallsign;
    if (!exchange.isEmpty()) {
        text += QLatin1Char(' ') + exchange;
    }
    return text;
}

QString draftCqCall(const QString& ownCallsign, bool includeContestTag)
{
    QString text = QStringLiteral("CQ");
    if (includeContestTag) {
        text += QStringLiteral(" CONTEST");
    }
    text += QStringLiteral(" DE ") + ownCallsign;
    return text;
}

} // namespace MessageDrafter
} // namespace Contestprogramm
