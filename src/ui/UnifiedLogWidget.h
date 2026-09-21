#pragma once

#include "app/ContestSettings.h"
#include "data/ContestDefinition.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <optional>

class QEvent;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QTableView;
class QTimer;
class QVBoxLayout;

namespace Contestprogramm {

class ChatFeedModel;
class LogTableModel;
class UnifiedFeedModel;

// Martin's explicit layout change (see this task's own report for the
// verbatim quotes): "band und grid bitte weg. eingabe sollte im
// gleichen fenster wie die logs sein. alles im gleichen raster" -- one
// panel titled "Log", the live entry row visually at the top of the
// same panel the logged history and not-yet-worked spot/chat
// candidates use below it, replacing EntryBarWidget + LogTableView +
// (both) ChatFeedView as MainWindow's four separate panels.
//
// A second pass (mockup "E", see this task's own prompt for the exact
// path) replaced this widget's original "two stacked QTableViews with
// identical fixed column widths" implementation, in response to
// Martin's "grafik gefaellt mir jetzt gar nicht" -- picking all four of
// concept/colour/density/spacing as problems: forcing the live entry
// row into literal QTableView cell widths left RST/Nr./Grid visibly
// cramped, and the whole-row amber background used far more colour than
// this codebase's own <=2%-coverage house style (StyleKit.h) allows
// anywhere else. The entry row is now a plain QWidget (m_entryRow, an
// EntryRowFrame -- see the .cpp) laid out with a normal QHBoxLayout --
// Call / per-contest exchange sub-fields / a stretch / the "Ges." sent-
// exchange preview / the dupe-status pill -- sized to its own content
// instead of a shared table's column grid, with a single 2px amber left
// accent bar (the same visual language PanelHeaderBar's own accent bar
// already uses) replacing the old full-row amber fill. It sits directly
// above m_feedTable (log history + not-yet-worked spot/chat candidates,
// unchanged) inside the SAME PanelContainerWidget/"Log" header this
// widget has always been registered under (see MainWindow.cpp) -- zero
// layout gap, one shared panel frame, so it still reads as one
// continuous surface with one header, matching the mockup.
//
// Columns shown in the feed table below: Zeit / Call / Exch Ges. /
// Exch Emp. / km / ° / Status (see the Column enum). Band AND Mode
// ("mode wird nicht benötigt" -- Martin's later follow-up, given the
// identical treatment as Band) are deliberately not visible
// columns/controls here -- see this task's report for where they still
// live (CAT-driven internal state, surfaced only in the status bar) and
// why. Mode is still tracked as real data this widget needs: the RST
// exchange sub-field (see setCurrentMode()/applyRstDefaults() below)
// auto-defaults to "59"/"599" the moment a mode is known, per
// DXLog.net's own documented behaviour (dxlog.net/docs, verified for
// this task).
//
// Field navigation in the entry row started from DXLog.net's documented
// model (dxlog.net/docs/index.php/Main_Window) and was adjusted by the
// operator directly, 2026-09-11: [Space] AND [Tab] both step to the
// next *relevant* field, skipping RST (it is "rarely changed" and
// almost always already correct via applyRstDefaults() -- still
// directly reachable by clicking it with the mouse); [Enter] logs the
// QSO from whichever entry-row field currently has
// focus, not only the last one in sequence. See the eventFilter()/
// nextRelevantField()/nextFieldForTab() private members below -- this
// logic is entirely about QLineEdit focus chains and is unaffected by
// which container widget those QLineEdits happen to live in.
//
// Implementation note (the one structural judgment call this widget's
// mockup-E pass makes that the mockup itself doesn't have to resolve):
// the ORIGINAL "two stacked QTableViews" design existed specifically
// because a literal single QAbstractItemModel/QTableView was rejected:
// QTableView deletes every setIndexWidget() attached to a row the
// moment that row's model does a full reset (beginResetModel/
// endResetModel), and ChatFeedModel resets on every single incoming
// spot -- often several times a minute mid-contest -- which would
// otherwise blow away the callsign field's cursor/focus/in-progress
// text out from under the operator's typing. That reason does not go
// away here -- but a plain QWidget entry row sidesteps it even more
// directly than the old frozen-single-row QTableView did: m_entryRow's
// QLineEdits are ordinary child widgets in an ordinary QHBoxLayout, not
// index widgets on ANY QAbstractItemModel, so there is no model reset
// for m_feedTable's churn to ever propagate into. m_feedTable keeps its
// own separate, freely-resettable UnifiedFeedModel exactly as before
// (log history + a divider + not-yet-worked candidates, rebuilt on
// every LogTableModel/ChatFeedModel change) -- only the entry side of
// the old split is gone, because it no longer needs to be a table at
// all.
class UnifiedLogWidget : public QWidget {
    Q_OBJECT

public:
    // Column layout for the feed table (log history + spot/chat
    // candidates) -- public so tests can query a specific cell's role
    // data directly (see tests/test_unifiedlog_widget.cpp) without
    // depending on the .cpp's internal model classes. The entry row no
    // longer shares this column grid (see the class comment) -- it is a
    // normal widget row sized to its own content.
    // ColumnSerial ("QSO#") leads the row, matching DXLog.net's own
    // column order (QSO#, Band, Time, Callsign, Sent, Nr, Rcvd, A, Pts,
    // Stn -- we only reproduce the columns whose data genuinely already
    // exists in this codebase, see setViewMode()'s doc comment). It is
    // always a real column in the model (UnifiedFeedModel::historyData()
    // fills it from QsoRecord::serialSent, Style::unknownDash() for a
    // QSO with none) -- only its VISIBILITY in the QTableView toggles
    // with the view mode, see setViewMode().
    enum Column {
        ColumnSerial = 0,
        ColumnTime,
        ColumnCall,
        ColumnExchangeSent,
        ColumnExchangeRcvd,
        ColumnDistanceKm,
        ColumnBearingDeg,
        ColumnStatus,
        // DXLog-Vollspalten-only columns (setViewMode()), appended after
        // the original Compact set so existing logical indices above
        // never shift -- visual ORDER for DXLog-Vollspalten mode is
        // instead achieved via QHeaderView::moveSection() in
        // applyColumnOrder(), not by index order, so appending here is
        // safe. Split out of the composed ColumnExchangeSent/
        // ColumnExchangeRcvd strings, matching DXLog.net's own literal
        // column set (QSO# / Band / Zeit / Call / Sent / Nr. / Rcvd /
        // Nr./Grid -- verified against the design-canvas proposal Martin
        // approved, "log-dxlog-anordnung", 2026-09-11: "unser design
        // bleibt, nur die anordnugn 1:!"). Each value is real data
        // QsoRecord already stores as its own field (band/rstSent/
        // serialSent/rstRcvd/serialRcvd/gridSquare) -- nothing invented,
        // same HAUSSTIL-rule-7 discipline the rest of this widget
        // already follows.
        ColumnBand,
        ColumnRstSent,
        ColumnSerialSent,
        ColumnRstRcvd,
        ColumnSerialGridRcvd,
        ColumnCount
    };

