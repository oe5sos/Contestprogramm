#pragma once

#include "core/ColorTheme.h"
#include "core/RotorDialStyle.h"

#include <QString>
#include <QStringList>

namespace Contestprogramm {

class ContestDatabase;

// Operator/station settings, persisted in ContestDatabase's `settings`
// key/value table (see the plan's SQLite schema comment listing the
// expected keys). TCI host/port and ON4KST credentials are carried here
// as plain fields already -- per the schema -- so the settings table
// shape does not need to change when those network features land in a
// later phase; only SettingsDialog's UI grows rows then.
struct ContestSettings {
    // Workflow toggle, per the plan's "Run- vs. S&P-Modus" nachziehen
    // item: "kein eigener Engine-Unterbau, eher wie sich Eingabezeile/
    // Suggestion-Panel verhalten." The one concrete behavioral
    // difference wired up in this pass: MainWindow's CAT-autofill
    // signal handlers skip overwriting the current band/mode
    // while Run is active AND the entry row has unsent content (see
    // UnifiedLogWidget::hasUnsentContent) -- a background VFO tick must
    // not stomp on an in-progress exchange mid-pileup. SearchAndPounce
    // keeps today's behavior (always autofill from CAT) and stays the
    // default so existing settings/tests are unaffected.
    enum class OperatingMode { Run, SearchAndPounce };

    QString ownCallsign;
    QString ownGrid;
    double ownElevationM = 0.0;
    double antennaHeightM = 0.0;
    QString tciHost = QStringLiteral("127.0.0.1");
    int tciPort = 50001;
    // Hamlib rigctld -- the contest station's actual CAT radio path
    // (Kenwood TS-590 / Elecraft K3), per the plan's "Funkgerät:
    // klassischer TRX statt zwingend Longpath/SDR" section. rigctld's
    // default port 4532 is a sibling to rotctld's 4533.
    QString rigctldHost = QStringLiteral("127.0.0.1");
    int rigctldPort = 4532;
    QString on4kstUsername;
    QString on4kstPassword;
    // Classic telnet DX cluster -- a second, independent spot source
    // alongside ON4KST (core/DxClusterClient.h), per the plan's module
    // table. No fixed public host the way ON4KST has one: DX clusters
    // are numerous and operator-chosen, so host/port are plain
    // settings fields like the CAT rig's, not a hardcoded constant.
    QString clusterHost;
    int clusterPort = 7300; // DxClusterClient's own default (see its header)
    double radiusKm = 300.0;
    QString activeContestId;
    OperatingMode operatingMode = OperatingMode::SearchAndPounce;

    // Which rotor slot (if any) commands a given contest band -- an
    // explicit per-band assignment rather than inferring it from a
    // label string match, since a rotor's label is now free text (see
    // rotor1Label/rotor2Label below) and can no longer double as a band
    // identity. None means no rotor is assigned to that band (e.g. both
    // slots disabled, or the operator deliberately left it unassigned).
    enum class RotorSlot { None, Slot1, Slot2 };

    // Two independent rotor "slots" (Kern-Welle 2 / plan Phase 3: "Zwei
    // getrennte Rotoren, nicht einer" -- originally two fixed masts, one
    // for 2m, one for 70cm). Generalized here per the operator's actual
    // request: each slot is independently optional (rotor1Enabled/
    // rotor2Enabled -- disabling a slot removes its RotorWidget from the
    // UI entirely, not just leaves it showing disconnected, see
    // MainWindow::applyRotorWidgetSettings) and freely relabelable
    // (rotor1Label/rotor2Label are plain operator text, not a fixed band
    // identity) -- so the same two slots can be repurposed for a
    // single-rotor station, or a completely different band setup (e.g.
    // renamed to "Kurzwelle"/"HF"). Defaults reproduce the original
    // fixed two-mast VHF/UHF behavior unchanged: both slots enabled,
    // labelled "2m"/"70cm", on two distinct loopback ports so both can
    // run on the same operating machine without a port clash; rotctld
    // itself is operator-started, same posture as rigctldHost/-Port
    // above -- this program only ever connects as a client.
    bool rotor1Enabled = true;
    QString rotor1Label = QStringLiteral("2m");
    QString rotor1Host = QStringLiteral("127.0.0.1");
    int rotor1Port = 4533; // rotctld's default
    bool rotor1SecondAntennaEnabled = false;
    double rotor1SecondAntennaOffsetDeg = 0.0;

