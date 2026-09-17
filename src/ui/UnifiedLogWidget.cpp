#include "ui/UnifiedLogWidget.h"

#include "core/SpotCandidate.h"
#include "data/QsoRecord.h"
#include "models/ChatFeedModel.h"
#include "models/LogTableModel.h"
#include "ui/StyleKit.h"

#include <QAbstractTableModel>
#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {

// 200ms per EntryBarWidget's own debounce -- long enough that a fast
// typist does not fire a lookup per keystroke, short enough the autofill
// still feels immediate once they stop.
constexpr int kCallsignLookupDebounceMs = 200;

// Local short aliases for UnifiedLogWidget's public Column/Role enums
// (see UnifiedLogWidget.h) -- those are the single authoritative
// definition (public so tests can query a cell's role data directly);
// these aliases just keep the rest of this file's internal code
// readable without an "UnifiedLogWidget::" prefix on every column
// reference.
constexpr int ColSerial = UnifiedLogWidget::ColumnSerial;
constexpr int ColTime = UnifiedLogWidget::ColumnTime;
constexpr int ColCall = UnifiedLogWidget::ColumnCall;
constexpr int ColExchSent = UnifiedLogWidget::ColumnExchangeSent;
constexpr int ColExchRcvd = UnifiedLogWidget::ColumnExchangeRcvd;
constexpr int ColKm = UnifiedLogWidget::ColumnDistanceKm;
constexpr int ColDeg = UnifiedLogWidget::ColumnBearingDeg;
constexpr int ColStatus = UnifiedLogWidget::ColumnStatus;
constexpr int ColBand = UnifiedLogWidget::ColumnBand;
constexpr int ColRstSent = UnifiedLogWidget::ColumnRstSent;
constexpr int ColSerialSent = UnifiedLogWidget::ColumnSerialSent;
constexpr int ColRstRcvd = UnifiedLogWidget::ColumnRstRcvd;
constexpr int ColSerialGridRcvd = UnifiedLogWidget::ColumnSerialGridRcvd;
constexpr int ColCount = UnifiedLogWidget::ColumnCount;

// Extra data() roles the shared PillDelegate below reads -- a small
// rounded status/source capsule (KST/CLU on a candidate row, UNGÜLTIG on
// an invalid history row), painted on top of whatever the normal cell
// background already is rather than a separate widget, so it survives
// UnifiedFeedModel's frequent resets with no widget-lifecycle cost at
// all. The entry row's own DUPE/NEU pill is a real QLabel now (see
// UnifiedLogWidget::updateStatusPill()) -- it is not part of any
// QAbstractItemModel any more, so it has no need of these roles, but
// they stay the shared vocabulary the feed table's pills use.
constexpr int kPillTextRole = UnifiedLogWidget::PillTextRole;
constexpr int kPillBgRole = UnifiedLogWidget::PillBgRole;
constexpr int kPillFgRole = UnifiedLogWidget::PillFgRole;
constexpr int kPillBorderRole = UnifiedLogWidget::PillBorderRole;

QVariant columnHeaderText(int section)
{
    switch (section) {
    // DXLog.net's own literal header for this column -- see the
    // Column enum's doc comment in UnifiedLogWidget.h.
    case ColSerial: return QStringLiteral("QSO#");
    case ColTime: return QStringLiteral("Zeit");
    case ColCall: return QStringLiteral("Call");
    case ColExchSent: return QStringLiteral("Exch Ges.");
    case ColExchRcvd: return QStringLiteral("Exch Emp.");
    case ColKm: return QStringLiteral("km");
    case ColDeg: return QString::fromUtf8("°");
    case ColStatus: return QStringLiteral("Status");
    // DXLog-Vollspalten-only -- DXLog.net's own literal header text for
    // each (see the Column enum's doc comment in the header).
    case ColBand: return QStringLiteral("Band");
    case ColRstSent: return QStringLiteral("Sent");
    case ColSerialSent: return QStringLiteral("Nr.");
    case ColRstRcvd: return QStringLiteral("Rcvd");
    case ColSerialGridRcvd: return QStringLiteral("Nr./Grid");
    default: return QVariant();
    }
}

// Fixed pixel widths for the feed table only (log history + spot/chat
// candidates) -- the entry row above it no longer shares this column
// grid at all (see UnifiedLogWidget.h's class comment for the mockup-E
// restructuring), so these no longer need to accommodate the old
// cramped 3-sub-field-in-one-cell entry row; re-tuned down from the
// original {70,150,120,230,70,60,160} now that ColExchRcvd only ever
// has to fit a plain display string (a logged QSO's composed received
// exchange, or a candidate's bare grid), never a live multi-field
// editor host. Every column is QHeaderView::Fixed (the header is hidden
// entirely -- see the constructor -- so there is no visible per-column
// resize affordance regardless) -- but the LAST visible column (Status,
// in both view modes -- see setViewMode()'s own applyColumnOrder()
// calls) grows to soak up a panel dragged wider than this array's own
// total, so it no longer leaves dead, grid-less space on the right
// (operator, 2026-09-11: "rechts sind frei felder die nicht gebraucht
// werden") -- capped at kStatusColumnMaxWidth (same-day follow-up,
// after the fill made the column itself look like the same dead space
// it was meant to fix: "benötigen wir die letzte spalte im log?"), and
// matched by the entry row's own Status cell (see syncEntryRowWidth()).
// See syncStatusColumnWidth() for the actual sizing (not Qt's own
// setStretchLastSection(), which has no notion of a maximum). ColSerial
// (leads the array to match its position in the Column enum) is hidden
// by default (see setViewMode()) so its width only matters once the
// operator switches to "DXLog-Vollspalten".
// DXLog-Vollspalten-only widths (Band/Sent/Nr./Rcvd/Nr.-Grid) appended
// after the original Compact set, matching the Column enum's own
// append-only ordering -- see its doc comment. "Nr./Grid" is the widest
// of the five: it has to fit e.g. "002 JN59FF" in the same mono font as
// every other cell.
// Call narrowed 150->100px, then back up to 115px (operator, 2026-09-11:
// first "abstand zwischen rufzeichen und 59 kann kleiner werden", then
// the whole ladder moved up a step -- see kFontBody below -- and 100px
// no longer had the same margin to spare it did at the smaller size).
// Every width other than Status/RST/Serial (short, fixed-format values
// that don't grow with the font) bumped by roughly the same ~15-20%
// this same font-size step needs, so nothing that used to just fit at
// kFontSmall starts clipping at kFontBody.
// Status base width cut 160->90 (operator, 2026-09-14, pointing at the
// Log panel: "können wir beim log die letzte spalte weg lassen. dann
// ist das fenster auch kleiner ohne balken" -- the column itself stays,
// since it is the only place "UNGÜLTIG" and the KST/CLU candidate-source
// tag are shown (see this task's own clarifying question/answer), but
// 90px is close to what the pill text actually needs instead of the old
// 160px minimum that read as a big blank trailing block on most rows.
constexpr int kColumnWidths[ColCount] = {55, 70, 115, 130, 175, 75, 60, 90, 60, 55, 55, 55, 130};

// How far the trailing Status column (and the entry row's own matching
// cell) may grow to fill a wider panel -- see kColumnWidths' own comment
// and syncStatusColumnWidth(). Its content is always short (a blank
// valid row, or "DUPE"/"UNGÜLTIG"/"KST"/"CLU"), so this is headroom, not
// a tight fit -- just a ceiling so the column stops reading as
// unlabelled dead space on a panel dragged wider than the rest of the
// grid needs. Cut 220->110 alongside kColumnWidths' own base-width cut
// above, same 2026-09-14 request -- the old 220px ceiling was itself
// most of the "too-wide last column" complaint, not just the 160px
// floor.
constexpr int kStatusColumnMaxWidth = 110;

// DXLog.net's own rule (dxlog.net/docs, verified for this task): "RST
// sent and RST rcvd items are set to 59 (if the mode is SSB or FM) or
// 599 (if the mode is CW, RTTY, or PSK)". Unknown/empty mode falls back
// to 59 -- VHF/UHF contest operators run predominantly phone, so
// "assume SSB" is the more useful default than leaving it blank.
// MainWindow.cpp carries its own copy of this same small mapping for
// composing the SENT exchange preview/record (see this task's report:
// two small self-contained anonymous-namespace helpers, matching this
// codebase's existing per-file-helper style, e.g. CabrilloExporter.cpp's
// own cabrilloModeCode(), rather than a new shared utility header for
// four lines of logic).
QString defaultRstForMode(const QString& mode)
{
    const QString m = mode.trimmed().toUpper();
    if (m == QStringLiteral("CW") || m == QStringLiteral("RTTY") || m == QStringLiteral("PSK")) {
        return QStringLiteral("599");
    }
    return QStringLiteral("59");
}

// The "Nr./Grid" column's own combined text (Serial-received + Grid-
// received, space-joined, RST deliberately excluded -- see
// UnifiedFeedModel::flags()'s own comment on why RST never belongs in
// this column at all) -- shared by historyData()'s EditRole (what a
// double-click editor pre-fills) and its DisplayRole (what the static
// cell shows), so both stay byte-for-byte the same composition instead
// of two copies of this drifting apart.
QString serialGridRcvdText(const QsoRecord& record)
{
    QStringList parts;
    if (record.serialRcvd) {
        parts << QString::number(*record.serialRcvd);
    }
    if (!record.gridSquare.isEmpty()) {
        parts << record.gridSquare;
    }
    return parts.join(QLatin1Char(' '));
}

// Matches MainWindow.cpp's own operatingModeButtonText() wording
// exactly (its m_modeToggleButton text) -- two small self-contained
// anonymous-namespace helpers doing the same one-line mapping in two
// translation units, the same "no new shared utility header for four
// lines of logic" pattern defaultRstForMode() above already follows
// (see its own comment), rather than exporting either as shared API for
// this one string.
QString operatingModeStatusText(ContestSettings::OperatingMode mode)
{
    return mode == ContestSettings::OperatingMode::Run
        ? QStringLiteral("Modus: Run")
        : QStringLiteral("Modus: S&P");
}

// Flat field style matching m_feedTable's own plain-text cells --
// operator, 2026-09-11, pointing at DXLog.net's real "Contest recorder"
// screenshot, row 10 (the in-progress entry, literally the next row of
// the same grid, no boxed/highlighted fields of any kind): "die
// eingabezeile sollte keine seperate zeile sein ... gleiche grafik uns
// stil wie die fertigen qso." Replaces the old boxed/amber-tinted
// EntryBarWidget-era look entirely -- no background, same mono/
// kFontBody/kTextPrimary the table itself uses. `autoFilled` only
// nudges the text to kTextSecondary (the same dimming family
// m_feedTable already uses for a worked/inactive row -- see
// candidateData()'s own kTextInactive treatment) rather than a coloured
// box, so "the program filled this in, still overridable" survives as a
// subtle cue without reintroducing a separate visual language.
//
// A single right-hand gridline WAS deliberately left off here at first
// -- a follow-up correction (operator, same day: "mache auch einen
// raster über das callsign, was ich eingebe usw.! wie bei den bereits
// abgeschlossenen qso") pointed out m_feedTable itself is NOT borderless
// either: Style::appStyleSheet() gives every QTableView a real
// `gridline-color` between cells (StyleKit.cpp: "QTableView ...
// gridline-color: %3" -- Style::kBorder()). Matching the table's own look
// therefore means matching ITS gridlines too, not omitting them --
// this is that same kBorder colour, one thin line on the trailing edge
// of each field, everywhere the table itself would have a column
// boundary.
QString flatFieldStyle(bool autoFilled)
{
    return QStringLiteral("QLineEdit { background: transparent; border: none; border-right: 1px solid %1;"
                           " padding: 0; color: %2; }")
        .arg(Style::kBorder(), autoFilled ? Style::kTextSecondary() : Style::kTextPrimary());
}

// The entry row's dupe indicator -- now a plain QLabel styled exactly
// like m_feedTable's Status column shows a real logged row (see
// UnifiedFeedModel::historyData()'s own comment: "Status: blank for a
// valid, already-logged row"): blank text, no pill/box, while the
// operator is still typing a new (non-dupe) callsign, matching DXLog's
// own row 10 (blank "Stn" cell). Only a real dupe gets a small red
// badge -- the same PillDelegate visual family m_feedTable's own
// UNGÜLTIG/DUPE cells already use one section down, just painted via a
// QLabel stylesheet here instead of a delegate, since this one row is
// never part of that QAbstractItemModel.
QString dupePillStyle()
{
    return QStringLiteral(
        "QLabel { background: %1; border: 1px solid %2; color: %3;"
        " border-radius: %4px; padding: 0 8px; }")
        .arg(Style::kRedBg(), Style::kRedBorder(), Style::kRedText())
        .arg(Style::kRadius - 1);
}

