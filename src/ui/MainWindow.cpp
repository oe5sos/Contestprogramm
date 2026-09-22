#include "ui/MainWindow.h"

#include "app/AppController.h"
#include "core/BandUtils.h"
#include "core/BandmapModel.h"
#include "core/BeamHeading.h"
#include "core/ColorTheme.h"
#include "core/Transverter.h"
#include "core/CallsignLocatorLookup.h"
#include "core/CheckPartialIndex.h"
#include "core/DxClusterClient.h"
#include "core/EsmPlanner.h"
#include "core/OnlineScoreboard.h"
#include "core/Maidenhead.h"
#include "core/RigctldClient.h"
#include "core/RotctldClient.h"
#include "core/WeatherClient.h"
#include "core/assistant/MessageDrafter.h"
#include "core/assistant/NextTargetSuggester.h"
#include "core/terrain/HorizonProfile.h"
#include "core/terrain/SrtmTileLoader.h"
#include "core/terrain/TerrainDataManager.h"
#include "data/AdifExporter.h"
#include "data/BackupRestore.h"
#include "data/CabrilloExporter.h"
#include "data/ContestScoring.h"
#include "data/ContestDatabase.h"
#include "data/DupeChecker.h"
#include "data/DupeRescore.h"
#include "data/LogCheck.h"
#include "core/ClockCheck.h"
#include "data/ReadinessCheck.h"
#include "data/LogFileReader.h"
#include "data/QsoRecord.h"
#include "models/ChatFeedModel.h"
#include "models/LogTableModel.h"
#include "ui/BackupRestoreDialog.h"
#include "ui/BandmapWidget.h"
#include "ui/CheckPartialWidget.h"
#include "ui/CabrilloExportDialog.h"
#include "ui/ContestPickerDialog.h"
#include "ui/ContestRulesEditor.h"
#include "ui/CwMacroPanel.h"
#include "ui/EdiExportDialog.h"
#include "ui/EsmTemplatesDialog.h"
#include "ui/LayoutProfileManager.h"
#include "ui/LogCheckWindow.h"
#include "ui/ReadinessWindow.h"
#include "ui/AboutDialog.h"
#include "ui/UpdateDialog.h"
#include "ui/ShortcutsWindow.h"
#include "ui/TransverterDialog.h"
#include "ui/MapWidget.h"
#include "ui/MultiplierWindow.h"
#include "ui/PanelContainerWidget.h"
#include "ui/PanelHeaderBar.h"
#include "ui/PanelLayoutManager.h"
#include "ui/ProfileRail.h"
#include "ui/RateMeterWidget.h"
#include "ui/RotorWidget.h"
#include "ui/ScoreboardDialog.h"
#include "ui/SkedPanel.h"
#include "ui/StatisticsWindow.h"
#include "ui/SettingsDialog.h"
#include "ui/StyleKit.h"
#include "ui/SuggestionPanel.h"
#include "ui/UnifiedLogWidget.h"
#include "ui/UtcClockWidget.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QTimeZone>
#include <QHash>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMoveEvent>
#include <QPair>
#include <QPushButton>
#include <QRect>
#include <QResizeEvent>
#include <QSet>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <algorithm>
#include <memory>
#include <optional>

namespace Contestprogramm {

namespace {

// Composes the exchange text sent/received in the declared field order
// of `def.exchangeFields()` (per the plan: "hängt exchange_fields in
// deklarierter Reihenfolge zusammen") -- fields without a value in
// `values` are skipped rather than leaving a gap. Auto-increment
// integer fields ("serial") are zero-padded to 3 digits, matching the
// plan's own Cabrillo example line ("001", "002") -- see
// buildReceivedExchangeValues/buildSentExchangeValues below, which do
// that padding before calling this.
QString composeExchange(const ContestDefinition& def, const QMap<QString, QString>& values)
{
    QStringList parts;
    for (const ContestDefinition::ExchangeField& field : def.exchangeFields()) {
        const QString value = values.value(field.key);
        if (!value.isEmpty()) {
            parts << value;
        }
    }
    return parts.join(QStringLiteral(" "));
}

// UnifiedLogWidget's "Exch Emp." sub-fields are built dynamically from
// def.exchangeFields() (see UnifiedLogWidget::setExchangeFields), so
// MainWindow can no longer assume the grid/serial keys are literally
// named "grid"/"serial" -- a ContestRulesEditor save can rename them.
// These two helpers locate "the grid field" / "the serial field" by
// the same type/auto_increment markers UnifiedLogWidget itself uses
// (see UnifiedLogWidget::applyKnownExchange), so every place that used
// to read a fixed key now agrees on the same generalization.
const ContestDefinition::ExchangeField* findFieldByType(const ContestDefinition& def, const QString& type)
{
    for (const ContestDefinition::ExchangeField& field : def.exchangeFields()) {
        if (field.type == type) {
            return &field;
        }
    }
    return nullptr;
}

const ContestDefinition::ExchangeField* findAutoIncrementField(const ContestDefinition& def)
{
    for (const ContestDefinition::ExchangeField& field : def.exchangeFields()) {
        if (field.autoIncrement) {
            return &field;
        }
    }
    return nullptr;
}

// DXLog.net's own rule (dxlog.net/docs, verified for this task): "RST
// sent and RST rcvd items are set to 59 (if the mode is SSB or FM) or
// 599 (if the mode is CW, RTTY, or PSK)". Unknown/empty mode falls back
// to 59, matching this file's own m_currentMode default. Mirrors
// UnifiedLogWidget.cpp's own defaultRstForMode() -- two small
// self-contained anonymous-namespace helpers rather than a shared
// utility header for four lines of logic, matching this codebase's
// existing per-file-helper style (e.g. cabrilloModeCode() in
// CabrilloExporter.cpp).
QString defaultRstForMode(const QString& mode)
{
    const QString m = mode.trimmed().toUpper();
    if (m == QStringLiteral("CW") || m == QStringLiteral("RTTY") || m == QStringLiteral("PSK")) {
        return QStringLiteral("599");
    }
    return QStringLiteral("59");
}

// The exchange-sent values: own grid into whichever field is grid6-
// typed, the next serial (zero-padded to 3 digits) into whichever
// field is auto-incrementing, "59"/"599" (per `mode`) into whichever
// field is rst-typed. Shared by currentSentExchangeText() and
// handleLogRequested() so both compose the sent line the same way.
QMap<QString, QString> buildSentExchangeValues(const ContestDefinition& def, const ContestSettings& settings,
                                                int serialSent, const QString& mode)
{
    QMap<QString, QString> values;
    if (const ContestDefinition::ExchangeField* serialField = findAutoIncrementField(def)) {
        values.insert(serialField->key, QString::number(serialSent).rightJustified(3, QLatin1Char('0')));
    }
    if (const ContestDefinition::ExchangeField* gridField = findFieldByType(def, QStringLiteral("grid6"))) {
        values.insert(gridField->key, settings.ownGrid);
    }
    if (const ContestDefinition::ExchangeField* rstField = findFieldByType(def, QStringLiteral("rst"))) {
        values.insert(rstField->key, defaultRstForMode(mode));
    }
    return values;
}

// The exchange-received values straight from UnifiedLogWidget::
// exchangeReceived(), with the same zero-padding applied to whichever
// field is auto-incrementing (grid6-typed fields are already
// upper-cased by exchangeReceived() itself). Empty fields are dropped
// rather than composed as a gap (composeExchange does that too, this
// just avoids handing it a value that is only whitespace).
QMap<QString, QString> buildReceivedExchangeValues(const ContestDefinition& def, const QMap<QString, QString>& exchangeReceived)
{
    QMap<QString, QString> values;
    for (const ContestDefinition::ExchangeField& field : def.exchangeFields()) {
        QString value = exchangeReceived.value(field.key).trimmed();
        if (value.isEmpty()) {
            continue;
        }
        if (field.autoIncrement) {
            bool ok = false;
            const int number = value.toInt(&ok);
            if (ok && number > 0) {
                value = QString::number(number).rightJustified(3, QLatin1Char('0'));
            }
        }
        values.insert(field.key, value);
    }
    return values;
}

// A bearing is meaningless between two stations in the same locator
// square (centre to centre is 0 km; the maths returned "180" for the
// operator's own-square test QSO, 2026-09-21) -- unknown is a dash,
// HAUSSTIL rule 7, not a fabricated heading.
std::optional<double> bearingIfApart(double distanceKm, double bearingDeg)
{
    if (distanceKm < 0.5) {
        return std::nullopt;
    }
    return bearingDeg;
}

// Type-aware split of a hand-edited received exchange ("Nr./Grid" cell,
// see UnifiedLogWidget's setData): the RST is the record's own (that
// column is not editable, and setData prepends it verbatim when it is
// set), everything after it goes to the field it LOOKS like -- a
// locator to grid6, an integer to the serial. Positional parsing lost
// data in the operator's own log (2026-09-21): "JN67UT" typed alone
// was taken as a failed serial and the locator vanished; "59003" on a
// row without RST became the RST.
struct EditedExchange {
    QString rst;
    std::optional<int> serial;
    QString grid;
};

EditedExchange parseEditedExchange(const ContestDefinition& def, const QString& knownRst, const QString& text)
{
    EditedExchange out;
    QStringList tokens = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (findFieldByType(def, QStringLiteral("rst")) && !knownRst.isEmpty() && !tokens.isEmpty()
        && tokens.first().compare(knownRst, Qt::CaseInsensitive) == 0) {
        out.rst = knownRst;
        tokens.removeFirst();
    }
    const bool hasSerial = findAutoIncrementField(def) != nullptr;
    const bool hasGrid = findFieldByType(def, QStringLiteral("grid6")) != nullptr;
    for (const QString& token : tokens) {
        if (hasGrid && out.grid.isEmpty() && isValidGridSquare(token.toUpper())) {
            out.grid = token.toUpper();
            continue;
        }
        bool isNumber = false;
        const int number = token.toInt(&isNumber);
        if (hasSerial && !out.serial && isNumber && number > 0) {
            out.serial = number;
        }
    }
    return out;
}

QString rigctldStateText(RigctldClient::State state)
{
    switch (state) {
    case RigctldClient::State::Connected:    return QStringLiteral("verbunden");
    case RigctldClient::State::Connecting:   return QStringLiteral("verbindet...");
    case RigctldClient::State::Error:        return QStringLiteral("Fehler");
    case RigctldClient::State::Disconnected: return QStringLiteral("getrennt");
    }
    return QStringLiteral("getrennt");
}

QString operatingModeButtonText(ContestSettings::OperatingMode mode)
{
    return mode == ContestSettings::OperatingMode::Run
        ? QStringLiteral("Modus: Run")
        : QStringLiteral("Modus: S&P");
}

// A small connect/disconnected status badge, per HAUSSTIL rule 5
// ("Zustand steht als umrandete Kapsel"). Colours come from
// Style::badgeStyle() (the named kBadgeOkBg/kBadgeOffBg pairs), never
// an invented one-off colour.
QLabel* makeStatusBadge(QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

void setStatusBadge(QLabel* label, bool ok, const QString& text)
{
    label->setText(text);
    label->setStyleSheet(Style::badgeStyle(ok));
}

} // namespace

MainWindow::MainWindow(AppController& appController, QWidget* parent)
    : QMainWindow(parent)
    , m_appController(appController)
{
    setWindowTitle(QStringLiteral("Contestprogramm"));
    // App-wide dark palette comes from Style::appStyleSheet() (see
    // main.cpp); this window only needs the panel-level chrome below.

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    // Top bar: UTC clock + contest-end countdown, right-aligned, per the
    // plan's "UTC-Uhr + Countdown oben in der Titelzeile" UI direction
    // and the design mockup's clock treatment (Main.dc.html). The
    // countdown half's visibility is a live toggle further down (see
    // the filter row below); contestEndUtc/countdownVisible themselves
    // come from ContestSettings via applyClockSettings().
    auto* topBarRow = new QWidget(central);
    auto* topBarLayout = new QHBoxLayout(topBarRow);
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    topBarLayout->addStretch();
    // Same affordance PanelHeaderBar::setOptionsAffordanceEnabled() gives
    // individual panels (⚙, Style::iconButtonStyle()) -- "Einstellungen"
    // used to live only in Datei > Einstellungen..., buried behind the
    // menu bar. Martin's explicit ask: it must also sit directly on the
    // window itself. This is the one always-visible strip every layout
    // (including the map-dominant one) keeps fixed, so it is the natural
    // permanent home for it, right beside the clock it already anchors.
    auto* settingsButton = new QPushButton(QString::fromUtf8("⚙"), topBarRow);
    settingsButton->setFixedSize(24, 22);
    settingsButton->setCursor(Qt::PointingHandCursor);
    settingsButton->setToolTip(QStringLiteral("Einstellungen"));
    settingsButton->setStyleSheet(Style::iconButtonStyle());
    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);
    topBarLayout->addWidget(settingsButton);
    m_utcClockWidget = new UtcClockWidget(topBarRow);
    topBarLayout->addWidget(m_utcClockWidget);
    layout->addWidget(topBarRow);