    // Hamlib model/device/baud for rotctld itself, one triple per slot --
    // ported alongside core/RotctldProcess.h + core/HamlibInstaller.h
    // (both architecturally identical to Longpath/NereusSDR's own,
    // which has only one rotor and so only one such triple; this
    // project generalizes to two, matching every other rotor1*/rotor2*
    // pair above). rotor1Host/rotor1Port above stay what RotctldClient
    // actually connects to (rotctld's own TCP control port); these three
    // are only consulted when this program starts rotctld itself for
    // that slot (RotctldProcess::start()), so an operator who runs
    // rotctld their own way -- unaffected either way, matching that
    // class's own class-comment stance -- never has to touch them.
    // Default 601 = Yaesu GS-232A (core/RotorModels.h's own first/
    // default entry): the operator confirmed 2026-09-11 the station's
    // rotor will be Yaesu. rotor1Device/rotor2Device default to empty
    // (RotctldProcess::arguments() then omits -r entirely and lets
    // Hamlib pick its own default device, same as an unset network
    // model) -- there is no sound default serial path across
    // operating systems, so guessing one would be worse than leaving it
    // blank until the operator names their actual port (e.g.
    // /dev/tty.usbserial-XXXX on macOS, COM3 on Windows). 9600 baud
    // matches commonRotorBauds()'s own middle-of-the-road default and
    // is what most Hamlib-supported controllers ship configured for.
    int rotor1HamlibModel = 601;
    QString rotor1Device;
    int rotor1Baud = 9600;

    bool rotor2Enabled = true;
    QString rotor2Label = QStringLiteral("70cm");
    QString rotor2Host = QStringLiteral("127.0.0.1");
    int rotor2Port = 4534; // a second, distinct local rotctld instance
    bool rotor2SecondAntennaEnabled = false;
    double rotor2SecondAntennaOffsetDeg = 0.0;

    // See rotor1HamlibModel/rotor1Device/rotor1Baud above -- same
    // fields, slot 2.
    int rotor2HamlibModel = 601;
    QString rotor2Device;
    int rotor2Baud = 9600;

    // Explicit band -> rotor-slot assignment, one entry per contest band
    // (144/432/1296) -- extends the pattern the original band1296RotorRef
    // used to solve "which mast handles 23cm" to every band, since a
    // slot's label can no longer be pattern-matched as a band identity.
    // Defaults reproduce the original fixed wiring: 144 -> slot 1 ("2m"),
    // 432 -> slot 2 ("70cm"), 1296 shares slot 1 (matching the old
    // band1296RotorRef default of "2m").
    RotorSlot band144RotorSlot = RotorSlot::Slot1;
    RotorSlot band432RotorSlot = RotorSlot::Slot2;
    RotorSlot band1296RotorSlot = RotorSlot::Slot1;

    // Which of RotorWidget's four paint styles both rotor compasses
    // use (core/RotorDialStyle.h) -- one operator-wide setting, not
    // per-slot like rotor1Enabled/rotor2Enabled above, since both
    // compasses should always look the same (see SettingsDialog's
    // rotor-display combo and MainWindow::applyRotorWidgetSettings(),
    // which applies this to both m_rotor1Widget/m_rotor2Widget
    // together). Default FullCompass matches RotorWidget's only
    // rendering before this setting existed, per the operator's own
    // request for additional selectable styles ("vielleicht mehrere
    // Möglichkeiten, dann kann individuell ausgewählt werden").
    RotorDialStyle rotorDialStyle = RotorDialStyle::FullCompass;

