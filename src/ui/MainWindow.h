#pragma once

#include "core/BandmapModel.h"
#include "core/SkedList.h"
#include "core/CheckPartialIndex.h"

#include <QMainWindow>
#include <QSet>
#include <QString>

class QCloseEvent;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QMoveEvent;
class QPushButton;
class QResizeEvent;
class QTimer;

namespace Contestprogramm {

class AppController;
class ContestDefinition;
class BandmapWidget;
class OnlineScoreboard;
class CheckPartialWidget;
class ContestRulesEditor;
class CwMacroPanel;
class LayoutProfileManager;
class LogTableModel;
class MapWidget;
class MultiplierWindow;
class PanelContainerWidget;
class PanelHeaderBar;
class PanelLayoutManager;
class ProfileRail;
class RateMeterWidget;
class LogCheckWindow;
class ReadinessWindow;
class ClockCheck;
class StatisticsWindow;
class RotctldClient;
class RotorWidget;
class SkedPanel;
class SuggestionPanel;
class UnifiedLogWidget;
class UtcClockWidget;

// Wires UnifiedLogWidget (entry row + log history + spot/chat
// candidates, one merged panel -- see ui/UnifiedLogWidget.h) +
// RateMeterWidget + MultiplierWindow to AppController, which owns the
// database, dupe checker, multiplier tracker, RigctldClient,
// On4kstClient, DxClusterClient, GeoFilter and the two ChatFeedModel
// instances (see app/AppController.h). MainWindow itself owns the UI
// widgets and the UI-facing wiring: CAT-radio autofill into
// UnifiedLogWidget (subject to the Run-mode guard below), click-to-fill
// from a spot/chat candidate row, grid-autofill lookups, history-row
// hand-corrections/invalid-toggle (see UnifiedLogWidget's
// historyCallsignEditRequested/historyExchangeRcvdEditRequested/
// historyInvalidToggleRequested), and the status bar labels. Band has
// no dedicated UI control any more (Martin: "band ... bitte weg") --
// MainWindow tracks it internally (m_currentBand, CAT-driven,
// defaulting to the active contest's first band) purely for the data
// that still needs it (dupe scope, Cabrillo/ADIF export, rotor band
// routing) and surfaces it only in the status bar's CAT badge. Mode
// gets the identical later follow-up treatment ("mode wird nicht
// benötigt"): m_currentMode, CAT-driven, defaulting to "SSB" until CAT
// says otherwise, surfaced in that same CAT badge alongside band.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(AppController& appController, QWidget* parent = nullptr);
    ~MainWindow() override;