    auto* filterRow = new QWidget(central);
    auto* filterLayout = new QHBoxLayout(filterRow);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    auto* filterLabel = new QLabel(QStringLiteral("Grid-Filter:"), filterRow);
    filterLabel->setFont(Style::capsFont(filterLabel->font()));
    filterLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextScale()));
    filterLayout->addWidget(filterLabel);
    m_gridFilterEdit = new QLineEdit(filterRow);
    m_gridFilterEdit->setMaximumWidth(120);
    m_gridFilterEdit->setFont(Style::monoFont(m_gridFilterEdit->font(), Style::kFontBody));
    filterLayout->addWidget(m_gridFilterEdit);
    auto* rawFeedCheck = new QCheckBox(QStringLiteral("Chat/Cluster: Rohdaten (ungefiltert)"), filterRow);
    filterLayout->addWidget(rawFeedCheck);
    // Same visual family as rawFeedCheck above -- two more independent
    // display toggles, not buried in SettingsDialog since (unlike most
    // settings there) the operator wants to flip these live, mid-session.
    auto* cwMacroVisibleCheck = new QCheckBox(QStringLiteral("CW-Makros anzeigen"), filterRow);
    cwMacroVisibleCheck->setChecked(m_appController.settings().cwMacroPanelVisible);
    filterLayout->addWidget(cwMacroVisibleCheck);
    auto* countdownVisibleCheck = new QCheckBox(QStringLiteral("Contest-Countdown anzeigen"), filterRow);
    countdownVisibleCheck->setChecked(m_appController.settings().countdownVisible);
    filterLayout->addWidget(countdownVisibleCheck);
    // Enter Sends Message (core/EsmPlanner.h) -- flipped live like the
    // two toggles above; the texts live under Datei > ESM-Texte.
    auto* esmCheck = new QCheckBox(QStringLiteral("ESM (Enter sendet, CW)"), filterRow);
    esmCheck->setChecked(m_appController.settings().esmEnabled);
    filterLayout->addWidget(esmCheck);
    // The transverter switch (core/Transverter.h): shown once a
    // transverter is set up (Datei > Transverter...), on = the rig's
    // IF is the band on the antenna.
    m_transverter = TransverterSetup::load(m_appController.database());
    m_appController.setTransverter(m_transverter);
    m_transverterCheck = new QCheckBox(filterRow);
    m_transverterCheck->setObjectName(QStringLiteral("transverterCheck"));
    filterLayout->addWidget(m_transverterCheck);
    syncTransverterCheck();
    connect(m_transverterCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_transverter.enabled == on) {
            return;
        }
        m_transverter.enabled = on;
        m_transverter.save(m_appController.database());
        m_appController.setTransverter(m_transverter);
        // The rig has not moved, but what its frequency means has.
        if (m_appController.rigctldClient().isConnected()) {
            applyRigFrequency(m_appController.rigctldClient().frequencyHz());
        }
        refreshBandmap();
        updateStatusBar();
    });
    filterLayout->addStretch();
    layout->addWidget(filterRow);

    // Kern-Welle "movable/resizable/dockable panels": everything below
    // used to be a fixed QVBoxLayout/QSplitter arrangement (entry bar in
    // a bare panel, CW row, a 3-way Log/ON4KST/Cluster QSplitter, a
    // rotor+map row, a bare rate-meter panel). It now lives in one
    // freely-arrangeable canvas instead -- see ui/PanelLayoutManager.h's
    // class comment for the scoping (a port of Longpath's own
    // ContainerWidget/ContainerManager, restricted to the single dock
    // mode -- absolute position within a canvas, like Longpath's
    // OverlayDocked -- this project actually needs) and ui/
    // PanelContainerWidget.h for the per-panel chrome. The UTC clock and
    // filter row above stay fixed, non-dockable strips -- small
    // persistent controls, not the operator "windows" Martin asked to
    // make movable/resizable/lockable. The QRects below are this
    // canvas's first-run default layout (roughly today's arrangement:
    // entry bar top, log/chat feeds in a row beneath it, rotor row/map/
    // rate meter below that) -- overwritten by whatever the operator
    // actually arranges from the second launch on, via
    // PanelLayoutManager's own persistence (ContestDatabase's `settings`
    // key/value table, the same store ContestSettings already uses).
    m_panelLayoutManager = new PanelLayoutManager(m_appController.database(), central, this);
    layout->addWidget(m_panelLayoutManager->canvas(), 1);

    // CW F-key macro row, per the plan's CW-Unterstützung nachziehen
    // item -- now the very top row of the canvas: the merged Log panel
    // below it absorbed the old standalone entry bar (Martin: "eingabe
    // sollte im gleichen fenster wie die logs sein" -- see
    // ui/UnifiedLogWidget.h), so there is no separate entry-bar row left
    // for this to sit under any more. Hidden by default (ContestSettings::
    // cwMacroPanelVisible) -- the operator wants it out of the way
    // unless actually operating CW; the "CW-Makros anzeigen" checkbox in
    // the filter row above shows/hides it and persists the choice (now
    // via m_cwMacroPanelContainer -- see the connect() below and the
    // class comment in MainWindow.h). This is purely display --
    // RigctldClient::sendMorse() and the macro templates themselves are
    // unaffected by the panel being hidden, and it stays part of the
    // dockable layout (position/size/lock persist) even while hidden,
    // rather than being torn down.
    auto* cwContent = new QWidget(this);
    auto* cwLayout = new QHBoxLayout(cwContent);
    cwLayout->setContentsMargins(0, 0, 0, 0);
    auto* cwLabel = new QLabel(QStringLiteral("CW:"), cwContent);
    cwLabel->setFont(Style::capsFont(cwLabel->font()));
    cwLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextScale()));
    cwLayout->addWidget(cwLabel);
    m_cwMacroPanel = new CwMacroPanel(cwContent);
    cwLayout->addWidget(m_cwMacroPanel);
    cwLayout->addStretch();
    // "cwMacroRow" is both this panel's PanelLayoutManager persistence
    // key and its objectName (see PanelContainerWidget's constructor) --
    // the same stable findChild<QWidget*>() test hook the pre-docking
    // bare m_cwRow widget already carried under that exact name.
    // Height 48 (was 56) -- part of the 2026-09-13 default-layout
    // shrink (see unifiedlog/rotorrow/map/suggestion/ratemeter's own
    // comments below): a real ~923px-tall screen (Martin's own, found
    // live) could not fit the old defaults' 1130px total canvas height
    // at all -- "Rate"/the chat panels ended up below the visible
    // window with no way to reach them short of "Fenster zurücksetzen"
    // plus manually resizing every panel. New total: 799px. A single
    // row of macro buttons has no real need for 56px; still comfortably
    // above any button's own natural minimum height.
    // Height 72, not 48: bench-found live, 2026-09-14 -- PanelContainerWidget::
    // kMinHeight is 72, a generic floor trySetGeometry() applies to every
    // panel regardless of its own registerPanel() default (see that
    // function's own comment), so a smaller request here was always
    // silently rounded back up to 72 anyway. Writing 72 here directly
    // keeps this literal honest about what the panel actually renders
    // at, instead of implying a shorter row than the one every other
    // panel's own y-coordinate below is actually measured against.
    m_cwMacroPanelContainer = m_panelLayoutManager->registerPanel(
        QStringLiteral("cwMacroRow"), QStringLiteral("CW-Makros"), cwContent,
        /*contentHasOwnChrome=*/false, QRect(0, 0, 1440, 72), QRect(0, 0, 1372, 72));
    m_cwMacroPanelContainer->setVisible(m_appController.settings().cwMacroPanelVisible);

    // The merged Log panel: EntryBarWidget + LogTableView + both
    // ChatFeedViews collapsed into this one dockable panel -- see
    // ui/UnifiedLogWidget.h's class comment for the full rationale.
    // Registered under a NEW id ("unifiedlog"), deliberately NOT the old
    // "log" id reused: Martin's real, already-running database has a
    // PanelLayout_log row left over from the narrow (698x537) old
    // log-only panel (confirmed live: `SELECT * FROM settings WHERE
    // key='PanelLayout_log'` in ~/Library/Application Support/
    // Contestprogramm/Contestprogramm/contestprogramm.sqlite) -- reusing
    // "log" would have silently loaded that stale narrow geometry for
    // this much bigger merged panel instead of the generous default
    // below. A fresh id sidesteps the collision entirely; see this
    // task's report for the full judgment call. The old "entry"/
    // "chat_on4kst"/"chat_cluster" PanelLayout_* rows are simply never
    // looked up again now that those ids are not re-registered --
    // harmless orphaned settings rows, not a crash risk (see
    // PanelLayoutManager::finalizeInitialLayout()'s panel(id)-not-found
    // guard).
    //
    // Full canvas width, matching the "Log wird zur schmalen unteren
    // Leiste" direction from the 2026-09-13/14 kartendominante-
    // Anordnung proposal round (operator picked "A -- Kartenreihe
    // groß"): this panel moves from its old near-top slab down to a
    // slim strip along the very bottom of the canvas (y=585, see the
    // map/rotorrow/suggestion/ratemeter comments below for how their
    // combined bottom edge frees this space), trading log height for
    // the map's own. 95px still shows the entry row and the status line
    // before scrolling -- the feed table itself was always scrollable,
    // so this only changes how much is visible up front. Bottom edge
    // 585+95=680 -- see the map panel's own comment for why that stays
    // inside a real ~690px-tall canvas rather than the more generous
    // 799/950 the file's older comments assumed.
    m_logModel = new LogTableModel(this);
    m_unifiedLog = new UnifiedLogWidget(this);
    m_unifiedLog->setLogModel(m_logModel);
    m_unifiedLog->setChatModels(&m_appController.on4kstFeedModel(), &m_appController.clusterFeedModel());
    PanelContainerWidget* logContainer = m_panelLayoutManager->registerPanel(
        QStringLiteral("unifiedlog"), QStringLiteral("Log"), m_unifiedLog,
        /*contentHasOwnChrome=*/false, QRect(0, 585, 1440, 95),
        // Compact design (PanelLayoutManager::kCompactDesignCanvas,
        // 1372×692): rotors and map on top, the log across the bottom
        // with room for a few history rows, the bandmap beside it.
        QRect(0, 450, 1010, 242));
    // A live clock fixed to the Log panel's own header -- operator,
    // 2026-09-11: "uhrzeit fehlt, muss fix beim logfenster sein",
    // matching DXLog.net's own "Contest recorder" window, which shows
    // time right inside the log window itself, not only in a separate
    // global title bar (the top bar's own UtcClockWidget stays as-is).
    if (logContainer && logContainer->headerBar()) {
        logContainer->headerBar()->setClockVisible(true);
        // The Log panel's own ⚙ view-mode affordance (Kompakt / DXLog-
        // Vollspalten) -- operator, same 2026-09-11 DXLog.net-screenshot
        // follow-up as the clock above, picking proposals "1 und 2".
        // PanelHeaderBar has no notion of what its panel's content can
        // show (see its own setOptionsAffordanceEnabled() doc comment),
        // so MainWindow -- which owns both m_unifiedLog and
        // m_appController.settings() -- builds and shows the menu
        // itself, same "caller decides" contract RotorWidget's own ⚙
        // popup already uses one level down.
        logContainer->headerBar()->setOptionsAffordanceEnabled(true);
        m_logHeaderBar = logContainer->headerBar();
        connect(m_logHeaderBar, &PanelHeaderBar::optionsRequested, this, &MainWindow::showLogViewOptionsPopup);
    }

    // Up to two independent rotor compasses, per the plan's Phase 3
    // direction ("die UI zeigt entsprechend zwei Kompass-Widgets
    // nebeneinander") -- generalized so either slot can be disabled
    // entirely (see ContestSettings::rotor1Enabled/rotor2Enabled). The
    // widgets themselves are created lazily by applyRotorWidgetSettings()
    // below, not here -- this row starts out holding only the trailing
    // stretch so insertWidget(0, ...)/insertWidget(1, ...) there always
    // lands a newly (re-)enabled slot in the right place regardless of
    // which slot(s) already exist. Each RotorWidget paints its own full
    // panel chrome (background, border, header bar) itself -- see
    // RotorWidget::paintEvent() -- so this panel's own header (registered
    // below) reads as one instrument *group*, not a doubled header; the
    // dockable-panel system tolerates the rotor slots appearing/
    // disappearing at runtime the same way the old fixed layout did,
    // since applyRotorSlot() below still only ever touches m_rotorLayout,
    // never this panel's own container.
    m_rotorRow = new QWidget(this);
    m_rotorLayout = new QHBoxLayout(m_rotorRow);
    m_rotorLayout->setContentsMargins(0, 0, 0, 0);
    m_rotorLayout->setSpacing(10);
    m_rotorLayout->addStretch();
    // y=632: below the merged Log panel's own bottom edge (64+560=624)
    // plus an 8px gap.
    // Height 370 (was 272, originally 236): RotorWidget::minimumSizeHint()
    // grew again this session when the Digital dial style (RotorDialStyle::
    // Digital) was added -- its own glass-panel readout
    // (kDigitalTextAreaHeight, RotorWidget.cpp) is taller than the other
    // three styles' shared plain-text kTextAreaHeight footer, and this
    // pass's own live-app verification screenshot found the old 272px
    // default clipping Digital's ENTFERNUNG panel, target-station
    // caption and connection-status row entirely (PanelContainerWidget's
    // generic kMinWidth/kMinHeight resize floor has no idea what a
    // specific content widget actually needs, so an outgrown default
    // just silently clips instead of refusing to shrink). 370, not a
    // literal 440 (the un-trimmed sum of Digital's own geometry
    // constants) -- that same verification pass also found 440 pushed
    // the panel's TOP off the visible canvas on a real, already-
    // populated layout, so RotorWidget.cpp's own Digital padding/gap
    // constants were tightened first (see their own comment) to bring
    // the actual minimum down to a number that fits real screens, not
    // just grown blindly to match the mockup's literal, unconstrained
    // padding. Same re-tuning practice as the 236->272 bump before it
    // (RotorWidget's own three-column AKTUELL/ZIEL/ENTFERNUNG block) and
    // UnifiedLogWidget's own entry-row width re-tune: grow the default
    // to fit what the content now actually draws, rather than leaving a
    // stale number that silently clips again.
    // x=0, width=620 (was 708, 300): that same earlier readout pass also
    // grew RotorWidget's own minimum WIDTH, 220 -> 300 (RotorWidget.cpp's
    // kReadoutMinWidth -- the three big AKTUELL/ZIEL/ENTFERNUNG numbers
    // measure ~90px each at kFontDisplay and were overlapping at the old
    // 220px). With both rotor slots enabled (the default), two widgets
    // at 300px plus m_rotorLayout's 10px spacing need >=610px, which the
    // old 300px-wide row/708 x-offset could never have fit even before
    // this pass (a pre-existing mismatch this pass's own live-app
    // verification screenshot exposed). Moved to x=0 (rather than
    // widened in place) so its new right edge -- 620 -- stays clear of
    // the Karte panel at x=1016 without having to also move that
    // unrelated panel; still directly below the Log panel above it
    // (x=0..1276), same as before.
    // y=78 (was 326) -- 2026-09-13/14, kartendominante Anordnung ("A --
    // Kartenreihe groß", operator's pick from the 3-proposal round):
    // this row moves back up to sit directly under cwMacroRow (0+72=72
    // plus a 6px gap -- see that panel's own comment for why 72, not
    // 48), the same slot unifiedlog used to occupy -- the two swapped
    // places, freeing unifiedlog's old near-top slab for the map below
    // to grow into instead. Width/height unchanged (620x365):
    // RotorWidget::minimumSizeHint()'s own height (kHeaderHeight + 220 +
    // kTextAreaHeight = 28+220+114 = 362px) is a hard floor -- shrinking
    // this row below it would squeeze the dial/readout below what
    // RotorWidget itself can legibly draw, not just make the panel
    // smaller. This row's own bottom edge (78+365=443) is deliberately
    // NOT where the map/suggestion/rate column ends -- see the map
    // panel's own comment below for why they run taller.
    m_rotorRowContainer = m_panelLayoutManager->registerPanel(
        QStringLiteral("rotorrow"), QStringLiteral("Rotoren"), m_rotorRow,
        /*contentHasOwnChrome=*/false, QRect(0, 78, 620, 365), QRect(0, 0, 620, 250));

    // MapWidget ("Karte / Verbindungen") is its own panel now -- it used
    // to share m_rotorRow with the rotor compasses, but this wave's own
    // panel list separates it out (see this wave's design notes). It
    // paints its own full panel chrome itself (background, border,
    // header bar with its own title) -- see MapWidget::paintEvent() --
    // so it registers chromeless, the same reasoning each individual
    // RotorWidget uses one level down inside the rotor row above.
    m_mapWidget = new MapWidget(this);
    // View and layer toggles live in the settings table; a fresh
    // install starts on the radar (the design sheet the operator chose,
    // 2026-09-20) -- each view's own layer defaults are the widget's.
    m_mapWidget->applyPreferencesText(m_appController.database().settingValue(
        QStringLiteral("map_preferences"), QStringLiteral("view=radar")));
    connect(m_mapWidget, &MapWidget::preferencesChanged, this, [this] {
        m_appController.database().setSettingValue(QStringLiteral("map_preferences"), m_mapWidget->preferencesText());
        // The beamwidth set in the map's ⚙ menu is also the cone the
        // rotor dials draw -- one number, two instruments.
        applyRotorBeamwidths();
    });
    // "Zweitantenne" from the map's ⚙ menu is the station setting the
    // settings dialog edits: stored once, then re-applied to the rotor
    // dials and the map alike.
    connect(m_mapWidget, &MapWidget::secondAntennaToggled, this, [this](int rotor, bool enabled) {
        ContestSettings settings = m_appController.settings();
        (rotor == 1 ? settings.rotor1SecondAntennaEnabled : settings.rotor2SecondAntennaEnabled) = enabled;
        m_appController.setSettings(settings);
        applyRotorWidgetSettings();
    });
    // 2026-09-13/14, kartendominante Anordnung ("A -- Kartenreihe
    // groß"): the map becomes the single biggest panel on the canvas,
    // both wider AND taller than before. x=910: right of rotorrow
    // (0-620) and the suggestion/ratemeter column (630-900, see those
    // panels' own comments for why 270 not 300), the same 10px gap the
    // row already uses elsewhere.
    // Width 530 (was 336, then briefly 660, then 500) -- 660 was picked
    // 2026-09-14 after the settings row grew a 9th checkbox and
    // overflowed 336px, but that pass also pushed the *window's own*
    // default width to 1600, which turned out to be the real mistake:
    // this operator's screen is only 1470 points wide (confirmed live via
    // `osascript -e 'tell application "Finder" to get bounds of window of
    // desktop'`, not guessed), so every launch since then had macOS
    // squeeze the whole layout back down to fit -- a different-looking
    // result each time depending on exactly how that squeeze landed,
    // which read as "verschiebt sich von selbst" but was really just a
    // default that never fit the actual screen. 500 was the first
    // corrected width (940+500=1440), but still left this panel's own
    // settings row visibly tight even after abbreviating every label
    // (see buildSettingsRow()'s own comment) -- 530 (910+530=1440, same
    // total) claims back the 30px freed by narrowing the suggestion/
    // ratemeter column instead, real width traded for real width rather
    // than exceeding the screen again.
    // y=78 matches rotorrow's own top edge (see its own comment for why
    // 78, not 56). Height 501 -- taller than rotorrow's own row (365)
    // because this panel's bottom edge (78+501=579) is set to match the
    // suggestion/ratemeter column's own combined bottom edge (see those
    // panels' comments below), not rotorrow's -- the map visually spans
    // both the "instrument row" and the space below it that would
    // otherwise sit empty, which is the actual mechanism behind "Karte
    // übernimmt den Rest der Zeile plus eine zweite Reihe darunter":
    // one tall panel, not two stacked map panels. Bench-found live,
    // 2026-09-13/14: a real saved window on a real laptop screen
    // restores to a canvas only ~690px tall (not the 799-950 earlier
    // comments in this file assumed) -- 501 keeps the whole layout's
    // bottom edge (see unifiedlog's own comment) inside that real
    // ceiling with a real safety margin, instead of relying on
    // clampPanelsToCanvas() to trim the overflow at random. Only the
    // WIDTH changes in this pass -- that real ~690px height ceiling is
    // unrelated to and unaffected by the width fix above.
    // Header mode: the container's PanelHeaderBar carries title, lock
    // and the ⚙ -- top right with its own symbol like every other panel
    // (operator, 2026-09-20) -- which opens the map's own layer menu.
    PanelContainerWidget* mapContainer = m_panelLayoutManager->registerPanel(
        QStringLiteral("map"), QStringLiteral("Karte / Verbindungen"), m_mapWidget,
        /*contentHasOwnChrome=*/false, QRect(910, 78, 530, 501), QRect(630, 0, 742, 442));
    if (mapContainer && mapContainer->headerBar()) {
        mapContainer->headerBar()->setOptionsAffordanceEnabled(true);
        connect(mapContainer->headerBar(), &PanelHeaderBar::optionsRequested, this, [this, mapContainer] {
            // A fresh menu per click, parented to the main window and
            // gone on close -- the same shape as the Log panel's ⚙
            // popup (see showLogViewOptionsPopup). A QMenu kept alive as
            // a child of the map widget crashed inside Cocoa's popup
            // path (stale platform window) once the widget had been
            // re-parented into its panel container.
            auto* menu = new QMenu(this);
            menu->setAttribute(Qt::WA_DeleteOnClose);
            m_mapWidget->populateOptionsMenu(menu);
            PanelHeaderBar* header = mapContainer->headerBar();
            const QSize menuSize = menu->sizeHint();
            menu->popup(header->mapToGlobal(QPoint(header->width() - menuSize.width() - 6, header->height())));
        });
    }
    // Clicking a station marker on the map -- same signal shape (and the
    // same consumer, which also commands the rotor when one is
    // configured) ChatFeedView/UnifiedLogWidget's own candidateActivated
    // already uses. Operator, 2026-09-13: "klickbare Stationen".
    connect(m_mapWidget, &MapWidget::candidateActivated, this, &MainWindow::handleCandidateActivated);

    // Betriebsassistent (plan section of the same name) -- the one
    // still-open "Kern" item from the plan's priority list. Sits in the
    // gap between rotorrow's own right edge (0+620=620) and the map
    // panel's left edge (940), same y as rotorrow (see its own
    // 2026-09-13/14 comment above) so their top edges line up. Width
    // 340 -> 300 (kartendominante Anordnung): gives the freed 40px to
    // the map panel. 300, not narrower -- SuggestionPanel's own button
    // row ("Ziel übernehmen" + "Ruf senden", see its own buttonLayout)
    // needs roughly 220px before the panel's 14px side margins even
    // start, so 300 keeps real slack rather than running the buttons
    // flush against the panel edge. Height still 365, same as rotorrow
    // -- this panel's own ratemeter neighbour picks up the extra
    // vertical room instead (see ratemeter's own comment below).
    m_suggestionPanel = new SuggestionPanel(this);
    // Width 300->270, same 2026-09-14 screen-width-budget pass as the map
    // panel's own comment -- still a real ~26px of slack past the ~248px
    // floor this panel's own button row needs (220 + 2*14 margins, see
    // this class's original comment above), just less of it, freeing 30px
    // for the map's settings row instead.
    m_panelLayoutManager->registerPanel(QStringLiteral("suggestion"), QStringLiteral("Nächstes Ziel"),
                                         m_suggestionPanel, /*contentHasOwnChrome=*/false, QRect(630, 78, 270, 365),
                                         QRect(0, 258, 310, 184));
    connect(m_suggestionPanel, &SuggestionPanel::targetAccepted, this, &MainWindow::handleCandidateActivated);
    connect(m_suggestionPanel, &SuggestionPanel::sendRequested, this, &MainWindow::handleSuggestionSendRequested);

    // Rate meter now gets a real header -- it used to be a bare,
    // header-less panel (RateMeterWidget's own applyPanelFrameStyle()
    // border only), but a dockable panel needs a header to drag by, the
    // same reasoning the merged Log panel above gained one for.
    m_rateMeterWidget = new RateMeterWidget(this);
    // 2026-09-13/14, kartendominante Anordnung ("A -- Kartenreihe
    // groß"): ratemeter gives up its own old full-width bottom row
    // entirely -- that whole row now belongs to unifiedlog's new
    // bottom-strip placement (see its own comment above) -- and moves
    // into the suggestion panel's column instead, directly below it.
    // x/width match suggestion (630, 300) so the column's left/right
    // edges line up; y=449 is suggestion's own bottom edge (78+365)
    // plus a 6px gap (see rotorrow's own comment for why 6, not 8).
    // Height 130 -- RateMeterWidget's own labels gained
    // setWordWrap(true) in this same pass (see that file's own comment),
    // so this fits its three content rows without the old full-width
    // row's extra slack. This sets the map panel's own bottom edge two
    // panels over (449+130=579 -- see the map panel's own comment for
    // why that number matters). A machine with an already-persisted
    // "ratemeter" geometry from before this change keeps its own saved
    // position (PanelLayoutManager persists per-panel) until "Fenster
    // zurücksetzen"; this default only affects a fresh install/reset.
    // Width 270, matching suggestion's own column above (see that
    // registerPanel() call's own comment for why 270, not 300).
    m_panelLayoutManager->registerPanel(QStringLiteral("ratemeter"), QStringLiteral("Rate"), m_rateMeterWidget,
                                         /*contentHasOwnChrome=*/false, QRect(630, 449, 270, 130),
                                         QRect(318, 258, 302, 184));
    // Check Partial, under the rate panel in the same 270px column --
    // hidden in every already-saved profile (LayoutProfileManager's
    // "a panel the profile never saved starts hidden"), switched on
    // via Fenster > Panels > Check.
    m_checkPartialWidget = new CheckPartialWidget(this);
    connect(m_checkPartialWidget, &CheckPartialWidget::callsignChosen, this,
            [this](const QString& callsign, const QString& grid) { handleCandidateActivated(callsign, grid, 0); });
    connect(m_checkPartialWidget, &CheckPartialWidget::scpLoadRequested, this, &MainWindow::loadScpFile);
    m_panelLayoutManager->registerPanel(QStringLiteral("checkpartial"), QStringLiteral("Check"), m_checkPartialWidget,
                                         /*contentHasOwnChrome=*/false, QRect(630, 585, 270, 150));
    // Bandmap -- same "starts hidden in a saved profile" rule as Check.
    m_bandmapWidget = new BandmapWidget(this);
    connect(m_bandmapWidget, &BandmapWidget::spotActivated, this,
            [this](const QString& callsign, const QString& grid, qint64 freqHz) {
        // A bandmap click is a QSY: tune first, then fill the entry row
        // the way a feed-row click does (rotor included).
        if (m_appController.rigctldClient().isConnected() && freqHz > 0) {
            // Through the transverter: 1296.300 on the antenna is
            // 144.300 on the rig.
            m_appController.rigctldClient().setFrequency(m_transverter.rigFrequencyHz(freqHz));
        }
        handleCandidateActivated(callsign, grid, freqHz);
    });
    m_panelLayoutManager->registerPanel(QStringLiteral("bandmap"), QStringLiteral("Bandmap"), m_bandmapWidget,
                                         /*contentHasOwnChrome=*/false, QRect(910, 520, 250, 260),
                                         QRect(1018, 450, 354, 242));
    // Skeds (design sheet B, 2026-09-18) -- hidden in saved profiles
    // like Check and Bandmap.
    m_skedPanel = new SkedPanel(this);
    connect(m_skedPanel, &SkedPanel::addRequested, this, &MainWindow::addSkedFromEntry);
    connect(m_skedPanel, &SkedPanel::skedActivated, this, &MainWindow::activateSked);
    connect(m_skedPanel, &SkedPanel::suggestionAccepted, this, [this](int skedId) {
        m_appController.database().updateSkedState(skedId, Sked::State::Open);
        reloadSkeds();
    });
    connect(m_skedPanel, &SkedPanel::deleteRequested, this, [this](int skedId) {
        m_appController.database().deleteSked(skedId);
        reloadSkeds();
    });
    m_panelLayoutManager->registerPanel(QStringLiteral("skeds"), QStringLiteral("Skeds"), m_skedPanel,
                                         /*contentHasOwnChrome=*/false, QRect(0, 720, 620, 262));

    m_panelLayoutManager->finalizeInitialLayout();

    // Left-side profile rail, "wie bei longpath" (operator, 2026-09-14)
    // -- Longpath's own ProfileRail sits at the very left, full window
    // height, outside its central content column
    // (MainWindow.cpp:4089: "Profilschiene ganz links über die volle
    // Höhe, wie bei Zeus"); reproduced here the same way, wrapping the
    // existing `central` (toolbar rows + panel canvas) rather than
    // living inside it. Constructed AFTER finalizeInitialLayout() so
    // LayoutProfileManager's first-run migration snapshots each panel's
    // already-restored (legacy PanelLayout_*) geometry as profile "1",
    // not whatever placeholder state a not-yet-laid-out panel would
    // otherwise report.
    m_layoutProfileManager = new LayoutProfileManager(
        m_appController.database(), *m_panelLayoutManager,
        {QStringLiteral("unifiedlog"), QStringLiteral("rotorrow"), QStringLiteral("map"),
         QStringLiteral("suggestion"), QStringLiteral("ratemeter"), QStringLiteral("checkpartial"),
         QStringLiteral("bandmap"), QStringLiteral("skeds")},
        this);
    // A fresh database only: the panels get the design that fits the
    // canvas once it has its real size, and profile "1" is snapshotted
    // again from that (its constructor's snapshot saw the unshown
    // window) -- see PanelLayoutManager::kCompactDesignCanvas.
    m_panelLayoutManager->setFreshInstall(!m_panelLayoutManager->hadSavedLayout()
                                          && m_layoutProfileManager->isFirstLaunch());
    connect(m_panelLayoutManager, &PanelLayoutManager::initialDesignApplied, this,
            [this] { m_layoutProfileManager->saveActiveProfileState(); });
    m_profileRail = new ProfileRail(this);
    m_profileRail->setProfiles(m_layoutProfileManager->profileNames(), m_layoutProfileManager->activeProfile());
    connect(m_layoutProfileManager, &LayoutProfileManager::profilesChanged, this, [this]() {
        m_profileRail->setProfiles(m_layoutProfileManager->profileNames(), m_layoutProfileManager->activeProfile());
    });
    connect(m_profileRail, &ProfileRail::profileActivated, this,
            [this](const QString& name) { m_layoutProfileManager->switchTo(name); });
    connect(m_profileRail, &ProfileRail::newProfileRequested, this,
            [this]() { m_layoutProfileManager->createProfile(); });
    connect(m_profileRail, &ProfileRail::duplicateRequested, this,
            [this](const QString& name) { m_layoutProfileManager->duplicateProfile(name); });
    connect(m_profileRail, &ProfileRail::removeRequested, this,
            [this](const QString& name) { m_layoutProfileManager->removeProfile(name); });
    connect(m_profileRail, &ProfileRail::renameRequested, this, &MainWindow::handleProfileRenameRequested);

    auto* outer = new QWidget(this);
    auto* outerLayout = new QHBoxLayout(outer);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(m_profileRail);
    outerLayout->addWidget(central, 1);
    setCentralWidget(outer);

    m_rigctldStatusLabel = makeStatusBadge(this);
    m_on4kstStatusLabel = makeStatusBadge(this);
    m_clusterStatusLabel = makeStatusBadge(this);
    m_gridRadiusLabel = new QLabel(this);
    m_gridRadiusLabel->setFont(Style::monoFont(m_gridRadiusLabel->font(), Style::kFontSmall));
    m_gridRadiusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextTertiary()));
    m_weatherStatusLabel = new QLabel(this);
    m_weatherStatusLabel->setFont(Style::monoFont(m_weatherStatusLabel->font(), Style::kFontSmall));
    m_weatherStatusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextTertiary()));
    m_weatherStatusLabel->setText(QStringLiteral("Wetter: %1").arg(Style::unknownDash()));
    m_modeToggleButton = new QPushButton(this);
    statusBar()->addPermanentWidget(m_rigctldStatusLabel);
    statusBar()->addPermanentWidget(m_on4kstStatusLabel);
    statusBar()->addPermanentWidget(m_clusterStatusLabel);
    statusBar()->addPermanentWidget(m_gridRadiusLabel);
    statusBar()->addPermanentWidget(m_weatherStatusLabel);
    statusBar()->addPermanentWidget(m_modeToggleButton);

    // Rough tropo-ducting indicator (plan's "Wetter-/Tropo-Daten"
    // section) -- readingChanged fires on every successful Open-Meteo
    // fetch; a failed fetch (WeatherClient::fetchFailed) deliberately
    // has no handler here, leaving the last-known reading (or the
    // startup unknown-dash text above) on screen rather than blanking
    // it on a transient network hiccup -- see WeatherClient::
    // fetchFailed()'s own doc comment.
    connect(&m_appController.weatherClient(), &WeatherClient::readingChanged, this,
            [this](const WeatherReading& reading) {
                // QString::arg() has no printf-style "%%" escape -- a
                // literal "%" needs no escaping at all, only the "%1"-
                // style placeholders are special.
                m_weatherStatusLabel->setText(QStringLiteral("Wetter: %1°C · %2% · %3 hPa")
                                                   .arg(QString::number(reading.temperatureC, 'f', 1),
                                                        QString::number(reading.humidityPercent, 'f', 0),
                                                        QString::number(reading.pressureMslHpa, 'f', 0)));
            });

    connect(m_unifiedLog, &UnifiedLogWidget::logRequested, this, &MainWindow::handleLogRequested);
    connect(m_unifiedLog, &UnifiedLogWidget::formChanged, this, &MainWindow::recheckDupeIndicator);
    connect(m_unifiedLog, &UnifiedLogWidget::formChanged, this, &MainWindow::refreshCheckPartial);
    connect(m_unifiedLog, &UnifiedLogWidget::formChanged, this, [this] { m_incompleteExchangeEnterArmed = false; });
    // Calls heard on ON4KST/cluster feed the Check panel's "Seen" source
    // -- the people actually around tonight, ahead of any static list.
    const auto noteSeenCall = [this](const SpotCandidate& candidate) {
        m_checkPartialIndex.addSeenCall(candidate.callsign, candidate.grid);
    };
    connect(&m_appController.on4kstClient(), &On4kstClient::spotReceived, this, noteSeenCall);
    connect(&m_appController.on4kstClient(), &On4kstClient::chatLineReceived, this, noteSeenCall);
    connect(&m_appController.dxClusterClient(), &DxClusterClient::spotReceived, this, noteSeenCall);
    connect(m_unifiedLog, &UnifiedLogWidget::callsignLookupRequested, this, &MainWindow::handleCallsignLookupRequested);
    connect(m_unifiedLog, &UnifiedLogWidget::receivedGridChanged, this, &MainWindow::handleReceivedGridChanged);
    connect(m_unifiedLog, &UnifiedLogWidget::candidateActivated, this, &MainWindow::handleCandidateActivated);
    connect(m_unifiedLog, &UnifiedLogWidget::historyCallsignEditRequested, this, &MainWindow::handleHistoryCallsignEditRequested);
    connect(m_unifiedLog, &UnifiedLogWidget::historyExchangeRcvdEditRequested, this, &MainWindow::handleHistoryExchangeRcvdEditRequested);
    connect(m_unifiedLog, &UnifiedLogWidget::historyInvalidToggleRequested, this, &MainWindow::handleHistoryInvalidToggleRequested);
    connect(m_unifiedLog, &UnifiedLogWidget::historyTimeEditRequested, this, &MainWindow::handleHistoryTimeEditRequested);
    // The five-minute log backup (AppController's LogBackup) reports
    // into the status bar: a written copy briefly, a failure for longer
    // -- a modal box every five minutes would be worse than the fault.
    if (LogBackup* backup = m_appController.logBackup()) {
        connect(backup, &LogBackup::backupWritten, this, [this](const QString& path) {
            statusBar()->showMessage(QStringLiteral("Log gesichert: %1").arg(QFileInfo(path).fileName()), 5000);
        });
        connect(backup, &LogBackup::backupFailed, this, [this](const QString& error) {
            statusBar()->showMessage(QStringLiteral("Log-Sicherung fehlgeschlagen: %1").arg(error), 20000);
        });
    }
    connect(m_unifiedLog, &UnifiedLogWidget::chatMessageSendRequested, this, &MainWindow::handleSuggestionSendRequested);
    connect(m_unifiedLog, &UnifiedLogWidget::cqDraftRequested, this, &MainWindow::handleCqDraftRequested);
    connect(m_unifiedLog, &UnifiedLogWidget::awayStateChanged, this, &MainWindow::handleAwayToggled);
    // Tier 3 (QRZ/HamQTH) result arrives later, asynchronously -- see
    // handleCallsignLookupRequested()/handleExternalCallsignLookupFinished()
    // below.
    connect(&m_appController.callsignLocatorLookup(), &CallsignLocatorLookup::externalLookupFinished,
            this, &MainWindow::handleExternalCallsignLookupFinished);
    connect(m_gridFilterEdit, &QLineEdit::textChanged, m_unifiedLog, &UnifiedLogWidget::setGridFilter);
    connect(rawFeedCheck, &QCheckBox::toggled, &m_appController.on4kstFeedModel(), &ChatFeedModel::setShowRawFeed);
    connect(rawFeedCheck, &QCheckBox::toggled, &m_appController.clusterFeedModel(), &ChatFeedModel::setShowRawFeed);
    connect(cwMacroVisibleCheck, &QCheckBox::toggled, this, [this](bool visible) {
        m_cwMacroPanelContainer->setVisible(visible);
        ContestSettings settings = m_appController.settings();
        settings.cwMacroPanelVisible = visible;
        m_appController.setSettings(settings);
    });
    connect(countdownVisibleCheck, &QCheckBox::toggled, this, [this](bool visible) {
        m_utcClockWidget->setCountdownVisible(visible);
        ContestSettings settings = m_appController.settings();
        settings.countdownVisible = visible;
        m_appController.setSettings(settings);
    });
    connect(esmCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        ContestSettings settings = m_appController.settings();
        settings.esmEnabled = enabled;
        m_appController.setSettings(settings);
    });
    connect(m_modeToggleButton, &QPushButton::clicked, this, &MainWindow::toggleOperatingMode);

    // Rate-derived adaptive chat filtering (plan's "Adaptive
    // Chat-Filterung" (c)): forward the last-10-minutes count
    // RateMeterWidget already computes into both feed models rather
    // than duplicating the qsoCountSince() query here.
    connect(m_rateMeterWidget, &RateMeterWidget::last10MinRateChanged, this, [this](int count) {
        m_appController.on4kstFeedModel().setCurrentRatePerTenMinutes(count);
        m_appController.clusterFeedModel().setCurrentRatePerTenMinutes(count);
    });

    connect(&m_appController.rigctldClient(), &RigctldClient::stateChanged, this, &MainWindow::updateStatusBar);
    connect(&m_appController.rigctldClient(), &RigctldClient::frequencyChanged, this, [this](qint64 hz) {
        // Run-mode guard, per ContestSettings::OperatingMode: a
        // background VFO tick must not stomp on an in-progress
        // exchange mid-pileup while running. SearchAndPounce keeps
        // today's always-autofill behavior.
        if (m_appController.settings().operatingMode == ContestSettings::OperatingMode::Run
            && m_unifiedLog->hasUnsentContent()) {
            return;
        }
        applyRigFrequency(hz);
    });
    connect(&m_appController.rigctldClient(), &RigctldClient::modeChanged, this,
            [this](const QString& mode, int /*passbandHz*/) {
        if (m_appController.settings().operatingMode == ContestSettings::OperatingMode::Run
            && m_unifiedLog->hasUnsentContent()) {
            return;
        }
        const QString contestMode = contestModeForRigctldMode(mode);
        if (!contestMode.isEmpty()) {
            // Mode has no visible UI control any more (see the class
            // comment in MainWindow.h) -- this is the one and only
            // place m_currentMode is set from a live radio, mirroring
            // frequencyChanged's own m_currentBand assignment just
            // above.
            m_currentMode = contestMode;
            m_unifiedLog->setCurrentMode(contestMode);
            updateStatusBar();
        }
    });

    connect(&m_appController.on4kstClient(), &On4kstClient::connected, this, &MainWindow::updateStatusBar);
    connect(&m_appController.on4kstClient(), &On4kstClient::disconnected, this, [this]() {
        // The next login always restarts in the default 144/432 room
        // (On4kstClient::kChatIdVhfUhf) -- forget the last-synced room
        // so a reconnect's own loggedIn signal below re-evaluates
        // m_currentBand instead of skipping a switch it thinks already
        // happened.
        m_currentOn4kstRoom.clear();
        updateStatusBar();
    });
    connect(&m_appController.on4kstClient(), &On4kstClient::loggedIn, this, [this](int /*chatId*/) {
        syncOn4kstRoomForCurrentBand();
        updateStatusBar();
    });
    connect(&m_appController.on4kstClient(), &On4kstClient::loginFailed, this, &MainWindow::updateStatusBar);

    connect(&m_appController.dxClusterClient(), &DxClusterClient::connected, this, &MainWindow::updateStatusBar);
    connect(&m_appController.dxClusterClient(), &DxClusterClient::disconnected, this, &MainWindow::updateStatusBar);

    // Bandmap feed: every spot with a frequency, from either source;
    // the rig's frequency report moves the marker (and, via the
    // connection above that sets m_currentBand first, the band);
    // the timer ages spots out.
    const auto noteBandmapSpot = [this](const SpotCandidate& candidate) {
        if (candidate.freqHz > 0) {
            m_bandmapModel.addSpot(candidate);
            refreshBandmap();
        }
    };
    connect(&m_appController.on4kstClient(), &On4kstClient::spotReceived, this, noteBandmapSpot);
    connect(&m_appController.dxClusterClient(), &DxClusterClient::spotReceived, this, noteBandmapSpot);
    connect(&m_appController.rigctldClient(), &RigctldClient::frequencyChanged, this, [this](qint64) { refreshBandmap(); });
    auto* bandmapTimer = new QTimer(this);
    bandmapTimer->setInterval(15000);
    connect(bandmapTimer, &QTimer::timeout, this, &MainWindow::refreshBandmap);
    bandmapTimer->start();

    // A chat line addressed to us that names a frequency is a sked
    // proposal: it lands in the Skeds panel as a suggestion, one click
    // makes it real.
    connect(&m_appController.on4kstClient(), &On4kstClient::chatLineReceived, this, [this](const SpotCandidate& candidate) {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        const auto suggestion = SkedRules::suggestionFromChat(candidate, m_appController.settings().ownCallsign, now);
        if (!suggestion) {
            return;
        }
        for (const Sked& existing : m_skeds) {
            if (existing.callsign == suggestion->callsign && existing.band == suggestion->band
                && (existing.state == Sked::State::Open || existing.state == Sked::State::Suggested)
                && std::abs(existing.timeUtc.secsTo(suggestion->timeUtc)) < 120) {
                return; // already there
            }
        }
        Sked sked = *suggestion;
        if (sked.grid.isEmpty()) {
            sked.grid = m_appController.database().lastKnownGridForCallsign(sked.callsign).value_or(QString());
        }
        m_appController.database().insertSked(m_appController.settings().activeContestId, sked);
        reloadSkeds();
        statusBar()->showMessage(QStringLiteral("Sked-Vorschlag von %1: %2 %3 UTC (Skeds-Panel)")
                                     .arg(sked.callsign)
                                     .arg(sked.freqHz / 1000000.0, 0, 'f', 3)
                                     .arg(sked.timeUtc.time().toString(QStringLiteral("HH:mm"))),
                                 8000);
    });
    auto* skedTimer = new QTimer(this);
    skedTimer->setInterval(15000);
    connect(skedTimer, &QTimer::timeout, this, &MainWindow::refreshSkeds);
    skedTimer->start();

    m_scoreboard = new OnlineScoreboard(this);
    connect(m_scoreboard, &OnlineScoreboard::posted, this,
            [this](const QString& summary) { statusBar()->showMessage(summary, 5000); });
    connect(m_scoreboard, &OnlineScoreboard::failed, this,
            [this](const QString& error) { statusBar()->showMessage(error, 20000); });

    // A terrain sector that was Unknown (its SRTM tile still loading)
    // resolving to a real classification later must still reach the
    // compass ring -- see refreshTerrainSectors()'s own doc comment.
    connect(&m_appController.terrainDataManager(), &TerrainDataManager::classificationChanged, this,
            &MainWindow::refreshTerrainSectors);

    // A ContestRulesEditor save reaches UnifiedLogWidget through this --
    // same "dialog closes, MainWindow re-applies the active contest"
    // shape openSettingsDialog() already uses below, just signal-driven
    // (see AppController::reloadContestDefinitions).
    connect(&m_appController, &AppController::contestDefinitionsChanged, this, &MainWindow::applyActiveContestDefinition);
    // A rotctld this program started for a rotor slot (see
    // AppController::rotctldLaunchFor) failed or died: Hamlib's own
    // reason, first line, where the operator looks first.
    connect(&m_appController, &AppController::rotctldFailed, this, [this](int slot, const QString& message) {
        // Hamlib's stderr is several lines; the one naming the port
        // ("Unable to open /dev/tty... - No such file or directory")
        // says more than the first ("rot_open: error = ...").
        QString reason;
        for (const QString& line : message.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            if (line.contains(QStringLiteral("Unable to open")) || line.contains(QStringLiteral("Permission denied"))
                || line.contains(QStringLiteral("not found"))) {
                reason = line.trimmed();
                break;
            }
        }
        if (reason.isEmpty()) {
            reason = message.section(QLatin1Char('\n'), 0, 0).trimmed();
        }
        (slot == 1 ? m_rotctldError1 : m_rotctldError2) = reason;
        qWarning().noquote() << QStringLiteral("rotctld (Rotor %1): %2").arg(slot).arg(message);
        statusBar()->showMessage(QStringLiteral("rotctld (Rotor %1): %2").arg(slot).arg(reason), 15000);
    });

    // Rotor widgets themselves (and their azimuthChanged/stateChanged
    // wiring) are created on demand by applyRotorWidgetSettings() below
    // -- see applyRotorSlot() -- since either slot may start disabled.

    // MapWidget's live-update wiring, per the plan's "refreshes when a
    // new QSO is logged... and when new spot candidates arrive": rather
    // than tapping On4kstClient::spotReceived/DxClusterClient::
    // spotReceived directly (which would duplicate GeoFilter/DupeChecker
    // classification MapWidget has no business redoing), this taps each
    // ChatFeedModel's own modelReset -- fired by rebuildVisibleRows()
    // every time a candidate arrives, a contest switch reclassifies
    // "worked", or the rate-derived visibility threshold moves -- the
    // same "read the model, do not invent a parallel path" approach
    // UnifiedLogWidget's own feed table uses for display.
    connect(&m_appController.on4kstFeedModel(), &QAbstractItemModel::modelReset, this, &MainWindow::refreshMapWidget);
    connect(&m_appController.clusterFeedModel(), &QAbstractItemModel::modelReset, this, &MainWindow::refreshMapWidget);
    // Same reasoning, for the Betriebsassistent's suggestion -- a newly
    // arriving/newly filtered candidate can change which station is the
    // current highest-scored target.
    connect(&m_appController.on4kstFeedModel(), &QAbstractItemModel::modelReset, this,
            &MainWindow::refreshSuggestionPanel);
    connect(&m_appController.clusterFeedModel(), &QAbstractItemModel::modelReset, this,
            &MainWindow::refreshSuggestionPanel);

    connect(m_cwMacroPanel, &CwMacroPanel::macroActivated, this, [this](const QString& templateText) {
        const QString text = CwMacroPanel::substitute(templateText, m_unifiedLog->callsign(), currentSentExchangeText());
        m_appController.rigctldClient().sendMorse(text);
        statusBar()->showMessage(QStringLiteral("CW: %1").arg(text), 3000);
    });
    connect(m_cwMacroPanel, &CwMacroPanel::stopRequested, this, [this]() {
        m_appController.rigctldClient().stopMorse();
        statusBar()->showMessage(QStringLiteral("CW gestoppt"), 2000);
    });
    // F1..F6 = the macro buttons, Esc = stop keying -- N1MM+/DXLog.net's
    // keyboard habit, whether or not the macro row is on screen. Window
    // scope, so a dialog's own Esc stays its own.
    for (int i = 0; i < 6; ++i) {
        auto* shortcut = new QShortcut(QKeySequence(Qt::Key_F1 + i), this);
        shortcut->setContext(Qt::WindowShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, i]() { m_cwMacroPanel->activateMacro(i); });
    }
    auto* stopShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    stopShortcut->setContext(Qt::WindowShortcut);
    connect(stopShortcut, &QShortcut::activated, m_cwMacroPanel, &CwMacroPanel::stopRequested);
    // Alt+W wipes the entry row (N1MM+'s "Wipe"): a busted call or a
    // station that went away, gone with one chord instead of field by
    // field.
    // PgUp/PgDn: keyer speed ±2 WpM (N1MM+'s keys), sent to the rig as
    // KEYSPD and remembered; the rig gets the stored speed once it
    // connects, so a restart does not silently key at the rig's own.
    auto* fasterShortcut = new QShortcut(QKeySequence(Qt::Key_PageUp), this);
    fasterShortcut->setContext(Qt::WindowShortcut);
    connect(fasterShortcut, &QShortcut::activated, this, [this]() { adjustCwSpeed(+2); });
    auto* slowerShortcut = new QShortcut(QKeySequence(Qt::Key_PageDown), this);
    slowerShortcut->setContext(Qt::WindowShortcut);
    connect(slowerShortcut, &QShortcut::activated, this, [this]() { adjustCwSpeed(-2); });
    connect(&m_appController.rigctldClient(), &RigctldClient::stateChanged, this, [this]() {
        if (m_appController.rigctldClient().isConnected()) {
            m_appController.rigctldClient().setKeyerSpeed(m_appController.settings().cwSpeedWpm);
        }
    });
    auto* wipeShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_W), this);
    wipeShortcut->setContext(Qt::WindowShortcut);
    connect(wipeShortcut, &QShortcut::activated, this, [this]() {
        m_unifiedLog->resetForNextEntry();
        refreshCheckPartial();
    });

    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&Datei"));
    // Operator, 2026-09-21: "sehe im log jetzt nicht, datei neu oder
    // datei speichern" -- the two things a file menu is expected to
    // start with, in this program's terms: a new log (the old one is
    // archived, nothing deleted) and a save (a backup copy -- the log
    // itself is a database that writes every QSO at once and copies
    // itself every minute). Same handlers the entries further down had;
    // they moved up here.
    QAction* newLogAction = fileMenu->addAction(QStringLiteral("&Neues Log beginnen (altes archivieren)..."));
    newLogAction->setObjectName(QStringLiteral("newLogAction"));
    connect(newLogAction, &QAction::triggered, this, &MainWindow::archiveActiveContest);
    QAction* backupNowAction = fileMenu->addAction(QStringLiteral("Log jetzt &sichern (Kopie)"));
    backupNowAction->setObjectName(QStringLiteral("backupNowAction"));
    backupNowAction->setShortcut(QKeySequence::Save);
    connect(backupNowAction, &QAction::triggered, this, &MainWindow::backupLogNow);
    fileMenu->addSeparator();
    QAction* settingsAction = fileMenu->addAction(QStringLiteral("&Einstellungen..."));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);
    // A proper contest-selection window, per the operator's explicit
    // request, distinct from the combo box buried in SettingsDialog --
    // see ui/ContestPickerDialog.h's class comment for scope.
    QAction* contestPickerAction = fileMenu->addAction(QStringLiteral("Contest &wählen..."));
    connect(contestPickerAction, &QAction::triggered, this, &MainWindow::openContestPicker);
    QAction* contestRulesAction = fileMenu->addAction(QStringLiteral("Contest-&Regeln..."));
    connect(contestRulesAction, &QAction::triggered, this, &MainWindow::openContestRulesEditor);
    QAction* exportCabrilloAction = fileMenu->addAction(QStringLiteral("&Cabrillo exportieren..."));
    connect(exportCabrilloAction, &QAction::triggered, this, &MainWindow::exportCabrillo);
    QAction* exportAdifAction = fileMenu->addAction(QStringLiteral("&ADIF exportieren..."));
    connect(exportAdifAction, &QAction::triggered, this, &MainWindow::exportAdif);
    // The IARU-R1/ÖVSV submission format -- see EdiExporter.h for why
    // Cabrillo alone is not enough for a VHF/UHF contest entry.
    // What the robot would find, found first -- see data/LogCheck.h.
    QAction* checkLogAction = fileMenu->addAction(QStringLiteral("Log &prüfen..."));
    connect(checkLogAction, &QAction::triggered, this, &MainWindow::openLogCheckWindow);
    // Before the first CQ: station, contest, links, data -- see
    // data/ReadinessCheck.h.
    QAction* readinessAction = fileMenu->addAction(QStringLiteral("Start&check (bereit?)..."));
    connect(readinessAction, &QAction::triggered, this, &MainWindow::openReadinessWindow);
    QAction* exportEdiAction = fileMenu->addAction(QStringLiteral("&EDI exportieren (REG1TEST)..."));
    connect(exportEdiAction, &QAction::triggered, this, &MainWindow::exportEdi);
    QAction* loadScpAction = fileMenu->addAction(QStringLiteral("SCP-&Liste laden..."));
    connect(loadScpAction, &QAction::triggered, this, &MainWindow::loadScpFile);
    QAction* importOldLogsAction = fileMenu->addAction(QStringLiteral("Locator aus alten Logs übernehmen (EDI/ADIF)..."));
    connect(importOldLogsAction, &QAction::triggered, this, &MainWindow::importOldLogs);
    QAction* transverterAction = fileMenu->addAction(QStringLiteral("Trans&verter..."));
    connect(transverterAction, &QAction::triggered, this, &MainWindow::openTransverterDialog);
    QAction* esmTemplatesAction = fileMenu->addAction(QStringLiteral("ESM-&Texte..."));
    connect(esmTemplatesAction, &QAction::triggered, this, &MainWindow::openEsmTemplatesDialog);
    QAction* scoreboardAction = fileMenu->addAction(QStringLiteral("&Online-Scoreboard..."));
    connect(scoreboardAction, &QAction::triggered, this, &MainWindow::openScoreboardDialog);
    QAction* postScoreAction = fileMenu->addAction(QStringLiteral("Score &jetzt senden"));
    connect(postScoreAction, &QAction::triggered, this, &MainWindow::postScoreNow);
    fileMenu->addSeparator();
    QAction* restoreAction = fileMenu->addAction(QStringLiteral("Sicherung &wiederherstellen..."));
    connect(restoreAction, &QAction::triggered, this, &MainWindow::restoreBackup);
    // A second copy of every backup on a stick or in a cloud folder --
    // the log survives the laptop (see LogBackup::setMirrorDirectory).
    QAction* mirrorAction = fileMenu->addAction(QStringLiteral("Zweiter Sicherungsordner..."));
    mirrorAction->setObjectName(QStringLiteral("backupMirrorAction"));
    connect(mirrorAction, &QAction::triggered, this, &MainWindow::chooseBackupMirror);
    QAction* clearMirrorAction = fileMenu->addAction(QStringLiteral("Zweiten Sicherungsordner entfernen"));
    clearMirrorAction->setObjectName(QStringLiteral("backupMirrorClearAction"));
    connect(clearMirrorAction, &QAction::triggered, this, &MainWindow::clearBackupMirror);
    connect(fileMenu, &QMenu::aboutToShow, this, [this, clearMirrorAction] {
        const LogBackup* backup = m_appController.logBackup();
        clearMirrorAction->setEnabled(backup && !backup->mirrorDirectory().isEmpty());
    });
    if (LogBackup* backup = m_appController.logBackup()) {
        connect(backup, &LogBackup::mirrorFailed, this, [this](const QString& error) {
            qWarning().noquote() << error;
            statusBar()->showMessage(error, 15000);
        });
    }
    fileMenu->addSeparator();
    QAction* quitAction = fileMenu->addAction(QStringLiteral("&Beenden"));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto* windowMenu = menuBar()->addMenu(QStringLiteral("&Fenster"));
    // Since the km scoring these are no multipliers -- the window is the
    // worked/open large-square list, and says so.
    QAction* multiplierAction = windowMenu->addAction(QStringLiteral("&Locator-Felder..."));
    connect(multiplierAction, &QAction::triggered, this, &MainWindow::openMultiplierWindow);
    QAction* statisticsAction = windowMenu->addAction(QStringLiteral("&Statistik..."));
    connect(statisticsAction, &QAction::triggered, this, &MainWindow::openStatisticsWindow);
    windowMenu->addSeparator();
    // "Fenster zurücksetzen" -- the real Longpath ContainerManager has no
    // literal equivalent (Thetis/Longpath never lost track of a
    // container's default position the way a first-time-docking system
    // needs a rescue action for), but PanelLayoutManager keeps each
    // panel's registration-time default around for exactly this purpose
    // -- a reasonable, in-scope addition per this wave's own task notes.
    QAction* resetLayoutAction = windowMenu->addAction(QStringLiteral("Fenster &zurücksetzen"));

    // Per-panel show/hide checkboxes -- the necessary other half of the
    // profile rail: a brand-new profile starts with every one of these
    // five panels hidden (LayoutProfileManager's own explicit design),
    // and without a way to bring one back there would be no way out of
    // a blank canvas. Scoped to the same five ids LayoutProfileManager
    // itself tracks (NOT cwMacroRow -- see that class's own comment on
    // why that panel stays on its independent checkbox).
    windowMenu->addSeparator();
    auto* panelsMenu = windowMenu->addMenu(QStringLiteral("&Panels"));
    struct PanelMenuEntry { QString id; QString label; };
    static const QVector<PanelMenuEntry> kPanelMenuEntries = {
        {QStringLiteral("unifiedlog"), QStringLiteral("Log")},
        {QStringLiteral("rotorrow"), QStringLiteral("Rotoren")},
        {QStringLiteral("map"), QStringLiteral("Karte / Verbindungen")},
        {QStringLiteral("suggestion"), QStringLiteral("Nächstes Ziel")},
        {QStringLiteral("ratemeter"), QStringLiteral("Rate")},
        {QStringLiteral("checkpartial"), QStringLiteral("Check")},
        {QStringLiteral("bandmap"), QStringLiteral("Bandmap")},
        {QStringLiteral("skeds"), QStringLiteral("Skeds")},
    };
    auto panelActions = std::make_shared<QVector<QPair<QString, QAction*>>>();
    for (const PanelMenuEntry& entry : kPanelMenuEntries) {
        QAction* action = panelsMenu->addAction(entry.label);
        action->setCheckable(true);
        connect(action, &QAction::toggled, this, [this, id = entry.id](bool visible) {
            if (PanelContainerWidget* panel = m_panelLayoutManager->panel(id)) {
                panel->setVisible(visible);
                // A panel switched on from the menu comes to the front --
                // otherwise a newly added one (Check, Bandmap, Skeds) can
                // sit invisibly behind the map at its default place --
                // and onto the canvas, if its place lies outside it (the
                // large design's spot on a 13" screen).
                if (visible) {
                    m_panelLayoutManager->revealPanel(id);
                }
                // Visibility lives in the profile alone: keep it.
                m_layoutProfileManager->saveActiveProfileState();
            }
        });
        panelActions->append({entry.id, action});
    }
    auto syncPanelMenuChecks = [this, panelActions]() {
        for (const auto& [id, action] : *panelActions) {
            if (PanelContainerWidget* panel = m_panelLayoutManager->panel(id)) {
                const QSignalBlocker blocker(action);
                // isHidden(), not isVisible(): before the window is
                // shown every panel is "not visible", which left every
                // entry unchecked at startup until a profile switch.
                action->setChecked(!panel->isHidden());
            }
        }
    };
    syncPanelMenuChecks();
    connect(m_layoutProfileManager, &LayoutProfileManager::profilesChanged, this, syncPanelMenuChecks);
    // And whenever the menu opens: the compact design of a fresh install
    // hides two panels after this constructor has run.
    connect(windowMenu, &QMenu::aboutToShow, this, syncPanelMenuChecks);
    connect(resetLayoutAction, &QAction::triggered, this, [this, syncPanelMenuChecks]() {
        m_panelLayoutManager->resetToDefaultLayout();
        syncPanelMenuChecks();
    });

    // Hilfe: the keys and gestures, for the operator at three in the
    // morning.
    auto* helpMenu = menuBar()->addMenu(QStringLiteral("&Hilfe"));
    QAction* shortcutsAction = helpMenu->addAction(QStringLiteral("&Tastenkürzel..."));
    connect(shortcutsAction, &QAction::triggered, this, &MainWindow::openShortcutsWindow);
    // "bei HELP oben ein Button ... neuerste Version aktualisieren"
    // (operator, 2026-09-21): GitHub release -> download -> swap ->
    // restart, see ui/UpdateDialog.h.
    QAction* updateAction = helpMenu->addAction(QStringLiteral("Auf neueste Version &aktualisieren..."));
    updateAction->setObjectName(QStringLiteral("updateAction"));
    connect(updateAction, &QAction::triggered, this, [this] {
        auto* dialog = new UpdateDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    helpMenu->addSeparator();
    QAction* aboutAction = helpMenu->addAction(QStringLiteral("Über &Contestprogramm..."));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::openAboutDialog);

    applyActiveContestDefinition();
    applyRotorWidgetSettings();
    applyClockSettings();
    refreshLogTable();
    refreshMapWidget();
    refreshSuggestionPanel();
    m_mapWidget->setVisibleRangeKm(m_appController.settings().radiusKm);
    m_rateMeterWidget->setSource(&m_appController.database(), m_appController.settings().activeContestId);
    updateStatusBar();

    // First run: no callsign known yet, so nothing useful can be logged.
    // Opened from the event loop, after main() has shown the window at
    // its real size -- not from inside this constructor, where the modal
    // dialog's own event processing laid the canvas out at Qt's tiny
    // pre-show default and clampPanelsToCanvas() crammed every panel
    // into that (a fresh install's first screen was a heap of overlapping
    // panels in the wrong corner, 2026-09-21), and where the dialog
    // centred itself on a window that had no on-screen position yet
    // (half of it off the left screen edge).
    if (m_appController.settings().ownCallsign.isEmpty()) {
        QTimer::singleShot(0, this, [this] {
            if (m_appController.settings().ownCallsign.isEmpty()) {
                openSettingsDialog();
            }
        });
    }

    // Keyboard focus belongs in the Callsign field from the first
    // moment (2026-09-21, fresh start: it sat in the top bar's grid
    // filter, so the first callsign typed filtered the log instead of
    // starting a QSO). From the event loop, once the window is on
    // screen -- and after the first-run dialog above has closed.
    QTimer::singleShot(0, this, [this] { m_unifiedLog->focusCallsign(); });

    // Restore the window size/position the operator last actually used
    // (see the closeEvent()/resizeEvent()/moveEvent() overrides below)
    // -- only fall back to the fixed default on a genuinely fresh
    // install (empty settings row) or a corrupt/incompatible saved
    // blob (restoreGeometry() itself returns false in that case).
    const QString savedGeometry = m_appController.database().settingValue(QStringLiteral("MainWindowGeometry"));
    bool geometryRestored = false;
    if (!savedGeometry.isEmpty()) {
        geometryRestored = restoreGeometry(QByteArray::fromBase64(savedGeometry.toUtf8()));
    }
    if (!geometryRestored) {
        // 950 -- 2026-09-13. Started as a screen-height fix (see
        // cwMacroRow's own comment for the original ~923px-tall-screen
        // reasoning); the kartendominante-Anordnung repositioning above
        // (see unifiedlog/rotorrow/map/suggestion/ratemeter's own
        // comments) moved the default canvas's own bottom edge again,
        // now to y=680 (deliberately tightened further, 2026-09-14,
        // once a real saved window on a real laptop restored to a
        // canvas only ~690px tall -- see the map panel's own comment) --
        // comfortably inside 950 with plenty of room to spare on a
        // taller screen, and no longer relying on clampPanelsToCanvas()
        // to make up the difference on a shorter one.
        // Width 1440, not 1300 or the briefly-tried 1600 -- 1600 was the
        // actual mistake this whole default-geometry saga traced back to
        // (see the map panel's own comment): this operator's real screen
        // is only 1470 points wide, confirmed live via `osascript`, not
        // guessed. 1440 leaves a real 30px margin instead of exactly
        // matching (which would still risk clipping a pixel or two of
        // window chrome/shadow) or exceeding it (which is what forced
        // macOS to silently resize the window smaller on every single
        // launch, differently each time depending on exactly how that
        // resize landed -- the actual mechanism behind "nach jedem
        // update stehen alle fenster wieder anders").
        resize(1440, 950);
    }

    m_geometrySaveTimer = new QTimer(this);
    m_geometrySaveTimer->setSingleShot(true);
    m_geometrySaveTimer->setInterval(400);
    connect(m_geometrySaveTimer, &QTimer::timeout, this, &MainWindow::persistWindowGeometryDebounced);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveWindowGeometry();
    QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    reflowRotorRowForCanvasWidth();
    if (m_geometrySaveTimer != nullptr) {
        m_geometrySaveTimer->start();
    }
}