    // Which of the five selectable colour palettes the whole app uses
    // (core/ColorTheme.h) -- operator, 2026-09-12: "gelber Hintergrund,
    // schwarze Schrift, ev auch was in blau, hell und dunkel... DIE 4
    // BITTE", after a live side-by-side mockup review. Applied by
    // MainWindow::applyColorTheme() (ui/StyleKit.h's Style::
    // setActiveTheme() plus the live-repaint half), not AppController --
    // AppController is app-layer and never includes ui/, same reasoning
    // core/RotorDialStyle.h/core/ColorTheme.h document for keeping these
    // enums out of ui/ headers in the first place. Default Bernstein
    // matches the app's original, only palette.
    ColorTheme colorTheme = ColorTheme::Bernstein;

    // Column view mode for the Log panel's own feed table (ui/
    // UnifiedLogWidget.h) -- operator follow-up to seeing a screenshot
    // of DXLog.net's real "Contest recorder" log window (2026-09-11):
    // "Kompakt" is today's existing column set, unchanged, and stays
    // the default; "DxLogFullColumns" adds a QSO serial/sequence-number
    // column matching DXLog's own numbering (real data already tracked
    // per QSO -- QsoRecord::serialSent, see
    // ContestDatabase::nextSerialForContest() -- not an invented
    // column; DXLog's "Pts"/"A"/"Stn" columns are deliberately NOT
    // reproduced, see this task's report for why). Nested here, like
    // OperatingMode above, rather than in its own core/ header the way
    // RotorDialStyle is: UnifiedLogWidget.h already depends on this
    // header directly for OperatingMode (its DXLog-style status line,
    // same task), so there is no cross-layer-include reason to split
    // this one out too.
    enum class LogViewMode { Compact, DxLogFullColumns };

    LogViewMode logViewMode = LogViewMode::Compact;

    // Where the Log panel's live entry row (Call/RST/Nr./Grid/Ges.) sits
    // relative to the feed table below it -- operator follow-up
    // (2026-09-11), after repeatedly pointing at the exact same spot in
    // a screenshot of the running app ("zeile unter 04:31" / "genau
    // unter OE5SOS", the table's own last visible row): Top is today's
    // unchanged layout (entry row above the table); Bottom moves the
    // SAME entry row -- unchanged design, per the operator's own
    // "gleiches design für das schreiben des nächsten logs" -- to sit
    // directly after the table's last row, continuing it visually with
    // no gap, like DXLog.net/Excel's "type in the next line" feel. Never
    // reorders the table's own row order (chronologically ascending) --
    // only the entry row's position changed, not the table's sort. Selectable
    // any time via the Log panel's own ⚙ (see
    // MainWindow::showLogViewOptionsPopup()), same convention as
    // logViewMode above.
    enum class LogEntryRowPosition { Top, Bottom };

    // Default changed to Bottom (2026-09-11, same follow-up as the
    // comment above): once the feed table's own order became
    // chronologically ascending to match DXLog.net exactly
    // ("fortlaufend"), Top stopped making sense as a default -- an
    // entry row sitting above QSO#1 (the OLDEST row) no longer reads as
    // "the next line to type," only Bottom (directly after the newest
    // row) does. Top is still selectable via the Log panel's own ⚙ for
    // an operator who prefers it, just no longer the initial value.
    LogEntryRowPosition logEntryRowPosition = LogEntryRowPosition::Bottom;

    // CW F-key macro templates (Kern-Welle 2), sent via
    // RigctldClient::sendMorse() after CwMacroPanel substitutes
    // {call}/{exchange}. A simple macro panel, not a full keyer -- see
    // ui/CwMacroPanel.h.
    QStringList cwMacros = {
        QStringLiteral("{call}"),
        QStringLiteral("{call} 5NN {exchange}"),
        QStringLiteral("{exchange}"),
        QStringLiteral("TU"),
        QStringLiteral("?"),
    };