    // Extra data() roles the status/source-pill cells carry -- see
    // UnifiedLogWidget.cpp's PillDelegate, used by m_feedTable's Status
    // column (DUPE/UNGÜLTIG on a history row, KST/CLU on a candidate
    // row). The entry row's own dupe/status pill is a real QLabel now
    // (see kStatusPillObjectName below), not a delegate-painted cell, so
    // it does not use these roles -- but they stay public for the same
    // test-visibility reason as Column above.
    enum Role {
        PillTextRole = Qt::UserRole + 1,
        PillBgRole,
        PillFgRole,
        PillBorderRole,
    };

    // objectName test hooks (same convention as PanelContainerWidget's
    // own id-as-objectName / the pre-docking "cwMacroRow" widget) --
    // findChild<QWidget*>(kEntryRowObjectName)/findChild<QTableView*>(
    // kFeedTableObjectName)/findChild<QLabel*>(kStatusPillObjectName)
    // let a test reach the right part of the tree directly without this
    // header needing to expose the private model classes themselves.
    static constexpr const char* kEntryRowObjectName = "unifiedLogEntryRow";
    // The QScrollArea wrapping the entry row (see m_entryRowScroll's own
    // doc comment) -- the actual m_mainLayout item, so a test checking
    // the entry row's POSITION relative to m_feedTable/m_statusLine
    // must look this up instead of kEntryRowObjectName (which finds the
    // row's own content widget, one level deeper, not itself a direct
    // m_mainLayout child any more).
    static constexpr const char* kEntryRowScrollObjectName = "unifiedLogEntryRowScroll";
    static constexpr const char* kFeedTableObjectName = "unifiedLogFeedTable";
    static constexpr const char* kStatusPillObjectName = "unifiedLogStatusPill";
    // The DXLog-style status strip (see setOperatingMode()'s doc
    // comment below) sitting between the entry row and the feed table,
    // and its own two labels -- same "objectName test hook" convention
    // as kStatusPillObjectName above, so a test can findChild<QLabel*>()
    // each one directly rather than relying on child construction order.
    static constexpr const char* kStatusLineObjectName = "unifiedLogStatusLine";
    static constexpr const char* kLastQsoLabelObjectName = "unifiedLogLastQsoLabel";
    static constexpr const char* kOperatingModeLabelObjectName = "unifiedLogOperatingModeLabel";
    // The chat quick-send row (see chatMessageSendRequested's own doc
    // comment below) -- same objectName-as-test-hook convention as the
    // rest of this list.
    static constexpr const char* kChatInputObjectName = "unifiedLogChatInput";
    static constexpr const char* kChatCqButtonObjectName = "unifiedLogChatCqButton";
    static constexpr const char* kChatSendButtonObjectName = "unifiedLogChatSendButton";
    static constexpr const char* kAwayToggleButtonObjectName = "unifiedLogAwayToggleButton";

