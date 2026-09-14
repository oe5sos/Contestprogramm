#pragma once

#include "core/SpotCandidate.h"

#include <QString>

namespace Contestprogramm {

// Turns ON4KST telnet lines into SpotCandidate values. Two paths, per
// the plan's "ON4KST-Telnet-Client" section:
//   - DL| lines are structured/pipe-delimited (no regex-guessing needed,
//     unlike DxClusterClient's free-text "DX de" parsing).
//   - CH|/CR| chat lines are free text; a callsign/grid are extracted
//     best-effort and the grid may legitimately be absent.
class SpotParser {
public:
    // "DL|unix_time|dx_utc|spotter|qrg|dx|info|spotter_locator|dx_locator|"
    // -> a SpotCandidate for the spotted (dx) station. Returns false if
    // `line` is not a well-formed DL| line (wrong tag, too few fields,
    // or an empty dx callsign).
    //
    // `qrg`'s unit is not pinned down any further in the wtKST doc
    // excerpt the plan quotes -- assumed kHz here, matching the
    // DX-cluster convention (see the freqKhz-to-MHz division in
    // NereusSDR's DxClusterClient), pending the plan's own "Offene
    // Punkte" live-verification step against the real server.
    static bool parseDxSpotLine(const QString& line, SpotCandidate& candidateOut);

    // "CH|chat_id|date|callsign|firstname|destination|msg|highlight|"
    // (or the login-batch form "CR|...", same field shape) -> a
    // SpotCandidate carrying callsign + the raw line always; `grid` is
    // filled only when `msg` contains a bare 4- or 6-character locator
    // token. An absent grid is not a parse failure -- this returns true
    // whenever the line has the CH|/CR| shape and a non-empty callsign,
    // grid found or not.
    static bool parseChatLine(const QString& line, SpotCandidate& candidateOut);
};

} // namespace Contestprogramm
