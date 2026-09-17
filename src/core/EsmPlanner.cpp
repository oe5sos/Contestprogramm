#include "core/EsmPlanner.h"

#include "data/ContestDefinition.h"

namespace Contestprogramm {

EsmPlan planEnter(EsmMode mode, bool callsignPresent, bool exchangeComplete, const EsmTemplates& templates)
{
    EsmPlan plan;
    if (mode == EsmMode::Run) {
        if (!callsignPresent) {
            plan.templateText = templates.cq;
        } else if (!exchangeComplete) {
            plan.templateText = templates.runExchange;
            plan.focusExchange = true;
        } else {
            plan.templateText = templates.tu;
            plan.logQso = true;
        }
        return plan;
    }
    if (!callsignPresent) {
        plan.templateText = templates.myCall;
    } else if (!exchangeComplete) {
        plan.templateText = templates.myCall;
        plan.focusExchange = true;
    } else {
        plan.templateText = templates.spExchange;
        plan.logQso = true;
    }
    return plan;
}

bool exchangeComplete(const ContestDefinition& definition, const QMap<QString, QString>& received)
{
    for (const ContestDefinition::ExchangeField& field : definition.exchangeFields()) {
        if (field.type == QStringLiteral("rst")) {
            continue;
        }
        if (received.value(field.key).trimmed().isEmpty()) {
            return false;
        }
    }
    return true;
}

QString substituteEsm(const QString& templateText, const QString& callsign, const QString& sentExchange,
                      const QString& ownCallsign)
{
    QString text = templateText;
    text.replace(QStringLiteral("{call}"), callsign.trimmed().toUpper());
    text.replace(QStringLiteral("{exchange}"), sentExchange.trimmed());
    text.replace(QStringLiteral("{mycall}"), ownCallsign.trimmed().toUpper());
    return text.simplified();
}

} // namespace Contestprogramm