    explicit UnifiedLogWidget(QWidget* parent = nullptr);

    // Non-owning; the caller (MainWindow, via AppController) keeps these
    // alive for as long as this widget exists -- same ownership pattern
    // LogTableView::setSourceModel/ChatFeedView::setSourceModel already
    // used. `onKst`/`cluster` may be passed together or left null until
    // both are available; a null chat model simply omits the "Spots &
    // Chat" section below the log history.
    void setLogModel(LogTableModel* model);
    void setChatModels(ChatFeedModel* onKst, ChatFeedModel* cluster);

    // Tears down and rebuilds the entry row's per-contest exchange
    // sub-fields, one labelled value cell per entry in `fields` (same
    // generalization EntryBarWidget::setExchangeFields used to provide
    // -- a contest with more than one received-exchange field, or
    // differently-named/-typed fields from a ContestRulesEditor save,
    // gets the right inline controls instead of one hardcoded text
    // box). Call whenever the active contest changes. Values already
    // typed under a key that survives the rebuild are preserved; values
    // under a key that no longer exists are dropped along with their
    // field.
    void setExchangeFields(const QVector<ContestDefinition::ExchangeField>& fields);

    QString callsign() const;

    // key -> current text, one entry per field passed to the last
    // setExchangeFields() call. A grid6-typed field's text is
    // upper-cased; other types are returned as typed.
    QMap<QString, QString> exchangeReceived() const;

    // `detail` (only read while isDupe) is the sentence the status line
    // shows instead of "Letzter QSO" -- which earlier QSO this duplicates
    // (number, UTC time, band), composed by MainWindow.
    void setDupeIndicator(bool isDupe, const QString& detail = QString());

    // The exchange this program would send right now (own grid + next
    // serial) -- shown read-only in the "Ges." field on the right of
    // the entry row (dim/amber, per the approved mockup).
    void setSentExchangePreview(const QString& text);

    // Live distance/bearing to whatever grid is currently typed in the
    // received exchange's grid6-typed sub-field -- shown in the entry
    // row's own Km/° cells (m_entryKmLabel/m_entryDegLabel), std::nullopt
    // while the typed text isn't (yet) a valid 4/6-character grid.
    // Reversed from the original mockup-E decision to omit this
    // entirely (operator, 2026-09-11: "die grad anzeige von meinem
    // standort zum qsp partner sollten sofort angezeigt werden") -- a
    // logged QSO's actual distance/bearing still shows in the feed
    // table below too (Column::ColumnDistanceKm/ColumnBearingDeg,
    // unchanged), this is just the same values previewed live BEFORE
    // logging. MainWindow's receivedGridChanged -> handleReceivedGridChanged
    // wiring already computed and discarded these values even when this
    // was a no-op -- see that method's own doc comment.
    void setEntryDistanceBearing(const std::optional<double>& distanceKm, const std::optional<double>& bearingDeg);

    // Mode is CAT-driven internal state with no visible control any
    // more (see the class comment) -- MainWindow calls this from
    // RigctldClient's decoded mode (Run-mode guarded the same way
    // EntryBarWidget::setCurrentMode was, one level up in MainWindow),
    // and this widget uses it purely to keep the RST exchange
    // sub-field's "59"/"599" default current (see applyRstDefaults()).
    void setCurrentMode(const QString& mode);
    // From a clicked candidate row (callsign; grid goes through
    // setExchangeFieldValue below, same as EntryBarWidget's own
    // click-to-fill wiring did).
    void setCallsign(const QString& callsign);

    // Sets one exchange-received sub-field's text by key; a no-op if no
    // field with that key exists in the row built by the last
    // setExchangeFields() call.
    void setExchangeFieldValue(const QString& key, const QString& value);

    // Grid-autofill result (core/CallsignLocatorLookup.h /
    // ContestDatabase::knownExchangeForCallsign): prefills the
    // grid6-typed sub-field from `gridSquare` and the auto-increment
    // sub-field from `serialRcvd`. Only fills a sub-field that is
    // currently empty -- an explicit click-to-fill or the operator's
    // own typing always takes priority.
    void applyKnownExchange(const QString& gridSquare, const std::optional<int>& serialRcvd);

    // Is there an in-progress entry the operator has not yet logged?
    // Used by MainWindow's Run-mode CAT-autofill guard.
    bool hasUnsentContent() const;