    // CW F-key macro row (ui/CwMacroPanel.h) visibility, per the
    // operator's own words ("FM und 144 benötigt niemand" session --
    // separate request: the CW row should stay out of the way unless
    // actually wanted). Default false/hidden; a checkbox in MainWindow's
    // filter row (matching the existing "Chat/Cluster: Rohdaten"
    // checkbox's visual family) toggles and persists this. Purely a
    // display toggle -- RigctldClient::sendMorse() and the macro
    // templates themselves are unaffected either way.
    bool cwMacroPanelVisible = false;
    // Enter Sends Message (core/EsmPlanner.h) -- off by default, the
    // "ESM" checkbox in the filter row flips it live. The five texts
    // are what Enter keys in each state; {call}/{exchange}/{mycall}.
    bool esmEnabled = false;
    QString esmCq = QStringLiteral("CQ TEST {mycall} {mycall} TEST");
    QString esmRunExchange = QStringLiteral("{call} {exchange}");
    QString esmTu = QStringLiteral("TU {mycall}");
    QString esmMyCall = QStringLiteral("{mycall}");
    QString esmSpExchange = QStringLiteral("{exchange}");

    // Contest-end timestamp for the top-bar countdown (ui/
    // UtcClockWidget.h), operator-set in SettingsDialog -- there is no
    // way to derive this from a ContestDefinition (a 24h VHF/UHF contest
    // has a known duration but not a fixed calendar start/end baked into
    // the JSON). ISO-8601 UTC string; empty means "unset", in which case
    // the countdown shows Style::unknownDash() rather than a fake
    // 00:00:00 or a wrong guess -- HAUSSTIL rule 7.
    QString contestEndUtc;

    // Countdown visibility -- independently toggleable from the UTC
    // clock itself, per the operator's explicit request (the plan's own
    // UI section already calls for this: "Countdown ... separat
    // ein-/ausblendbar über eine Einstellung (nicht fix sichtbar)").
    // Default false/hidden, matching that "nicht fix sichtbar" and the
    // same default-hidden posture as cwMacroPanelVisible above -- it
    // only becomes useful once contestEndUtc is actually set, so it does
    // not default to shown-but-empty.
    bool countdownVisible = false;

    // Callbook provider for the external (network) tier of callsign ->
    // locator autofill (core/CallsignLocatorLookup.h) -- the third and
    // last lookup tier, after the operator's own log and the imported/
    // cached imported_locators table. None (the default) means the
    // external tier is off entirely: this dials out to a third party
    // using the operator's own account, so it must never activate itself
    // without an explicit provider choice, same posture as
    // On4kstClient/DxClusterClient only dialing out once real settings
    // are present. QRZ.com and HamQTH are the two contest-logger-
    // standard XML callbook APIs (N1MM+/DXLog.net both support this same
    // pair).
    enum class CallbookProvider { None, Qrz, HamQth };

    CallbookProvider callbookProvider = CallbookProvider::None;
    QString callbookUsername;
    QString callbookPassword;

    // core/BroadcastPublisher.h -- the "UDP-Contact-Broadcast-Standard"
    // item from the plan: N1MM Logger+'s own documented external-UDP
    // XML format (n1mmwp.hamdocs.com/appendices/external-udp-
    // broadcasts/), also implemented by DXLog.net under its own
    // "N1MM QSO format" broadcast option. Port 12060 is N1MM's own
    // documented default for the Contact/Radio packets this class
    // sends. Disabled by default -- this puts data on the local
    // network, so it must not switch itself on for an operator who
    // never asked for it, same reasoning as On4kstClient/DxClusterClient
    // above only dialing out once real settings are present.
    bool broadcastEnabled = false;
    QString broadcastHost = QStringLiteral("127.0.0.1");
    int broadcastPort = 12060;

    // Reads every key from `database`'s settings table, falling back to
    // this struct's current field values for any key that is not
    // present yet (e.g. first run).
    void loadFrom(const ContestDatabase& database);

    // Writes every field to `database`'s settings table.
    void saveTo(ContestDatabase& database) const;
};

} // namespace Contestprogramm