// --- Entry row layout constants -----------------------------------
//
// The entry row is now sized off the SAME kColumnWidths array
// m_feedTable itself uses (see the file-scope kColumnWidths definition
// below) -- see rebuildEntryRowLayout() -- rather than its own
// independent field-width constants: the whole point of the 2026-09-11
// restyle (see flatFieldStyle()'s own comment) is that the entry row
// IS the table's own column grid, one more row, not a differently-
// proportioned panel next to it. What remains here is genuinely
// independent of the table: horizontal cell padding and the DXLog-style
// status line's own height.
constexpr int kEntryRowHPadding = 10; // matches m_feedTable's own default QTableView cell padding
// Not related to the entry row's own layout (added for the DXLog-style
// status line, see setOperatingMode()'s doc comment) -- a slim single-
// text-line strip, narrower than PanelHeaderBar's own 30px
// kHeaderHeight since it is read-only chrome with no button row to fit.
constexpr int kStatusLineHeight = 24;

// Paints a small rounded status/source capsule for any cell that
// carries kPillTextRole, on top of the cell's normal background/text
// (painted first via the base QStyledItemDelegate::paint(), so a
// dimmed already-worked candidate row's colour still shows through
// underneath). Falls back to plain QStyledItemDelegate behavior for
// every other cell. Used only by m_feedTable now -- the entry row's own
// status pill is a real QLabel (see statusPillStyle() above).
class PillDelegate : public QStyledItemDelegate {
public:
    explicit PillDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);

        const QVariant textVar = index.data(kPillTextRole);
        if (!textVar.isValid()) {
            return;
        }
        const QString text = textVar.toString();
        if (text.isEmpty()) {
            return;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        QFont font = option.font;
        font.setPixelSize(Style::kFontCaption);
        font.setWeight(QFont::DemiBold);
        painter->setFont(font);
        const QFontMetrics metrics(font);

        const int pillH = 18;
        const int pillW = metrics.horizontalAdvance(text) + 16;
        const QRect pillRect(option.rect.left() + 6, option.rect.center().y() - pillH / 2, pillW, pillH);

        const QColor bg(index.data(kPillBgRole).toString());
        const QColor fg(index.data(kPillFgRole).toString());
        const QColor border(index.data(kPillBorderRole).toString());

        painter->setPen(QPen(border, 1));
        painter->setBrush(bg);
        painter->drawRoundedRect(pillRect, 4, 4);
        painter->setPen(fg);
        painter->drawText(pillRect, Qt::AlignCenter, text);

        painter->restore();
    }
};

// Splits ColSerialGridRcvd's own composed "Serial Grid" text into two
// independently drawn halves with a real divider line between them --
// operator, 2026-09-11, after finding the old combined "Exch Emp." cell
// let an edit reach RST too (see UnifiedFeedModel::flags()'s own
// comment): "das gehört auch optisch geteilt mit einem raster", the
// same per-field grid-line language the entry row's own flatFieldStyle()
// already uses (border-right, Style::kBorder()). Uses the style's own
// SE_ItemViewItemText rect so the LEFT half starts at exactly the x
// every other plain-text column's default-rendered text would -- no
// visual seam versus its neighbours, just one added internal divider.
// The split point is a fixed pixel offset, not content-width-dependent
// -- matching this codebase's general "fixed grid, not organic" column
// philosophy (kColumnWidths) -- so the divider doesn't jump around as
// the serial number grows from "1" to "999".
class SerialGridDelegate : public QStyledItemDelegate {
public:
    explicit SerialGridDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QString fullText = index.data(Qt::DisplayRole).toString();
        const int splitPos = fullText.indexOf(QLatin1Char(' '));
        if (splitPos <= 0) {
            // Nothing to split (empty, the unknown-dash placeholder, or
            // a serial-only/grid-only value) -- plain default paint.
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        // QStyledItemDelegate::paint() re-derives its OWN working copy's
        // text via initStyleOption(index) internally, regardless of what
        // is passed in as `option` -- clearing a local copy's `text`
        // before calling it does nothing (Qt overwrites it right back
        // from the model). The override below is the only way to
        // actually suppress the base class's single-string text draw:
        // it runs via virtual dispatch from INSIDE this same paint()
        // call, blanking `text` for exactly this splittable case, so
        // this draws background/selection/focus only -- our own two-part
        // text goes on top of that, not underneath a duplicate.
        QStyledItemDelegate::paint(painter, option, index);

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

        QColor textColor = index.data(Qt::ForegroundRole).value<QColor>();
        if (!textColor.isValid()) {
            textColor = opt.palette.color(QPalette::Text);
        }
        if (opt.state & QStyle::State_Selected) {
            textColor = opt.palette.color(QPalette::HighlightedText);
        }

        const QString serialPart = fullText.left(splitPos);
        const QString gridPart = fullText.mid(splitPos + 1);
        const int leftWidth = qMax(18, qMin(46, textRect.width() - 20));
        const int dividerX = textRect.left() + leftWidth + kEntryRowHPadding / 2;

        painter->save();
        painter->setFont(opt.font);
        painter->setPen(textColor);
        painter->drawText(QRect(textRect.left(), textRect.top(), leftWidth, textRect.height()),
                           Qt::AlignVCenter | Qt::AlignLeft, serialPart);

        painter->setPen(QColor(Style::kBorder()));
        painter->drawLine(dividerX, textRect.top(), dividerX, textRect.bottom());

        painter->setPen(textColor);
        painter->drawText(QRect(dividerX + kEntryRowHPadding / 2, textRect.top(),
                                 textRect.right() - (dividerX + kEntryRowHPadding / 2), textRect.height()),
                           Qt::AlignVCenter | Qt::AlignLeft, gridPart);
        painter->restore();
    }

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        // Only for the splittable case (see paint() above) -- a plain
        // dash/serial-only/grid-only value goes through the ordinary,
        // unmodified base-class draw untouched.
        if (option->text.indexOf(QLatin1Char(' ')) > 0) {
            option->text.clear();
        }
    }
};

// The entry row's own chrome: a flat panel-background fill, the single
// 2px amber left accent bar that replaces the old full-row amber tint
// (see UnifiedLogWidget.h's class comment -- this is the concrete fix
// for Martin's "too much colour" complaint), and a 1px bottom border
// shared with m_feedTable directly below it, with zero layout gap, so
// the two read as one continuous panel surface with one header (the
// "Log" PanelHeaderBar MainWindow already registers this whole widget
// under -- see MainWindow.cpp's registerPanel(QStringLiteral("unifiedlog"), ...)).
// Painting its own background explicitly (rather than relying on
// Style::appStyleSheet()'s generic "QWidget { background }" QSS
// cascade) keeps this row visually identical to its parent panel
// (kPanelBg) regardless of stylesheet inheritance quirks, the same
// reasoning PanelHeaderBar::paintEvent() already applies to its own
// header strip.
class EntryRowFrame : public QWidget {
public:
    explicit EntryRowFrame(QWidget* parent = nullptr) : QWidget(parent) {}

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        // No amber accent bar any more -- this row IS a table row now,
        // not a distinct panel beside one (see flatFieldStyle()'s own
        // comment on the 2026-09-11 DXLog.net "row 10" reference); a
        // left accent bar was exactly the kind of "separate design
        // language" cue the operator pointed at.
        //
        // The bottom line is now always drawn, regardless of
        // setEntryRowPosition() -- a follow-up correction the same day
        // ("mache auch einen raster über das callsign, was ich eingebe
        // usw.! wie bei den bereits abgeschlossenen qso") pointed out
        // every one of m_feedTable's OWN rows has a bottom gridline
        // (Style::kBorder(), via appStyleSheet()'s QTableView rule) --
        // this row is one more row of that same grid regardless of
        // which end of the table it sits at, so it keeps the same
        // gridline unconditionally. Same colour as the table's own
        // gridline-color (kBorder), not the dimmer kBorderSubtle this
        // used before that correction.
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.fillRect(rect(), QColor(Style::kPanelBg()));
        painter.setPen(QColor(Style::kBorder()));
        painter.drawLine(0, height() - 1, width() - 1, height() - 1);
    }
};

} // namespace