    // Clears callsign and every exchange-received sub-field for the
    // next contact, clears the live km/bearing preview (a no-op, see
    // setEntryDistanceBearing() above), and resets the status pill to
    // "NEU"; mode is left as-is (it rarely changes contact-to-contact)
    // but the RST sub-field (if the active contest has one) is
    // immediately re-defaulted to "59"/"599" per the current mode,
    // matching DXLog.net's own behaviour at the start of a fresh entry
    // -- then focuses the callsign field.
    void resetForNextEntry();
    // ESM (see core/EsmPlanner.h): after keying "<call> <exchange>" the
    // cursor belongs in the first still-empty received field, so the
    // reply can be typed without a Tab.
    void focusFirstEmptyExchangeField();
    // Selects and scrolls to the logged QSO with this database id (the
    // log-check window's "jump to the QSO"); a no-op when the id is not
    // among the history rows. Returns whether it was found.
    bool selectHistoryQso(int qsoId);

    // Grid-square substring filter over the log-history rows only --
    // same scope m_gridFilterEdit already had against LogTableView's
    // QSortFilterProxyModel; the entry row and the spot/chat candidate
    // rows are always shown regardless of this filter, matching
    // today's behavior (the filter never touched ChatFeedView either).
    void setGridFilter(const QString& text);

    // DXLog-style status line (operator, 2026-09-11, after being shown
    // a screenshot of DXLog.net's real "Contest recorder" log window:
    // status line above the grid reading e.g. "11:46:44 HA2NA ... SR
    // 0545z SS 1613z RUN"). This is the minimal proposal Martin picked
    // ("1 und 2"): the most recently logged QSO's callsign + time, and
    // the current Run/S&P operating mode (ContestSettings::
    // OperatingMode -- NOT this widget's own CAT-driven m_currentMode/
    // setCurrentMode(), which is SSB/CW/FM/RTTY; see that method's own
    // doc comment for the unrelated concept it tracks). Sunrise/sunset
    // (DXLog's own "SR"/"SS" fields) are deliberately NOT reproduced --
    // see this task's report: a correct solar-position calculation
    // needs its own verified-against-a-known-reference port (the same
    // discipline core/Maidenhead.h's own distance/bearing math already
    // went through), which is out of scope for this pass; faking a
    // value would violate HAUSSTIL rule 7 far worse than simply leaving
    // the field out.
    //
    // The "most recently logged QSO" half updates itself: this widget
    // already holds the LogTableModel pointer passed to setLogModel()
    // and listens to its own modelReset/dataChanged directly (see the
    // .cpp), so no separate push from MainWindow is needed there --
    // only the operating-mode half needs an explicit setter, since
    // ContestSettings::operatingMode lives outside any model this
    // widget already watches.
    void setOperatingMode(ContestSettings::OperatingMode mode);

    // Column view mode for the feed table below the entry row --
    // "Kompakt" (today's existing column set/order, the default) or
    // "DxLogFullColumns": DXLog.net's own literal column order (QSO# /
    // Band / Zeit / Call / Sent / Nr. / Rcvd / Nr./Grid), with this
    // widget's own extra km/°/Status columns kept trailing (Martin: "our
    // own design stays, only the arrangement is 1:1" -- those three
    // aren't DXLog columns at all, so hiding them would remove real data
    // DXLog never had, not match it). Per the ⚙ options popup MainWindow
    // builds on PanelHeaderBar::optionsRequested (this widget has no ⚙
    // of its own -- see PanelHeaderBar.h's own doc comment on why the
    // caller, not PanelHeaderBar or this widget, owns what such a click
    // opens). Only toggles QTableView column visibility/order, never the
    // model's column count -- every column is always real data, see the
    // Column enum's own doc comment.
    void setViewMode(ContestSettings::LogViewMode mode);
    ContestSettings::LogViewMode viewMode() const { return m_viewMode; }

    // Where the entry row sits relative to the feed table -- Top
    // (today's layout, default) or Bottom (directly after the table's
    // last row, no gap, same entry-row design unchanged -- see
    // ContestSettings::LogEntryRowPosition's own doc comment for the
    // exact operator request this answers). Reorders m_entryRow/
    // m_statusLine/m_feedTable within m_mainLayout; never touches the
    // feed table's own row order (chronologically ascending either way,
    // see UnifiedFeedModel::rebuild()) or the entry row's own fields/
    // styling.
    void setEntryRowPosition(ContestSettings::LogEntryRowPosition position);
    ContestSettings::LogEntryRowPosition entryRowPosition() const { return m_entryRowPosition; }

    // Places `text` into the chat quick-send field, focused and fully
    // selected -- so the operator can either press Enter immediately
    // (zero further typing) or start typing to override it first. Used
    // by MainWindow in response to cqDraftRequested() below: this
    // widget has no ContestSettings/MessageDrafter dependency (same
    // capability-separation reasoning as chatMessageSendRequested), so
    // MainWindow composes the actual CQ text and hands it back here.
    void setChatInputDraft(const QString& text);