void MainWindow::reflowRotorRowForCanvasWidth() {
    // Full three-column canvas width the default layout is hand-tuned
    // for (rotorrow 0-620, suggestion/ratemeter 630-900, map 910-1440 --
    // see each panel's own registerPanel() comment). Below this width
    // the columns no longer fit side by side anyway (clampPanelsToCanvas()
    // is already pushing/overlapping them), so recentering rotorrow
    // within whatever room is actually left is a real improvement, not a
    // fight against the intended layout. At or above it, this function
    // must do nothing at all -- unconditionally recentering rotorrow at
    // the default canvas width would move it from x=0 to roughly x=400,
    // straight on top of the suggestion/map columns it normally sits
    // beside (operator, 2026-09-14: "rotoren sollten sich auch mittig
    // zentrieren und dann auch ggf. kleiner werden, wenn das fenster
    // kleiner wird" -- the "ggf./wenn kleiner" is load-bearing here).
    //
    // kFullLayoutWidth is 1440 minus the central widget's own 24px
    // margin (setContentsMargins(12,12,12,12) around m_panelLayoutManager
    // ->canvas() -- see the constructor), NOT the raw 1440 the panel
    // QRects below are dimensioned against: canvas()->width() is always
    // 24px narrower than the window's own content width, so comparing
    // against the bare 1440 made this trigger even at the intended
    // default window size (bench-found live, 2026-09-14 -- a fresh
    // 1440-wide window still recentered rotorrow to ~x=400 on top of
    // the map/suggestion columns instead of leaving it at its default
    // x=0).
    // Since 2026-09-21 the width of the design in force (PanelLayout-
    // Manager::designWidth(): 1440 for the large design, 1372 for the
    // compact one a 13" screen gets) -- the old literal 1416 was above
    // the 1372 px the default window actually leaves the canvas, so the
    // rotor row was recentred onto the map column at the default size.
    constexpr int kRotorRowDefaultWidth = 620;
    constexpr int kMargin = 10;

    if (m_rotorRowContainer == nullptr || m_panelLayoutManager == nullptr) {
        return;
    }
    QWidget* canvas = m_panelLayoutManager->canvas();
    if (canvas == nullptr) {
        return;
    }
    const int canvasWidth = canvas->width();
    if (canvasWidth <= 0 || canvasWidth >= m_panelLayoutManager->designWidth()) {
        return;
    }

    const int availableWidth = std::max(0, canvasWidth - kMargin * 2);
    // trySetGeometry() itself still clamps this up to the content's real
    // minimumSizeHint() (RotorWidget's own 300px-per-dial floor -- 600px
    // for the row's two dials together) if availableWidth undershoots
    // it -- see that method's own comment.
    const int targetWidth = std::min(kRotorRowDefaultWidth, availableWidth);
    QRect r = m_rotorRowContainer->geometry();
    r.setWidth(targetWidth);
    r.moveLeft(std::max(kMargin, (canvasWidth - targetWidth) / 2));
    m_rotorRowContainer->trySetGeometry(r);

    // Below the two-dial floor, trySetGeometry() just clamped the width
    // back up past what x above was centered for -- re-center once more
    // against the width it actually ended up with, so an extreme-narrow
    // window doesn't leave the (now wider-than-requested) panel pushed
    // off-center toward the left margin instead.
    if (m_rotorRowContainer->width() != targetWidth) {
        QRect corrected = m_rotorRowContainer->geometry();
        corrected.moveLeft(std::max(kMargin, (canvasWidth - corrected.width()) / 2));
        m_rotorRowContainer->trySetGeometry(corrected);
    }
}