    // The restore itself, without the dialogs (public for the tests):
    // safety backup, close, replace the file, restartRequested().
    // False = nothing replaced (the safety backup failed) or the copy
    // failed; restartRequested() is emitted whenever the connection was
    // closed.
    bool performRestore(const QString& backupPath, QString* errorOut = nullptr);

signals:
    // Datei > Sicherung wiederherstellen has put a backup in place of
    // the database: main() releases the instance lock, starts a fresh
    // process on it and quits this one.
    void restartRequested();

private slots:
    void handleLogRequested();
    void recheckDupeIndicator();
    void openSettingsDialog();
    void openContestRulesEditor();
    void openContestPicker();
    void exportCabrillo();
    void exportAdif();
    void exportEdi();
    void loadScpFile();
    void importOldLogs();
    void archiveActiveContest();
    void refreshCheckPartial();
    void openMultiplierWindow();
    void openStatisticsWindow();
    // Datei > Log prüfen... (data/LogCheck.h): the window, its refresh,
    // and the jump from one of its rows to the QSO in the log.
    void openLogCheckWindow();
    void refreshLogCheck();
    // Datei > Startcheck (bereit?)... (data/ReadinessCheck.h): the
    // window and the snapshot it is refreshed from.
    void openReadinessWindow();
    void refreshReadiness();
    void jumpToQso(int qsoId);
    // After a history-row correction: re-derive every dupe flag of the
    // active contest (data/DupeRescore.h), write the ones that changed,
    // patch their rows in place. Returns how many changed.
    int rescoreDupes();
    void handleCandidateActivated(const QString& callsign, const QString& grid, qint64 freqHz);
    void handleCallsignLookupRequested(const QString& callsign);
    // Live km/bearing preview for the in-progress entry -- recomputes
    // from ContestSettings::ownGrid + whatever is currently typed in the
    // active contest's grid6-typed exchange sub-field (see
    // UnifiedLogWidget::receivedGridChanged) and pushes the result back
    // via UnifiedLogWidget::setEntryDistanceBearing().
    void handleReceivedGridChanged(const QString& grid);
    // Tier 3 (QRZ/HamQTH) result, arriving asynchronously well after
    // handleCallsignLookupRequested() returned -- see core/
    // CallsignLocatorLookup.h. Re-checks the entry row's callsign field
    // still matches `callsign` before applying anything (the operator
    // may have changed or cleared it while the HTTP round trip was in
    // flight), the same staleness guard UnifiedLogWidget's own debounce
    // timer already applies to itself.
    void handleExternalCallsignLookupFinished(const QString& callsign, bool found, const QString& grid);
    // History-row hand-corrections and the invalid-toggle (see
    // UnifiedLogWidget's historyCallsignEditRequested/
    // historyExchangeRcvdEditRequested/historyInvalidToggleRequested
    // doc comments for the DXLog.net-equivalent scope: Call/RST/Serial/
    // Grid are correctable in place, there is deliberately no delete --
    // a QSO is marked invalid instead). Each patches ContestDatabase
    // then LogTableModel::updateRecord() for just the affected row,
    // rather than a full refreshLogTable() reset -- see
    // LogTableModel::updateRecord()'s own doc comment for why a reset
    // is specifically the wrong tool here (a QTableView cell editor is
    // still mid-commit on the very edit that triggered this).
    void handleHistoryCallsignEditRequested(int qsoId, const QString& newCallsign);
    void handleHistoryExchangeRcvdEditRequested(int qsoId, const QString& newText);
    void handleHistoryInvalidToggleRequested(int qsoId);
    void handleHistoryTimeEditRequested(int qsoId, const QString& newText);
    void backupLogNow();
    void restoreBackup();
    void adjustCwSpeed(int deltaWpm);
    void openEsmTemplatesDialog();
    void openScoreboardDialog();
    void postScoreNow();
    void toggleOperatingMode();
    void updateStatusBar();
    // Kern-Welle 2: rotor + CW wiring. Creates/destroys each slot's
    // RotorWidget to match ContestSettings::rotor1Enabled/rotor2Enabled
    // (see the class comment) and otherwise keeps an existing widget's
    // label/second-antenna/badge in sync -- called once from the
    // constructor and again after every SettingsDialog save. A private
    // slot (not just a plain method) purely so tests can drive it via
    // QMetaObject::invokeMethod without going through a modal
    // SettingsDialog::exec() -- it is never itself connected to a
    // signal.
    void applyRotorWidgetSettings();
    // The rotor dials' beam cones take the map's beamwidth preferences.
    void applyRotorBeamwidths();
    // Rebuilds MapWidget's worked/spotted station set from the current
    // contest's logged QSOs (ContestDatabase::qsosWithGrid) plus both
    // ChatFeedModels' currently-visible, not-yet-worked rows -- called
    // once from the constructor, again after every successful QSO log
    // and every SettingsDialog save (own grid/callsign can change
    // there), and connected to both ChatFeedModels' modelReset signal
    // so a newly arriving/newly filtered spot candidate reaches the map
    // live, the same "tap the existing model, do not invent a parallel
    // path" approach UnifiedLogWidget's own feed table uses for its
    // display. A private slot (not just a plain method) for the same reason
    // applyRotorWidgetSettings() is -- QMetaObject::invokeMethod-able
    // by tests without needing a live network/DB event.
    void refreshMapWidget();
    // Betriebsassistent (plan section of the same name): recomputes the
    // single best next-target suggestion across both ChatFeedModel
    // instances (core/assistant/NextTargetSuggester.h) and its drafted
    // ON4KST message (core/assistant/MessageDrafter.h), then pushes the
    // result into m_suggestionPanel -- called from the same reactive
    // points refreshMapWidget() already is (both feeds' modelReset, a
    // fresh QSO log, a history-row edit/invalidate, a Settings/contest
    // switch), since all of those can change which candidate is
    // reachable/still-needed/highest-scored. Skips overwriting the
    // panel while the entry row already has a callsign typed in (see
    // the plan's "kein konkurrierender Zielwechsel während eines
    // laufenden QSO") -- a private slot (not just a plain method) for
    // the same QMetaObject::invokeMethod-testability reason
    // refreshMapWidget() is.
    void refreshSuggestionPanel();
    // "Ruf senden" from m_suggestionPanel, AND (since 2026-09-12)
    // UnifiedLogWidget's own chat quick-send row (m_unifiedLog::
    // chatMessageSendRequested is connected to this same slot) -- the
    // one place that actually calls On4kstClient::sendChatMessage(),
    // per the plan's capability-separation requirement (see
    // SuggestionPanel's own class comment, which applies identically to
    // UnifiedLogWidget's chat row).
    void handleSuggestionSendRequested(const QString& text);
    // UnifiedLogWidget's "CQ" button (m_unifiedLog::cqDraftRequested) --
    // composes MessageDrafter::draftCqCall(settings.ownCallsign) and
    // hands it back via UnifiedLogWidget::setChatInputDraft(), rather
    // than sending directly: the operator still gets a visible draft to
    // review/edit before Enter actually sends it, same confirm-before-
    // send principle as everywhere else this program can reach
    // On4kstClient.
    void handleCqDraftRequested();
    // UnifiedLogWidget's Away/Zurück toggle (m_unifiedLog::
    // awayStateChanged) -- the one place that calls
    // On4kstClient::sendAway()/sendBack(), same capability-separation
    // reasoning as handleSuggestionSendRequested above.
    void handleAwayToggled(bool away);
    // Rebuilds UnifiedLogWidget's mode list + dynamic exchange-received
    // sub-fields, and resets m_currentBand to the new contest's first
    // band, from the currently active ContestDefinition (see
    // ContestSettings::activeContestId) -- called once from the
    // constructor, again whenever AppController::contestDefinitionsChanged
    // fires (a ContestRulesEditor save, see the constructor's connect()),
    // and again after every SettingsDialog/ContestPickerDialog contest
    // switch. A private slot (not just a plain method) for the same
    // reason applyRotorWidgetSettings() is -- QMetaObject::invokeMethod-
    // able by tests without needing a real modal SettingsDialog::exec()
    // or ContestPickerDialog::exec().
    void applyActiveContestDefinition();
    // Pulls a fresh 360-degree terrain sweep from
    // AppController::terrainDataManager() and pushes it into whichever
    // RotorWidget instance(s) currently exist (see RotorWidget::
    // setTerrainSectors()) -- called once at the end of
    // applyRotorWidgetSettings() (covers both initial setup and a
    // freshly (re)created RotorWidget after an enabled-flag toggle) and
    // connected to TerrainDataManager::classificationChanged so a
    // sector that resolves later (once its SRTM tile finishes loading)
    // reaches the compass ring live, without the operator needing to
    // reopen Settings. A private slot (not just a plain method) for the
    // same reason applyRotorWidgetSettings() is -- QMetaObject::
    // invokeMethod-able by tests without needing a real async tile
    // fetch to complete.
    void refreshTerrainSectors();
    // The terrain horizon around the own locator (core/terrain/
    // HorizonProfile.h) for the map's rim and skyline; computed once per
    // grid/elevation/antenna-height combination, then cached.
    QVector<double> horizonProfileForOwnStation();
    // Fires ~400ms after the last resizeEvent()/moveEvent(), so a live
    // window drag/resize doesn't hammer the settings table on every
    // pixel -- see saveWindowGeometry()'s doc comment for the full
    // persistence story (this is the crash-safety half; closeEvent()
    // is the clean-quit half).
    void persistWindowGeometryDebounced();
    // Keeps the Rotoren panel centered and, if the window has been
    // narrowed below the full three-column layout's own design width,
    // shrinks it to fit -- operator, 2026-09-14: "rotoren sollten sich
    // auch mittig zentrieren und dann auch ggf. kleiner werden, wenn das
    // fenster kleiner wird". See the .cpp definition for why this only
    // engages below kFullLayoutWidth rather than always recentering.
    void reflowRotorRowForCanvasWidth();
    // Only the rename action needs a modal prompt (the new name) --
    // activate/duplicate/remove act on the clicked badge's name alone.
    void handleProfileRenameRequested(const QString& name);

protected:
    // Window-geometry persistence (Martin: "der letzte stand sollte
    // immer gespeichert bleiben. ich muss immer wieder die fenster neu
    // verschieben") -- QMainWindow::saveGeometry()/restoreGeometry(),
    // base64-encoded into the same settings key/value store
    // PanelLayoutManager already uses for per-panel geometry (see
    // ContestDatabase::settingValue/setSettingValue). closeEvent() is
    // the guaranteed clean-quit save; resizeEvent()/moveEvent() also
    // schedule a debounced save (persistWindowGeometryDebounced()) so a
    // force-quit/crash mid-session still keeps a recent geometry, the
    // same "don't lose it to a crash" philosophy PanelLayoutManager
    // already applies to panel geometry.
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;

private:
    void refreshSentExchangePreview();
    void refreshLogTable();
    // Pushes ContestSettings::contestEndUtc/countdownVisible into
    // m_utcClockWidget -- called once from the constructor and again
    // after every SettingsDialog save (contestEndUtc lives there; the
    // countdown-visible checkbox in the filter row updates the widget
    // directly on toggle instead of going through this, see the
    // constructor).
    void applyClockSettings();
    // Re-derives MultiplierTracker's worked-multiplier sets for the
    // active contest, then both ChatFeedModels' cached worked/score
    // state (ChatFeedModel::refreshWorkedAndScores()) -- call after any
    // QSO log/edit/invalidate. Neither refreshes on its own: a QSO just
    // logged does not, by itself, touch MultiplierTracker at all (it
    // only ever ran on a contest switch/Settings save before this), and
    // even a fresh MultiplierTracker recompute would not reach a
    // candidate ChatFeedModel already cached the score for -- see
    // ChatFeedModel::refreshWorkedAndScores()'s own doc comment for the
    // concrete "already-worked station keeps scoring as new/needed"
    // failure this closes.
    void refreshMultiplierAndFeedScores();
    // Writes saveGeometry()'s current QByteArray (base64-encoded) to
    // the "MainWindowGeometry" settings key -- called from closeEvent()
    // immediately and from persistWindowGeometryDebounced() after the
    // resize/move settle timer fires.
    void saveWindowGeometry();
    const ContestDefinition* findContestDefinition(const QString& contestId) const;
    static QString contestModeForRigctldMode(const QString& rigMode);
    // "/CHAT value" room per band -- "GHZ" for 1296 (ON4KST's
    // microwave room), "144" for 144/432 (the shared VHF/UHF room this
    // program logs into by default, see On4kstClient::kChatIdVhfUhf).
    // Empty for a band this program doesn't operate/an as-yet-unknown
    // band -- syncOn4kstRoomForCurrentBand() below then leaves the
    // current room alone rather than switching to a meaningless value.
    static QString on4kstRoomValueForBand(const QString& band);
    // Automatic 23cm room-switch (plan's chat-capability item 4,
    // 2026-09-12): m_currentBand is already fully CAT-driven with no
    // manual UI control (see the class comment above) -- reusing that
    // same live signal to switch ON4KST rooms automatically, rather
    // than adding a manual toggle, keeps this consistent with how
    // Band/Mode already work and needs zero operator action. Called
    // from the rigctldClient frequencyChanged handler (band may have
    // just changed) and from On4kstClient::loggedIn (the login itself
    // always starts in the default 144/432 room, per
    // On4kstClient::kChatIdVhfUhf -- if the radio is already parked on
    // 23cm at that moment, the room must be corrected right away, not
    // only on the next frequency tick). Only calls switchRoom() when
    // the target room actually differs from m_currentOn4kstRoom AND the
    // client is logged in (switching before login would transmit
    // "/CHAT ..." ahead of the LOGIN handshake).
    void syncOn4kstRoomForCurrentBand();