// The freely-resettable "feed" model: logged history (proxying
// LogTableModel, chronologically ascending/oldest-first -- matching
// DXLog.net's own real order, see rebuild()'s own comment), a quiet
// divider row, then not-yet-worked spot/chat
// candidates from both ChatFeedModel instances. Rebuilt in full
// (beginResetModel/endResetModel) on every underlying modelReset --
// safe here specifically because this model holds no live editor
// widgets, only display cells and PillDelegate-painted pills. The entry
// row (m_entryRow, a plain QWidget -- see UnifiedLogWidget.h's class
// comment) is not part of this model, or any model, at all any more, so
// this model's frequent resets have nothing left to threaten.
class UnifiedFeedModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit UnifiedFeedModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void setLogModel(LogTableModel* model)
    {
        if (m_logModel) {
            disconnect(m_logModel, nullptr, this, nullptr);
        }
        m_logModel = model;
        if (m_logModel) {
            connect(m_logModel, &QAbstractItemModel::modelReset, this, &UnifiedFeedModel::rebuild);
            // A history-row hand-correction/invalid-toggle patches
            // LogTableModel in place (LogTableModel::updateRecord)
            // rather than resetting it -- row count/order is unchanged,
            // only cell values are, so this just re-paints the affected
            // rows rather than rebuilding m_rows (a full rebuild would
            // also risk resetting this model synchronously out from
            // under the QTableView cell editor that is still mid-commit
            // on the very edit that triggered it).
            connect(m_logModel, &QAbstractItemModel::dataChanged, this, [this] {
                if (!m_rows.isEmpty()) {
                    emit dataChanged(index(0, 0), index(m_rows.size() - 1, ColCount - 1));
                }
            });
        }
        rebuild();
    }

    void setChatModels(ChatFeedModel* onKst, ChatFeedModel* cluster)
    {
        if (m_onKst) {
            disconnect(m_onKst, nullptr, this, nullptr);
        }
        if (m_cluster) {
            disconnect(m_cluster, nullptr, this, nullptr);
        }
        m_onKst = onKst;
        m_cluster = cluster;
        if (m_onKst) {
            connect(m_onKst, &QAbstractItemModel::modelReset, this, &UnifiedFeedModel::rebuild);
        }
        if (m_cluster) {
            connect(m_cluster, &QAbstractItemModel::modelReset, this, &UnifiedFeedModel::rebuild);
        }
        rebuild();
    }

    void setGridFilter(const QString& text)
    {
        if (m_gridFilter == text) {
            return;
        }
        m_gridFilter = text;
        rebuild();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override { return parent.isValid() ? 0 : m_rows.size(); }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override { return parent.isValid() ? 0 : ColCount; }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
            return QVariant();
        }
        const RowRef& ref = m_rows.at(index.row());
        switch (ref.kind) {
        case RowKind::History: return historyData(ref.sourceRow, index.column(), role);
        case RowKind::Divider: return dividerData(index.column(), role);
        case RowKind::Candidate: return candidateData(ref, index.column(), role);
        }
        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
            return QAbstractTableModel::headerData(section, orientation, role);
        }
        return columnHeaderText(section);
    }

    // -1 when no chat models are set yet (no divider row at all).
    int dividerRow() const { return m_dividerRow; }

    bool candidateInfoForRow(int row, QString* callsign, QString* grid, qint64* freqHz) const
    {
        if (row < 0 || row >= m_rows.size()) {
            return false;
        }
        const RowRef& ref = m_rows.at(row);
        if (ref.kind != RowKind::Candidate || !ref.chatModel) {
            return false;
        }
        const SpotCandidate& candidate = ref.chatModel->candidateAt(ref.sourceRow);
        if (callsign) {
            *callsign = candidate.callsign;
        }
        if (grid) {
            *grid = candidate.grid;
        }
        if (freqHz) {
            *freqHz = candidate.freqHz;
        }
        return true;
    }

    // The QSO's real database id for a History row at `row`, or -1 if
    // `row` is not a History row (Divider/Candidate) or out of range --
    // used by handleFeedRowClicked()'s Status-column invalid-toggle.
    int historyQsoIdForRow(int row) const
    {
        if (!m_logModel || row < 0 || row >= m_rows.size()) {
            return -1;
        }
        const RowRef& ref = m_rows.at(row);
        if (ref.kind != RowKind::History) {
            return -1;
        }
        return m_logModel->recordAt(ref.sourceRow).id;
    }

    // Only a History row's Call/Nr.-Grid cells are editable -- NOT
    // ColRstRcvd, deliberately (operator, 2026-09-11: "59 ist immer fix,
    // kann man nicht ändern, man soll auch nicht hier reinklicken
    // können" -- RST is a fixed contest-report convention, never
    // genuinely-exchanged information, so it must not even open an
    // editor). ColExchRcvd (the old single combined "59 001 JN67VV"
    // cell) is likewise excluded now -- it used to be the only editable
    // received-exchange cell in Compact view, which is exactly what let
    // an edit reach RST in the first place: double-clicking anywhere in
    // that one cell (operator, same day: "ich habe bei 59 reingeklickt
    // und der locator war in der gleichen raster") opened the WHOLE
    // string, RST included. Both view modes now always show the split
    // Rcvd columns instead (see setViewMode()), so ColSerialGridRcvd is
    // the one and only editable received-exchange cell in either mode.
    // See UnifiedLogWidget.h's historyCallsignEditRequested/
    // historyExchangeRcvdEditRequested doc comment for the DXLog.net-
    // equivalent scope this covers (date/time/frequency/operator go
    // through a separate dialog in real DXLog.net, not sub-fields of
    // this cell).
    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        const Qt::ItemFlags base = QAbstractTableModel::flags(index);
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
            return base;
        }
        const RowRef& ref = m_rows.at(index.row());
        if (ref.kind == RowKind::History
            && (index.column() == ColCall || index.column() == ColSerialGridRcvd || index.column() == ColTime)) {
            return base | Qt::ItemIsEditable;
        }
        return base;
    }

    // Commits an in-place edit of a History row's Call or Nr.-Grid
    // cell -- does not touch LogTableModel/the database directly (this
    // model has no reference to either); it just re-emits the edit as a
    // signal for UnifiedLogWidget to bubble up to MainWindow (which owns
    // AppController::database() and the active ContestDefinition needed
    // to re-derive RST/Serial/Grid from the typed text). The actual
    // on-screen update then arrives back through LogTableModel::
    // updateRecord()'s dataChanged (see setLogModel() above), not from
    // here directly.
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override
    {
        if (role != Qt::EditRole || !index.isValid() || index.row() < 0 || index.row() >= m_rows.size() || !m_logModel) {
            return false;
        }
        const RowRef& ref = m_rows.at(index.row());
        if (ref.kind != RowKind::History) {
            return false;
        }
        const QsoRecord& record = m_logModel->recordAt(ref.sourceRow);
        if (index.column() == ColCall) {
            const QString newCall = value.toString().trimmed().toUpper();
            if (newCall.isEmpty()) {
                return false;
            }
            emit historyCallsignEditRequested(record.id, newCall);
            return true;
        }
        if (index.column() == ColTime) {
            // Typed as "HH:MM" (keeps the date) or "YYYY-MM-DD HH:MM";
            // MainWindow::handleHistoryTimeEditRequested() parses and
            // rejects anything else with a message, not here.
            const QString text = value.toString().trimmed();
            if (text.isEmpty()) {
                return false;
            }
            emit historyTimeEditRequested(record.id, text);
            return true;
        }
        if (index.column() == ColSerialGridRcvd) {
            // Re-attach the record's OWN existing RST unchanged as the
            // first token -- MainWindow::handleHistoryExchangeRcvdEditRequested()
            // re-derives RST/Serial/Grid from this text POSITIONALLY, by
            // the active ContestDefinition's declared field order (RST
            // first, matching every shipped contest_definitions/*.json
            // and this row's own visual RST-then-Nr./Grid order). Without
            // this, the operator's typed Serial digits would land in the
            // RST slot instead -- exactly the "59 darf nicht editierbar
            // sein" the operator was reacting to, just moved into the
            // re-parse instead of the click.
            const QString rest = value.toString().trimmed();
            const QString composed =
                record.rstRcvd.isEmpty() ? rest : (record.rstRcvd + QLatin1Char(' ') + rest);
            emit historyExchangeRcvdEditRequested(record.id, composed);
            return true;
        }
        return false;
    }

public slots:
    void rebuild()
    {
        beginResetModel();
        m_rows.clear();
        m_dividerRow = -1;

        if (m_logModel) {
            // Chronological ascending (oldest first, growing downward) --
            // DXLog.net's OWN real "Contest recorder" behaviour (operator,
            // 2026-09-11, after a screenshot proving it directly: QSO#1
            // at the top through QSO#9, THEN the live entry row as
            // QSO#10, continuing the SAME sequence -- "fortlaufend").
            // The previous newest-first order here was wrong -- see this
            // same comment's own earlier (incorrect) claim that
            // newest-first was "the DXLog.net/N1MM+ convention"; it
            // wasn't, confirmed against the operator's real screenshot,
            // not assumed from memory. LogTableModel's own row order
            // (ORDER BY id ASC) already matches this directly, so this
            // loop is now a straight pass-through, not a reversal.
            for (int r = 0; r < m_logModel->rowCount(); ++r) {
                if (!m_gridFilter.isEmpty()) {
                    const QString grid = m_logModel->data(m_logModel->index(r, LogTableModel::ColumnGrid)).toString();
                    if (!grid.contains(m_gridFilter, Qt::CaseInsensitive)) {
                        continue;
                    }
                }
                m_rows.append({RowKind::History, r, nullptr});
            }
        }

        if (m_onKst && m_cluster) {
            m_dividerRow = m_rows.size();
            m_rows.append({RowKind::Divider, -1, nullptr});
            appendCandidates(m_onKst);
            appendCandidates(m_cluster);
        }

        endResetModel();
        emit rebuilt();
    }

signals:
    // A reset (see rebuild() above) silently drops any QTableView::
    // setSpan() previously applied to the divider row -- the view
    // reapplies it in response to this.
    void rebuilt();

    // See setData() above -- bubbled up verbatim by UnifiedLogWidget as
    // its own same-named public signals.
    void historyCallsignEditRequested(int qsoId, const QString& newCallsign);
    void historyExchangeRcvdEditRequested(int qsoId, const QString& newText);
    void historyTimeEditRequested(int qsoId, const QString& newText);

private:
    enum class RowKind { History, Divider, Candidate };
    struct RowRef {
        RowKind kind;
        int sourceRow = -1;
        ChatFeedModel* chatModel = nullptr; // Candidate rows only
    };

    void appendCandidates(ChatFeedModel* model)
    {
        for (int r = 0; r < model->rowCount(); ++r) {
            m_rows.append({RowKind::Candidate, r, model});
        }
    }

    QVariant historyData(int sourceRow, int column, int role) const
    {
        if (!m_logModel) {
            return QVariant();
        }
        const QsoRecord& record = m_logModel->recordAt(sourceRow);

        // Qt's default cell editor (QStyledItemDelegate, no custom
        // delegate needed here -- see flags()/setData() above) pre-fills
        // from EditRole, not DisplayRole -- the raw record field, not
        // the "unknown is a dash" formatted text LogTableModel's own
        // DisplayRole values give the other columns below.
        if (role == Qt::EditRole) {
            switch (column) {
            case ColCall: return record.callsign;
            // Serial+Grid only, RST excluded -- see setData()'s own
            // comment on why RST is re-attached separately rather than
            // ever entering the editable text at all.
            case ColSerialGridRcvd: return serialGridRcvdText(record);
            // The shown HH:mm; a corrected time keeps the date unless
            // one is typed along (see setData()).
            case ColTime: return m_logModel->data(m_logModel->index(sourceRow, LogTableModel::ColumnTime));
            default: return QVariant();
            }
        }

        if (record.isInvalid) {
            // A QSO marked invalid (see this task's report on
            // DXLog.net's own "mark invalid, never delete" model) reads
            // as struck-through and dimmed across the whole row -- the
            // same visual family a worked-and-dimmed candidate row
            // already uses one section down (candidateData() below).
            if (role == Qt::ForegroundRole) {
                return QColor(Style::kTextInactive());
            }
            if (role == Qt::FontRole) {
                QFont font;
                font.setStrikeOut(true);
                return font;
            }
        }

        if (role != Qt::DisplayRole) {
            if (column == ColStatus && record.isInvalid) {
                // UNGÜLTIG pill -- red, the same warning family DUPE
                // already uses on the entry row, clicking it again (see
                // handleFeedRowClicked()) toggles it back.
                if (role == kPillTextRole) { return QStringLiteral("UNGÜLTIG"); }
                if (role == kPillBgRole) { return Style::kRedBg(); }
                if (role == kPillFgRole) { return Style::kRedText(); }
                if (role == kPillBorderRole) { return Style::kRedBorder(); }
            }
            return QVariant();
        }
        switch (column) {
        // Real per-QSO data (QsoRecord::serialSent, from
        // ContestDatabase::nextSerialForContest() at log time -- see
        // MainWindow::handleLogRequested()), read straight off `record`
        // since LogTableModel itself has no matching column of its own
        // to proxy through (unlike every other case here). Unknown is a
        // dash, not "0" -- HAUSSTIL rule 7 -- for the (today
        // unreachable, since every logged QSO gets a serial) case where
        // it was never set.
        case ColSerial: return record.serialSent ? QString::number(*record.serialSent) : Style::unknownDash();
        case ColTime: return m_logModel->data(m_logModel->index(sourceRow, LogTableModel::ColumnTime));
        case ColCall: return m_logModel->data(m_logModel->index(sourceRow, LogTableModel::ColumnCallsign));
        // "Exch Ges." / "Exch Emp." == what we sent / what we received,
        // matching LogTableModel's own ColumnExchangeSent/-Rcvd exactly.
        case ColExchSent: return m_logModel->data(m_logModel->index(sourceRow, LogTableModel::ColumnExchangeSent));
        case ColExchRcvd: return m_logModel->data(m_logModel->index(sourceRow, LogTableModel::ColumnExchangeRcvd));
        case ColKm: return m_logModel->data(m_logModel->index(sourceRow, LogTableModel::ColumnDistanceKm));
        case ColDeg: return m_logModel->data(m_logModel->index(sourceRow, LogTableModel::ColumnBearingDeg));
        // DXLog-Vollspalten-only -- read straight off `record`, same
        // "no matching LogTableModel column to proxy through" reasoning
        // ColSerial above already documents. Each is real per-QSO data
        // QsoRecord already stores as its own field (see QsoRecord.h);
        // unknown is a dash, not a fabricated value -- HAUSSTIL rule 7.
        case ColBand: return record.band.isEmpty() ? Style::unknownDash() : record.band;
        case ColRstSent: return record.rstSent.isEmpty() ? Style::unknownDash() : record.rstSent;
        case ColSerialSent: return record.serialSent ? QString::number(*record.serialSent) : Style::unknownDash();
        case ColRstRcvd: return record.rstRcvd.isEmpty() ? Style::unknownDash() : record.rstRcvd;
        case ColSerialGridRcvd: {
            // DXLog.net combines Nr. and Grid into one trailing column
            // on the received side only (the sent side never carries an
            // own-grid field) -- see the mockup's own header/sample row.
            // Now this cell's whole column is ALWAYS shown (both view
            // modes -- see setViewMode()) and is the one editable
            // received-exchange cell (see flags()/setData() above); its
            // own SerialGridDelegate paints a real divider line between
            // the two values (operator, 2026-09-11: "das gehört auch
            // optisch geteilt mit einem raster").
            const QString text = serialGridRcvdText(record);
            return text.isEmpty() ? Style::unknownDash() : text;
        }
        default: return QVariant(); // Status: blank for a valid, already-logged row, matching the mockup.
        }
    }

    QVariant dividerData(int column, int role) const
    {
        if (column != ColTime) {
            return QVariant();
        }
        if (role == Qt::DisplayRole) {
            return QString::fromUtf8("Spots & Chat — noch nicht gearbeitet");
        }
        if (role == Qt::ForegroundRole) {
            return QColor(Style::kTextScale());
        }
        if (role == Qt::FontRole) {
            return Style::capsFont(QFont());
        }
        return QVariant();
    }

    QVariant candidateData(const RowRef& ref, int column, int role) const
    {
        if (!ref.chatModel) {
            return QVariant();
        }
        const SpotCandidate& candidate = ref.chatModel->candidateAt(ref.sourceRow);

        if (role == Qt::DisplayRole) {
            switch (column) {
            case ColCall: return candidate.callsign;
            // The candidate's own grid, previewed in the "Nr./Grid"
            // column (ColExchRcvd is always hidden now -- see
            // setViewMode()'s own comment -- so this is the one visible
            // received-exchange column in either mode) -- it is
            // literally the exchange-grid value this contact would send
            // if worked. Zeit/Exch-Ges. stay blank: SpotCandidate
            // carries no logged timestamp slot, and nothing has been
            // sent yet.
            case ColSerialGridRcvd: return candidate.grid;
            case ColKm: return ref.chatModel->data(ref.chatModel->index(ref.sourceRow, ChatFeedModel::ColumnDistanceKm));
            case ColDeg: return ref.chatModel->data(ref.chatModel->index(ref.sourceRow, ChatFeedModel::ColumnBearingDeg));
            default: return QVariant();
            }
        }

        const bool worked = ref.chatModel
                                 ->data(ref.chatModel->index(ref.sourceRow, ChatFeedModel::ColumnCallsign), ChatFeedModel::DupeRole)
                                 .toBool();
        if (role == Qt::ForegroundRole && worked) {
            // Dimmed, per ChatFeedModel's own DupeRole convention (only
            // reachable here when the raw-feed toggle is on -- filtered
            // mode already excludes worked candidates entirely).
            return QColor(Style::kTextInactive());
        }

        if (column == ColStatus) {
            // KST/CLU source pill, replacing the old separate
            // "ON4KST"/"Cluster" panel headers -- amber/blue, the same
            // named families this row's own autofill tint (amber) and
            // MapWidget's spotted-station colour (kBlueBg, see
            // MapWidget::markerColor/gridLabelColor) already establish,
            // not an invented one-off colour. The backgrounds
            // (kAmberBg/kInsetBg) are likewise both existing named
            // StyleKit tokens, not new hex literals.
            const bool isKst = (ref.chatModel == m_onKst);
            if (role == kPillTextRole) {
                return isKst ? QStringLiteral("KST") : QStringLiteral("CLU");
            }
            if (role == kPillBgRole) {
                return isKst ? Style::kAmberBg() : Style::kInsetBg();
            }
            if (role == kPillFgRole) {
                return isKst ? Style::kAmberText() : Style::kBlueBg();
            }
            if (role == kPillBorderRole) {
                return isKst ? Style::kAmberDim() : Style::kBlueBorder();
            }
        }
        return QVariant();
    }

    LogTableModel* m_logModel = nullptr;
    ChatFeedModel* m_onKst = nullptr;
    ChatFeedModel* m_cluster = nullptr;
    QString m_gridFilter;
    QVector<RowRef> m_rows;
    int m_dividerRow = -1;
};