void MainWindow::moveEvent(QMoveEvent* event) {
    QMainWindow::moveEvent(event);
    if (m_geometrySaveTimer != nullptr) {
        m_geometrySaveTimer->start();
    }
}

void MainWindow::persistWindowGeometryDebounced() {
    saveWindowGeometry();
}

void MainWindow::handleProfileRenameRequested(const QString& name) {
    bool ok = false;
    const QString newName = QInputDialog::getText(this, QStringLiteral("Profil umbenennen"),
                                                    QStringLiteral("Neuer Name:"), QLineEdit::Normal, name, &ok);
    if (ok && m_layoutProfileManager != nullptr) {
        m_layoutProfileManager->renameProfile(name, newName);
    }
}

void MainWindow::saveWindowGeometry() {
    const QByteArray geometry = saveGeometry();
    m_appController.database().setSettingValue(QStringLiteral("MainWindowGeometry"), QString::fromUtf8(geometry.toBase64()));
}

MainWindow::~MainWindow() = default;

const ContestDefinition* MainWindow::findContestDefinition(const QString& contestId) const
{
    return m_appController.findContestDefinition(contestId);
}

QString MainWindow::contestModeForRigctldMode(const QString& rigMode)
{
    // rigctld mode names come from Hamlib's rmode_t string table
    // (RIG_MODE_* -> "USB"/"LSB"/"CW"/"CWR"/"FM"/"RTTY"/... per
    // hamlib/rig.h); UnifiedLogWidget's mode combo only offers the four
    // contest-relevant buckets set up in applyActiveContestDefinition().
    const QString upper = rigMode.trimmed().toUpper();
    if (upper == QStringLiteral("USB") || upper == QStringLiteral("LSB")) {
        return QStringLiteral("SSB");
    }
    if (upper == QStringLiteral("CW") || upper == QStringLiteral("CWR")) {
        return QStringLiteral("CW");
    }
    if (upper == QStringLiteral("FM") || upper == QStringLiteral("WFM")) {
        return QStringLiteral("FM");
    }
    if (upper == QStringLiteral("RTTY") || upper == QStringLiteral("RTTYR")) {
        return QStringLiteral("RTTY");
    }
    return QString();
}

void MainWindow::applyActiveContestDefinition()
{
    const ContestDefinition* def = findContestDefinition(m_appController.settings().activeContestId);
    if (def) {
        // Band has no dedicated UI control any more (see the class
        // comment in MainWindow.h) -- m_currentBand still needs a
        // sensible starting value for dupe scope/export/rotor routing
        // before CAT ever reports a frequency (or when it never
        // connects at all), so it defaults to the new contest's first
        // declared band, mirroring EntryBarWidget's own old band combo
        // (which likewise defaulted to index 0 on a fresh setBands()
        // call). Left alone if the current value is still one of the
        // new contest's bands (e.g. re-applying the same contest, or
        // two contests sharing a band list).
        if (!def->bands().contains(m_currentBand)) {
            m_currentBand = def->bands().isEmpty() ? QString() : def->bands().first();
        }
        // Mode gets the identical treatment ("mode wird nicht benötigt"
        // -- Martin's later follow-up): no declared list to default
        // from any more (there is no UnifiedLogWidget::setModes() left
        // to call -- VHF/UHF contests mix modes freely and always did,
        // this list was only ever a fixed UI convenience, never read
        // from JSON), so this defaults to "SSB" the same way
        // m_currentBand defaults to the contest's first band: a
        // sensible starting value before CAT ever reports a mode (or if
        // it never connects at all). Pushed into UnifiedLogWidget
        // BEFORE setExchangeFields() below, so its own RST auto-default
        // (see UnifiedLogWidget::applyRstDefaults(), run at the end of
        // that call) already sees the right mode.
        if (m_currentMode.isEmpty()) {
            m_currentMode = QStringLiteral("SSB");
        }
        // A single-mode contest (the Marconi Memorial: "modes": ["CW"])
        // starts in that mode -- without CAT nothing else would ever
        // set it, and every QSO would go into the log as SSB with a
        // 59. A live radio still overrides this on its next mode report.
        if (def->modes().size() == 1 && !def->modes().contains(m_currentMode)) {
            m_currentMode = def->modes().first();
        }
        m_unifiedLog->setCurrentMode(m_currentMode);
        // The literal gap this task closes: the entry row's exchange
        // sub-fields now actually reflect the active ContestDefinition's
        // fields, instead of two hardcoded grid/serial boxes -- see
        // UnifiedLogWidget::setExchangeFields. Runs on startup, on every
        // SettingsDialog contest switch, and on every ContestRulesEditor
        // save (see the contestDefinitionsChanged connection above).
        m_unifiedLog->setExchangeFields(def->exchangeFields());
        // The score rows (km per band, ODX) need the own locator and the
        // contest's band order/scoring rule -- both can change with the
        // same settings/contest switch that lands here.
        m_rateMeterWidget->setScoring(m_appController.settings().ownGrid, def->bands(), def->scoring(),
                                      def->multiplierField());
    }
    reloadCheckPartialSources();
    refreshScoreboard();
    if (m_skedPanel) {
        m_skedPanel->setOwnGrid(m_appController.settings().ownGrid);
    }
    reloadSkeds();
    refreshSentExchangePreview();
    // The bandmap's axis follows m_currentBand, which this function may
    // just have set to the contest's first band: without this it kept
    // its 144 MHz default all night on a UHF contest whenever CAT
    // never reported a frequency (found 2026-09-21 on a fresh start).
    refreshBandmap();
    updateStatusBar();
}

void MainWindow::refreshSentExchangePreview()
{
    // "What would I send right now" -- own grid + next serial, kept
    // visible in the entry row's dim "Exch Ges." cell (UnifiedLogWidget::
    // setSentExchangePreview) instead of only existing internally at log
    // time. Cheap enough (one indexed COUNT query) to call on every
    // contest switch and after every logged QSO rather than caching it.
    m_unifiedLog->setSentExchangePreview(currentSentExchangeText());
}

void MainWindow::applyRotorWidgetSettings()
{
    const ContestSettings settings = m_appController.settings();

    // MapWidget moved out to its own dockable panel (see the
    // constructor) -- m_rotorRow now holds only the rotor compasses
    // themselves plus a trailing stretch, so slot 1 inserts at index 0.
    applyRotorSlot(settings.rotor1Enabled, settings.rotor1Label, m_appController.rotor1Client(), m_rotor1Widget, 0);
    // Slot 2 always inserts right after slot 1 if slot 1 currently has a
    // widget, or at the front otherwise -- keeps the row in slot-number
    // order no matter which slot(s) are enabled/disabled right now (see
    // the m_rotorRow class comment in the constructor).
    applyRotorSlot(settings.rotor2Enabled, settings.rotor2Label, m_appController.rotor2Client(), m_rotor2Widget,
                    m_rotor1Widget ? 1 : 0);

    if (m_rotor1Widget) {
        m_rotor1Widget->setSecondAntenna(settings.rotor1SecondAntennaEnabled, settings.rotor1SecondAntennaOffsetDeg);
        m_rotor1Widget->setExtraBandBadge(
            settings.band1296RotorSlot == ContestSettings::RotorSlot::Slot1 ? QStringLiteral("+23cm") : QString());
        m_rotor1Widget->setDialStyle(settings.rotorDialStyle);
    }
    if (m_rotor2Widget) {
        m_rotor2Widget->setSecondAntenna(settings.rotor2SecondAntennaEnabled, settings.rotor2SecondAntennaOffsetDeg);
        m_rotor2Widget->setExtraBandBadge(
            settings.band1296RotorSlot == ContestSettings::RotorSlot::Slot2 ? QStringLiteral("+23cm") : QString());
        m_rotor2Widget->setDialStyle(settings.rotorDialStyle);
    }
    applyRotorBeamwidths();

    m_cwMacroPanel->setMacroTemplates(settings.cwMacros);

    // The map draws one cone per antenna: a rotor with a second antenna
    // shows both directions (operator, 2026-09-20).
    if (m_mapWidget) {
        m_mapWidget->setRotor1SecondAntenna(settings.rotor1SecondAntennaEnabled, settings.rotor1SecondAntennaOffsetDeg);
        m_mapWidget->setRotor2SecondAntenna(settings.rotor2SecondAntennaEnabled, settings.rotor2SecondAntennaOffsetDeg);
    }

    refreshTerrainSectors();
}

void MainWindow::applyRotorBeamwidths()
{
    // The map's "Öffnungswinkel Rotor N" preference (persisted with the
    // rest of map_preferences) is the one place the beamwidth lives;
    // the dials' cones follow it.
    if (!m_mapWidget) {
        return;
    }
    if (m_rotor1Widget) {
        m_rotor1Widget->setBeamwidthDeg(m_mapWidget->rotor1BeamwidthDeg());
    }
    if (m_rotor2Widget) {
        m_rotor2Widget->setBeamwidthDeg(m_mapWidget->rotor2BeamwidthDeg());
    }
}

void MainWindow::refreshTerrainSectors()
{
    // 144 MHz -- matches AppController's own fixed-band simplification
    // for GeoFilter's terrain hookup (see AppController.cpp's own
    // comment on why: one shared GeoFilter across both bands, and 144
    // MHz is the wider/more conservative first-Fresnel-zone case of the
    // two, so a Clear result here is also Clear at 432 MHz, never the
    // reverse).
    const QVector<LineOfSightClass> sectors = m_appController.terrainDataManager().sectorSweep(144.0);
    if (m_rotor1Widget) {
        m_rotor1Widget->setTerrainSectors(sectors);
    }
    if (m_rotor2Widget) {
        m_rotor2Widget->setTerrainSectors(sectors);
    }
    // Same sweep, same "wash the rim, never a restriction" contract --
    // see MapWidget::setTerrainSectors()'s own doc comment. Direction-
    // only data (a fixed reference distance, per TerrainDataManager::
    // sectorSweep()'s own doc comment), so the one sweep already
    // computed for the rotor compasses applies unchanged here too, no
    // separate computation needed.
    if (m_mapWidget) {
        m_mapWidget->setTerrainSectors(sectors);
        m_mapWidget->setHorizonProfile(horizonProfileForOwnStation());
    }
}

QVector<double> MainWindow::horizonProfileForOwnStation()
{
    const ContestSettings settings = m_appController.settings();
    if (!isValidGridSquare(settings.ownGrid)) {
        return {};
    }
    // The exact position the settings dialog can store (keys written
    // by its "genauer Standort" option) beats the locator's centre:
    // for a hilltop station the skyline from the summit and from the
    // valley floor of the same square are different mountains.
    ContestDatabase& db = m_appController.database();
    double lat = 0.0;
    double lon = 0.0;
    calculateLatLonFromGridSquare(settings.ownGrid, lat, lon);
    if (db.settingValue(QStringLiteral("use_exact_own_location")) == QStringLiteral("1")) {
        bool okLat = false;
        bool okLon = false;
        const double exactLat = db.settingValue(QStringLiteral("own_exact_latitude")).toDouble(&okLat);
        const double exactLon = db.settingValue(QStringLiteral("own_exact_longitude")).toDouble(&okLon);
        if (okLat && okLon && std::fabs(exactLat) <= 90.0 && std::fabs(exactLon) <= 180.0) {
            lat = exactLat;
            lon = exactLon;
        }
    }
    const QString key = QStringLiteral("%1,%2|%3|%4").arg(lat, 0, 'f', 5).arg(lon, 0, 'f', 5)
                            .arg(settings.ownElevationM).arg(settings.antennaHeightM);
    if (key == m_horizonProfileKey) {
        return m_horizonProfile;
    }
    SrtmTileLoader& loader = m_appController.terrainDataManager().tileLoader();
    loader.ensureTileAvailable(lat, lon);
    // Own elevation from the settings when given, else the terrain at
    // the locator's centre; antenna height 10 m unless set.
    const double ground = settings.ownElevationM > 0.0 ? settings.ownElevationM : loader.elevationAt(lat, lon).value_or(0.0);
    const double eye = ground + (settings.antennaHeightM > 0.0 ? settings.antennaHeightM : 10.0);
    m_horizonProfile = computeHorizonProfile(loader, lat, lon, eye);
    m_horizonProfileKey = key;
    return m_horizonProfile;
}