    // Creates `*widget` (inserted into m_rotorLayout at `insertIndex`,
    // wired to `client`'s azimuthChanged/stateChanged) if `enabled` and
    // it does not exist yet; updates its label in place if it already
    // exists; tears it down (removed from the layout, hidden, then
    // deleteLater()d -- an actually-disappears removal, not merely a
    // disconnected-looking widget left sitting in the row) if `enabled`
    // is now false.
    void applyRotorSlot(bool enabled, const QString& label, RotctldClient& client, RotorWidget*& widget, int insertIndex);
    // Commands whichever rotor `freqHz` maps to (if any, and if it is
    // connected) to the candidate's bearing, and updates that rotor's
    // RotorWidget target display -- see AppController::activeRotorForBand
    // and BeamHeading::plan. A no-op when the band/bearing cannot be
    // determined (freqHz == 0, an unrecognized band, or an invalid grid),
    // or when the mapped rotor is disabled/not connected -- this never
    // guesses.
    void commandRotorForCandidate(const QString& callsign, const QString& grid, qint64 freqHz);
    RotorWidget* rotorWidgetForClient(RotctldClient* client) const;
    // Pushes a connected/azimuth pair into m_mapWidget's matching heading
    // slot (see MapWidget::setRotor1Heading()/setRotor2Heading()) --
    // `&client == &m_appController.rotor1Client()` picks the slot, same
    // comparison rotorWidgetForClient() already uses. `connected` and
    // `azimuthDeg` are both passed in rather than re-read from `client`:
    // `connected` so the disabled-slot path in applyRotorSlot() can call
    // this with `false` to clear a heading whose RotctldClient may still
    // itself be connected (rotor slot off, link not torn down); `azimuthDeg`
    // so a call driven by RotorWidget::azimuthDegChanged() (which also
    // fires from a simulated, hardware-free turn -- see that signal's
    // own comment) mirrors the WIDGET's own heading, not client.azimuthDeg(),
    // which a simulated turn never touches at all.
    void pushRotorHeadingToMap(RotctldClient& client, bool connected, double azimuthDeg, const QString& label);
    // The exchange text that would be sent right now (next serial + own
    // grid), for CwMacroPanel's "{exchange}" placeholder -- same
    // composition handleLogRequested() uses for exchangeSent.
    QString currentSentExchangeText() const;