UnifiedLogWidget::UnifiedLogWidget(QWidget* parent)
    : QWidget(parent)
    , m_entryRow(new EntryRowFrame(this))
    , m_feedTable(new QTableView(this))
    , m_feedModel(new UnifiedFeedModel(this))
    , m_callsignEdit(new QLineEdit(this))
    , m_sentExchangeLabel(new QLabel(this))
    , m_statusPillLabel(new QLabel(this))
    , m_callsignLookupTimer(new QTimer(this))
{
    // --- Entry row: one more row of m_feedTable's own grid, not a ----
    // --- separate panel (2026-09-11 DXLog.net "row 10" restyle) -------
    // See flatFieldStyle()'s own comment and rebuildEntryRowLayout()
    // below, which does the actual per-column sizing/ordering (mode-
    // aware: Compact vs DxLogFullColumns) every time it is needed --
    // construction just builds the persistent value widgets themselves
    // once.
    m_entryRow->setObjectName(QLatin1String(kEntryRowObjectName));
    // Fixed vertical size policy: this row's height comes entirely from
    // its own children's fixed sizes, matching m_feedTable's own fixed
    // per-row height -- the same "no scrollbars, deterministic height"
    // intent the original frozen single-row entry table had.
    m_entryRow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // See m_entryRowScroll's own doc comment in the header -- keeps
    // m_entryRow at its own natural (column-sum) width and lets the
    // viewport scroll it horizontally, driven from m_feedTable's own
    // horizontal scrollbar (connected once m_feedTable exists, below).
    m_entryRowScroll = new QScrollArea(this);
    m_entryRowScroll->setObjectName(QLatin1String(kEntryRowScrollObjectName));
    m_entryRowScroll->setWidgetResizable(false);
    m_entryRowScroll->setFrameShape(QFrame::NoFrame);
    m_entryRowScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_entryRowScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_entryRowScroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_entryRowScroll->setWidget(m_entryRow);

    m_entryRowLayout = new QHBoxLayout(m_entryRow);
    m_entryRowLayout->setContentsMargins(0, 0, 0, 0);
    m_entryRowLayout->setSpacing(0);

    m_callsignEdit->setPlaceholderText(QStringLiteral("Callsign"));
    m_callsignEdit->setMaxLength(16);
    // Regular weight, matching m_feedTable's own Call column exactly --
    // no FontRole::Bold anywhere in historyData()/candidateData() for
    // that column (only an invalid row's strike-through changes its
    // font at all).
    m_callsignEdit->setFont(Style::monoFont(m_callsignEdit->font(), Style::kFontBody));
    m_callsignEdit->setStyleSheet(flatFieldStyle(false));
    m_callsignEdit->setFrame(false);
    m_callsignEdit->setTextMargins(kEntryRowHPadding, 0, kEntryRowHPadding, 0);
    // DXLog.net's real model (dxlog.net/docs/index.php/Main_Window,
    // verified for this task -- see the report): "[Enter] is used to
    // log a contact" from ANY entry-row field, not only the last one in
    // sequence -- a plain signal-to-signal connection, same as every
    // exchange sub-field gets in rebuildExchangeCell() below. Getting
    // from Call to the exchange sub-fields is [Space]/[Tab] (see
    // eventFilter()/nextFieldForTab()), never Enter.
    connect(m_callsignEdit, &QLineEdit::returnPressed, this, &UnifiedLogWidget::logRequested);
    connect(m_callsignEdit, &QLineEdit::textChanged, this, &UnifiedLogWidget::formChanged);
    connect(m_callsignEdit, &QLineEdit::textChanged, this, &UnifiedLogWidget::onCallsignTextChanged);
    m_callsignEdit->installEventFilter(this);

    // Live-ticking Time cell -- see its own doc comment in the header.
    // Same flat styling as rebuildEntryRowLayout()'s throwaway blank
    // cells (built once here instead, since this one is never actually
    // blank).
    m_entryTimeLabel = new QLabel(this);
    m_entryTimeLabel->setFont(Style::monoFont(m_entryTimeLabel->font(), Style::kFontBody));
    m_entryTimeLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent; border-right: 1px solid %2; padding: 0 %3px;")
            .arg(Style::kTextPrimary(), Style::kBorder())
            .arg(kEntryRowHPadding));
    m_liveClockTimer = new QTimer(this);
    m_liveClockTimer->setInterval(1000);
    connect(m_liveClockTimer, &QTimer::timeout, this, [this]() {
        m_entryTimeLabel->setText(QDateTime::currentDateTimeUtc().time().toString(QStringLiteral("HH:mm")));
    });
    m_liveClockTimer->start();
    m_entryTimeLabel->setText(QDateTime::currentDateTimeUtc().time().toString(QStringLiteral("HH:mm")));

    // Live km/bearing preview -- operator, 2026-09-11: "die grad anzeige
    // von meinem standort zum qsp partner sollten sofort angezeigt
    // werden" (a direct reversal of the earlier mockup-E decision to
    // omit this, see setEntryDistanceBearing()'s own doc comment).
    // Persistent, same "survives rebuildEntryRowLayout() via detach/
    // reattach, not delete/recreate" reasoning as m_entryTimeLabel just
    // above -- MainWindow keeps pushing fresh values into these via
    // setEntryDistanceBearing() as the operator types the other
    // station's grid, so they must be the SAME instance across any
    // later rebuild, not a throwaway addBlank() cell.
    m_entryKmLabel = new QLabel(Style::unknownDash(), this);
    m_entryDegLabel = new QLabel(Style::unknownDash(), this);
    for (QLabel* label : {m_entryKmLabel, m_entryDegLabel}) {
        label->setFont(Style::monoFont(label->font(), Style::kFontBody));
        label->setStyleSheet(
            QStringLiteral("color: %1; background: transparent; border-right: 1px solid %2; padding: 0 %3px;")
                .arg(Style::kTextPrimary(), Style::kBorder())
                .arg(kEntryRowHPadding));
    }

    // Placeholder host for the per-contest exchange sub-fields, torn
    // down and rebuilt by rebuildExchangeCell() every setExchangeFields()
    // call. Empty at construction time; the shipped app always calls
    // setExchangeFields() shortly after construction (see
    // MainWindow.cpp).
    m_exchangeFieldsHost = new QWidget(m_entryRow);
    auto* emptyHostLayout = new QHBoxLayout(m_exchangeFieldsHost);
    emptyHostLayout->setContentsMargins(0, 0, 0, 0);
    emptyHostLayout->setSpacing(0);

    m_sentExchangeLabel->setText(Style::unknownDash());
    m_sentExchangeLabel->setFont(Style::monoFont(m_sentExchangeLabel->font(), Style::kFontBody));
    m_sentExchangeLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent; border-right: 1px solid %2; padding: 0 %3px;")
            .arg(Style::kTextPrimary(), Style::kBorder())
            .arg(kEntryRowHPadding));

    // No coloured pill box while the row is still just "new" -- blank,
    // matching m_feedTable's own Status column on an ordinary valid
    // logged row (see UnifiedFeedModel::historyData()'s own comment)
    // and DXLog's row 10 itself (blank "Stn" cell). Only updateStatusPill(true)
    // (a real dupe) paints the small red badge -- see dupePillStyle().
    m_statusPillLabel->setObjectName(QLatin1String(kStatusPillObjectName));
    m_statusPillLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    updateStatusPill(false);

    // rebuildEntryRowLayout() (called from setViewMode() below, and
    // again on every later setViewMode() call) does the actual
    // addWidget()s in the right order/widths for the current column
    // mode -- nothing more to add to m_entryRowLayout here.

    // --- DXLog-style status line (see setOperatingMode()'s doc comment
    // in UnifiedLogWidget.h) -- a thin strip directly above the feed
    // table, one line: "most recently logged QSO" on the left, the
    // current Run/S&P operating mode on the right. kStatusBarBg/
    // kStatusBarBorder (StyleKit.h: "Status bar, per StyleConstants.h")
    // is exactly this surface's own token family, not the panel/inset
    // fills used elsewhere in this row. WA_StyledBackground is required
    // for a plain QWidget to paint its own stylesheet background at all
    // -- see Style::applyPanelFrameStyle()'s own comment on this exact
    // Qt gotcha; not reusing that helper directly here since it also
    // draws a full rounded panel border, which this flat single-line
    // strip does not want.
    m_statusLine = new QWidget(this);
    m_statusLine->setObjectName(QLatin1String(kStatusLineObjectName));
    m_statusLine->setAttribute(Qt::WA_StyledBackground, true);
    m_statusLine->setStyleSheet(QStringLiteral("QWidget#%1 { background: %2; border-bottom: 1px solid %3; }")
                                     .arg(QLatin1String(kStatusLineObjectName), Style::kStatusBarBg(),
                                          Style::kStatusBarBorder()));
    m_statusLine->setFixedHeight(kStatusLineHeight);
    auto* statusLineLayout = new QHBoxLayout(m_statusLine);
    statusLineLayout->setContentsMargins(kEntryRowHPadding, 0, kEntryRowHPadding, 0);
    statusLineLayout->setSpacing(kEntryRowHPadding);

    m_lastQsoLabel = new QLabel(m_statusLine);
    m_lastQsoLabel->setObjectName(QLatin1String(kLastQsoLabelObjectName));
    m_lastQsoLabel->setFont(Style::monoFont(m_lastQsoLabel->font(), Style::kFontSmall));
    m_lastQsoLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(Style::kTextSecondary()));
    statusLineLayout->addWidget(m_lastQsoLabel);
    statusLineLayout->addStretch(1);

    // Amber, matching m_sentExchangeLabel's own "a value worth the
    // operator's attention" treatment just above, not a state-badge
    // colour (this is plain read-only status text, not a clickable
    // control -- Style.h's blue = interactive/commanded rule does not
    // apply to it).
    m_operatingModeLabel = new QLabel(m_statusLine);
    m_operatingModeLabel->setObjectName(QLatin1String(kOperatingModeLabelObjectName));
    m_operatingModeLabel->setFont(Style::capsFont(m_operatingModeLabel->font()));
    m_operatingModeLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(Style::kAmberText()));
    statusLineLayout->addWidget(m_operatingModeLabel);

    // --- Feed table: log history + not-yet-worked spot/chat rows -----
    // Sits directly below the entry row with zero layout gap -- the
    // entry row's own EntryRowFrame::paintEvent() draws the 1px shared
    // border between them, and both live inside the same "Log"
    // PanelHeaderBar panel MainWindow registers this whole widget under
    // -- see UnifiedLogWidget.h's class comment.
    m_feedTable->setObjectName(QLatin1String(kFeedTableObjectName));
    m_feedTable->setModel(m_feedModel);
    m_feedTable->horizontalHeader()->hide();
    m_feedTable->verticalHeader()->hide();
    m_feedTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    // A History row's Call/Exch Emp. cells are editable in place
    // (double-click, or select-then-F2/Enter) -- see UnifiedFeedModel::
    // flags()/setData() above; Divider/Candidate rows and every other
    // History column stay non-editable via those same flags(), so a
    // broad trigger set here is safe.
    m_feedTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_feedTable->setAlternatingRowColors(true);
    // Deliberately not sortingEnabled(true) (unlike the old LogTableView):
    // this one column grid now serves three structurally different row
    // kinds (log history, a divider, spot/chat candidates) that must
    // stay in that fixed relative order -- a free column-header sort
    // would scramble them together.
    m_feedTable->setSortingEnabled(false);
    m_feedTable->setFont(Style::monoFont(m_feedTable->font(), Style::kFontBody));
    // Row height no longer relies on Qt's own lazily-computed default
    // (which does not retroactively track a later setFont() call) --
    // pinned explicitly off THIS font's metrics so every entry-row cell
    // (which reads this same defaultSectionSize(), see
    // rebuildEntryRowLayout()'s own comment) sizes correctly the first
    // time, not just after some row happens to get resized to content.
    // Padding matches the original DXLog-mockup CSS this whole grid
    // follows (log-final-layout.html: "td{padding:7px 10px}").
    m_feedTable->verticalHeader()->setDefaultSectionSize(QFontMetrics(m_feedTable->font()).height() + 14);
    // NOT setStretchLastSection() -- that has no notion of a maximum,
    // and an unbounded-width Status column just relocates the "looks
    // like unused dead space" problem into a real column instead of
    // fixing it (operator, 2026-09-11: "benötigen wir die letzte spalte
    // im log?"). syncStatusColumnWidth() does the same fill, capped at
    // kStatusColumnMaxWidth -- called below once now, and again from
    // resizeEvent()/setViewMode() whenever the panel width or the
    // visible column set changes.
    m_feedTable->setItemDelegateForColumn(ColStatus, new PillDelegate(m_feedTable));
    m_feedTable->setItemDelegateForColumn(ColSerialGridRcvd, new SerialGridDelegate(m_feedTable));
    connect(m_feedTable, &QTableView::clicked, this, &UnifiedLogWidget::handleFeedRowClicked);
    connect(m_feedModel, &UnifiedFeedModel::rebuilt, this, &UnifiedLogWidget::rebuildFeedRows);
    connect(m_feedModel, &UnifiedFeedModel::historyCallsignEditRequested, this, &UnifiedLogWidget::historyCallsignEditRequested);
    connect(m_feedModel, &UnifiedFeedModel::historyExchangeRcvdEditRequested, this, &UnifiedLogWidget::historyExchangeRcvdEditRequested);
    connect(m_feedModel, &UnifiedFeedModel::historyTimeEditRequested, this, &UnifiedLogWidget::historyTimeEditRequested);
    // Drives m_entryRowScroll from m_feedTable's own horizontal
    // scrollbar (see that member's doc comment) -- one-directional:
    // m_entryRowScroll's own scrollbar is always hidden, the operator
    // only ever drags the table's, so there is no loop to guard against.
    connect(m_feedTable->horizontalScrollBar(), &QAbstractSlider::valueChanged, this,
            [this](int value) { m_entryRowScroll->horizontalScrollBar()->setValue(value); });

    configureFeedColumns();
    // Default Compact view -- hides ColumnSerial until the operator
    // opts into "DXLog-Vollspalten" via the Log panel's own ⚙ (see
    // setViewMode()'s doc comment).
    setViewMode(m_viewMode);

    m_callsignLookupTimer->setSingleShot(true);
    m_callsignLookupTimer->setInterval(kCallsignLookupDebounceMs);
    connect(m_callsignLookupTimer, &QTimer::timeout, this, &UnifiedLogWidget::onCallsignLookupTimeout);

    // --- Chat quick-send row --------------------------------------
    // Operator, 2026-09-12, during a quiet spell ("wenig qso beim
    // rufen"): wants to leave an ON4KST chat message "mit wenig
    // aufwand". Two low-effort paths into the SAME field: type freely
    // and press Enter/"Senden", or click "CQ" to have MainWindow drop
    // a ready-to-send "CQ DE <call>" text in (see setChatInputDraft()'s
    // own doc comment) and just press Enter. Always the panel's last
    // row -- setEntryRowPosition() appends it after the Top/Bottom
    // branch every time, so it stays put regardless of where the entry
    // row itself sits. Plain QLineEdit/QPushButton, no per-widget
    // stylesheet: Style::appStyleSheet() already styles both globally
    // (unlike the entry row's own flatFieldStyle() cells, which
    // deliberately mimic m_feedTable's column grid -- this row is a
    // distinct control, not another table column).
    m_chatRow = new QWidget(this);
    m_chatRow->setAttribute(Qt::WA_StyledBackground, true);
    m_chatRow->setObjectName(QLatin1String("unifiedLogChatRow"));
    m_chatRow->setStyleSheet(QStringLiteral("QWidget#unifiedLogChatRow { border-top: 1px solid %1; }")
                                  .arg(Style::kBorder()));
    auto* chatRowLayout = new QHBoxLayout(m_chatRow);
    chatRowLayout->setContentsMargins(kEntryRowHPadding, 6, kEntryRowHPadding, 6);
    chatRowLayout->setSpacing(6);

    // Away/Zurück presence toggle -- protocol-level "/AWAY", "/BACK"
    // (On4kstClient::sendAway()/sendBack(), built 2026-09-09 but never
    // wired to any UI control until now). Checkable QPushButton, no
    // per-widget stylesheet: Style::appStyleSheet() already gives
    // QPushButton:checked its own highlighted look globally (the same
    // rule any other checkable control in this codebase relies on) --
    // the label itself flips between the two commands' names so the
    // button always reads as "what clicking it does next", not "what
    // state it's in".
    m_awayToggleButton = new QPushButton(QStringLiteral("Away"), m_chatRow);
    m_awayToggleButton->setObjectName(QLatin1String(kAwayToggleButtonObjectName));
    m_awayToggleButton->setCheckable(true);
    m_awayToggleButton->setToolTip(QStringLiteral("Als abwesend/zurück im ON4KST-Chat melden"));
    connect(m_awayToggleButton, &QPushButton::toggled, this, [this](bool checked) {
        m_awayToggleButton->setText(checked ? QStringLiteral("Zurück") : QStringLiteral("Away"));
        emit awayStateChanged(checked);
    });
    chatRowLayout->addWidget(m_awayToggleButton);

    m_chatCqButton = new QPushButton(QStringLiteral("CQ"), m_chatRow);
    m_chatCqButton->setObjectName(QLatin1String(kChatCqButtonObjectName));
    m_chatCqButton->setToolTip(QStringLiteral(
        "CQ-Text mit eigenem Rufzeichen einsetzen -- danach Enter zum Senden"));
    connect(m_chatCqButton, &QPushButton::clicked, this, &UnifiedLogWidget::cqDraftRequested);
    chatRowLayout->addWidget(m_chatCqButton);

    m_chatInputEdit = new QLineEdit(m_chatRow);
    m_chatInputEdit->setObjectName(QLatin1String(kChatInputObjectName));
    m_chatInputEdit->setPlaceholderText(QStringLiteral("Chat-Nachricht an ON4KST ..."));
    m_chatInputEdit->setFont(Style::monoFont(m_chatInputEdit->font(), Style::kFontBody));
    chatRowLayout->addWidget(m_chatInputEdit, 1);

    m_chatSendButton = new QPushButton(QStringLiteral("Senden"), m_chatRow);
    m_chatSendButton->setObjectName(QLatin1String(kChatSendButtonObjectName));
    chatRowLayout->addWidget(m_chatSendButton);

    auto sendChatInput = [this]() {
        const QString text = m_chatInputEdit->text().trimmed();
        if (text.isEmpty()) {
            return;
        }
        emit chatMessageSendRequested(text);
        m_chatInputEdit->clear();
    };
    connect(m_chatInputEdit, &QLineEdit::returnPressed, this, sendChatInput);
    connect(m_chatSendButton, &QPushButton::clicked, this, sendChatInput);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    // setEntryRowPosition() does the actual addWidget() calls (Top's
    // order matches this class's original, unchanged construction
    // order) -- one place owns the ordering instead of duplicating it
    // here and in that method.
    setEntryRowPosition(m_entryRowPosition);

    updateStatusLine();
    m_callsignEdit->setFocus();
}