void MainWindow::applyRotorSlot(bool enabled, const QString& label, RotctldClient& client, RotorWidget*& widget, int insertIndex)
{
    if (enabled) {
        if (!widget) {
            widget = new RotorWidget(label, m_rotorRow);
            m_rotorLayout->insertWidget(insertIndex, widget);
            // `widget` as the connect() context means both connections
            // auto-disconnect the moment the widget is destroyed below
            // (the disabled-slot path) -- no dangling-pointer risk in
            // the stateChanged lambda's captured `client`/`widget`.
            connect(&client, &RotctldClient::azimuthChanged, widget, &RotorWidget::setAzimuthDeg);
            connect(&client, &RotctldClient::stateChanged, widget, [widget, &client]() {
                widget->setConnected(client.isConnected());
            });
            // Karte/Verbindungen's own "Rotoren" heading spoke (operator,
            // 2026-09-14, replacing the removed "Links" toggle) -- same
            // live signals as the RotorWidget connections just above,
            // fanned out to m_mapWidget too. `widget` as the connect()
            // context (not m_mapWidget) matches this block's own opening
            // comment: these must be torn down together with the
            // RotorWidget on the disabled-slot path below, or a later
            // re-enable would stack a second, redundant pair of
            // connections on top instead of replacing them. `label` is
            // captured by value (this lambda's own copy, not
            // `widget->bandLabel()`, which could be stale mid-toggle)
            // since it is exactly the string this call was just given
            // for the widget itself.
            // Second argument is `true` (this slot HAS a panel), not
            // `client.isConnected()` -- operator, 2026-09-14: "die zeiger
            // der rotoren werden noch nicht auf 0 grad angezeigt". The
            // dial itself (RotorWidget) already draws its needle at
            // whatever m_azimuthDeg holds regardless of live connection
            // (only the RING goes dashed/warn when disconnected, see
            // paintFullCompassDial()) -- the map's own spoke needs the
            // same "always show what the slot is tracking" rule, not a
            // stricter one of its own.
            connect(&client, &RotctldClient::azimuthChanged, widget, [this, &client, label](double az) {
                pushRotorHeadingToMap(client, true, az, label);
            });
            connect(&client, &RotctldClient::stateChanged, widget, [this, &client, label]() {
                pushRotorHeadingToMap(client, true, client.azimuthDeg(), label);
            });
            // Mirrors the dial's OWN heading onto the map -- not just a
            // duplicate of the two connections above. This one also
            // fires from RotorWidget::startSimulatedTurn()'s hardware-
            // free animation (setAzimuthDeg() emits azimuthDegChanged()
            // regardless of what drove it, see that signal's own
            // comment), which never touches `client` at all -- without
            // this, a simulated turn moved the dial's own needle but
            // left the map's heading spoke frozen (operator, 2026-09-14:
            // "zeiger sollte sich parallel auch in der karte drehen").
            connect(widget, &RotorWidget::azimuthDegChanged, widget, [this, &client, label](double az) {
                pushRotorHeadingToMap(client, true, az, label);
            });
            // Double-click-to-aim (operator, 2026-09-14, "dies haben wir
            // bei longpath") -- exact same stop-aware plan()+setAzimuth()
            // pair commandRotorForCandidate() already uses for a map/
            // suggestion-panel click, just triggered from inside the
            // dial itself instead of from a candidate row, and with no
            // target-station identity to attach (RotorWidget::
            // setTargetBearing() is NOT called here -- an aimed-but-
            // uncommitted-to-a-callsign heading has no "SP9XYZ · 471 km"
            // caption to show, unlike the candidate-click path).
            connect(widget, &RotorWidget::rotateRequested, widget, [&client](double bearingDeg) {
                if (!client.isConnected()) {
                    return;
                }
                const BeamHeading::Move move = BeamHeading::plan(client.azimuthDeg(), bearingDeg, BeamHeading::Stop::None);
                if (move.reachable) {
                    client.setAzimuth(move.targetDeg);
                }
            });
            // The rotor panel's own ⚙ options popup (RotorWidget::
            // showOptionsPopup()) -- that widget already applies the
            // chosen style to itself. rotorDialStyle is one
            // operator-wide ContestSettings field shared by both rotor
            // compasses (see its own comment) though, so only
            // MainWindow, which sees both live RotorWidget instances,
            // is in a position to persist it and push it to the OTHER
            // one too -- via applyRotorWidgetSettings(), the exact same
            // round trip SettingsDialog's "Rotor-Anzeige" combo already
            // goes through in openSettingsDialog().
            connect(widget, &RotorWidget::dialStyleRequested, widget, [this](RotorDialStyle style) {
                ContestSettings settings = m_appController.settings();
                if (settings.rotorDialStyle == style) {
                    return;
                }
                settings.rotorDialStyle = style;
                m_appController.setSettings(settings);
                applyRotorWidgetSettings();
            });
            // Prime state immediately -- this slot may have been
            // re-enabled while its RotctldClient was already connected
            // (e.g. toggled off and back on without ever disconnecting
            // the underlying rotctld link), so the new widget must not
            // start out claiming "getrennt"/AZ unknown by default.
            widget->setConnected(client.isConnected());
            widget->setAzimuthDeg(client.azimuthDeg());
            pushRotorHeadingToMap(client, true, widget->azimuthDeg(), label);
        } else {
            widget->setBandLabel(label);
            pushRotorHeadingToMap(client, true, widget->azimuthDeg(), label);
        }
    } else if (widget) {
        m_rotorLayout->removeWidget(widget);
        widget->hide();
        widget->deleteLater();
        widget = nullptr;
        // Slot turned off -- the map must not keep drawing a heading
        // spoke for a rotor that no longer has a panel at all, even if
        // its RotctldClient link happens to stay up underneath.
        // azimuthDeg is unused on this path (MapWidget's own
        // drawHeading() returns immediately when connected is false),
        // so client.azimuthDeg() is just a harmless placeholder here.
        pushRotorHeadingToMap(client, false, client.azimuthDeg(), label);
    }
}

void MainWindow::pushRotorHeadingToMap(RotctldClient& client, bool connected, double azimuthDeg, const QString& label)
{
    if (!m_mapWidget) {
        return;
    }
    // `connected` heißt hier "dieser Steckplatz hat ein Bedienfeld"
    // (siehe die Aufrufstellen) -- ob der Draht zum Rotor wirklich
    // steht, ist eine zweite Frage, und die Karte soll sie ehrlich
    // beantworten statt eine Peilung vorzutäuschen.
    const bool isRotor1 = &client == &m_appController.rotor1Client();
    if (isRotor1) {
        m_mapWidget->setRotor1Heading(connected, azimuthDeg, label);
    } else {
        m_mapWidget->setRotor2Heading(connected, azimuthDeg, label);
    }
    m_mapWidget->setRotorLinkLive(isRotor1 ? 1 : 2, connected && client.isConnected());
}

RotorWidget* MainWindow::rotorWidgetForClient(RotctldClient* client) const
{
    if (client == &m_appController.rotor1Client()) {
        return m_rotor1Widget;
    }
    if (client == &m_appController.rotor2Client()) {
        return m_rotor2Widget;
    }
    return nullptr;
}

void MainWindow::commandRotorForCandidate(const QString& callsign, const QString& grid, qint64 freqHz)
{
    const ContestSettings settings = m_appController.settings();
    if (freqHz <= 0 || !isValidGridSquare(settings.ownGrid) || !isValidGridSquare(grid)) {
        // No known frequency, or no usable geometry -- do not guess
        // which rotor (if any) this candidate belongs to.
        return;
    }
    const QString band = bandLabelForFrequencyHz(freqHz);
    if (band.isEmpty()) {
        return;
    }
    RotctldClient* rotor = m_appController.activeRotorForBand(band);
    if (!rotor || !rotor->isConnected()) {
        return;
    }

    const double bearingDeg = calculateBearingInDegrees(settings.ownGrid, grid);
    const double distanceKm = calculateDistanceKm(settings.ownGrid, grid);

    // Stop::None: no rotor's actual mechanical end-stop position is
    // configured anywhere in ContestSettings yet -- this is a
    // deliberate placeholder default (the parameter is wired through so
    // stop-awareness only needs a settings field + this call site to
    // change later), not a claim about the real hardware's limits.
    const BeamHeading::Move move = BeamHeading::plan(rotor->azimuthDeg(), bearingDeg, BeamHeading::Stop::None);
    if (move.reachable) {
        rotor->setAzimuth(move.targetDeg);
    }

    if (RotorWidget* widget = rotorWidgetForClient(rotor)) {
        widget->setTargetBearing(bearingDeg, distanceKm, callsign, grid);
    }
}

QString MainWindow::currentSentExchangeText() const
{
    const ContestSettings settings = m_appController.settings();
    const ContestDefinition* def = findContestDefinition(settings.activeContestId);
    if (!def) {
        return QString();
    }
    const int serialSent = m_appController.database().nextSerialForContest(
        settings.activeContestId, def->serialScope() == QStringLiteral("band") ? m_currentBand : QString());
    return composeExchange(*def, buildSentExchangeValues(*def, settings, serialSent, m_currentMode));
}

void MainWindow::refreshLogTable()
{
    if (!m_logModel) {
        return;
    }
    m_logModel->setRecords(m_appController.database().qsosForContest(m_appController.settings().activeContestId));
    if (m_logCheckWindow && m_logCheckWindow->isVisible()) {
        refreshLogCheck();
    }
}

void MainWindow::refreshMultiplierAndFeedScores()
{
    const QString contestId = m_appController.settings().activeContestId;
    if (const ContestDefinition* def = findContestDefinition(contestId)) {
        m_appController.multiplierTracker().recompute(contestId, *def);
    }
    m_appController.on4kstFeedModel().refreshWorkedAndScores();
    m_appController.clusterFeedModel().refreshWorkedAndScores();
}

void MainWindow::applyClockSettings()
{
    const ContestSettings settings = m_appController.settings();
    // The definition's schedule gives start and end for any year (see
    // data/ContestSchedule.h); a manually set contest end still wins.
    // Counting towards the next occurrence: once this year's contest
    // is over, next year's is the one the readout should name.
    const ContestDefinition* def = findContestDefinition(settings.activeContestId);
    const ContestSchedule schedule = def ? def->schedule() : ContestSchedule();
    ContestWindow window;
    if (!settings.contestEndUtc.trimmed().isEmpty()) {
        window = effectiveContestWindow(schedule, settings.contestEndUtc, QDateTime::currentDateTimeUtc());
    } else {
        window = schedule.windowUpcoming(QDateTime::currentDateTimeUtc());
    }
    m_utcClockWidget->setContestWindow(window.startUtc, window.endUtc);
    m_utcClockWidget->setCountdownVisible(settings.countdownVisible);
}

void MainWindow::refreshMapWidget()
{
    if (!m_mapWidget) {
        return;
    }
    const ContestSettings settings = m_appController.settings();
    m_mapWidget->setOwnGrid(settings.ownGrid);
    m_mapWidget->setOwnLabel(settings.ownCallsign);

    QVector<MapWidget::Station> stations;
    QSet<QString> workedCallsigns;

    // Worked stations: every logged QSO in the active contest that has
    // a grid to plot (ContestDatabase::qsosWithGrid already does the
    // "has a grid" filtering in SQL).
    const QVector<QsoRecord> worked = m_appController.database().qsosWithGrid(settings.activeContestId);
    for (const QsoRecord& record : worked) {
        if (record.isInvalid) {
            continue; // struck from the log, not a worked station
        }
        MapWidget::Station station;
        station.callsign = record.callsign;
        station.grid = record.gridSquare;
        station.worked = true;
        station.freqHz = record.freqHz.value_or(0);
        // Same QDateTime::fromString(..., Qt::ISODate) round-trip
        // RateMeterWidget/LogTableModel/UnifiedLogWidget's own status
        // line already use for this exact field -- invalid (a malformed
        // or missing timestamp) leaves workedAtUtc invalid too, which
        // MapWidget's aging fade already treats as "don't fade" rather
        // than guessing an age.
        station.workedAtUtc = QDateTime::fromString(record.timestampUtc, Qt::ISODate);
        stations.append(station);
        workedCallsigns.insert(record.callsign.trimmed().toUpper());
    }

    // Spotted-but-unworked candidates: both ChatFeedModels' currently
    // visible rows are, by construction, already the "in range and not
    // worked" survivors of GeoFilter/DupeChecker (see ChatFeedModel::
    // rebuildVisibleRows's hard-exclusion step) -- unless the raw-feed
    // toggle is on, in which case dupes/out-of-range rows are dimmed
    // rather than removed, so DupeRole is still checked explicitly here
    // rather than trusting "visible" alone. Deduplicated by callsign
    // across both feeds (and against the worked set) so a station
    // spotted on both ON4KST and the cluster does not draw twice.
    QSet<QString> spottedCallsigns;
    auto appendSpotted = [&](ChatFeedModel& model) {
        for (int row = 0; row < model.rowCount(); ++row) {
            const bool alreadyWorked = model.data(model.index(row, ChatFeedModel::ColumnCallsign), ChatFeedModel::DupeRole).toBool();
            if (alreadyWorked) {
                continue;
            }
            const SpotCandidate& candidate = model.candidateAt(row);
            if (candidate.grid.isEmpty()) {
                continue;
            }
            const QString key = candidate.callsign.trimmed().toUpper();
            if (key.isEmpty() || workedCallsigns.contains(key) || spottedCallsigns.contains(key)) {
                continue;
            }
            spottedCallsigns.insert(key);
            MapWidget::Station station;
            station.callsign = candidate.callsign;
            station.grid = candidate.grid;
            station.worked = false;
            station.freqHz = candidate.freqHz;
            stations.append(station);
        }
    };
    appendSpotted(m_appController.on4kstFeedModel());
    appendSpotted(m_appController.clusterFeedModel());

    m_mapWidget->setStations(stations);
    // The radar's number column shows the same score as the Rate panel.
    const ContestDefinition* def = findContestDefinition(settings.activeContestId);
    const ContestScore score = computeContestScore(m_appController.database().qsosForContest(settings.activeContestId),
                                                  settings.ownGrid, def ? def->bands() : QStringList(),
                                                  def ? def->scoring() : QStringLiteral("distance_km"));
    m_mapWidget->setScoreSummary(score.validQsos, score.points,
                                 score.odxKm > 0 ? QStringLiteral("%1 %2 km").arg(score.odxCall).arg(score.odxKm) : QString());
}

void MainWindow::refreshSuggestionPanel()
{
    if (!m_suggestionPanel) {
        return;
    }
    // Don't yank the suggestion out from under an in-progress QSO -- see
    // this method's own doc comment in MainWindow.h.
    if (!m_unifiedLog->callsign().isEmpty()) {
        return;
    }

    QVector<NextTargetSuggester::Candidate> candidates;
    auto appendCandidates = [&](ChatFeedModel& model) {
        for (int row = 0; row < model.rowCount(); ++row) {
            // NextTargetSuggester's own doc comment relies on every row
            // handed to it already being hard-filtered (reachable, not
            // yet worked) -- true in the model's default filtered mode,
            // but NOT when the operator's "Roh-Feed" toggle is on
            // (ChatFeedModel::rebuildVisibleRows() then shows every row,
            // dupes/out-of-range included, dimmed rather than removed).
            // Re-check explicitly here via the same DupeRole/InRangeRole
            // roles refreshMapWidget()'s own appendSpotted() above
            // already uses for exactly this reason -- without this, the
            // raw-feed toggle could make the Betriebsassistent draft a
            // message to (and one confirm-click log) an already-worked
            // or out-of-range station.
            const bool alreadyWorked =
                model.data(model.index(row, ChatFeedModel::ColumnCallsign), ChatFeedModel::DupeRole).toBool();
            const bool inRange =
                model.data(model.index(row, ChatFeedModel::ColumnCallsign), ChatFeedModel::InRangeRole).toBool();
            if (alreadyWorked || !inRange) {
                continue;
            }
            NextTargetSuggester::Candidate entry;
            entry.candidate = model.candidateAt(row);
            entry.score = model.scoreAt(row);
            entry.geo = model.geoAt(row);
            candidates.append(entry);
        }
    };
    appendCandidates(m_appController.on4kstFeedModel());
    appendCandidates(m_appController.clusterFeedModel());

    const auto suggestion = NextTargetSuggester::suggest(candidates);
    if (!suggestion) {
        m_suggestionPanel->setSuggestion(std::nullopt, std::nullopt, std::nullopt, QString());
        return;
    }

    const QString draft = MessageDrafter::draftDirectedCall(suggestion->candidate.callsign,
                                                              m_appController.settings().ownCallsign,
                                                              currentSentExchangeText());
    const std::optional<double> distanceKm =
        suggestion->geo.distanceKnown ? std::optional<double>(suggestion->geo.distanceKm) : std::nullopt;
    const std::optional<double> bearingDeg =
        suggestion->geo.distanceKnown ? std::optional<double>(suggestion->geo.bearingDeg) : std::nullopt;
    m_suggestionPanel->setSuggestion(suggestion->candidate, distanceKm, bearingDeg, draft);
}

void MainWindow::handleSuggestionSendRequested(const QString& text)
{
    // The one place that actually reaches On4kstClient -- see
    // SuggestionPanel's own class comment for the capability-separation
    // reasoning (the panel itself cannot call this). Shared with
    // UnifiedLogWidget's chat quick-send row (see MainWindow.h's own
    // doc comment on this slot).
    m_appController.on4kstClient().sendChatMessage(text);
}

void MainWindow::handleCqDraftRequested()
{
    const QString draft = MessageDrafter::draftCqCall(m_appController.settings().ownCallsign);
    m_unifiedLog->setChatInputDraft(draft);
}

void MainWindow::handleAwayToggled(bool away)
{
    if (away) {
        m_appController.on4kstClient().sendAway();
    } else {
        m_appController.on4kstClient().sendBack();
    }
}

QString MainWindow::on4kstRoomValueForBand(const QString& band)
{
    if (band == QStringLiteral("1296")) {
        return QStringLiteral("GHZ");
    }
    if (band == QStringLiteral("144") || band == QStringLiteral("432")) {
        return QStringLiteral("144");
    }
    return QString();
}

void MainWindow::syncOn4kstRoomForCurrentBand()
{
    if (!m_appController.on4kstClient().isLoggedIn()) {
        return;
    }
    const QString targetRoom = on4kstRoomValueForBand(m_currentBand);
    if (targetRoom.isEmpty() || targetRoom == m_currentOn4kstRoom) {
        return;
    }
    m_appController.on4kstClient().switchRoom(targetRoom);
    m_currentOn4kstRoom = targetRoom;
}

void MainWindow::updateStatusBar()
{
    const ContestSettings settings = m_appController.settings();
    const ContestDefinition* def = findContestDefinition(settings.activeContestId);
    const QString contestName = def ? def->name() : settings.activeContestId;
    const int qsoCount = m_appController.database().qsoCountForContest(settings.activeContestId);
    statusBar()->showMessage(QStringLiteral("%1 | %2 | %3 QSOs")
                                  .arg(settings.ownCallsign.isEmpty() ? QStringLiteral("(kein Rufzeichen)") : settings.ownCallsign)
                                  .arg(contestName)
                                  .arg(qsoCount));

    // Connect/disconnected badges, per the plan's status-bar direction:
    // the named kBadgeOkBg/kGreenText vs. kBadgeOffBg/kTextInactive
    // pairs from StyleConstants.h, not an invented colour.
    const bool rigctldOk = m_appController.rigctldClient().state() == RigctldClient::State::Connected;
    // Band's one remaining surface now that it has no dedicated entry-
    // row control (see the class comment in MainWindow.h and this
    // task's report) -- appended to the CAT badge, matching the
    // approved mockup's own "CAT · 144 SSB" status-bar pill.
    QString catText = rigctldStateText(m_appController.rigctldClient().state());
    if (!m_currentBand.isEmpty()) {
        catText += QStringLiteral(" · %1 %2").arg(m_currentBand, m_currentMode);
    }
    setStatusBadge(m_rigctldStatusLabel, rigctldOk, QStringLiteral("CAT: %1").arg(catText));

    QString on4kstText;
    if (m_appController.on4kstClient().isLoggedIn()) {
        on4kstText = QStringLiteral("verbunden");
    } else if (m_appController.on4kstClient().isConnected()) {
        on4kstText = QStringLiteral("anmelden...");
    } else {
        on4kstText = QStringLiteral("getrennt");
    }
    setStatusBadge(m_on4kstStatusLabel, m_appController.on4kstClient().isLoggedIn(),
                    QStringLiteral("ON4KST: %1").arg(on4kstText));

    setStatusBadge(m_clusterStatusLabel, m_appController.dxClusterClient().isConnected(),
                    QStringLiteral("Cluster: %1")
                        .arg(m_appController.dxClusterClient().isConnected() ? QStringLiteral("verbunden") : QStringLiteral("getrennt")));

    // Unknown is a dash, not a zero/placeholder hyphen -- HAUSSTIL
    // rule 7. No own grid configured yet is genuinely unknown, not
    // "grid zero".
    m_gridRadiusLabel->setText(QStringLiteral("Grid %1 | Radius %2 km")
                                    .arg(settings.ownGrid.isEmpty() ? Style::unknownDash() : settings.ownGrid)
                                    .arg(settings.radiusKm, 0, 'f', 0));

    m_modeToggleButton->setText(operatingModeButtonText(settings.operatingMode));

    // Keeps the Log panel's own DXLog-style status line (ui/
    // UnifiedLogWidget.h's setOperatingMode()) and its ⚙ view-mode
    // choice in sync with ContestSettings on every path that reaches
    // this already-central refresh point -- toggleOperatingMode(),
    // openSettingsDialog(), a fresh QSO logged, and startup. Both
    // setters are cheap/idempotent when nothing actually changed (see
    // their own doc comments), so calling them unconditionally here
    // needs no dirty-check.
    m_unifiedLog->setOperatingMode(settings.operatingMode);
    m_unifiedLog->setViewMode(settings.logViewMode);
    m_unifiedLog->setEntryRowPosition(settings.logEntryRowPosition);
}