    // Keyboard focus into the Callsign field -- MainWindow calls this
    // once the window is on screen (2026-09-21: a fresh start left the
    // focus in the top bar's grid filter, so the first callsign typed
    // filtered the log instead of starting a QSO).
    void focusCallsign();

signals:
    void logRequested();
    void formChanged();

    // Emitted a short debounce after the callsign field's content
    // settles, whenever it is non-empty -- MainWindow queries
    // ContestDatabase::knownExchangeForCallsign()/
    // CallsignLocatorLookup on this, same as EntryBarWidget did.
    void callsignLookupRequested(const QString& callsign);

    // Live text of whichever exchange sub-field is grid6-typed, on
    // every keystroke -- MainWindow recomputes distance/bearing from
    // ContestSettings::ownGrid and calls setEntryDistanceBearing() back
    // (see that method's own doc comment). Empty when the field is
    // empty or the active contest has no grid6-typed field at all.
    void receivedGridChanged(const QString& grid);

    // A not-yet-worked spot/chat candidate row was clicked -- same
    // signal shape (and the same MainWindow::handleCandidateActivated
    // consumer) ChatFeedView::candidateActivated used.
    void candidateActivated(const QString& callsign, const QString& grid, qint64 freqHz);

    // A logged history row's Call cell / Exch Emp. cell was hand-edited
    // in place (double-click or Enter/F2 on the cell, DXLog.net-style --
    // see this task's report on the scope: Call/RST/Serial/Grid are
    // correctable this way, and since the time-correction pass so is
    // the time; frequency/operator are not).
    // `qsoId` is the QSO's real database id (LogTableModel::recordAt().id),
    // not a row index, which shifts as new QSOs are logged.
    // historyExchangeRcvdEditRequested's `newText` is the whole
    // composed received-exchange string as the operator left it (e.g.
    // "599 001 JN77QT") -- MainWindow re-derives the individual RST/
    // Serial/Grid values from it by the active ContestDefinition's own
    // declared field order, the same order composeExchange() already
    // joins them in.
    void historyCallsignEditRequested(int qsoId, const QString& newCallsign);
    void historyExchangeRcvdEditRequested(int qsoId, const QString& newText);
    // The Time cell, same mechanism: "HH:MM" keeps the QSO's date,
    // "YYYY-MM-DD HH:MM" sets both -- the log time is when Enter was
    // pressed, and after a pile-up that is not always when the QSO was.
    void historyTimeEditRequested(int qsoId, const QString& newText);

    // The Status cell of a logged history row was clicked -- toggles
    // that QSO's invalid flag. DXLog.net deliberately has no delete
    // function for a logged QSO (dxlog.net/docs/index.php/Menu_Edit,
    // verified for this task: "in the spirit of honest contest
    // logging... you don't") -- marking invalid is the real equivalent
    // (Ctrl+X in DXLog.net's own UI).
    void historyInvalidToggleRequested(int qsoId);

    // Chat quick-send row (operator, 2026-09-12: during a quiet spell
    // "wenig qso beim rufen" wants to leave a chat message "mit wenig
    // aufwand") -- the free-text field's Enter/"Senden" click, sent
    // exactly as typed. Same capability-separation rule as the rest of
    // this widget's signals: this widget has no On4kstClient/
    // AppController dependency, MainWindow is the only place that
    // actually calls On4kstClient::sendChatMessage() (see
    // MainWindow::handleSuggestionSendRequested, which this signal
    // shares -- same one send chokepoint, two UI sources).
    void chatMessageSendRequested(const QString& text);
    // The "CQ" button was clicked -- see setChatInputDraft() above for
    // why this is a signal (round-trip to MainWindow) rather than this
    // widget composing the CQ text itself.
    void cqDraftRequested();