QWidget* UnifiedLogWidget::buildFieldCell(QWidget* parent, QWidget* valueWidget, int width, int height,
                                           Qt::Alignment align)
{
    // No label above the value any more, no box around it -- just the
    // value widget itself at a fixed width, exactly like one of
    // m_feedTable's own plain cells (see flatFieldStyle()'s own
    // comment). Still returns a thin wrapping QWidget rather than
    // adding `valueWidget` straight into the caller's layout: a fixed-
    // width host keeps `valueWidget` from stretching past its column's
    // share when several sub-fields share one column group (see
    // rebuildExchangeCell()), which a bare addWidget(valueWidget, 0)
    // cannot express as cleanly.
    //
    // `height` is always m_feedTable's own row height (see
    // rebuildEntryRowLayout()'s own comment on why every entry-row cell
    // is pinned to it) -- set on BOTH `cell` and `valueWidget` itself:
    // QLineEdit's default vertical size policy is Fixed, so merely
    // giving the (taller) `cell`/host container the right height would
    // leave the QLineEdit sitting at its own shorter natural height,
    // top-aligned inside it, which is exactly the uneven-row-height
    // look this was meant to fix in the first place.
    auto* cell = new QWidget(parent);
    auto* h = new QHBoxLayout(cell);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);

    valueWidget->setParent(cell);
    valueWidget->setFixedHeight(height);
    if (auto* valueLabel = qobject_cast<QLabel*>(valueWidget)) {
        valueLabel->setAlignment(align | Qt::AlignVCenter);
    }
    h->addWidget(valueWidget);

    cell->setFixedSize(width, height);
    return cell;
}

void UnifiedLogWidget::configureFeedColumns()
{
    for (int col = 0; col < ColCount; ++col) {
        m_feedTable->setColumnWidth(col, kColumnWidths[col]);
        m_feedTable->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Fixed);
    }
}

void UnifiedLogWidget::syncStatusColumnWidth()
{
    // See kStatusColumnMaxWidth's own comment -- ColStatus fills a panel
    // wider than the rest of the fixed grid needs, but only up to that
    // cap; a still-wider panel just leaves a bounded, honest margin past
    // it (matching every other column's own fixed-width philosophy)
    // rather than growing one column without limit.
    int otherVisibleWidth = 0;
    for (int col = 0; col < ColCount; ++col) {
        if (col != ColStatus && !m_feedTable->isColumnHidden(col)) {
            otherVisibleWidth += m_feedTable->columnWidth(col);
        }
    }
    const int viewportWidth = m_feedTable->viewport()->width();
    const int desiredWidth =
        qBound(kColumnWidths[ColStatus], viewportWidth - otherVisibleWidth, kStatusColumnMaxWidth);
    m_feedTable->setColumnWidth(ColStatus, desiredWidth);
}