void MainWindow::handleLogRequested()
{
    const QString callsign = m_unifiedLog->callsign();
    const ContestSettings settings = m_appController.settings();

    // Enter Sends Message: Enter keys the text this state calls for
    // and only logs once the exchange is complete -- see
    // core/EsmPlanner.h. CW only: there is nothing to "send" in SSB
    // without a voice keyer, so Enter keeps its plain log meaning there.
    if (settings.esmEnabled && m_currentMode.trimmed().toUpper() == QStringLiteral("CW")) {
        const ContestDefinition* esmDef = findContestDefinition(settings.activeContestId);
        const bool complete = esmDef ? exchangeComplete(*esmDef, m_unifiedLog->exchangeReceived()) : true;
        EsmTemplates templates;
        templates.cq = settings.esmCq;
        templates.runExchange = settings.esmRunExchange;
        templates.tu = settings.esmTu;
        templates.myCall = settings.esmMyCall;
        templates.spExchange = settings.esmSpExchange;
        const EsmPlan plan = planEnter(settings.operatingMode == ContestSettings::OperatingMode::Run
                                           ? EsmMode::Run
                                           : EsmMode::SearchAndPounce,
                                       !callsign.isEmpty(), complete, templates);
        if (!plan.templateText.isEmpty()) {
            const QString text = substituteEsm(plan.templateText, callsign, currentSentExchangeText(), settings.ownCallsign);
            m_appController.rigctldClient().sendMorse(text);
            statusBar()->showMessage(QStringLiteral("ESM: %1").arg(text), 3000);
        }
        if (plan.focusExchange) {
            m_unifiedLog->focusFirstEmptyExchangeField();
        }
        if (!plan.logQso) {
            return;
        }
    }
    if (callsign.isEmpty()) {
        return;
    }
    // m_currentBand/m_currentMode, not UI fields -- see the class
    // comment in MainWindow.h.
    const QString band = m_currentBand;
    const QString mode = m_currentMode;
    const QMap<QString, QString> exchangeReceived = m_unifiedLog->exchangeReceived();

    const ContestDefinition* def = findContestDefinition(settings.activeContestId);

    // Enter on an incomplete exchange (no received number/locator --
    // the operator's own log had such rows, 2026-09-21): the first
    // Enter jumps to the missing field and says so, the way N1MM+'s
    // ESM does; a second Enter with nothing typed in between logs
    // anyway (the station faded before the locator came, the serial
    // sequence must go on). m_incompleteExchangeEnterArmed is cleared
    // by any edit to the entry row (formChanged).
    if (def && !exchangeComplete(*def, exchangeReceived)) {
        if (!m_incompleteExchangeEnterArmed) {
            m_incompleteExchangeEnterArmed = true;
            m_unifiedLog->focusFirstEmptyExchangeField();
            statusBar()->showMessage(
                QStringLiteral("Exchange unvollständig (Nummer/Locator fehlt) — Enter nochmals loggt trotzdem"), 6000);
            return;
        }
    }
    m_incompleteExchangeEnterArmed = false;
    const QStringList dupeScope = def ? def->dupeScope() : QStringList{QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")};

    const bool dupe = m_appController.dupeChecker().isDupe(callsign, band, mode, settings.activeContestId, dupeScope);
    // A query failure (not a genuine miss) makes `dupe` above
    // untrustworthy -- see DupeChecker::lastError()'s own doc comment.
    // Warn rather than silently logging a possible double QSO as new;
    // still proceeds with logging (a DB write failure would already
    // surface its own warning at insertQso() below, and refusing to log
    // at all over a dupe-CHECK failure would be worse mid-contest).
    if (!m_appController.dupeChecker().lastError().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                              QStringLiteral("Dupe-Prüfung fehlgeschlagen, Status könnte falsch sein:\n%1")
                                  .arg(m_appController.dupeChecker().lastError()));
    }

    // Per band (IARU R1: 001 for the first contact on each band) unless
    // the definition says one sequence for the whole contest.
    const int serialSent = m_appController.database().nextSerialForContest(
        settings.activeContestId, def && def->serialScope() == QStringLiteral("band") ? band : QString());

    // The grid_square/serial_rcvd DB columns (used for distance/
    // bearing, the dupe/multiplier/known-exchange machinery -- see
    // MultiplierTracker.cpp, which reads qsos.grid_square directly, not
    // the exchange text) are independent of which literal key the
    // active ContestDefinition uses -- find them by type/auto_increment
    // like UnifiedLogWidget itself does, per this task's whole point.
    QString gridRcvd;
    int serialRcvd = 0;
    QString rstRcvd;
    if (def) {
        if (const ContestDefinition::ExchangeField* gridField = findFieldByType(*def, QStringLiteral("grid6"))) {
            gridRcvd = exchangeReceived.value(gridField->key).trimmed().toUpper();
        }
        if (const ContestDefinition::ExchangeField* serialField = findAutoIncrementField(*def)) {
            serialRcvd = exchangeReceived.value(serialField->key).trimmed().toInt();
        }
        if (const ContestDefinition::ExchangeField* rstField = findFieldByType(*def, QStringLiteral("rst"))) {
            rstRcvd = exchangeReceived.value(rstField->key).trimmed();
        }
    }

    QsoRecord record;
    record.callsign = callsign;
    record.band = band;
    record.mode = mode;
    record.timestampUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    record.gridSquare = gridRcvd;
    if (isValidGridSquare(settings.ownGrid) && isValidGridSquare(gridRcvd)) {
        record.distanceKm = calculateDistanceKm(settings.ownGrid, gridRcvd);
        record.bearingDeg = bearingIfApart(*record.distanceKm, calculateBearingInDegrees(settings.ownGrid, gridRcvd));
    }
    if (const qint64 rfHz = currentRfFrequencyHz(); rfHz > 0) {
        record.freqHz = rfHz; // on the air, not the rig's IF
    }
    record.serialSent = serialSent;
    if (serialRcvd > 0) {
        record.serialRcvd = serialRcvd;
    }
    record.rstRcvd = rstRcvd;

    if (def) {
        record.exchangeSent = composeExchange(*def, buildSentExchangeValues(*def, settings, serialSent, mode));
        record.exchangeRcvd = composeExchange(*def, buildReceivedExchangeValues(*def, exchangeReceived));
        if (findFieldByType(*def, QStringLiteral("rst"))) {
            record.rstSent = defaultRstForMode(mode);
        }
    }

    record.contestId = settings.activeContestId;
    record.isDupe = dupe;
    record.source = QStringLiteral("manual");

    if (!m_appController.database().insertQso(record)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                              QStringLiteral("QSO konnte nicht gespeichert werden:\n%1").arg(m_appController.database().lastError()));
        return;
    }

    m_appController.publishQso(record);

    // The plan's "Rate-Potenzial aus einem frischen eigenen QSO" --
    // this QSO's own bearing is live evidence of an open path on this
    // band, feeding RecentPropagationTracker before the score refresh
    // below so a nearby-bearing candidate picks up the boost
    // immediately, not on the next unrelated refresh. Only a genuinely
    // fresh log, not a later history-row correction/invalidate (see
    // handleHistoryExchangeRcvdEditRequested()/
    // handleHistoryInvalidToggleRequested(), neither of which records
    // here) -- editing a QSO from an hour ago is not live evidence of
    // anything happening right now.
    if (record.bearingDeg.has_value()) {
        m_appController.recentPropagationTracker().recordQso(record.band, *record.bearingDeg,
                                                               QDateTime::currentDateTimeUtc());
    }

    // Before refreshMapWidget()/refreshSuggestionPanel() below -- both
    // read ChatFeedModel's worked/score state, which must reflect THIS
    // QSO (and any multiplier it just completed, and any propagation
    // boost it just seeded) before either runs.
    refreshMultiplierAndFeedScores();
    refreshLogTable();
    refreshMapWidget();
    refreshSuggestionPanel();
    updateStatusBar();
    reloadCheckPartialSources();
    refreshBandmap();
    refreshScoreboard();
    refreshSkeds();
    m_unifiedLog->resetForNextEntry();
    // The serial just advanced (this QSO consumed serialSent) -- the
    // preview must reflect the *next* one immediately, not the one that
    // was just logged.
    refreshSentExchangePreview();
    // Logged outside the contest period: a note, not a refusal --
    // practice QSOs before the start are how the dry run works, and
    // Datei > Log prüfen lists them as errors anyway before the export.
    if (def) {
        const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
        const ContestWindow window = effectiveContestWindow(def->schedule(), settings.contestEndUtc, nowUtc);
        if (window.isValid() && !window.contains(nowUtc)) {
            statusBar()->showMessage(QStringLiteral("Hinweis: QSO außerhalb des Contestzeitraums geloggt (%1)")
                                         .arg(window.describe()), 10000);
        }
    }
}

void MainWindow::recheckDupeIndicator()
{
    const QString callsign = m_unifiedLog->callsign();
    if (callsign.isEmpty()) {
        m_unifiedLog->setDupeIndicator(false);
        return;
    }
    const ContestSettings settings = m_appController.settings();
    const ContestDefinition* def = findContestDefinition(settings.activeContestId);
    const QStringList dupeScope = def ? def->dupeScope() : QStringList{QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")};
    const std::optional<int> earlierId =
        m_appController.dupeChecker().firstMatchId(callsign, m_currentBand, m_currentMode, settings.activeContestId, dupeScope);
    if (!earlierId) {
        m_unifiedLog->setDupeIndicator(false);
        return;
    }
    // Which QSO this duplicates -- its number, UTC time and band --
    // right in the log panel's status line, and that row highlighted
    // (operator, 2026-09-21: "sollte ein dupe kommen soll sofort die
    // nummer stehen, mit der ich geloggt habe, inkl. uhrzeit").
    QString detail = QStringLiteral("DUPE");
    if (const auto earlier = m_appController.database().qsoById(*earlierId)) {
        const QDateTime at = QDateTime::fromString(earlier->timestampUtc, Qt::ISODate);
        detail = QStringLiteral("DUPE: %1 schon geloggt als Nr. %2 um %3 UTC auf %4")
                     .arg(earlier->callsign,
                          earlier->serialSent ? QString::number(*earlier->serialSent).rightJustified(3, QLatin1Char('0'))
                                              : Style::unknownDash(),
                          at.isValid() ? at.toUTC().time().toString(QStringLiteral("HH:mm")) : earlier->timestampUtc,
                          earlier->band.isEmpty() ? Style::unknownDash() : earlier->band);
        m_unifiedLog->selectHistoryQso(*earlierId);
    }
    m_unifiedLog->setDupeIndicator(true, detail);
}

void MainWindow::handleCandidateActivated(const QString& callsign, const QString& grid, qint64 freqHz)
{
    m_unifiedLog->setCallsign(callsign);
    bool gridFieldSet = false;
    if (!grid.isEmpty()) {
        const ContestDefinition* def = findContestDefinition(m_appController.settings().activeContestId);
        const ContestDefinition::ExchangeField* gridField = def ? findFieldByType(*def, QStringLiteral("grid6")) : nullptr;
        if (gridField) {
            // Unconditionally calls QLineEdit::setText(), which already
            // fires the grid6 sub-field's textChanged ->
            // UnifiedLogWidget::receivedGridChanged -> the live km/
            // bearing preview, the same path a manual keystroke takes --
            // no separate handleReceivedGridChanged() call needed below
            // for this branch.
            m_unifiedLog->setExchangeFieldValue(gridField->key, grid);
            gridFieldSet = true;
        }
    }
    if (!gridFieldSet) {
        // No grid6 field in this contest, or the candidate carries no
        // grid at all -- clear a stale preview from a previous entry
        // rather than leave it showing the wrong contact's distance.
        handleReceivedGridChanged(QString());
    }
    recheckDupeIndicator();
    commandRotorForCandidate(callsign, grid, freqHz);
}

void MainWindow::handleCallsignLookupRequested(const QString& callsign)
{
    // Tier 1: the operator's own log for the active contest -- instant,
    // always-on, exactly as before this task.
    const auto known = m_appController.database().knownExchangeForCallsign(callsign, m_appController.settings().activeContestId);
    if (known.has_value()) {
        // applyKnownExchange() only fills a field that is still empty --
        // when it does, its own QLineEdit::setText() already fires the
        // grid6 sub-field's textChanged -> receivedGridChanged -> the
        // live km/bearing preview, same as a manual keystroke. No
        // separate call here (and no correct value to push if the field
        // was already non-empty and the fill was skipped).
        // Grid only. The received SERIAL of an earlier QSO with this
        // station is never the right prefill: on the other band the
        // station sends a fresh number, on the same band this is a
        // dupe -- and a prefilled wrong number gets logged with one
        // Enter (2026-09-21, seen live). N1MM+/DXLog never prefill it.
        m_unifiedLog->applyKnownExchange(known->gridSquare, std::nullopt);
        return;
    }

    // Tier 1b: any earlier contest in this database -- the station
    // worked last year from the same hill still sits in the same
    // square. Grid only; a serial belongs to one contest.
    if (const auto earlier = m_appController.database().lastKnownGridForCallsign(callsign)) {
        m_unifiedLog->applyKnownExchange(*earlier, std::nullopt);
        return;
    }

    // Tier 2: the imported/cached locator table (core/
    // CallsignLocatorLookup.h) -- still synchronous, instant, and
    // offline-safe. Populated by a one-time CSV import (SettingsDialog's
    // "Locator-Liste importieren...", N1MM+'s "Call History File"
    // concept) and/or by a previous tier-3 lookup this session already
    // cached.
    CallsignLocatorLookup& lookup = m_appController.callsignLocatorLookup();
    const auto localGrid = lookup.lookupLocal(callsign);
    if (localGrid.has_value()) {
        m_unifiedLog->applyKnownExchange(*localGrid, std::nullopt);
        return;
    }

    // Tier 3: a live QRZ.com/HamQTH lookup -- only once both misses above
    // AND a provider is actually configured (ContestSettings::
    // callbookProvider), and only fired off async; the result (if any)
    // arrives later in handleExternalCallsignLookupFinished(), never
    // here.
    if (lookup.isExternalLookupAvailable()) {
        lookup.lookupExternal(callsign);
    }
}

void MainWindow::handleExternalCallsignLookupFinished(const QString& callsign, bool found, const QString& grid)
{
    if (!found) {
        // Degrade silently -- no dialog, no status-bar noise, per the
        // task's own "no crash, no hang, just 'not found'" requirement.
        return;
    }
    // Stale-response guard: the operator may have already changed or
    // cleared the callsign field by the time this HTTP round trip
    // completes, and applying a grid for a callsign no longer in the
    // field would be actively wrong, not merely stale -- mirrors
    // UnifiedLogWidget's own onCallsignLookupTimeout()/
    // onCallsignTextChanged() debounce guard for the same reason.
    if (m_unifiedLog->callsign().compare(callsign, Qt::CaseInsensitive) != 0) {
        return;
    }
    m_unifiedLog->applyKnownExchange(grid, std::nullopt);
}

void MainWindow::handleReceivedGridChanged(const QString& grid)
{
    const ContestSettings settings = m_appController.settings();
    if (!isValidGridSquare(settings.ownGrid) || !isValidGridSquare(grid)) {
        m_unifiedLog->setEntryDistanceBearing(std::nullopt, std::nullopt);
        return;
    }
    const double distanceKm = calculateDistanceKm(settings.ownGrid, grid);
    m_unifiedLog->setEntryDistanceBearing(distanceKm,
                                           bearingIfApart(distanceKm, calculateBearingInDegrees(settings.ownGrid, grid)));
}

void MainWindow::handleHistoryCallsignEditRequested(int qsoId, const QString& newCallsign)
{
    // Fetch the full record first -- LogTableModel::updateRecord()
    // replaces the whole row, so every other already-displayed column
    // (band/mode/timestamp/exchange/...) must be carried through
    // unchanged, not just the one field this edit touched.
    const auto current = m_appController.database().qsoById(qsoId);
    if (!current) {
        return;
    }
    QString error;
    if (!m_appController.database().updateQsoCallsign(qsoId, newCallsign, &error)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                              QStringLiteral("QSO konnte nicht aktualisiert werden:\n%1").arg(error));
        return;
    }
    QsoRecord updated = *current;
    updated.callsign = newCallsign;
    m_logModel->updateRecord(updated);
    // The corrected call may now duplicate an earlier QSO, or no longer
    // be the dupe it was marked as (data/DupeRescore.h); the worked-
    // station map and the feed scoring read the callsign too.
    rescoreDupes();
    recheckDupeIndicator();
    refreshMultiplierAndFeedScores();
    refreshMapWidget();
    refreshSuggestionPanel();
}

void MainWindow::handleHistoryExchangeRcvdEditRequested(int qsoId, const QString& newText)
{
    const auto current = m_appController.database().qsoById(qsoId);
    if (!current) {
        return;
    }

    const ContestSettings settings = m_appController.settings();
    const ContestDefinition* def = findContestDefinition(settings.activeContestId);

    // Re-derive RST/Serial/Grid from the operator's own typed text, by
    // the active ContestDefinition's declared field order -- the same
    // order composeExchange()/buildReceivedExchangeValues() already
    // join/expect them in (RST, Serial, Grid, per the shipped
    // contest_definitions/*.json -- see this task's report), so
    // splitting on whitespace positionally is exactly as reliable as
    // that composition was in the first place, not fragile string
    // parsing of an arbitrary format.
    QString rstRcvd;
    QString gridRcvd;
    std::optional<int> serialRcvd;
    QString composedText = newText.trimmed();
    if (def) {
        // Type-aware, not positional -- see parseEditedExchange(). The
        // stored exchange text is then recomposed the same way a fresh
        // log composes it (zero-padded serial, field order), so the
        // Cabrillo export and the EDI's extra-exchange column see one
        // consistent form, not the raw cell text.
        const EditedExchange edited = parseEditedExchange(*def, current->rstRcvd, newText);
        rstRcvd = edited.rst;
        gridRcvd = edited.grid;
        serialRcvd = edited.serial;
        QMap<QString, QString> values;
        if (const ContestDefinition::ExchangeField* rstField = findFieldByType(*def, QStringLiteral("rst"))) {
            values.insert(rstField->key, rstRcvd);
        }
        if (const ContestDefinition::ExchangeField* serialField = findAutoIncrementField(*def)) {
            values.insert(serialField->key, serialRcvd ? QString::number(*serialRcvd) : QString());
        }
        if (const ContestDefinition::ExchangeField* gridField = findFieldByType(*def, QStringLiteral("grid6"))) {
            values.insert(gridField->key, gridRcvd);
        }
        composedText = composeExchange(*def, buildReceivedExchangeValues(*def, values));
    }

    // Distance/bearing are recomputed, same as at initial log time
    // (handleLogRequested()) -- a hand-corrected grid should not leave
    // a stale km/bearing pair behind.
    std::optional<double> distanceKm;
    std::optional<double> bearingDeg;
    if (isValidGridSquare(settings.ownGrid) && isValidGridSquare(gridRcvd)) {
        distanceKm = calculateDistanceKm(settings.ownGrid, gridRcvd);
        bearingDeg = bearingIfApart(*distanceKm, calculateBearingInDegrees(settings.ownGrid, gridRcvd));
    }

    QString error;
    if (!m_appController.database().updateQsoExchangeRcvd(qsoId, composedText, gridRcvd, serialRcvd, rstRcvd,
                                                            distanceKm, bearingDeg, &error)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                              QStringLiteral("QSO konnte nicht aktualisiert werden:\n%1").arg(error));
        return;
    }

    QsoRecord updated = *current;
    updated.exchangeRcvd = composedText;
    updated.gridSquare = gridRcvd;
    updated.serialRcvd = serialRcvd;
    updated.rstRcvd = rstRcvd;
    updated.distanceKm = distanceKm;
    updated.bearingDeg = bearingDeg;
    m_logModel->updateRecord(updated);
    // The grid may have changed -- keep the worked-station map and the
    // multiplier/feed scoring in sync, same as a fresh QSO log already
    // does (a corrected grid can turn a QSO into a completed multiplier,
    // or out of one).
    refreshMultiplierAndFeedScores();
    refreshMapWidget();
    refreshSuggestionPanel();
}

void MainWindow::handleHistoryInvalidToggleRequested(int qsoId)
{
    const auto current = m_appController.database().qsoById(qsoId);
    if (!current) {
        return;
    }
    const bool newInvalid = !current->isInvalid;
    QString error;
    if (!m_appController.database().setQsoInvalid(qsoId, newInvalid, &error)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                              QStringLiteral("QSO konnte nicht aktualisiert werden:\n%1").arg(error));
        return;
    }
    QsoRecord updated = *current;
    updated.isInvalid = newInvalid;
    m_logModel->updateRecord(updated);
    // A newly-invalidated (or un-invalidated) QSO can change the entry
    // row's dupe status, the worked-station map, AND the multiplier/
    // feed scoring -- see DupeChecker/MultiplierTracker/
    // CabrilloExporter/AdifExporter for the matching is_invalid
    // exclusion this reflects. It also frees (or re-takes) the
    // callsign for a later QSO of the same station: rescoreDupes().
    rescoreDupes();
    recheckDupeIndicator();
    refreshMultiplierAndFeedScores();
    refreshMapWidget();
    refreshSuggestionPanel();
}