    // The Away/Zurück toggle in the chat row changed state -- `away`
    // true right after the operator clicked it to step away (button now
    // reads "Zurück"), false when clicked again to return. Same
    // capability-separation rule as the rest of this row: this widget
    // only reports the operator's intent, MainWindow is the one place
    // that calls On4kstClient::sendAway()/sendBack().
    void awayStateChanged(bool away);

private slots:
    void onCallsignTextChanged();
    void onCallsignLookupTimeout();
    void handleFeedRowClicked(const QModelIndex& index);
    void rebuildFeedRows();

private:
    // Wraps `valueWidget` (a QLineEdit sub-field) in a fixed-width,
    // label-less, borderless host -- see flatFieldStyle()'s own comment
    // on the 2026-09-11 restyle this replaces the old boxed/labelled
    // version with. `align` only matters for a QLabel value. Returns the
    // new cell, already parented under `parent`; the caller still owns
    // adding it to a layout.
    QWidget* buildFieldCell(QWidget* parent, QWidget* valueWidget, int width, int height, Qt::Alignment align = Qt::AlignLeft);
    // Sizes/orders the entry row's cells to match m_feedTable's own
    // current column widths/order exactly (Compact or
    // DxLogFullColumns, see kColumnWidths and applyColumnOrder()'s own
    // two order lists) -- called from setViewMode() (both at
    // construction and on every later mode switch). Detaches and
    // reorders the persistent value widgets (m_callsignEdit,
    // m_exchangeFieldsHost, m_sentExchangeLabel, m_statusPillLabel);
    // deletes and rebuilds only the blank placeholder cells for columns
    // this row has no live value for (Time/Km/°/Serial/Band -- unknown
    // until the QSO is actually logged, Style::unknownDash() same as
    // everywhere else in this codebase, HAUSSTIL rule 7).
    void rebuildEntryRowLayout();
    // Combined pixel width of m_feedTable's own received-exchange
    // columns -- the "Rcvd"+"Nr./Grid" pair, ALWAYS (both view modes,
    // see setViewMode()'s own comment on always splitting RST out of
    // Nr./Grid; see the Column enum's own doc comment for what each
    // column holds). Used to size both the entry row's own sub-field
    // edits (rebuildExchangeCell()) and its placeholder cell width
    // (rebuildEntryRowLayout()) identically, so the entry row keeps
    // lining up with the table's own columns underneath it.
    int rcvdGroupWidth() const;
    // The column widths actually in use: kColumnWidths (the design
    // grid) after fitColumnsToViewport() has shrunk them for a panel
    // narrower than the grid (see UnifiedLogWidget.cpp's
    // kMinColumnWidths comment). Both the table and the entry row read
    // these, so they keep lining up in every panel width.
    int columnWidthFor(int col) const;
    // Recomputes m_columnWidths for the current viewport width and
    // visible column set; returns true when any width changed.
    bool fitColumnsToViewport();
    // The view mode's own column set (setViewMode()), before any
    // give-way hiding by fitColumnsToViewport().
    bool columnWantedByViewMode(int col) const;
    // Re-pins every entry-row cell to columnWidthFor() in place (no
    // widget is recreated -- typing/focus survive a resize).
    void applyEntryRowWidths();
    // Per-sub-field widths of the received-exchange group, summing to
    // rcvdGroupWidth() (see the .cpp for the weighting).
    QVector<int> exchangeFieldWidths() const;
    void setFieldAutoFilled(QLineEdit* field, bool autoFilled);
    static bool isFieldAutoFilled(const QLineEdit* field);
    void rebuildExchangeCell(const QMap<QString, QString>& previousValues);
    void configureFeedColumns();
    void applyDividerSpan();
    // Caps m_feedTable's own maximum height to exactly its current row
    // content when the entry row sits at the Bottom (so the entry row
    // touches the table's own last row directly, no trailing blank
    // table area pushing it down) -- a no-op cap (QWIDGETSIZE_MAX) in
    // Top mode, restoring today's original "table fills the panel"
    // behaviour. Called from rebuildFeedRows() (row count changed) and
    // setEntryRowPosition() (mode toggled).
    void syncFeedTableHeight();
    // Grows m_feedTable's own Status column to fill a panel wider than
    // kColumnWidths' own fixed total, capped at kStatusColumnMaxWidth --
    // see that constant's own comment. Called from configureFeedColumns()
    // (initial width), setViewMode() (the visible column set changed,
    // which changes how much width is "other"), and resizeEvent() (the
    // panel's own width changed).
    void syncStatusColumnWidth();
    // Mirrors m_feedTable's own syncStatusColumnWidth() for the entry
    // row's Status cell -- grows it to fill any panel width beyond
    // kColumnWidths' own fixed total, capped at kStatusColumnMaxWidth,
    // so the entry row and the table stay column-aligned at any panel
    // width instead of only agreeing up to that fixed total. Called
    // from rebuildEntryRowLayout() and resizeEvent() (the panel's own
    // width changing is exactly the case this exists for).
    void syncEntryRowWidth();
    // Drives m_feedTable's VISUAL column order to exactly `logicalOrder`
    // (must list every Column value exactly once) via repeated
    // QHeaderView::moveSection() calls -- used by setViewMode() so
    // Compact and DxLogFullColumns can each have their own column order
    // independent of the Column enum's fixed logical/index order (which
    // stays stable for every other piece of code that indexes by
    // Column, e.g. historyData()/candidateData() below).
    void applyColumnOrder(const QVector<int>& logicalOrder);
    // Text + colour pairing for the entry row's own dupe/status pill
    // (kStatusPillObjectName) -- DUPE (red) / NEU (green), the same
    // named colour families UnifiedFeedModel's PillDelegate-painted
    // pills already use one section down, just applied to a real QLabel
    // here instead of painted per-cell.
    void updateStatusPill(bool dupe);
    // DXLog.net: "RST sent and RST rcvd items are set to 59 (if the
    // mode is SSB or FM) or 599 (if the mode is CW, RTTY, or PSK)"
    // (dxlog.net/docs, verified for this task) -- applied to whichever
    // exchange sub-field is "rst"-typed, the moment a mode is known,
    // whenever that field is still empty or still showing its own
    // previous auto-filled value (an operator's own typed RST is never
    // overwritten, same "auto-filled, still overridable" convention
    // setFieldAutoFilled()/isFieldAutoFilled() already give grid/serial
    // autofill elsewhere in this class).
    void applyRstDefaults();
    // [Space]'s "next relevant field" per DXLog.net (see the class
    // comment): callsign, then every exchange sub-field EXCEPT one
    // whose type is "rst" -- nullptr once `current` is the last
    // relevant field.
    // Rewrites a plain number in the received-number field as "004".
    void padSerialField(QLineEdit* field) const;
    QLineEdit* nextRelevantField(QLineEdit* current) const;
    // [Tab]'s own field chain -- since 2026-09-11 (see the class
    // comment) functionally identical to nextRelevantField() above
    // (RST skipped there too now); kept as its own named method rather
    // than merged into one, since eventFilter() still dispatches on
    // which key was pressed and a future divergence between the two
    // keys is plausible. nullptr once `current` is the last relevant
    // field (eventFilter() then lets the keypress fall through to Qt's
    // own default Tab handling, e.g. into the feed table below, rather
    // than trapping focus in the entry row). Handled explicitly here,
    // in eventFilter(), rather than via QWidget::setTabOrder(): an
    // explicit chain is simple and deterministic regardless of how the
    // entry row's own internal widget tree is nested (call cell /
    // dynamic exchange-fields host / "Ges." cell / status pill, see the
    // .cpp).
    QLineEdit* nextFieldForTab(QLineEdit* current) const;
    bool eventFilter(QObject* watched, QEvent* event) override;
    // Re-syncs the entry row's Status cell width whenever the panel
    // itself is resized (see syncEntryRowWidth()'s own doc comment) --
    // dragging the "Log" panel wider is exactly the case that leaves
    // dead, grid-less space on the right if this isn't re-run.
    void resizeEvent(QResizeEvent* event) override;
    // Refreshes m_statusLine's two labels from m_logModel's current
    // last row (if any) and m_operatingMode -- see setOperatingMode()'s
    // doc comment. Connected to m_logModel's modelReset/dataChanged in
    // setLogModel(), and called directly by setOperatingMode()/
    // setLogModel() itself.
    void updateStatusLine();