void UnifiedLogWidget::setLogModel(LogTableModel* model)
{
    // Own connections to `model`, independent of m_feedModel's own (see
    // UnifiedLogWidget.h's m_logModel doc comment) -- multiple listeners
    // on the same QAbstractItemModel signals is ordinary Qt, not a
    // conflict with m_feedModel::setLogModel()'s identical connect()
    // calls just below.
    if (m_logModel) {
        disconnect(m_logModel, nullptr, this, nullptr);
    }
    m_logModel = model;
    if (m_logModel) {
        connect(m_logModel, &QAbstractItemModel::modelReset, this, &UnifiedLogWidget::updateStatusLine);
        // A history-row hand-correction (see historyCallsignEditRequested/
        // historyExchangeRcvdEditRequested) patches LogTableModel in
        // place rather than resetting it -- catches the case where the
        // operator corrects the LAST row's own callsign/time.
        connect(m_logModel, &QAbstractItemModel::dataChanged, this, &UnifiedLogWidget::updateStatusLine);
    }
    m_feedModel->setLogModel(model);
    updateStatusLine();
}

void UnifiedLogWidget::setChatModels(ChatFeedModel* onKst, ChatFeedModel* cluster)
{
    m_feedModel->setChatModels(onKst, cluster);
}

void UnifiedLogWidget::setExchangeFields(const QVector<ContestDefinition::ExchangeField>& fields)
{
    // Preserve values for keys that survive the rebuild unchanged --
    // same "never silently discard what the operator already typed"
    // posture EntryBarWidget::setExchangeFields established.
    const QMap<QString, QString> previousValues = exchangeReceived();
    m_exchangeFields = fields;
    rebuildExchangeCell(previousValues);
}

void UnifiedLogWidget::rebuildExchangeCell(const QMap<QString, QString>& previousValues)
{
    m_exchangeEdits.clear();
    m_exchangeEditsByKey.clear();

    auto* host = new QWidget(m_entryRow);
    auto* hostLayout = new QHBoxLayout(host);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);

    // Same fixed row height every other entry-row cell uses -- see
    // rebuildEntryRowLayout()'s own comment on why this must be pinned
    // explicitly rather than left to each widget's own natural
    // sizeHint().
    const int rowHeight = m_feedTable->verticalHeader()->defaultSectionSize();

    // Weighted split of the received-exchange column group's own pixel
    // width, instead of a naive equal division across however many
    // sub-fields this contest declares (see rcvdGroupWidth()'s own
    // comment) -- a 6-character grid6 field genuinely needs more room
    // than a 2-3 digit RST/serial field, and dividing evenly left grid6
    // cramped right up against its own border regardless of how much
    // spare width rcvdGroupWidth() actually had. Operator, 2026-09-11,
    // on exactly that: "der raster schneidet fast die buchstaben ab".
    // Each type gets a sane minimum (grid6 widest, matching its own
    // 6-character max length -- see setMaxLength(6) below), scaled up
    // together if rcvdGroupWidth() leaves room to spare, with the LAST
    // field absorbing whatever rounding remainder is left so the
    // per-field widths still sum to exactly rcvdGroupWidth() (the fixed
    // width m_exchangeFieldsHost itself is set to in
    // rebuildEntryRowLayout()).
    const auto minFieldWidth = [](const QString& type) {
        if (type == QStringLiteral("grid6")) { return 74; }
        if (type == QStringLiteral("rst")) { return 46; }
        return 50; // "int" (serial) and any other/future type
    };
    QVector<int> fieldWidths;
    if (!m_exchangeFields.isEmpty()) {
        int totalMinWidth = 0;
        for (const ContestDefinition::ExchangeField& field : m_exchangeFields) {
            totalMinWidth += minFieldWidth(field.type);
        }
        const double scale = totalMinWidth > 0
            ? qMax(1.0, static_cast<double>(rcvdGroupWidth()) / static_cast<double>(totalMinWidth))
            : 1.0;
        int runningWidth = 0;
        for (int i = 0; i < m_exchangeFields.size(); ++i) {
            const bool isLastField = (i == m_exchangeFields.size() - 1);
            const int width = isLastField
                ? qMax(36, rcvdGroupWidth() - runningWidth)
                : qMax(36, qRound(minFieldWidth(m_exchangeFields.at(i).type) * scale));
            fieldWidths.append(width);
            runningWidth += width;
        }
    }

    int fieldIndex = 0;
    for (const ContestDefinition::ExchangeField& field : m_exchangeFields) {
        auto* edit = new QLineEdit(host);
        edit->setPlaceholderText(field.label);
        edit->setFont(Style::monoFont(edit->font(), Style::kFontBody));
        edit->setStyleSheet(flatFieldStyle(false));
        edit->setFrame(false);
        edit->setTextMargins(kEntryRowHPadding / 2, 0, kEntryRowHPadding / 2, 0);

        if (field.type == QStringLiteral("int")) {
            edit->setValidator(new QIntValidator(0, 999999, edit));
        } else if (field.type == QStringLiteral("grid6")) {
            edit->setMaxLength(6);
        } else if (field.type == QStringLiteral("rst")) {
            edit->setMaxLength(4); // "59"/"599", occasionally "5NN" etc.
        }

        const QString previous = previousValues.value(field.key);
        if (!previous.isEmpty()) {
            edit->setText(previous);
        }

        // DXLog.net's real model (dxlog.net/docs/index.php/Main_Window,
        // verified for this task -- see the report): "[Enter] is used
        // to log a contact" from ANY entry-row field, not only the last
        // one in sequence -- replaces the old progressive-Enter scheme
        // (advance to the next sub-field, log only from the last one).
        connect(edit, &QLineEdit::returnPressed, this, &UnifiedLogWidget::logRequested);
        // textEdited (not textChanged) fires only for user interaction,
        // never for applyKnownExchange()'s/applyRstDefaults()'s own
        // setText() calls, so this cannot immediately undo its own fill.
        connect(edit, &QLineEdit::textEdited, this, [this, edit] { setFieldAutoFilled(edit, false); });

        if (field.type == QStringLiteral("grid6")) {
            // Live km/bearing preview plumbing -- MainWindow recomputes
            // from ContestSettings::ownGrid on every keystroke here and
            // calls setEntryDistanceBearing() back, which now actually
            // renders into m_entryKmLabel/m_entryDegLabel (see that
            // method's own doc comment).
            connect(edit, &QLineEdit::textChanged, this, [this](const QString& text) {
                emit receivedGridChanged(text.trimmed().toUpper());
            });
        }

        edit->installEventFilter(this);

        hostLayout->addWidget(buildFieldCell(host, edit, fieldWidths.at(fieldIndex++), rowHeight));
        m_exchangeEdits.append(edit);
        m_exchangeEditsByKey.insert(field.key, edit);
    }

    m_entryRowLayout->replaceWidget(m_exchangeFieldsHost, host);
    m_exchangeFieldsHost->deleteLater();
    m_exchangeFieldsHost = host;

    // RST (if this contest declares one) gets its "59"/"599" default
    // the moment its field exists, same as on a fresh contest switch as
    // on first construction -- needs the now-current m_exchangeEdits/
    // m_exchangeFields (Tab's own field chain is computed live from
    // those two in nextFieldForTab(), so it needs no rebuild step here).
    applyRstDefaults();
}

QString UnifiedLogWidget::callsign() const
{
    return m_callsignEdit->text().trimmed().toUpper();
}

QMap<QString, QString> UnifiedLogWidget::exchangeReceived() const
{
    QMap<QString, QString> result;
    for (const ContestDefinition::ExchangeField& field : m_exchangeFields) {
        QLineEdit* edit = m_exchangeEditsByKey.value(field.key, nullptr);
        if (!edit) {
            continue;
        }
        QString text = edit->text().trimmed();
        if (field.type == QStringLiteral("grid6")) {
            text = text.toUpper();
        }
        result.insert(field.key, text);
    }
    return result;
}

void UnifiedLogWidget::setSentExchangePreview(const QString& text)
{
    m_sentExchangeLabel->setText(text.isEmpty() ? Style::unknownDash() : text);
}

void UnifiedLogWidget::setEntryDistanceBearing(const std::optional<double>& distanceKm,
                                                const std::optional<double>& bearingDeg)
{
    // See this method's doc comment in UnifiedLogWidget.h -- reversed
    // from the earlier mockup-E "no live readout" decision, per the
    // operator's own follow-up. Formatting matches LogTableModel::data()'s
    // own ColumnDistanceKm/ColumnBearingDeg exactly (LogTableModel.cpp):
    // one decimal place for km, zero for degrees, Style::unknownDash()
    // when not yet computable -- so a logged QSO's Km/° cell and this
    // live preview never disagree on formatting.
    m_entryKmLabel->setText(distanceKm ? QString::number(*distanceKm, 'f', 1) : Style::unknownDash());
    m_entryDegLabel->setText(bearingDeg ? QString::number(*bearingDeg, 'f', 0) : Style::unknownDash());
}

void UnifiedLogWidget::setDupeIndicator(bool isDupe)
{
    updateStatusPill(isDupe);
}

void UnifiedLogWidget::updateStatusPill(bool dupe)
{
    m_dupe = dupe;
    if (!dupe) {
        // Blank -- matching m_feedTable's own Status column on an
        // ordinary valid logged row and DXLog's row 10 itself. See
        // dupePillStyle()'s own comment.
        m_statusPillLabel->clear();
        m_statusPillLabel->setStyleSheet(QStringLiteral("background: transparent;"));
        return;
    }
    m_statusPillLabel->setText(QStringLiteral("DUPE"));
    QFont font = m_statusPillLabel->font();
    font.setPixelSize(Style::kFontCaption);
    font.setWeight(QFont::DemiBold);
    m_statusPillLabel->setFont(font);
    m_statusPillLabel->setStyleSheet(dupePillStyle());
}

void UnifiedLogWidget::setCurrentMode(const QString& mode)
{
    m_currentMode = mode;
    applyRstDefaults();
}

void UnifiedLogWidget::setCallsign(const QString& callsign)
{
    m_callsignEdit->setText(callsign);
}

void UnifiedLogWidget::setExchangeFieldValue(const QString& key, const QString& value)
{
    QLineEdit* edit = m_exchangeEditsByKey.value(key, nullptr);
    if (edit) {
        edit->setText(value);
    }
}

void UnifiedLogWidget::focusFirstEmptyExchangeField()
{
    for (const ContestDefinition::ExchangeField& field : m_exchangeFields) {
        QLineEdit* edit = m_exchangeEditsByKey.value(field.key, nullptr);
        if (edit && field.type != QStringLiteral("rst") && edit->text().trimmed().isEmpty()) {
            edit->setFocus();
            return;
        }
    }
}

void UnifiedLogWidget::resetForNextEntry()
{
    m_callsignEdit->clear();
    for (QLineEdit* edit : m_exchangeEdits) {
        edit->clear();
        setFieldAutoFilled(edit, false);
    }
    // DXLog.net re-defaults RST the moment a fresh entry starts, not
    // only once when the field is first built -- see applyRstDefaults()'s
    // own doc comment.
    applyRstDefaults();
    setDupeIndicator(false);
    setEntryDistanceBearing(std::nullopt, std::nullopt);
    m_callsignEdit->setFocus();
}

void UnifiedLogWidget::applyKnownExchange(const QString& gridSquare, const std::optional<int>& serialRcvd)
{
    for (const ContestDefinition::ExchangeField& field : m_exchangeFields) {
        QLineEdit* edit = m_exchangeEditsByKey.value(field.key, nullptr);
        if (!edit) {
            continue;
        }
        if (field.type == QStringLiteral("grid6") && !gridSquare.isEmpty() && edit->text().isEmpty()) {
            edit->setText(gridSquare);
            setFieldAutoFilled(edit, true);
        }
        if (field.autoIncrement && serialRcvd.has_value() && edit->text().isEmpty()) {
            edit->setText(QString::number(*serialRcvd));
            setFieldAutoFilled(edit, true);
        }
    }
}

bool UnifiedLogWidget::hasUnsentContent() const
{
    return !m_callsignEdit->text().trimmed().isEmpty();
}

void UnifiedLogWidget::setGridFilter(const QString& text)
{
    m_feedModel->setGridFilter(text);
}