void MainWindow::showLogViewOptionsPopup()
{
    if (!m_logHeaderBar) {
        return;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const ContestSettings::LogViewMode currentMode = m_appController.settings().logViewMode;
    const auto addModeAction = [this, menu, currentMode](ContestSettings::LogViewMode mode, const QString& label) {
        QAction* action = menu->addAction(label);
        action->setCheckable(true);
        action->setChecked(currentMode == mode);
        connect(action, &QAction::triggered, this, [this, mode]() {
            // Applies to m_unifiedLog immediately, same "act on yourself
            // first" order RotorWidget::showOptionsPopup() already
            // establishes for its own dial-style menu, then persists
            // and re-runs updateStatusBar() for a full resync (which
            // re-applies the same value -- harmless, see setViewMode()'s
            // own idempotent setColumnHidden() call).
            m_unifiedLog->setViewMode(mode);
            ContestSettings settings = m_appController.settings();
            settings.logViewMode = mode;
            m_appController.setSettings(settings);
            updateStatusBar();
        });
    };
    addModeAction(ContestSettings::LogViewMode::Compact, QStringLiteral("Kompakt"));
    addModeAction(ContestSettings::LogViewMode::DxLogFullColumns, QStringLiteral("DXLog-Vollspalten"));

    menu->addSeparator();

    // Eingabezeile Oben/Unten -- see ContestSettings::LogEntryRowPosition's
    // own doc comment for the operator request this answers. Same
    // "act on yourself first, then persist, then resync" pattern as
    // addModeAction() above.
    const ContestSettings::LogEntryRowPosition currentPosition = m_appController.settings().logEntryRowPosition;
    const auto addPositionAction = [this, menu, currentPosition](ContestSettings::LogEntryRowPosition position,
                                                                   const QString& label) {
        QAction* action = menu->addAction(label);
        action->setCheckable(true);
        action->setChecked(currentPosition == position);
        connect(action, &QAction::triggered, this, [this, position]() {
            m_unifiedLog->setEntryRowPosition(position);
            ContestSettings settings = m_appController.settings();
            settings.logEntryRowPosition = position;
            m_appController.setSettings(settings);
            updateStatusBar();
        });
    };
    addPositionAction(ContestSettings::LogEntryRowPosition::Top, QStringLiteral("Eingabezeile: Oben"));
    addPositionAction(ContestSettings::LogEntryRowPosition::Bottom, QStringLiteral("Eingabezeile: Unten"));

    // Below the header bar, right-aligned with its own right edge -- the
    // same "below, right-aligned" rule RotorWidget::showOptionsPopup()
    // uses for its own ⚙-triggered popup, anchored here to the whole
    // header bar rather than the ⚙ button itself: PanelHeaderBar does
    // not expose that button's own geometry (by design -- see its own
    // class comment: "what opens ... is entirely the caller's choice"),
    // and the button sits right at the header's own right edge (options,
    // then the lock button, both close to the margin), so this is a
    // close, harmless approximation, not a guess at unknown geometry.
    const QSize menuSize = menu->sizeHint();
    const QPoint below = m_logHeaderBar->mapToGlobal(
        QPoint(m_logHeaderBar->width() - menuSize.width() - 6, m_logHeaderBar->height()));
    menu->popup(below);
}

void MainWindow::toggleOperatingMode()
{
    ContestSettings settings = m_appController.settings();
    settings.operatingMode = (settings.operatingMode == ContestSettings::OperatingMode::Run)
        ? ContestSettings::OperatingMode::SearchAndPounce
        : ContestSettings::OperatingMode::Run;
    m_appController.setSettings(settings);
    updateStatusBar();
}

void MainWindow::openSettingsDialog()
{
    const ColorTheme previousColorTheme = m_appController.settings().colorTheme;
    SettingsDialog dialog(m_appController.settings(), m_appController.availableContestDefinitions(),
                           &m_appController.callsignLocatorLookup(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_appController.setSettings(dialog.settings());
    // Style::setActiveTheme() only takes effect for freshly-drawn
    // stylesheet strings (main.cpp applies it once at startup) -- every
    // dockable panel's own frame/badge stylesheet was already baked
    // with the OLD theme's hex values at construction time (see ui/
    // StyleKit.cpp's kXxx() functions), so a live-switch mid-session
    // would need re-invoking every one of those call sites individually.
    // Simpler and safer for now: tell the operator plainly, rather than
    // risk some corner of the UI staying stuck in the old palette.
    if (m_appController.settings().colorTheme != previousColorTheme) {
        // Operator, 2026-09-21 ("hat sich nichts geändert"): the plain
        // "wirksam beim nächsten Start" notice read as nothing having
        // happened. The choice IS saved (setSettings above); a fresh
        // process is the one reliable way onto it, and the restore path
        // (performRestore) already restarts the program that way -- so
        // offer that restart right here instead of leaving it to him.
        QMessageBox box(QMessageBox::Question, QStringLiteral("Contestprogramm"),
                        QStringLiteral("Das Farbthema „%1“ ist gespeichert und wird beim Neustart wirksam.\n\n"
                                       "Jetzt neu starten?")
                            .arg(colorThemeDisplayName(m_appController.settings().colorTheme)),
                        QMessageBox::NoButton, this);
        QPushButton* restartButton = box.addButton(QStringLiteral("Neu starten"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Später"), QMessageBox::RejectRole);
        box.setDefaultButton(restartButton);
        box.exec();
        if (box.clickedButton() == restartButton) {
            saveWindowGeometry();
            emit restartRequested();
            return;
        }
    }
    applyActiveContestDefinition();
    applyRotorWidgetSettings();
    applyClockSettings();
    refreshLogTable();
    refreshMapWidget();
    refreshSuggestionPanel();
    m_mapWidget->setVisibleRangeKm(m_appController.settings().radiusKm);
    m_rateMeterWidget->setSource(&m_appController.database(), m_appController.settings().activeContestId);
    updateStatusBar();
}

void MainWindow::openContestRulesEditor()
{
    ContestRulesEditor dialog(m_appController.availableContestDefinitions(), m_appController.settings().activeContestId, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    // ContestRulesEditor already wrote the override file itself (see
    // its onSave()) -- reloadContestDefinitions() re-scans
    // shipped+override JSON and emits contestDefinitionsChanged(),
    // which applyActiveContestDefinition() (connected in the
    // constructor) picks up to rebuild UnifiedLogWidget's exchange row.
    m_appController.reloadContestDefinitions();

    ContestSettings settings = m_appController.settings();
    const QString saved = dialog.savedContestId();
    if (dialog.deletedSelectedContest()) {
        // Der Contest kann jetzt weg sein (selbst angelegt und
        // zurückgesetzt). War er der aktive, muss ein anderer her --
        // sonst steht das Programm ohne Exchange-Zeile da.
        if (settings.activeContestId == saved && !m_appController.findContestDefinition(saved)) {
            const QVector<ContestDefinition>& available = m_appController.availableContestDefinitions();
            settings.activeContestId = available.isEmpty() ? QString() : available.first().id();
            m_appController.setSettings(settings);
            applyActiveContestDefinition();
        }
        return;
    }
    // Ein gerade angelegter Contest ist der, mit dem weitergearbeitet
    // werden soll -- ihn erst noch in "Contest wählen..." suchen zu
    // müssen wäre ein Umweg ohne Zweck.
    if (!saved.isEmpty() && saved != settings.activeContestId && m_appController.findContestDefinition(saved)) {
        settings.activeContestId = saved;
        m_appController.setSettings(settings);
        applyActiveContestDefinition();
    }
}

void MainWindow::openContestPicker()
{
    ContestPickerDialog dialog(m_appController.availableContestDefinitions(), m_appController.settings().activeContestId, this);
    // The locator is asked every time (see the dialog's class comment):
    // prefilled with the current one; the square the settings' exact
    // position lies in is offered when it differs (raw keys, like
    // horizonProfileForOwnStation()).
    QString exactGrid;
    {
        const ContestDatabase& db = m_appController.database();
        if (db.settingValue(QStringLiteral("use_exact_own_location")) == QStringLiteral("1")) {
            bool okLat = false;
            bool okLon = false;
            const double lat = db.settingValue(QStringLiteral("own_exact_latitude")).toDouble(&okLat);
            const double lon = db.settingValue(QStringLiteral("own_exact_longitude")).toDouble(&okLon);
            if (okLat && okLon && (lat != 0.0 || lon != 0.0) && std::fabs(lat) <= 90.0 && std::fabs(lon) <= 180.0) {
                exactGrid = gridSquareFromLatLon(lat, lon).toUpper();
            }
        }
    }
    dialog.setOwnGrid(m_appController.settings().ownGrid, exactGrid);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString selectedId = dialog.selectedContestId();
    const QString grid = dialog.ownGrid();
    const bool contestChanged = !selectedId.isEmpty() && selectedId != m_appController.settings().activeContestId;
    const bool gridChanged = isValidGridSquare(grid) && grid != m_appController.settings().ownGrid.trimmed().toUpper();
    if (!contestChanged && !gridChanged) {
        return;
    }
    // Same propagation path SettingsDialog's own contest combo already
    // triggers on accept (see openSettingsDialog() above) -- one way for
    // activeContestId (and the own locator) to change to actually reach
    // the running UI, not two independently-maintained copies of it.
    ContestSettings settings = m_appController.settings();
    if (contestChanged) {
        settings.activeContestId = selectedId;
    }
    if (gridChanged) {
        settings.ownGrid = grid;
    }
    m_appController.setSettings(settings);
    applyActiveContestDefinition();
    // The countdown follows the definition's schedule now, so a contest
    // switch must re-derive it too, not only the settings dialog.
    applyClockSettings();
    applyRotorWidgetSettings();
    refreshLogTable();
    refreshMapWidget();
    refreshSuggestionPanel();
    m_mapWidget->setVisibleRangeKm(m_appController.settings().radiusKm);
    m_rateMeterWidget->setSource(&m_appController.database(), m_appController.settings().activeContestId);
    if (gridChanged) {
        // The terrain and horizon follow the own position too.
        refreshTerrainSectors();
        statusBar()->showMessage(QStringLiteral("Eigener Locator jetzt %1.").arg(grid), 8000);
    }
    updateStatusBar();
}

void MainWindow::exportCabrillo()
{
    const ContestSettings settings = m_appController.settings();
    const ContestDefinition* def = findContestDefinition(settings.activeContestId);
    if (!def) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"), QStringLiteral("Kein aktiver Contest ausgewählt."));
        return;
    }

    // Erst die Angaben, die kein QSO beantworten kann (Leistung,
    // Bedienerklasse, Hilfsmittel), dann die Datei -- der Robot liest
    // sie, und falsch angegeben landet das Log in der falschen
    // Wertungsklasse.
    CabrilloExportDialog categoriesDialog(CabrilloCategories::load(m_appController.database()), this);
    if (categoriesDialog.exec() != QDialog::Accepted) {
        return;
    }
    const CabrilloCategories categories = categoriesDialog.categories();
    categories.save(m_appController.database());

    const QString suggested = settings.activeContestId + QStringLiteral(".cbr");
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Cabrillo exportieren"), suggested,
                                                        QStringLiteral("Cabrillo-Log (*.cbr *.log)"));
    if (path.isEmpty()) {
        return;
    }

    CabrilloExporter exporter(m_appController.database());
    const QString text = exporter.exportContest(settings.activeContestId, *def, settings, categories);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                              QStringLiteral("Datei konnte nicht geschrieben werden:\n%1").arg(file.errorString()));
        return;
    }
    file.write(text.toUtf8());
    file.close();

    statusBar()->showMessage(QStringLiteral("Cabrillo-Log exportiert: %1").arg(path), 5000);
}

void MainWindow::exportAdif()
{
    const ContestSettings settings = m_appController.settings();
    if (settings.activeContestId.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"), QStringLiteral("Kein aktiver Contest ausgewählt."));
        return;
    }

    const QString suggested = settings.activeContestId + QStringLiteral(".adi");
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("ADIF exportieren"), suggested,
                                                        QStringLiteral("ADIF-Log (*.adi *.adif)"));
    if (path.isEmpty()) {
        return;
    }

    AdifExporter exporter(m_appController.database());
    const QString text = exporter.exportContest(settings.activeContestId);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                              QStringLiteral("Datei konnte nicht geschrieben werden:\n%1").arg(file.errorString()));
        return;
    }
    file.write(text.toUtf8());
    file.close();

    statusBar()->showMessage(QStringLiteral("ADIF-Log exportiert: %1").arg(path), 5000);
}

void MainWindow::exportEdi()
{
    const ContestSettings settings = m_appController.settings();
    const ContestDefinition* def = findContestDefinition(settings.activeContestId);
    if (!def) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"), QStringLiteral("Kein aktiver Contest ausgewählt."));
        return;
    }

    // The dialog writes the files itself (one per band) -- see
    // EdiExportDialog.h; only the confirmation is shown here.
    EdiExportDialog dialog(m_appController.database(), *def, settings, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QStringList files = dialog.writtenFiles();
    statusBar()->showMessage(QStringLiteral("EDI-Log exportiert: %1").arg(files.join(QStringLiteral(", "))), 8000);
}

void MainWindow::reloadCheckPartialSources()
{
    if (!m_checkPartialWidget) {
        return;
    }
    const ContestSettings settings = m_appController.settings();
    QVector<QPair<QString, QString>> logCalls;
    for (const QsoRecord& record : m_appController.database().qsosForContest(settings.activeContestId)) {
        if (!record.isInvalid) {
            logCalls.append({record.callsign, record.band});
        }
    }
    m_checkPartialIndex.setLogCalls(logCalls);
    m_checkPartialIndex.setHistoryCalls(m_appController.database().allImportedLocators());

    // The SCP list is loaded once from the remembered path; a missing
    // or unreadable file just leaves that source empty (and the panel's
    // status line offering to load one).
    if (!m_scpLoaded) {
        m_scpLoaded = true;
        const QString path = m_appController.database().settingValue(QStringLiteral("scp_file_path"));
        QFile file(path);
        if (!path.isEmpty() && file.open(QIODevice::ReadOnly)) {
            m_checkPartialIndex.setScpCalls(CheckPartialIndex::parseScp(file.readAll()));
        }
    }
    const QString scpPath = m_appController.database().settingValue(QStringLiteral("scp_file_path"));
    m_checkPartialWidget->setSources(m_checkPartialIndex.scpCount(),
                                     m_checkPartialIndex.scpCount() > 0 ? QFileInfo(scpPath).fileName() : QString(),
                                     m_checkPartialIndex.historyCount(), m_checkPartialIndex.seenCount());
    refreshCheckPartial();
}

void MainWindow::refreshCheckPartial()
{
    if (!m_checkPartialWidget) {
        return;
    }
    const QString fragment = m_unifiedLog->callsign();
    m_checkPartialWidget->setMatches(fragment, m_checkPartialIndex.matches(fragment, m_currentBand));
}

void MainWindow::importOldLogs()
{
    // Old contest logs from any program (EDI is what the robots took,
    // ADIF is what every logger exports) feed the locator memory --
    // N1MM+'s Call History, grown from what was actually worked. Only
    // call + locator are taken; nothing is logged as a QSO.
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Alte Logs (EDI/ADIF)"), QString(),
        QStringLiteral("Contest-Logs (*.edi *.adi *.adif);;Alle Dateien (*)"));
    if (paths.isEmpty()) {
        return;
    }
    int imported = 0;
    int withoutGrid = 0;
    QStringList unreadable;
    for (const QString& path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            unreadable << QFileInfo(path).fileName();
            continue;
        }
        const QVector<ImportedQso> qsos = LogFileReader::parse(file.readAll(), path);
        if (qsos.isEmpty()) {
            unreadable << QFileInfo(path).fileName();
            continue;
        }
        for (const ImportedQso& qso : qsos) {
            if (isValidGridSquare(qso.grid)) {
                m_appController.database().upsertImportedLocator(qso.callsign, qso.grid);
                ++imported;
            } else {
                ++withoutGrid;
            }
        }
    }
    reloadCheckPartialSources();
    QString summary = QStringLiteral("%1 Rufzeichen mit Locator übernommen").arg(imported);
    if (withoutGrid > 0) {
        summary += QStringLiteral(", %1 ohne Locator übersprungen").arg(withoutGrid);
    }
    if (!unreadable.isEmpty()) {
        summary += QStringLiteral("\nNicht lesbar (kein EDI/ADIF?): %1").arg(unreadable.join(QStringLiteral(", ")));
    }
    QMessageBox::information(this, QStringLiteral("Contestprogramm"), summary);
}

void MainWindow::loadScpFile()
{
    const QString previous = m_appController.database().settingValue(QStringLiteral("scp_file_path"));
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("SCP-Liste laden"), previous,
                                                      QStringLiteral("Super-Check-Partial-Listen (*.scp *.txt *.dta);;Alle Dateien (*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                             QStringLiteral("SCP-Liste konnte nicht gelesen werden:\n%1").arg(file.errorString()));
        return;
    }
    const QStringList calls = CheckPartialIndex::parseScp(file.readAll());
    if (calls.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                             QStringLiteral("In dieser Datei steht kein Rufzeichen (erwartet: eines je Zeile, # für Kommentare)."));
        return;
    }
    m_checkPartialIndex.setScpCalls(calls);
    m_appController.database().setSettingValue(QStringLiteral("scp_file_path"), path);
    m_checkPartialWidget->setSources(m_checkPartialIndex.scpCount(), QFileInfo(path).fileName(),
                                     m_checkPartialIndex.historyCount(), m_checkPartialIndex.seenCount());
    refreshCheckPartial();
    statusBar()->showMessage(QStringLiteral("SCP-Liste geladen: %1 Rufzeichen aus %2").arg(calls.size()).arg(QFileInfo(path).fileName()), 5000);
}

void MainWindow::handleHistoryTimeEditRequested(int qsoId, const QString& newText)
{
    const auto current = m_appController.database().qsoById(qsoId);
    if (!current) {
        return;
    }
    const QDateTime original = QDateTime::fromString(current->timestampUtc, Qt::ISODate);
    const QString text = newText.trimmed();
    // "HH:mm" / "HH:mm:ss" keep the QSO's own date; a full
    // "yyyy-MM-dd HH:mm[:ss]" sets both. Anything else is refused with
    // the accepted forms spelled out, and the row keeps its old time.
    QDateTime corrected;
    for (const char* timeOnly : {"HH:mm", "HH:mm:ss"}) {
        const QTime t = QTime::fromString(text, QString::fromLatin1(timeOnly));
        if (t.isValid() && original.isValid()) {
            corrected = QDateTime(original.date(), t, QTimeZone::UTC);
            break;
        }
    }
    if (!corrected.isValid()) {
        for (const char* full : {"yyyy-MM-dd HH:mm", "yyyy-MM-dd HH:mm:ss"}) {
            const QDateTime dt = QDateTime::fromString(text, QString::fromLatin1(full));
            if (dt.isValid()) {
                corrected = QDateTime(dt.date(), dt.time(), QTimeZone::UTC);
                break;
            }
        }
    }
    if (!corrected.isValid()) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                             QStringLiteral("Zeit nicht verstanden: \"%1\"\nErlaubt: HH:MM (UTC, Datum bleibt) "
                                            "oder YYYY-MM-DD HH:MM.").arg(text));
        return;
    }
    const QString iso = corrected.toString(Qt::ISODate);
    QString error;
    if (!m_appController.database().updateQsoTimestamp(qsoId, iso, &error)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                              QStringLiteral("QSO konnte nicht aktualisiert werden:\n%1").arg(error));
        return;
    }
    QsoRecord updated = *current;
    updated.timestampUtc = iso;
    m_logModel->updateRecord(updated);
    // The rate windows and the EDI TDate span both read the timestamp,
    // and which of two QSOs with the same station is the dupe is a
    // question of time order.
    m_rateMeterWidget->refresh();
    rescoreDupes();
}

int MainWindow::rescoreDupes()
{
    const ContestSettings settings = m_appController.settings();
    const ContestDefinition* def = findContestDefinition(settings.activeContestId);
    if (!def || !m_logModel) {
        return 0;
    }
    const QVector<QsoRecord> records = m_appController.database().qsosForContest(settings.activeContestId);
    const QVector<DupeFlagChange> changes = recomputeDupeFlags(records, def->dupeScope());
    int applied = 0;
    for (const DupeFlagChange& change : changes) {
        QString error;
        if (!m_appController.database().setQsoDupe(change.qsoId, change.isDupe, &error)) {
            statusBar()->showMessage(QStringLiteral("Dupe-Status konnte nicht gespeichert werden: %1").arg(error), 20000);
            continue;
        }
        // Patched row by row, never a model reset: this runs from a
        // history-cell edit whose editor may still be mid-commit (see
        // UnifiedFeedModel::setLogModel's own note on that).
        if (const auto refreshed = m_appController.database().qsoById(change.qsoId)) {
            m_logModel->updateRecord(*refreshed);
        }
        ++applied;
    }
    if (applied > 0) {
        m_rateMeterWidget->refresh();
        statusBar()->showMessage(QStringLiteral("Dupe-Status von %1 QSO%2 nach der Korrektur angepasst")
                                     .arg(applied).arg(applied == 1 ? QString() : QStringLiteral("s")), 8000);
    }
    return applied;
}

void MainWindow::archiveActiveContest()
{
    // The contest is over (or the test QSOs from the dry run are in the
    // way): move everything logged under the active contest id to an
    // archive id, so the next contest with the same definition starts
    // at 001 with no dupes against last year and a clean EDI -- N1MM's
    // "New log in database", DXLog's new file, without a second file.
    // The old QSOs stay in the database: locator memory and backups
    // keep them, nothing is deleted.
    const ContestSettings settings = m_appController.settings();
    const QString contestId = settings.activeContestId;
    if (contestId.isEmpty()) {
        return;
    }
    ContestDatabase& db = m_appController.database();
    const QVector<QsoRecord> records = db.qsosForContest(contestId);
    if (records.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Contestprogramm"),
                                 QStringLiteral("Das Log des aktiven Contests ist leer -- nichts zu archivieren."));
        return;
    }
    QString lastDate = QStringLiteral("undatiert");
    for (const QsoRecord& record : records) {
        const QDateTime ts = QDateTime::fromString(record.timestampUtc, Qt::ISODate);
        if (ts.isValid()) {
            const QString date = ts.toUTC().date().toString(Qt::ISODate);
            if (lastDate == QStringLiteral("undatiert") || date > lastDate) {
                lastDate = date;
            }
        }
    }
    QString archiveId = QStringLiteral("%1@%2").arg(contestId, lastDate);
    // A second archive on the same day gets a suffix rather than merging
    // into the first.
    const QStringList existing = db.contestIdsInLog();
    for (int n = 2; existing.contains(archiveId); ++n) {
        archiveId = QStringLiteral("%1@%2-%3").arg(contestId, lastDate).arg(n);
    }
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Contestprogramm"),
        QStringLiteral("%1 QSOs des Contests \"%2\" werden unter \"%3\" archiviert.\n\n"
                       "Danach beginnt das Log leer: Seriennummern ab 001, keine Dupes gegen die alten QSOs, "
                       "ein leerer EDI-Export. Die alten QSOs bleiben in der Datenbank (Locator-Gedächtnis, "
                       "Sicherungen) -- aber EDI/ADIF des alten Logs vorher exportieren!\n\nJetzt archivieren?")
            .arg(records.size())
            .arg(contestId, archiveId),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    QString error;
    const int moved = db.archiveContest(contestId, archiveId, &error);
    if (moved < 0) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                             QStringLiteral("Archivieren fehlgeschlagen:\n%1").arg(error));
        return;
    }
    // Everything that reads the active contest's QSOs, same sequence
    // openContestPicker() uses after a contest switch.
    applyActiveContestDefinition();
    refreshMultiplierAndFeedScores();
    refreshLogTable();
    refreshMapWidget();
    refreshSuggestionPanel();
    refreshBandmap();
    m_rateMeterWidget->setSource(&db, contestId);
    updateStatusBar();
    if (LogBackup* backup = m_appController.logBackup()) {
        backup->backupNow(true);
    }
    statusBar()->showMessage(QStringLiteral("%1 QSOs archiviert unter %2 -- Log ist leer").arg(moved).arg(archiveId), 10000);
}

void MainWindow::adjustCwSpeed(int deltaWpm)
{
    ContestSettings settings = m_appController.settings();
    settings.cwSpeedWpm = std::clamp(settings.cwSpeedWpm + deltaWpm, 5, 60);
    m_appController.setSettings(settings);
    m_appController.rigctldClient().setKeyerSpeed(settings.cwSpeedWpm);
    statusBar()->showMessage(QStringLiteral("CW %1 WpM").arg(settings.cwSpeedWpm), 2000);
}

void MainWindow::backupLogNow()
{
    LogBackup* backup = m_appController.logBackup();
    if (!backup) {
        return;
    }
    QString error;
    const QString path = backup->backupNow(true, &error);
    if (path.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                             QStringLiteral("Log-Sicherung fehlgeschlagen:\n%1").arg(error));
        return;
    }
    statusBar()->showMessage(QStringLiteral("Log gesichert: %1").arg(path), 8000);
}

void MainWindow::chooseBackupMirror()
{
    LogBackup* backup = m_appController.logBackup();
    if (!backup) {
        return;
    }
    const QString chosen = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Zweiter Sicherungsordner (USB-Stick, Cloud-Ordner)"),
        backup->mirrorDirectory().isEmpty() ? QDir::homePath() : backup->mirrorDirectory());
    if (chosen.isEmpty()) {
        return;
    }
    backup->setMirrorDirectory(chosen);
    m_appController.database().setSettingValue(QStringLiteral("backup_mirror_dir"), chosen);
    // A first copy right away, so the stick is never empty.
    QString error;
    const QString path = backup->backupNow(true, &error);
    if (path.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("Zweiter Sicherungsordner: %1 — Sicherung fehlgeschlagen: %2").arg(chosen, error), 15000);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Zweiter Sicherungsordner: %1 — Log gesichert.").arg(chosen), 8000);
}

void MainWindow::clearBackupMirror()
{
    LogBackup* backup = m_appController.logBackup();
    if (!backup) {
        return;
    }
    backup->setMirrorDirectory(QString());
    m_appController.database().setSettingValue(QStringLiteral("backup_mirror_dir"), QString());
    statusBar()->showMessage(QStringLiteral("Zweiter Sicherungsordner entfernt — Sicherungen nur noch neben der Datenbank."), 8000);
}

void MainWindow::restoreBackup()
{
    LogBackup* backup = m_appController.logBackup();
    if (!backup) {
        return;
    }
    BackupRestoreDialog dialog(LogBackup::listBackups(backup->directory()), this);
    if (dialog.exec() != QDialog::Accepted || dialog.selectedPath().isEmpty()) {
        return;
    }
    const QString chosen = dialog.selectedPath();
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Contestprogramm"),
        QStringLiteral("Das Log wird auf den Stand dieser Sicherung zurückgesetzt:\n%1\n\nDer jetzige Stand wird "
                       "vorher gesichert, das Programm startet danach neu. Fortfahren?")
            .arg(QFileInfo(chosen).fileName()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!performRestore(chosen, &error)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                             QStringLiteral("Wiederherstellen fehlgeschlagen, nichts geändert:\n%1").arg(error));
    }
}