    // Opens the Log panel's own ⚙ view-mode menu (Kompakt / DXLog-
    // Vollspalten -- see ui/UnifiedLogWidget.h's setViewMode() doc
    // comment) -- connected to m_logHeaderBar's PanelHeaderBar::
    // optionsRequested. Same heap-allocated + WA_DeleteOnClose +
    // non-blocking popup() QMenu pattern RotorWidget::showOptionsPopup()
    // already established for this exact "⚙ opens a small view-choice
    // menu" affordance; not a private slot (like that method) since it
    // is only ever reached via the signal/function-pointer connect()
    // below, never QMetaObject::invokeMethod.
    void showLogViewOptionsPopup();

    AppController& m_appController;

    UnifiedLogWidget* m_unifiedLog = nullptr;
    LogTableModel* m_logModel = nullptr;
    // CAT-driven, no dedicated UI control -- see the class comment above.
    QString m_currentBand;
    // CAT-driven, no dedicated UI control -- see the class comment
    // above. Defaults to "SSB" (applyActiveContestDefinition()) the
    // same way m_currentBand defaults to the active contest's first
    // band -- a sensible starting value before CAT ever reports a mode.
    QString m_currentMode;
    // The ON4KST room this program last actually switched into -- see
    // syncOn4kstRoomForCurrentBand()'s own doc comment. Empty until the
    // first sync, so that call's "differs from current" check does not
    // skip the very first (post-login) switch.
    QString m_currentOn4kstRoom;
    RateMeterWidget* m_rateMeterWidget = nullptr;
    // Check Partial (core/CheckPartialIndex.h + ui/CheckPartialWidget.h):
    // the index is refilled by reloadCheckPartialSources() on every
    // contest switch and logged QSO, the widget is refreshed from
    // the entry row's formChanged.
    CheckPartialIndex m_checkPartialIndex;
    CheckPartialWidget* m_checkPartialWidget = nullptr;
    void reloadCheckPartialSources();
    bool m_scpLoaded = false;
    QVector<double> m_horizonProfile;
    QString m_horizonProfileKey;
    // Bandmap (core/BandmapModel.h + ui/BandmapWidget.h): fed by both
    // spot feeds, redrawn on every rig frequency report, logged QSO and
    // a 15 s timer for the age-out.
    BandmapModel m_bandmapModel;
    BandmapWidget* m_bandmapWidget = nullptr;
    void refreshBandmap();
    // Online scoreboard (core/OnlineScoreboard.h): config follows the
    // settings/contest, records follow the log.
    OnlineScoreboard* m_scoreboard = nullptr;
    void refreshScoreboard();
    // Skeds (core/SkedList.h + ui/SkedPanel.h): kept in the database,
    // reloaded per contest, statuses refreshed every 15 s and after each
    // logged QSO; a suggestion arrives from the ON4KST chat.
    SkedPanel* m_skedPanel = nullptr;
    QVector<Sked> m_skeds;
    QSet<int> m_skedAlarmed;
    void reloadSkeds();
    void refreshSkeds();
    void addSkedFromEntry(const QString& callsign, const QString& grid, const QString& qrgText, const QString& timeText);
    void activateSked(int skedId);
    MultiplierWindow* m_multiplierWindow = nullptr;
    StatisticsWindow* m_statisticsWindow = nullptr;
    LogCheckWindow* m_logCheckWindow = nullptr;
    ReadinessWindow* m_readinessWindow = nullptr;
    ClockCheck* m_clockCheck = nullptr; // the Startcheck's clock round, created with the window
    // The last failure of a rotctld this program started, per slot --
    // shown by the Startcheck until the rotor connects.
    QString m_rotctldError1;
    QString m_rotctldError2;
    QWidget* m_rotorRow = nullptr;
    QHBoxLayout* m_rotorLayout = nullptr;
    RotorWidget* m_rotor1Widget = nullptr;
    RotorWidget* m_rotor2Widget = nullptr;
    MapWidget* m_mapWidget = nullptr;
    // The rotorrow panel's own container -- kept (unlike most other
    // panels, whose container is looked up via m_panelLayoutManager->
    // panel(id) on the rare occasion it's needed) because
    // reflowRotorRowForCanvasWidth() calls trySetGeometry() on it from
    // every resizeEvent(), so a map lookup there would run needlessly
    // often.
    PanelContainerWidget* m_rotorRowContainer = nullptr;
    SuggestionPanel* m_suggestionPanel = nullptr;
    CwMacroPanel* m_cwMacroPanel = nullptr;
    // The dockable-panel wrapper around the CW macro row's content ("CW:"
    // label + m_cwMacroPanel) -- objectName is "cwMacroRow" (set via
    // PanelLayoutManager::registerPanel()'s id), the same test-hook name
    // the pre-docking bare m_cwRow QWidget already carried
    // (findChild<QWidget*>("cwMacroRow")->isHidden() in
    // test_cw_macro_visibility.cpp). Its own setVisible() is now what
    // the "CW-Makros anzeigen" checkbox toggles (see the constructor),
    // instead of a separate inner row widget -- the container itself
    // stays part of the persisted dockable layout either way, only its
    // visibility changes.
    PanelContainerWidget* m_cwMacroPanelContainer = nullptr;
    // The merged Log panel's own header bar -- kept so
    // showLogViewOptionsPopup() can position its QMenu against it (see
    // that method) without also having to keep the whole
    // PanelContainerWidget* around for a purpose that needs only the
    // header.
    PanelHeaderBar* m_logHeaderBar = nullptr;
    PanelLayoutManager* m_panelLayoutManager = nullptr;
    // Left-side profile rail + the manager backing it -- operator,
    // 2026-09-14: "mache zusätzlich wie bei longpath auf der linken
    // seite eine leiste, dass ich ein zweites profil anlegen kann um
    // zb die karte dort alleine zu platzieren". See
    // LayoutProfileManager's own class comment for scope (five canvas
    // panels, not cwMacroRow) and ProfileRail's for the widget itself.
    ProfileRail* m_profileRail = nullptr;
    LayoutProfileManager* m_layoutProfileManager = nullptr;
    UtcClockWidget* m_utcClockWidget = nullptr;
    QLineEdit* m_gridFilterEdit = nullptr;
    QLabel* m_rigctldStatusLabel = nullptr;
    QLabel* m_on4kstStatusLabel = nullptr;
    QLabel* m_clusterStatusLabel = nullptr;
    QLabel* m_gridRadiusLabel = nullptr;
    // Rough tropo-ducting indicator (WeatherClient) -- plain readout
    // label, not a connect/disconnect badge, same treatment as
    // m_gridRadiusLabel above.
    QLabel* m_weatherStatusLabel = nullptr;
    QPushButton* m_modeToggleButton = nullptr;
    // See saveWindowGeometry()'s doc comment.
    QTimer* m_geometrySaveTimer = nullptr;
};

} // namespace Contestprogramm