    // The entry row -- a plain widget (an EntryRowFrame, see the .cpp),
    // NOT a QTableView/QAbstractItemModel of any kind (see the class
    // comment for why that split no longer needs to exist). Holds
    // m_callsignEdit's "Call" cell, the dynamically rebuilt per-contest
    // exchange-fields host, the "Ges." preview cell, and the status
    // pill, left to right in one QHBoxLayout.
    QWidget* m_entryRow;
    // Wraps m_entryRow so its horizontal scroll position can be driven
    // by m_feedTable's own horizontal scrollbar (see
    // syncEntryRowHorizontalScroll()) -- operator, 2026-09-11, after a
    // narrowed panel clipped the entry row's own text independently of
    // the table instead of scrolling together with it ("alles soll
    // sich automatisch zusammenschieben"): both the table and the entry
    // row use the SAME column pixel widths (kColumnWidths), so they
    // share one scroll range and stay column-aligned at any panel
    // width, not just the width that happens to fit every column. Its
    // own scrollbars are always hidden (Qt::ScrollBarAlwaysOff) --
    // m_feedTable's native one is the only one the operator ever sees
    // or drags; this one is purely driven programmatically.
    QScrollArea* m_entryRowScroll = nullptr;
    QHBoxLayout* m_entryRowLayout = nullptr;
    // Rebuilt wholesale by rebuildExchangeCell() every setExchangeFields()
    // call (QLayout::replaceWidget() swaps it in place at the same
    // position in m_entryRowLayout, between the Call cell and the
    // trailing stretch) -- same "tear down and rebuild" approach the
    // pre-merge EntryBarWidget/this widget's own original implementation
    // already used, just targeting a normal layout slot instead of a
    // QTableView index widget.
    QWidget* m_exchangeFieldsHost = nullptr;