bool MainWindow::performRestore(const QString& backupPath, QString* errorOut)
{
    LogBackup* backup = m_appController.logBackup();
    if (!backup) {
        return false;
    }
    // The state being replaced becomes the newest backup, so a wrong
    // pick can be undone the same way.
    QString error;
    if (backup->backupNow(true, &error).isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Der jetzige Stand konnte nicht gesichert werden: %1").arg(error);
        }
        return false;
    }
    const QString live = m_appController.database().filePath();
    m_appController.database().close();
    const bool ok = restoreDatabaseFile(backupPath, live, &error);
    if (!ok && errorOut) {
        *errorOut = error;
    }
    // Restart either way: the connection is closed, a fresh process on
    // whatever file is there now is the only sane state.
    emit restartRequested();
    return ok;
}

void MainWindow::openEsmTemplatesDialog()
{
    ContestSettings settings = m_appController.settings();
    EsmTemplates current;
    current.cq = settings.esmCq;
    current.runExchange = settings.esmRunExchange;
    current.tu = settings.esmTu;
    current.myCall = settings.esmMyCall;
    current.spExchange = settings.esmSpExchange;
    EsmTemplatesDialog dialog(current, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const EsmTemplates edited = dialog.templates();
    settings.esmCq = edited.cq;
    settings.esmRunExchange = edited.runExchange;
    settings.esmTu = edited.tu;
    settings.esmMyCall = edited.myCall;
    settings.esmSpExchange = edited.spExchange;
    m_appController.setSettings(settings);
}

void MainWindow::refreshBandmap()
{
    if (!m_bandmapWidget) {
        return;
    }
    const ContestSettings settings = m_appController.settings();
    const ContestDefinition* def = findContestDefinition(settings.activeContestId);
    const QStringList dupeScope = def ? def->dupeScope()
                                      : QStringList{QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")};
    QVector<BandmapSpot> spots = m_bandmapModel.spotsForBand(m_currentBand, QDateTime::currentDateTimeUtc());
    for (BandmapSpot& spot : spots) {
        spot.worked = m_appController.dupeChecker().isDupe(spot.callsign, m_currentBand, m_currentMode,
                                                          settings.activeContestId, dupeScope);
    }
    m_bandmapWidget->setBand(m_currentBand);
    m_bandmapWidget->setOwnFrequencyHz(currentRfFrequencyHz());
    m_bandmapWidget->setSpots(spots);
}

void MainWindow::refreshScoreboard()
{
    if (!m_scoreboard) {
        return;
    }
    const ContestSettings settings = m_appController.settings();
    const ContestDefinition* def = findContestDefinition(settings.activeContestId);
    ScoreboardConfig config;
    config.enabled = settings.scoreboardEnabled;
    config.url = QUrl(settings.scoreboardUrl);
    config.username = settings.scoreboardUsername;
    config.password = settings.scoreboardPassword;
    config.intervalMinutes = settings.scoreboardIntervalMinutes;
    config.contestName = settings.scoreboardContestName.isEmpty() ? settings.activeContestId : settings.scoreboardContestName;
    config.callsign = settings.ownCallsign;
    config.grid = settings.ownGrid;
    EdiStationInfo station;
    station.loadFrom(m_appController.database());
    config.club = station.club;
    config.scoring = def ? def->scoring() : QStringLiteral("distance_km");
    config.bandOrder = def ? def->bands() : QStringList();
    m_scoreboard->setConfig(config);
    m_scoreboard->setRecords(m_appController.database().qsosForContest(settings.activeContestId));
}

void MainWindow::openScoreboardDialog()
{
    ContestSettings settings = m_appController.settings();
    ScoreboardDialog dialog(settings, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    dialog.applyTo(settings);
    m_appController.setSettings(settings);
    refreshScoreboard();
}

void MainWindow::postScoreNow()
{
    refreshScoreboard();
    QString error;
    if (!m_scoreboard->postNow(&error)) {
        QMessageBox::information(this, QStringLiteral("Contestprogramm"),
                                 error.isEmpty() ? QStringLiteral("Online-Scoreboard ist nicht eingeschaltet "
                                                                  "(Datei > Online-Scoreboard...).")
                                                 : error);
    }
}

void MainWindow::reloadSkeds()
{
    if (!m_skedPanel) {
        return;
    }
    m_skeds = m_appController.database().skedsForContest(m_appController.settings().activeContestId);
    refreshSkeds();
}

void MainWindow::refreshSkeds()
{
    if (!m_skedPanel) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const ContestSettings settings = m_appController.settings();
    static const QStringList kCallBandScope{QStringLiteral("callsign"), QStringLiteral("band")};
    bool changed = false;
    for (Sked& sked : m_skeds) {
        if (sked.state != Sked::State::Open) {
            continue;
        }
        // Worked on that band since the sked was made: done -- the log
        // closes the sked, nobody has to tick it off.
        const QString band = sked.band.isEmpty() ? m_currentBand : sked.band;
        if (m_appController.dupeChecker().isDupe(sked.callsign, band, QString(), settings.activeContestId, kCallBandScope)) {
            sked.state = Sked::State::Done;
            m_appController.database().updateSkedState(sked.id, Sked::State::Done);
            changed = true;
        } else if (SkedRules::isOverdue(sked, now)) {
            sked.state = Sked::State::Missed;
            m_appController.database().updateSkedState(sked.id, Sked::State::Missed);
            changed = true;
        }
    }
    Q_UNUSED(changed);
    // One alarm per sked, two minutes ahead: status line + the system
    // beep -- the operator is on the radio, not on the screen.
    if (const Sked* next = SkedRules::nextOpen(m_skeds, now)) {
        const qint64 secs = now.secsTo(next->timeUtc);
        if (secs <= SkedRules::kAlarmBeforeSecs && !m_skedAlarmed.contains(next->id)) {
            m_skedAlarmed.insert(next->id);
            statusBar()->showMessage(QStringLiteral("SKED %1 %2 MHz %3 UTC -- %4")
                                         .arg(next->callsign)
                                         .arg(next->freqHz / 1000000.0, 0, 'f', 3)
                                         .arg(next->timeUtc.time().toString(QStringLiteral("HH:mm")))
                                         .arg(secs > 0 ? QStringLiteral("in %1 min").arg((secs + 30) / 60) : QStringLiteral("jetzt")),
                                     60000);
            QApplication::beep();
        }
    }
    m_skedPanel->setSkeds(m_skeds, now);
}

void MainWindow::addSkedFromEntry(const QString& callsign, const QString& grid, const QString& qrgText, const QString& timeText)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const auto time = SkedRules::parseManualTime(timeText, now);
    if (!time) {
        statusBar()->showMessage(QStringLiteral("Sked-Zeit nicht verstanden: \"%1\" (UTC HH:MM oder +Minuten)").arg(timeText), 6000);
        return;
    }
    Sked sked;
    sked.callsign = callsign.trimmed().toUpper();
    sked.grid = grid.trimmed().toUpper();
    if (sked.grid.isEmpty()) {
        sked.grid = m_appController.database().lastKnownGridForCallsign(sked.callsign).value_or(QString());
    }
    if (const auto hz = SkedRules::parseFrequencyHz(qrgText)) {
        sked.freqHz = *hz;
    } else if (qrgText.trimmed().isEmpty() && m_appController.rigctldClient().isConnected()) {
        sked.freqHz = currentRfFrequencyHz();
    } else if (!qrgText.trimmed().isEmpty()) {
        statusBar()->showMessage(QStringLiteral("Sked-Frequenz nicht verstanden: \"%1\" (MHz, z.B. 144.317)").arg(qrgText), 6000);
        return;
    }
    sked.band = sked.freqHz > 0 ? bandLabelForFrequencyHz(sked.freqHz) : m_currentBand;
    sked.timeUtc = *time;
    sked.state = Sked::State::Open;
    sked.source = QStringLiteral("manual");
    sked.createdUtc = now;
    if (!m_appController.database().insertSked(m_appController.settings().activeContestId, sked)) {
        statusBar()->showMessage(QStringLiteral("Sked konnte nicht gespeichert werden: %1").arg(m_appController.database().lastError()), 8000);
        return;
    }
    m_skedPanel->clearEntry();
    reloadSkeds();
}

void MainWindow::activateSked(int skedId)
{
    for (const Sked& sked : m_skeds) {
        if (sked.id != skedId) {
            continue;
        }
        // The sked's whole point: tune, turn, fill -- the QSO can start.
        if (sked.freqHz > 0 && m_appController.rigctldClient().isConnected()) {
            m_appController.rigctldClient().setFrequency(m_transverter.rigFrequencyHz(sked.freqHz));
        }
        handleCandidateActivated(sked.callsign, sked.grid, sked.freqHz);
        statusBar()->showMessage(QStringLiteral("Sked %1: %2 MHz, Rotor %3")
                                     .arg(sked.callsign)
                                     .arg(sked.freqHz / 1000000.0, 0, 'f', 3)
                                     .arg(sked.grid.isEmpty() ? QStringLiteral("(kein Locator)") : sked.grid),
                                 5000);
        return;
    }
}

void MainWindow::openMultiplierWindow()
{
    if (!m_multiplierWindow) {
        m_multiplierWindow = new MultiplierWindow(m_appController.multiplierTracker(), this);
        // A real, separate top-level window rather than an embedded
        // child -- see the class comment in MultiplierWindow.h. Parented
        // to `this` purely for lifetime cleanup when MainWindow closes.
        m_multiplierWindow->setWindowFlag(Qt::Window, true);
    }
    const ContestSettings settings = m_appController.settings();
    m_multiplierWindow->setContest(settings.activeContestId, findContestDefinition(settings.activeContestId));
    m_multiplierWindow->show();
    m_multiplierWindow->raise();
    m_multiplierWindow->activateWindow();
}

void MainWindow::openStatisticsWindow()
{
    if (!m_statisticsWindow) {
        m_statisticsWindow = new StatisticsWindow(m_appController.database(), this);
        m_statisticsWindow->setWindowFlag(Qt::Window, true);
    }
    const ContestSettings settings = m_appController.settings();
    const ContestDefinition* def = findContestDefinition(settings.activeContestId);
    m_statisticsWindow->setContest(settings.activeContestId, settings.ownGrid, def ? def->bands() : QStringList(),
                                   def ? def->scoring() : QStringLiteral("distance_km"));
    m_statisticsWindow->show();
    m_statisticsWindow->raise();
    m_statisticsWindow->activateWindow();
}

void MainWindow::openLogCheckWindow()
{
    if (!m_logCheckWindow) {
        m_logCheckWindow = new LogCheckWindow(this);
        m_logCheckWindow->setWindowFlag(Qt::Window, true);
        connect(m_logCheckWindow, &LogCheckWindow::refreshRequested, this, &MainWindow::refreshLogCheck);
        connect(m_logCheckWindow, &LogCheckWindow::qsoActivated, this, &MainWindow::jumpToQso);
    }
    refreshLogCheck();
    m_logCheckWindow->show();
    m_logCheckWindow->raise();
    m_logCheckWindow->activateWindow();
}

void MainWindow::refreshLogCheck()
{
    if (!m_logCheckWindow) {
        return;
    }
    const ContestSettings settings = m_appController.settings();
    const ContestDefinition* def = findContestDefinition(settings.activeContestId);
    if (!def) {
        m_logCheckWindow->setResult(LogCheckResult(), QString());
        return;
    }
    const QVector<QsoRecord> records = m_appController.database().qsosForContest(settings.activeContestId);
    const LogCheckContext context = logCheckContextFor(*def, settings, records, QDateTime::currentDateTimeUtc());
    m_logCheckWindow->setResult(checkLog(records, context), def->name());
}

void MainWindow::openReadinessWindow()
{
    if (!m_readinessWindow) {
        m_readinessWindow = new ReadinessWindow(this);
        m_readinessWindow->setWindowFlag(Qt::Window, true);
        connect(m_readinessWindow, &ReadinessWindow::refreshRequested, this, &MainWindow::refreshReadiness);
        connect(m_readinessWindow, &ReadinessWindow::settingsRequested, this, &MainWindow::openSettingsDialog);
        m_clockCheck = new ClockCheck(this);
        connect(m_clockCheck, &ClockCheck::finished, this, &MainWindow::refreshReadiness);
    }
    refreshReadiness();
    m_readinessWindow->show();
    m_readinessWindow->raise();
    m_readinessWindow->activateWindow();
}

void MainWindow::openShortcutsWindow()
{
    if (!m_shortcutsWindow) {
        m_shortcutsWindow = new ShortcutsWindow(this);
        m_shortcutsWindow->setWindowFlag(Qt::Window, true);
    }
    m_shortcutsWindow->show();
    m_shortcutsWindow->raise();
    m_shortcutsWindow->activateWindow();
}

qint64 MainWindow::currentRfFrequencyHz() const
{
    const RigctldClient& rig = m_appController.rigctldClient();
    if (!rig.isConnected() || rig.frequencyHz() <= 0) {
        return 0;
    }
    return m_transverter.rfFrequencyHz(rig.frequencyHz());
}

void MainWindow::applyRigFrequency(qint64 rigHz)
{
    // Band has no visible UI control (see the class comment in
    // MainWindow.h) -- this is the one and only place m_currentBand is
    // set from a live radio: the transverter's offset applied first, so
    // a rig showing 144.300 with the 23 cm transverter switched on names
    // 1296; and a band the contest does not have (the rig parked on
    // HF, a spot tuned in on 2 m during the UHF contest) leaves the
    // band as it was rather than dragging the log there.
    const QString bandLabel = bandLabelForFrequencyHz(m_transverter.rfFrequencyHz(rigHz));
    if (bandLabel.isEmpty()) {
        return;
    }
    const ContestDefinition* def = findContestDefinition(m_appController.settings().activeContestId);
    if (def && !def->bands().isEmpty() && !def->bands().contains(bandLabel)) {
        return;
    }
    const bool bandChanged = bandLabel != m_currentBand;
    m_currentBand = bandLabel;
    syncOn4kstRoomForCurrentBand();
    updateStatusBar();
    // The next serial is per band -- a band change shows the other
    // band's next number at once.
    if (bandChanged) {
        refreshSentExchangePreview();
        recheckDupeIndicator();
        refreshBandmap();
    }
}

void MainWindow::syncTransverterCheck()
{
    if (!m_transverterCheck) {
        return;
    }
    const QSignalBlocker blocker(m_transverterCheck);
    m_transverterCheck->setVisible(m_transverter.configured());
    m_transverterCheck->setText(QStringLiteral("Transverter %1").arg(m_transverter.describe()));
    m_transverterCheck->setChecked(m_transverter.active());
    m_transverterCheck->setToolTip(QStringLiteral("An: die Zwischenfrequenz des Funkgeräts ist das Band auf der Antenne "
                                                  "(Datei › Transverter…)"));
}

void MainWindow::openTransverterDialog()
{
    TransverterDialog dialog(m_transverter, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_transverter = dialog.setup();
    m_transverter.save(m_appController.database());
    m_appController.setTransverter(m_transverter);
    syncTransverterCheck();
    if (m_appController.rigctldClient().isConnected()) {
        applyRigFrequency(m_appController.rigctldClient().frequencyHz());
    }
    refreshBandmap();
    updateStatusBar();
    refreshReadiness();
}

void MainWindow::openAboutDialog()
{
    AboutDialog::Facts facts;
    facts.databasePath = m_appController.database().filePath();
    if (const LogBackup* backup = m_appController.logBackup()) {
        facts.backupDirectory = backup->directory();
        facts.mirrorDirectory = backup->mirrorDirectory();
    }
    AboutDialog dialog(facts, this);
    dialog.exec();
}

void MainWindow::refreshReadiness()
{
    if (!m_readinessWindow) {
        return;
    }
    const ContestSettings settings = m_appController.settings();
    ReadinessContext ctx;
    ctx.nowUtc = QDateTime::currentDateTimeUtc();
    ctx.ownCallsign = settings.ownCallsign;
    ctx.ownGrid = settings.ownGrid;
    // The exact position the settings dialog can store -- read by its
    // raw keys, like horizonProfileForOwnStation() does.
    {
        const ContestDatabase& db = m_appController.database();
        ctx.useExactOwnLocation = db.settingValue(QStringLiteral("use_exact_own_location")) == QStringLiteral("1");
        bool okLat = false;
        bool okLon = false;
        const double exactLat = db.settingValue(QStringLiteral("own_exact_latitude")).toDouble(&okLat);
        const double exactLon = db.settingValue(QStringLiteral("own_exact_longitude")).toDouble(&okLon);
        if (okLat && okLon && std::fabs(exactLat) <= 90.0 && std::fabs(exactLon) <= 180.0) {
            ctx.ownExactLatitude = exactLat;
            ctx.ownExactLongitude = exactLon;
        }
    }
    ctx.ownElevationM = settings.ownElevationM;
    ctx.antennaHeightM = settings.antennaHeightM;

    // The clock, asked anew once a minute while the window is open; the
    // answer arrives later and refreshes this snapshot again.
    if (m_clockCheck) {
        if (!m_clockCheck->isRunning()
            && (!m_clockCheck->hasResult() || m_clockCheck->checkedAtUtc().secsTo(ctx.nowUtc) >= 60)) {
            m_clockCheck->start();
        }
        ctx.clockChecked = m_clockCheck->hasResult();
        ctx.clockReachable = m_clockCheck->reachable();
        ctx.clockOffsetSecs = m_clockCheck->offsetSecs();
        ctx.clockSource = m_clockCheck->source();
    }

    if (const ContestDefinition* def = findContestDefinition(settings.activeContestId)) {
        ctx.contestFound = true;
        ctx.contestName = def->name();
        ctx.contestBands = def->bands();
        ctx.window = effectiveContestWindow(def->schedule(), settings.contestEndUtc, ctx.nowUtc);
        ctx.qsoCount = m_appController.database().qsoCountForContest(settings.activeContestId);
        // The next serial the way the definition numbers -- per band
        // for IARU Region 1 (ContestDefinition::serialScope()).
        if (def->serialScope() == QStringLiteral("band")) {
            for (const QString& band : def->bands()) {
                ctx.nextSerials.append({band, m_appController.database().nextSerialForContest(settings.activeContestId, band)});
            }
        } else {
            ctx.nextSerials.append({QString(), m_appController.database().nextSerialForContest(settings.activeContestId)});
        }
    }
    ctx.esmEnabled = settings.esmEnabled;
    ctx.transverterConfigured = m_transverter.configured();
    ctx.transverterActive = m_transverter.active();
    ctx.transverterText = m_transverter.describe();

    const auto rigLink = [](RigctldClient::State state) {
        switch (state) {
        case RigctldClient::State::Connected: return LinkState::Connected;
        case RigctldClient::State::Connecting: return LinkState::Connecting;
        case RigctldClient::State::Disconnected:
        case RigctldClient::State::Error: return LinkState::Disconnected;
        }
        return LinkState::Disconnected;
    };
    const auto rotorLink = [](RotctldClient::State state) {
        switch (state) {
        case RotctldClient::State::Connected: return LinkState::Connected;
        case RotctldClient::State::Connecting: return LinkState::Connecting;
        case RotctldClient::State::Disconnected:
        case RotctldClient::State::Error: return LinkState::Disconnected;
        }
        return LinkState::Disconnected;
    };
    if (!settings.rigctldHost.trimmed().isEmpty()) {
        ctx.catTarget = QStringLiteral("%1:%2").arg(settings.rigctldHost.trimmed()).arg(settings.rigctldPort);
        ctx.cat = rigLink(m_appController.rigctldClient().state());
    }
    const auto rotor = [&](bool enabled, const QString& label, const QString& host, int port, RotctldClient& client,
                           QString& lastError) {
        ReadinessContext::Rotor r;
        r.enabled = enabled;
        r.label = label;
        r.target = QStringLiteral("%1:%2").arg(host.trimmed()).arg(port);
        r.link = enabled ? rotorLink(client.state()) : LinkState::NotConfigured;
        if (r.link == LinkState::Connected) {
            lastError.clear();
        }
        r.rotctldError = lastError;
        return r;
    };
    ctx.rotor1 = rotor(settings.rotor1Enabled, settings.rotor1Label, settings.rotor1Host, settings.rotor1Port,
                       m_appController.rotor1Client(), m_rotctldError1);
    ctx.rotor2 = rotor(settings.rotor2Enabled, settings.rotor2Label, settings.rotor2Host, settings.rotor2Port,
                       m_appController.rotor2Client(), m_rotctldError2);
    ctx.on4kstConfigured = !settings.on4kstUsername.trimmed().isEmpty();
    ctx.on4kst = !ctx.on4kstConfigured ? LinkState::NotConfigured
                 : m_appController.on4kstClient().isConnected() ? LinkState::Connected
                                                                 : LinkState::Disconnected;
    if (!settings.clusterHost.trimmed().isEmpty()) {
        ctx.clusterTarget = QStringLiteral("%1:%2").arg(settings.clusterHost.trimmed()).arg(settings.clusterPort);
        ctx.cluster = m_appController.dxClusterClient().isConnected() ? LinkState::Connected : LinkState::Disconnected;
    }

    if (LogBackup* backup = m_appController.logBackup()) {
        ctx.backupDirectory = backup->directory();
        // The folder appears with the first copy; before that, its
        // parent (the data folder) has to take it.
        ctx.backupDirectoryWritable = QFileInfo::exists(ctx.backupDirectory)
            ? QFileInfo(ctx.backupDirectory).isWritable()
            : QFileInfo(QFileInfo(ctx.backupDirectory).path()).isWritable();
        const QVector<LogBackup::Entry> backups = LogBackup::listBackups(ctx.backupDirectory);
        if (!backups.isEmpty()) {
            ctx.lastBackupUtc = backups.first().utc;
        }
        ctx.mirrorDirectory = backup->mirrorDirectory();
        // Reachable = its folder exists (a stick) or can be made
        // (a cloud folder's subfolder); an unplugged stick has neither.
        ctx.mirrorWritable = !ctx.mirrorDirectory.isEmpty()
            && (QFileInfo(ctx.mirrorDirectory).isWritable()
                || (!QFileInfo::exists(ctx.mirrorDirectory)
                    && QFileInfo(QFileInfo(ctx.mirrorDirectory).path()).isWritable()));
    }
    {
        double lat = 0.0;
        double lon = 0.0;
        bool known = false;
        if (ctx.useExactOwnLocation && (ctx.ownExactLatitude != 0.0 || ctx.ownExactLongitude != 0.0)) {
            lat = ctx.ownExactLatitude;
            lon = ctx.ownExactLongitude;
            known = true;
        } else if (isValidGridSquare(settings.ownGrid)) {
            calculateLatLonFromGridSquare(settings.ownGrid, lat, lon);
            known = true;
        }
        ctx.terrainLoadedForOwnLocation = known
            && m_appController.terrainDataManager().tileLoader().isTileLoaded(SrtmTileLoader::tileNameForLatLon(lat, lon));
    }
    ctx.importedLocators = m_appController.database().importedLocatorCount();

    m_readinessWindow->setResult(checkReadiness(ctx));
}

void MainWindow::jumpToQso(int qsoId)
{
    if (!m_unifiedLog) {
        return;
    }
    m_panelLayoutManager->raisePanel(QStringLiteral("unifiedlog"));
    m_unifiedLog->selectHistoryQso(qsoId);
}

} // namespace Contestprogramm
