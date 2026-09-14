#pragma once

#include <QString>

namespace Contestprogramm {

// Formats the ON4KST chat text SuggestionPanel offers for the operator
// to review and, on explicit confirmation, actually send (see the
// plan's "Betriebsassistent" section) -- pure string assembly, no
// On4kstClient/network dependency, so it stays directly unit-testable
// and, per the plan's own "harte Regel, strukturell erzwungen"
// requirement, cannot itself send anything: only SuggestionPanel's
// confirm button (via On4kstClient::sendChatMessage()/sendCqCall(),
// wired in MainWindow) ever reaches the network.
//
// Two separate functions, not one with a mode flag, per the plan's own
// funkbetrieblich-correct distinction: "CQ" is exclusively the general,
// ungerichtete Ruf -- a message TO a specific, already-known/spotted
// station must never carry a "CQ" prefix.
namespace MessageDrafter {

// A directed call to a specific, already-spotted station, e.g.
// "SP9XYZ DE OE5SOS 047 JN67VV" -- `exchange` is the already-composed
// sent-exchange text (MainWindow::currentSentExchangeText(), serial +
// own grid etc., in the active ContestDefinition's own field order);
// omitted from the result when empty (e.g. no contest active yet)
// rather than leaving a dangling trailing space.
QString draftDirectedCall(const QString& targetCallsign, const QString& ownCallsign, const QString& exchange);

// A general, ungerichteter Ruf -- "CQ DE OE5SOS" or, with
// `includeContestTag`, "CQ CONTEST DE OE5SOS" (both forms are
// funkbetrieblich standard; which one an operator prefers is a
// SuggestionPanel-level choice, not baked in here).
QString draftCqCall(const QString& ownCallsign, bool includeContestTag = false);

} // namespace MessageDrafter

} // namespace Contestprogramm