    // DXLog-style status line, between the entry row and the feed
    // table -- see setOperatingMode()'s doc comment. Built in the
    // constructor body (like m_exchangeFieldsHost/m_entryRowLayout
    // above), not the initializer list, so its declaration position
    // here does not have to match construction order there.
    QWidget* m_statusLine = nullptr;
    QLabel* m_lastQsoLabel = nullptr;
    QLabel* m_operatingModeLabel = nullptr;
    ContestSettings::OperatingMode m_operatingMode = ContestSettings::OperatingMode::SearchAndPounce;
    ContestSettings::LogViewMode m_viewMode = ContestSettings::LogViewMode::Compact;
    // Bottom by default -- matches ContestSettings::logEntryRowPosition's
    // own default (see its doc comment for why Top stopped making sense
    // once the feed table's row order became chronologically ascending).
    ContestSettings::LogEntryRowPosition m_entryRowPosition = ContestSettings::LogEntryRowPosition::Bottom;
    // Guards rebuildEntryRowLayout()/rebuildExchangeCell() in
    // setViewMode() -- see that method's own comment on why this must
    // NOT rebuild on every call (MainWindow calls it on every CAT tick).
    bool m_entryRowLayoutBuilt = false;
    QVector<int> m_columnWidths;
    // Columns fitColumnsToViewport() hid on top of the view mode's own
    // set because even the floors did not fit (kGiveWayOrder).
    QVector<int> m_columnsHiddenByFit;
    // The blank QSO#/Band placeholder cells rebuildEntryRowLayout()
    // adds in DXLog-Vollspalten mode, kept so applyEntryRowWidths() can
    // resize them in place.
    QVector<QLabel*> m_entryRowBlanks;
    // History row count at the last rebuildFeedRows(), to scroll a
    // newly logged QSO into view exactly once.
    int m_lastHistoryRowCount = -1;
    // Owns m_entryRow/m_statusLine/m_feedTable's vertical order --
    // stored (not a constructor-local) so setEntryRowPosition() can
    // reorder them later without rebuilding the layout from scratch.
    QVBoxLayout* m_mainLayout = nullptr;

    QTableView* m_feedTable;
    UnifiedFeedModel* m_feedModel;
    // Non-owning, same lifetime contract as setLogModel()'s own doc
    // comment -- kept here (in addition to m_feedModel's own copy) so
    // updateStatusLine() can read the most recently logged row directly
    // without m_feedModel needing to expose its own display-order rows
    // for a purpose that has nothing to do with the feed table.
    LogTableModel* m_logModel = nullptr;

    QLineEdit* m_callsignEdit;
    // Read-only "Ges." (sent-exchange) preview -- see
    // setSentExchangePreview().
    QLabel* m_sentExchangeLabel;
    // The entry row's own DUPE/NEU pill -- see updateStatusPill().
    QLabel* m_statusPillLabel;
    bool m_dupe = false;
    QString m_dupeDetail;

    // The entry row's Time cell -- a live-ticking clock (HH:mm UTC,
    // matching LogTableModel::ColumnTime's own format exactly) rather
    // than a blank dash, per the operator's own follow-up (2026-09-11)
    // to the DXLog.net "row 10" restyle: "die uhrzeit sollte
    // automatisch schon als start sein, dann sieht man das besser" --
    // DXLog's own row 10 shows a live-ticking time too (the reference
    // screenshot's "11:46"), not a placeholder. Unlike QSO#/Band
    // (genuinely unknown until the QSO is logged), the CURRENT time is
    // always real, known data -- showing it is not a HAUSSTIL-rule-7
    // violation, it is the one entry-row cell that never needs a dash.
    // Persistent (unlike the other blank cells rebuildEntryRowLayout()
    // recreates each call) so m_liveClockTimer can keep updating the
    // same label instance across mode switches.
    QLabel* m_entryTimeLabel = nullptr;
    QTimer* m_liveClockTimer = nullptr;
    // The entry row's own Km/° cells -- live distance/bearing to
    // whatever grid is currently typed in the received exchange's
    // grid6-typed sub-field, updated by setEntryDistanceBearing() (see
    // its own doc comment for the operator request this answers).
    // Persistent for the same reason m_entryTimeLabel is: it must keep
    // being the SAME QLabel instance across a rebuildEntryRowLayout()
    // call (Compact/DxLogFullColumns switch, contest change, ...), not
    // a throwaway addBlank() cell, or an external push from MainWindow
    // would have nothing left to update.
    QLabel* m_entryKmLabel = nullptr;
    QLabel* m_entryDegLabel = nullptr;

    QTimer* m_callsignLookupTimer;

    // CAT-driven, no dedicated UI control -- see the class comment
    // above. Drives applyRstDefaults() only; MainWindow keeps its own
    // copy (m_currentMode) as the actual source of truth for
    // everything else (dupe scope, export, CW-macro context).
    QString m_currentMode;

    QVector<ContestDefinition::ExchangeField> m_exchangeFields;
    QVector<QLineEdit*> m_exchangeEdits;
    QMap<QString, QLineEdit*> m_exchangeEditsByKey;

    // Chat quick-send row -- see chatMessageSendRequested's own doc
    // comment. Always the panel's last row (setEntryRowPosition()
    // appends it after the Top/Bottom branch every time), independent
    // of the entry-row Top/Bottom placement above.
    QWidget* m_chatRow = nullptr;
    QPushButton* m_awayToggleButton = nullptr;
    QLineEdit* m_chatInputEdit = nullptr;
    QPushButton* m_chatCqButton = nullptr;
    QPushButton* m_chatSendButton = nullptr;
};

} // namespace Contestprogramm
