#pragma once

#include <QMap>
#include <QString>

namespace Contestprogramm {

class ContestDefinition;

// "Enter Sends Message", the N1MM+/DXLog.net keying flow: one key, and
// what it does depends on where the QSO stands --
//   Run   empty call              -> CQ
//         call, exchange missing  -> "<call> <my exchange>", cursor to the exchange
//         call + exchange         -> "TU", log
//   S&P   empty call              -> my call (answer a CQ)
//         call, exchange missing  -> my call, cursor to the exchange
//         call + exchange         -> my exchange, log
// A pure decision over the entry state; MainWindow keys the text via
// rigctld (CW only -- there is no voice keyer in this program) and
// logs when the plan says so. Off unless ContestSettings::esmEnabled.
struct EsmTemplates {
    QString cq = QStringLiteral("CQ TEST {mycall} {mycall} TEST");
    QString runExchange = QStringLiteral("{call} {exchange}");
    QString tu = QStringLiteral("TU {mycall}");
    QString myCall = QStringLiteral("{mycall}");
    QString spExchange = QStringLiteral("{exchange}");
};

struct EsmPlan {
    QString templateText; // empty: nothing to key
    bool logQso = false;
    bool focusExchange = false;
};

enum class EsmMode { Run, SearchAndPounce };

EsmPlan planEnter(EsmMode mode, bool callsignPresent, bool exchangeComplete, const EsmTemplates& templates);

// Every received field the contest asks for, except the RST the
// program defaults itself, has a value.
bool exchangeComplete(const ContestDefinition& definition, const QMap<QString, QString>& received);

// {call} -> the other station, {exchange} -> what we send, {mycall} ->
// own callsign. Literal, case-sensitive, like CwMacroPanel::substitute.
QString substituteEsm(const QString& templateText, const QString& callsign, const QString& sentExchange,
                      const QString& ownCallsign);

} // namespace Contestprogramm