void UnifiedLogWidget::setOperatingMode(ContestSettings::OperatingMode mode)
{
    m_operatingMode = mode;
    updateStatusLine();
}

void UnifiedLogWidget::setEntryRowPosition(ContestSettings::LogEntryRowPosition position)
{
    m_entryRowPosition = position;
    const bool bottom = (position == ContestSettings::LogEntryRowPosition::Bottom);

    // removeWidget() only detaches from the layout -- the widgets stay
    // alive as children of `this`, safe to re-add in the new order.
    // m_entryRowScroll (not m_entryRow itself) is the layout item --
    // see its own doc comment in the header.
    m_mainLayout->removeWidget(m_entryRowScroll);
    m_mainLayout->removeWidget(m_statusLine);
    m_mainLayout->removeWidget(m_feedTable);
    // Drain anything left -- specifically the trailing addStretch()
    // Bottom mode adds below (see the `bottom` branch) -- so toggling
    // back and forth does not accumulate orphaned stretch items every
    // call. Only ever a spacer item at this point, never a widget (the
    // three real widgets are already detached above).
    while (QLayoutItem* item = m_mainLayout->takeAt(0)) {
        delete item;
    }

    if (bottom) {
        // Status strip FIRST (matching DXLog.net's own placement: one
        // status line above the whole grid, not sandwiched between the
        // last QSO and the entry row), then the table, then the entry
        // row directly against the table's own last row with nothing
        // between them -- operator, 2026-09-11: "die zeile muss genau
        // über dem 4 ten qso sein stehen, es sollte genau so aussehen,
        // wie ein qso." Top mode below keeps its original, unrelated
        // adjacency (status line between entry row and table) --
        // nobody asked to change that one. No stretch factor on the
        // table here (unlike Top mode) -- syncFeedTableHeight() below
        // caps it to exactly its own content height, and giving a
        // height-capped widget a nonzero stretch factor is a known Qt
        // box-layout quirk: the leftover-past-the-cap space can land in
        // the wrong place (observed: ABOVE the status line, not below
        // the entry row). A dedicated trailing stretch instead owns
        // 100% of the leftover, so it unambiguously ends up below the
        // entry row where it belongs.
        m_mainLayout->addWidget(m_statusLine);
        m_mainLayout->addWidget(m_feedTable);
        m_mainLayout->addWidget(m_entryRowScroll);
        m_mainLayout->addStretch(1);
    } else {
        m_mainLayout->addWidget(m_entryRowScroll);
        m_mainLayout->addWidget(m_statusLine);
        m_mainLayout->addWidget(m_feedTable, 1);
    }

    // Chat quick-send row: always last, in BOTH modes -- the drain loop
    // above took it out along with everything else (see that loop's own
    // comment; a QWidgetItem's deletion does not delete the widget it
    // wraps, matching how m_entryRowScroll/m_statusLine/m_feedTable
    // already survive this same re-add every call), so it must be
    // explicitly re-added here every time, not just once at construction.
    m_mainLayout->addWidget(m_chatRow);

    // Border only on the edge that actually touches the table -- see
    syncFeedTableHeight();
}

void UnifiedLogWidget::setChatInputDraft(const QString& text)
{
    m_chatInputEdit->setText(text);
    m_chatInputEdit->setFocus();
    m_chatInputEdit->selectAll();
}

void UnifiedLogWidget::applyColumnOrder(const QVector<int>& logicalOrder)
{
    QHeaderView* header = m_feedTable->horizontalHeader();
    // Move each logical column into its target VISUAL position, left to
    // right -- once position i is settled, every earlier position stays
    // put for the rest of the loop, so this converges to exactly
    // `logicalOrder` regardless of the header's current order (hidden
    // columns still occupy a visual slot, so they must be listed too;
    // see the two full-ColCount call sites in setViewMode() below).
    for (int targetVisual = 0; targetVisual < logicalOrder.size(); ++targetVisual) {
        const int logical = logicalOrder.at(targetVisual);
        const int currentVisual = header->visualIndex(logical);
        if (currentVisual != targetVisual) {
            header->moveSection(currentVisual, targetVisual);
        }
    }
}

void UnifiedLogWidget::setViewMode(ContestSettings::LogViewMode mode)
{
    // MainWindow::updateStatusBar() calls this UNCONDITIONALLY on every
    // CAT frequency/mode tick, on4kst/cluster connect events, etc. (see
    // its own doc comment: "cheap/idempotent when nothing actually
    // changed... needs no dirty-check") -- real, frequent, steady-state
    // traffic, not just an operator's own ⚙ click. The column-hide/
    // -order calls below already tolerated that (Qt no-ops a
    // setColumnHidden()/moveSection() to a value it already has), but
    // rebuildEntryRowLayout()/rebuildExchangeCell() do not: they tear
    // down and rebuild real, focused, mid-typing QLineEdits every time
    // they run. Without this guard, live CAT traffic reconstructs the
    // entry row's Callsign/exchange fields out from under the operator
    // continuously -- exactly the "ich kann nichts eingeben" bug this
    // guard fixes (2026-09-11): only a GENUINE mode change (or the very
    // first call, before m_entryRowLayoutBuilt is set) rebuilds them.
    const bool needsEntryRowRebuild = (mode != m_viewMode) || !m_entryRowLayoutBuilt;
    m_viewMode = mode;
    const bool dxLog = (mode == ContestSettings::LogViewMode::DxLogFullColumns);

    m_feedTable->setColumnHidden(ColSerial, !dxLog);
    m_feedTable->setColumnHidden(ColBand, !dxLog);
    m_feedTable->setColumnHidden(ColRstSent, !dxLog);
    m_feedTable->setColumnHidden(ColSerialSent, !dxLog);
    // ColRstRcvd/ColSerialGridRcvd are ALWAYS visible now, in BOTH view
    // modes -- no longer gated by dxLog. Operator, 2026-09-11: after
    // double-clicking Compact's old combined "Exch Emp." cell to fix a
    // grid and finding RST editable right along with it ("ich habe bei
    // 59 reingeklickt und der locator war in der gleichen raster"), the
    // received exchange is now ALWAYS shown split (RST separately from
    // Nr./Grid) -- see flags()/setData() above for why only
    // ColSerialGridRcvd is actually editable. ColExchRcvd (the old
    // combined cell) is correspondingly ALWAYS hidden now -- superseded
    // by the two split columns in every mode, not just DxLogFullColumns.
    // Exch Ges. (the SENT side) is unaffected -- it was never editable
    // in the first place (a read-only preview of what we sent), so it
    // keeps its original Compact-only/DxLog-split behaviour.
    m_feedTable->setColumnHidden(ColRstRcvd, false);
    m_feedTable->setColumnHidden(ColSerialGridRcvd, false);
    m_feedTable->setColumnHidden(ColExchSent, dxLog);
    m_feedTable->setColumnHidden(ColExchRcvd, true);

    if (dxLog) {
        // DXLog.net's own literal order: QSO# / Band / Zeit / Call /
        // Sent / Nr. / Rcvd / Nr./Grid -- then this widget's own extra
        // km/°/Status columns trailing (see setViewMode()'s doc comment
        // in the header for why those stay rather than being hidden).
        applyColumnOrder({ColSerial, ColBand, ColTime, ColCall, ColRstSent, ColSerialSent, ColRstRcvd,
                           ColSerialGridRcvd, ColKm, ColDeg, ColStatus, ColExchSent, ColExchRcvd});
    } else {
        // Compact's own order, with the received exchange now split the
        // same way DxLogFullColumns always showed it (ColRstRcvd/
        // ColSerialGridRcvd where the old combined ColExchRcvd used to
        // sit) -- ColExchRcvd trails, hidden, alongside the DXLog-only
        // Sent-side split columns Compact still doesn't use.
        applyColumnOrder({ColSerial, ColTime, ColCall, ColExchSent, ColRstRcvd, ColSerialGridRcvd, ColKm, ColDeg,
                           ColStatus, ColBand, ColRstSent, ColSerialSent, ColExchRcvd});
    }

    // The entry row is sized off these same columns (see
    // rebuildEntryRowLayout()'s own comment) -- resync it only on a
    // real mode change (see needsEntryRowRebuild's own comment above),
    // re-running the exchange sub-fields at their new group width
    // (preserving whatever the operator already typed) if any are
    // already built.
    if (needsEntryRowRebuild) {
        rebuildEntryRowLayout();
        if (!m_exchangeFields.isEmpty()) {
            rebuildExchangeCell(exchangeReceived());
        }
        m_entryRowLayoutBuilt = true;
    }

    // The visible column set just (potentially) changed -- "other
    // visible width" for Status's own fill (syncStatusColumnWidth())
    // depends on it, so resync now rather than waiting for the next
    // panel resize.
    syncStatusColumnWidth();
}

int UnifiedLogWidget::rcvdGroupWidth() const
{
    // m_feedTable's received-exchange columns are ALWAYS the split
    // ColRstRcvd/ColSerialGridRcvd pair now, in BOTH view modes (see
    // setViewMode()'s own comment on always showing RST separately from
    // Nr./Grid) -- no more DxLogFullColumns-only branch here. The entry
    // row's own RST/Serial/Grid sub-fields size off this same total, or
    // the two stop lining up.
    return kColumnWidths[ColRstRcvd] + kColumnWidths[ColSerialGridRcvd];
}

void UnifiedLogWidget::rebuildEntryRowLayout()
{
    // Detach the persistent value widgets -- removeWidget() only
    // detaches (a safe no-op if the widget is not currently in the
    // layout, e.g. this method's very first call), their own state
    // (text, focus, signals) survives untouched.
    m_entryRowLayout->removeWidget(m_callsignEdit);
    m_entryRowLayout->removeWidget(m_exchangeFieldsHost);
    m_entryRowLayout->removeWidget(m_sentExchangeLabel);
    m_entryRowLayout->removeWidget(m_statusPillLabel);
    m_entryRowLayout->removeWidget(m_entryTimeLabel);
    m_entryRowLayout->removeWidget(m_entryKmLabel);
    m_entryRowLayout->removeWidget(m_entryDegLabel);

    // Delete every blank placeholder cell this method built last time --
    // the only kind of item still left in the layout at this point.
    QLayoutItem* item;
    while ((item = m_entryRowLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    const bool dxLog = (m_viewMode == ContestSettings::LogViewMode::DxLogFullColumns);

    // Every entry-row cell gets this SAME fixed height -- m_feedTable's
    // own default row height (defaultSectionSize(), which is defined
    // even with zero rows loaded, unlike rowHeight(0)). Operator,
    // 2026-09-11, pointing at a live screenshot: "die zeile mit 19.00
    // ist sehr klein, die darüber liegende viel größer" -- before this
    // fix, each cell's height came from its own natural (Qt-computed)
    // sizeHint, which for a frameless QLineEdit/QLabel at kFontSmall
    // collapsed noticeably SHORTER than the table's own real row height
    // -- so once real QSO rows exist above it, the entry row would
    // visibly not match them. Pinning every cell to the table's own
    // row height by construction removes that risk instead of hoping
    // two independent Qt sizeHint computations happen to agree.
    const int rowHeight = m_feedTable->verticalHeader()->defaultSectionSize();

    // Unknown until the QSO is actually logged -- Style::unknownDash(),
    // same convention every other "no data yet" cell in this codebase
    // uses (HAUSSTIL rule 7), not a fabricated value.
    const auto addBlank = [this, rowHeight](int width) {
        auto* label = new QLabel(Style::unknownDash(), m_entryRow);
        label->setFont(Style::monoFont(label->font(), Style::kFontBody));
        label->setStyleSheet(
            QStringLiteral("color: %1; background: transparent; border-right: 1px solid %2; padding: 0 %3px;")
                .arg(Style::kTextPrimary(), Style::kBorder())
                .arg(kEntryRowHPadding));
        label->setFixedSize(width, rowHeight);
        m_entryRowLayout->addWidget(label);
    };

    if (dxLog) {
        // QSO#/Band: not yet assigned/tracked for an in-progress entry
        // either -- blank (Time and Km/° below are different -- see
        // m_entryTimeLabel's/m_entryKmLabel's own doc comments).
        addBlank(kColumnWidths[ColSerial]);
        addBlank(kColumnWidths[ColBand]);
    }
    m_entryTimeLabel->setFixedSize(kColumnWidths[ColTime], rowHeight);
    m_entryRowLayout->addWidget(m_entryTimeLabel);

    m_callsignEdit->setFixedSize(kColumnWidths[ColCall], rowHeight);
    m_entryRowLayout->addWidget(m_callsignEdit);

    const int sentWidth = dxLog ? (kColumnWidths[ColRstSent] + kColumnWidths[ColSerialSent]) : kColumnWidths[ColExchSent];
    m_sentExchangeLabel->setFixedSize(sentWidth, rowHeight);
    m_entryRowLayout->addWidget(m_sentExchangeLabel);

    m_exchangeFieldsHost->setFixedSize(rcvdGroupWidth(), rowHeight);
    m_entryRowLayout->addWidget(m_exchangeFieldsHost);

    m_entryKmLabel->setFixedSize(kColumnWidths[ColKm], rowHeight);
    m_entryRowLayout->addWidget(m_entryKmLabel);
    m_entryDegLabel->setFixedSize(kColumnWidths[ColDeg], rowHeight);
    m_entryRowLayout->addWidget(m_entryDegLabel);

    m_entryRowLayout->addStretch(1);

    m_statusPillLabel->setFixedSize(kColumnWidths[ColStatus], rowHeight);
    m_entryRowLayout->addWidget(m_statusPillLabel);

    // With setWidgetResizable(false), QScrollArea does NOT resize its
    // contained widget to its sizeHint() the way a normal QLayout
    // parent would -- m_entryRow would otherwise stay at whatever
    // (possibly 0x0) size it last had, invisible inside the scroll
    // area's viewport. Height is fixed here (every cell is pinned to
    // `rowHeight`); WIDTH is handled by syncEntryRowWidth() below, since
    // it depends on the panel's current width, not just this layout.
    m_entryRowScroll->setFixedHeight(rowHeight);
    syncEntryRowWidth();
}

void UnifiedLogWidget::syncEntryRowWidth()
{
    // Operator, 2026-09-11, pointing at a widened "Log" panel: "die
    // abstände sind viel zu klein, rechts sind frei felder die nicht
    // gebraucht werden" -- kColumnWidths is a fixed pixel grid, so once
    // the panel is dragged wider than that grid's own total width, both
    // m_feedTable and this row used to just leave the extra width as
    // dead, grid-less space on the right. m_feedTable's own fix is
    // syncStatusColumnWidth() -- the last VISIBLE column (Status, in
    // both view modes, see setViewMode()'s own applyColumnOrder() calls)
    // grows to fill the panel, capped at kStatusColumnMaxWidth (same-day
    // follow-up, once an UNCAPPED fill just moved the "looks like dead
    // space" complaint into a real column: "benötigen wir die letzte
    // spalte im log?"). This mirrors that exact capped behaviour for the
    // entry row's own Status cell, so the two stay column-aligned at any
    // panel width instead of only agreeing up to kColumnWidths' own
    // fixed total.
    //
    // `baseWidth` is "every cell except Status", derived from the
    // status label's OWN current width rather than a separately tracked
    // constant -- self-correcting across repeated calls (Compact vs.
    // DxLogFullColumns already have different base totals via
    // rebuildEntryRowLayout()'s own dxLog branch, so there is no single
    // constant to subtract here anyway).
    const int viewportWidth = m_feedTable->viewport()->width();
    const int baseWidth = m_entryRow->sizeHint().width() - m_statusPillLabel->width();
    const int statusWidth = qBound(kColumnWidths[ColStatus], viewportWidth - baseWidth, kStatusColumnMaxWidth);
    const int rowHeight = m_feedTable->verticalHeader()->defaultSectionSize();
    m_statusPillLabel->setFixedSize(statusWidth, rowHeight);
    m_entryRow->resize(m_entryRow->sizeHint());
}

void UnifiedLogWidget::updateStatusLine()
{
    // LogTableModel keeps its own ascending (oldest-first, ORDER BY id
    // ASC) order (see UnifiedFeedModel::rebuild()'s own comment on this
    // same fact) -- the last row is therefore the most recently logged
    // QSO for ordinary live entry (a bulk historical import could in
    // principle insert out of chronological order, but there is no such
    // import path in this codebase today).
    if (m_logModel && m_logModel->rowCount() > 0) {
        const QsoRecord& last = m_logModel->recordAt(m_logModel->rowCount() - 1);
        const QDateTime loggedAt = QDateTime::fromString(last.timestampUtc, Qt::ISODate);
        const QString timeText = loggedAt.isValid()
            ? loggedAt.time().toString(QStringLiteral("HH:mm:ss")) + QStringLiteral("Z")
            : last.timestampUtc;
        m_lastQsoLabel->setText(QStringLiteral("Letzter QSO: %1  %2").arg(last.callsign, timeText));
    } else {
        // Unknown is a dash, not a blank/zero-QSO placeholder -- HAUSSTIL
        // rule 7. Genuinely true before the first QSO of a fresh contest
        // is logged.
        m_lastQsoLabel->setText(QStringLiteral("Letzter QSO: %1").arg(Style::unknownDash()));
    }
    m_operatingModeLabel->setText(operatingModeStatusText(m_operatingMode));
}

void UnifiedLogWidget::onCallsignTextChanged()
{
    m_callsignLookupTimer->start();
}

void UnifiedLogWidget::onCallsignLookupTimeout()
{
    const QString call = callsign();
    if (!call.isEmpty()) {
        emit callsignLookupRequested(call);
    }
}

void UnifiedLogWidget::handleFeedRowClicked(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }
    if (index.column() == ColStatus) {
        // A History row's Status cell, clicked -- toggle its invalid
        // flag (see UnifiedLogWidget.h's historyInvalidToggleRequested
        // doc comment). historyQsoIdForRow() returns -1 for a
        // Divider/Candidate row's Status cell, so this is a no-op there
        // (those rows carry no pill to click in the first place).
        const int qsoId = m_feedModel->historyQsoIdForRow(index.row());
        if (qsoId >= 0) {
            emit historyInvalidToggleRequested(qsoId);
        }
        return;
    }
    QString callsignValue;
    QString grid;
    qint64 freqHz = 0;
    if (m_feedModel->candidateInfoForRow(index.row(), &callsignValue, &grid, &freqHz)) {
        emit candidateActivated(callsignValue, grid, freqHz);
    }
}

void UnifiedLogWidget::rebuildFeedRows()
{
    applyDividerSpan();
    syncFeedTableHeight();
}

void UnifiedLogWidget::syncFeedTableHeight()
{
    if (m_entryRowPosition != ContestSettings::LogEntryRowPosition::Top) {
        // Bottom mode: hug exactly the table's current row content so
        // the entry row sits directly against the table's own last row
        // -- no trailing blank table area (m_feedTable's default
        // Expanding size policy otherwise claims the whole panel,
        // pushing the entry row all the way to the panel's bottom edge
        // regardless of how few rows are actually logged) -- operator,
        // 2026-09-11: "viel zu weit unten, direkt unterhalb die nächste
        // zeile." Recomputed on every model change (new QSO, new
        // candidate) and on every setEntryRowPosition() call.
        int height = m_feedTable->frameWidth() * 2;
        const int rowCount = m_feedModel->rowCount();
        for (int row = 0; row < rowCount; ++row) {
            height += m_feedTable->rowHeight(row);
        }
        m_feedTable->setMaximumHeight(qMax(height, 0));
        return;
    }
    // Top mode: unconstrained, exactly as before this method existed --
    // the table expands to fill the panel; the entry row sits above it
    // and is unaffected by how much of the panel the table fills.
    m_feedTable->setMaximumHeight(QWIDGETSIZE_MAX);
}

void UnifiedLogWidget::applyDividerSpan()
{
    m_feedTable->clearSpans();
    const int row = m_feedModel->dividerRow();
    if (row >= 0) {
        m_feedTable->setSpan(row, 0, 1, ColCount);
    }
}

void UnifiedLogWidget::setFieldAutoFilled(QLineEdit* field, bool autoFilled)
{
    field->setStyleSheet(flatFieldStyle(autoFilled));
    // A dynamic property, not just the stylesheet, so applyRstDefaults()
    // below can ask "is this field still showing an auto-filled value"
    // later (e.g. from a mode change) without depending on parsing its
    // own stylesheet string back out.
    field->setProperty("autoFilled", autoFilled);
}

bool UnifiedLogWidget::isFieldAutoFilled(const QLineEdit* field)
{
    return field->property("autoFilled").toBool();
}

void UnifiedLogWidget::applyRstDefaults()
{
    for (const ContestDefinition::ExchangeField& field : m_exchangeFields) {
        if (field.type != QStringLiteral("rst")) {
            continue;
        }
        QLineEdit* edit = m_exchangeEditsByKey.value(field.key, nullptr);
        if (!edit) {
            continue;
        }
        // Only ever replaces an empty field or one still showing its
        // own previous auto-filled value -- an operator's own typed RST
        // is never silently overwritten by a later mode change, same
        // "auto-filled, still overridable" convention grid/serial
        // autofill already use (applyKnownExchange() above).
        if (edit->text().isEmpty() || isFieldAutoFilled(edit)) {
            edit->setText(defaultRstForMode(m_currentMode));
            setFieldAutoFilled(edit, true);
        }
    }
}

QLineEdit* UnifiedLogWidget::nextRelevantField(QLineEdit* current) const
{
    QVector<QLineEdit*> relevant;
    relevant.append(m_callsignEdit);
    for (int i = 0; i < m_exchangeEdits.size(); ++i) {
        // DXLog.net: "[Space] to step through relevant entry fields" --
        // RST is excluded, it is "rarely changed" per that same docs
        // page.
        if (i < m_exchangeFields.size() && m_exchangeFields.at(i).type == QStringLiteral("rst")) {
            continue;
        }
        relevant.append(m_exchangeEdits.at(i));
    }
    const int idx = relevant.indexOf(current);
    if (idx < 0 || idx + 1 >= relevant.size()) {
        return nullptr;
    }
    return relevant.at(idx + 1);
}

QLineEdit* UnifiedLogWidget::nextFieldForTab(QLineEdit* current) const
{
    // Was "every field, RST included" per DXLog.net's own documented
    // [Tab] behaviour -- overridden by the operator directly, 2026-09-11
    // ("der cursor spring zu 59, was unwichtig ist, dies sollte
    // übersprungen werden"): RST is skipped here too now, same as
    // [Space]'s own nextRelevantField() -- the value is essentially
    // always "59"/"599" from applyRstDefaults() and only rarely needs a
    // manual override, which is still reachable by clicking directly
    // into the field with the mouse.
    QVector<QLineEdit*> relevant;
    relevant.append(m_callsignEdit);
    for (int i = 0; i < m_exchangeEdits.size(); ++i) {
        if (i < m_exchangeFields.size() && m_exchangeFields.at(i).type == QStringLiteral("rst")) {
            continue;
        }
        relevant.append(m_exchangeEdits.at(i));
    }
    const int idx = relevant.indexOf(current);
    if (idx < 0 || idx + 1 >= relevant.size()) {
        return nullptr;
    }
    return relevant.at(idx + 1);
}

bool UnifiedLogWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Space && keyEvent->modifiers() == Qt::NoModifier) {
            if (auto* current = qobject_cast<QLineEdit*>(watched)) {
                if (QLineEdit* next = nextRelevantField(current)) {
                    next->setFocus();
                    next->selectAll();
                }
                // Consumed unconditionally (even at the last relevant
                // field, where there is nowhere left to go) -- a space
                // character is never meaningful in a callsign, RST,
                // serial or grid square, so there is nothing useful for
                // the QLineEdit itself to do with this keystroke either
                // way.
                return true;
            }
        }
        if (keyEvent->key() == Qt::Key_Tab && keyEvent->modifiers() == Qt::NoModifier) {
            if (auto* current = qobject_cast<QLineEdit*>(watched)) {
                if (QLineEdit* next = nextFieldForTab(current)) {
                    next->setFocus();
                    next->selectAll();
                    return true;
                }
                // At the last field (Grid, normally) -- fall through to
                // Qt's own default Tab handling instead of trapping
                // focus inside the entry row.
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void UnifiedLogWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Guard against the handful of resize events Qt fires while this
    // widget's own children are still being constructed (before
    // setViewMode()'s first call has built the entry row at all).
    if (m_entryRowLayoutBuilt) {
        syncEntryRowWidth();
        syncStatusColumnWidth();
    }
}

} // namespace Contestprogramm

#include "UnifiedLogWidget.moc"
