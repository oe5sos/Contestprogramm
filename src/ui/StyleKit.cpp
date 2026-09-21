#include "ui/StyleKit.h"

#include <QWidget>

#include <algorithm>
#include <array>

namespace Contestprogramm::Style {

namespace {

// One full theme's worth of color values, in the exact declaration
// order the kXxx() functions below read them back in. A plain array of
// hex-string fields (not a named struct) -- adding/reordering a color
// only ever needs touching this one block plus the matching kXxx()
// getter, the same "one place per value" shape the rest of this file
// already follows.
enum ColorIndex {
    IdxAppBg, IdxPanelBg, IdxPanelHeadTop, IdxPanelHeadMid, IdxPanelHeadBot, IdxTitleBorder, IdxBorder, IdxBorderSubtle, IdxButtonBg, IdxButtonHover,
    IdxTextPrimary, IdxTextSecondary, IdxTextTertiary, IdxTextScale, IdxTextInactive,
    IdxBlueBg, IdxBlueBorder, IdxBlueText,
    IdxAmberText, IdxAmberDim, IdxAmberWarn, IdxAmberBg, IdxAmberBorder,
    IdxGreenBg, IdxGreenText, IdxGreenBorder,
    IdxRedBg, IdxRedText, IdxRedBorder,
    IdxBadgeOkBg, IdxBadgeOffBg, IdxBadgeWarnBg,
    IdxInsetBg, IdxInsetBorder, IdxGroove,
    IdxStatusBarBg, IdxStatusBarBorder,
    IdxInstrumentFace, IdxInstrumentGlowHi, IdxInstrumentGlowLo,
    IdxColorCount,
};

using Palette = std::array<const char*, IdxColorCount>;

// Bernstein: the original, still-default palette -- every value here is
// byte-for-byte the same literal this file shipped with before themes
// existed (see StyleKit.h's own class comment for where these came
// from). Gruen (below) is the one alternative; the 2026-09-12/13
// Gelb/Blau/Graphit palettes were removed 2026-09-21 at the operator's
// request ("nur 2 varianten anbieten"), see core/ColorTheme.h.
constexpr Palette kPaletteBernstein = {
    // PanelHeadTop/Mid/Bot/TitleBorder corrected 2026-09-14 to Longpath's
    // exact StyleConstants.h kTitleGradTop/Mid/Bot/kTitleBorder literals
    // (#26262b / #1c1c20 / #141417 / #0d0d0f) -- the earlier #16161a/
    // #111113 two-stop pair was an invented approximation, not a byte-
    // for-byte copy, and Bernstein is specifically the theme meant to be
    // pixel-identical to real Longpath.
    "#08080a", "#0c0c0e", "#26262b", "#1c1c20", "#141417", "#0d0d0f", "#2c2c31", "#1f1f23", "#1a1a1e", "#26262b",
    "#dcdce1", "#a6a6ac", "#909096", "#7e7e85", "#58585e",
    "#3576e0", "#2a5fbe", "#ffffff",
    "#d8a55f", "#6b5630", "#a8853f", "#33280f", "#6b5426",
    "#1c3a2a", "#6fa384", "#2c5c44",
    "#7a2c2e", "#f0dcdc", "#a86b6d",
    "#212b27", "#18181a", "#372c1d",
    "#050507", "#232329", "#1f1f23",
    "#0a0a0c", "#1f1f23",
    "#f2f2ec", "#817b5c", "#47463b",
};

// Gruen -- operator, 2026-09-21: "dieses farbeschema bitte zusätzlich
// einbauen", from a screenshot of a dark web page he sent as the
// example (charcoal page, lighter chips, near-white text, ONE green
// accent for the day bar/route line/legend, amber only for the small
// pass markers and the "ZUGABE" callout). Values sampled from that
// screenshot, not invented: page #1c2024, chips #272f35, rule
// #343a3f, text #e6ebe9 / #a6acae / #757b7d, green #309a68, green
// area fill #243c34, amber #c0881c. Role mapping: the "amber"
// (measured/highlight) slots carry the green -- that is what makes
// the header accent bar, the readings and the rate tiles look like
// his page -- while kAmberWarn keeps a real amber so a warning still
// reads as a warning; blue stays blue for the interactive/commanded
// family so a commanded rotor target and a worked station never share
// a hue; kGreenText (OK/worked) is a lighter mint than the accent
// green for the same reason. Insets are LIGHTER than the panel here
// (his chips are raised cards), the opposite of Bernstein's recessed
// #050507 -- deliberate, it is the page's own layering.
constexpr Palette kPaletteGruen = {
    "#16191c", "#1c2024", "#2a3238", "#222a2f", "#1c2327", "#121618", "#343a3f", "#2b3237", "#262e33", "#313a40",
    "#e6ebe9", "#a8aeb0", "#8b9295", "#757b7d", "#555c60",
    "#3f86e0", "#2f6cc0", "#ffffff",
    "#38ac74", "#256a4a", "#c9901f", "#243c34", "#2f6b4f",
    "#1e4a38", "#86cfa6", "#2f7a58",
    "#6e2a2c", "#f2dcdc", "#a8666a",
    "#1f3a30", "#1e2428", "#3a2e14",
    "#262e33", "#2f373c", "#262d32",
    "#16191c", "#2b3237",
    "#eef3f0", "#6f8a7c", "#3a4a42",
};

const Palette& paletteFor(ColorTheme theme)
{
    switch (theme) {
    case ColorTheme::Gruen:
        return kPaletteGruen;
    case ColorTheme::Bernstein:
        break;
    }
    return kPaletteBernstein;
}

// The one piece of mutable global state this file has -- deliberately
// a plain variable, not a QSettings-backed or signal-emitting object:
// MainWindow owns persistence (ContestSettings::colorTheme) and live-
// repaint (applyColorTheme()) already, so this only needs to be "the
// value every kXxx() call reads right now."
ColorTheme g_activeTheme = ColorTheme::Bernstein;

QString colorAt(ColorIndex idx)
{
    return QString::fromLatin1(paletteFor(g_activeTheme)[idx]);
}

} // namespace

void setActiveTheme(ColorTheme theme)
{
    g_activeTheme = theme;
}

ColorTheme activeTheme()
{
    return g_activeTheme;
}

QString kAppBg() { return colorAt(IdxAppBg); }
QString kPanelBg() { return colorAt(IdxPanelBg); }
QString kPanelHeadTop() { return colorAt(IdxPanelHeadTop); }
QString kPanelHeadMid() { return colorAt(IdxPanelHeadMid); }
QString kPanelHeadBot() { return colorAt(IdxPanelHeadBot); }
QString kTitleBorder() { return colorAt(IdxTitleBorder); }
QString kBorder() { return colorAt(IdxBorder); }
QString kBorderSubtle() { return colorAt(IdxBorderSubtle); }
QString kButtonBg() { return colorAt(IdxButtonBg); }
QString kButtonHover() { return colorAt(IdxButtonHover); }

QString kTextPrimary() { return colorAt(IdxTextPrimary); }
QString kTextSecondary() { return colorAt(IdxTextSecondary); }
QString kTextTertiary() { return colorAt(IdxTextTertiary); }
QString kTextScale() { return colorAt(IdxTextScale); }
QString kTextInactive() { return colorAt(IdxTextInactive); }

QString kBlueBg() { return colorAt(IdxBlueBg); }
QString kBlueBorder() { return colorAt(IdxBlueBorder); }
QString kBlueText() { return colorAt(IdxBlueText); }

QString kAmberText() { return colorAt(IdxAmberText); }
QString kAmberDim() { return colorAt(IdxAmberDim); }
QString kAmberWarn() { return colorAt(IdxAmberWarn); }
QString kAmberBg() { return colorAt(IdxAmberBg); }
QString kAmberBorder() { return colorAt(IdxAmberBorder); }

QString kGreenBg() { return colorAt(IdxGreenBg); }
QString kGreenText() { return colorAt(IdxGreenText); }
QString kGreenBorder() { return colorAt(IdxGreenBorder); }

QString kRedBg() { return colorAt(IdxRedBg); }
QString kRedText() { return colorAt(IdxRedText); }
QString kRedBorder() { return colorAt(IdxRedBorder); }

QString kBadgeOkBg() { return colorAt(IdxBadgeOkBg); }
QString kBadgeOffBg() { return colorAt(IdxBadgeOffBg); }
QString kBadgeWarnBg() { return colorAt(IdxBadgeWarnBg); }

QString kInsetBg() { return colorAt(IdxInsetBg); }
QString kInsetBorder() { return colorAt(IdxInsetBorder); }
QString kGroove() { return colorAt(IdxGroove); }

QString kStatusBarBg() { return colorAt(IdxStatusBarBg); }
QString kStatusBarBorder() { return colorAt(IdxStatusBarBorder); }

QString kInstrumentFace() { return colorAt(IdxInstrumentFace); }
QString kInstrumentGlowHi() { return colorAt(IdxInstrumentGlowHi); }
QString kInstrumentGlowLo() { return colorAt(IdxInstrumentGlowLo); }

QFont capsFont(const QFont& base, int px)
{
    QFont f = base;
    f.setPixelSize(px);
    f.setWeight(QFont::DemiBold);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::AbsoluteSpacing, px * kCapsTracking);
    return f;
}

QFont monoFont(const QFont& base, int px, QFont::Weight weight)
{
    QFont f = base;
    f.setPixelSize(px);
    f.setWeight(weight);
    // SF Mono first, Menlo as fallback -- matches the approved mockup's
    // own CSS exactly (log-final-mockup/Main.dc.html: --mono:"SF Mono",
    // "Menlo",monospace), which this codebase had drifted from (plain
    // "Menlo" only). SF Mono is Apple's own tighter/narrower monospace
    // (macOS system font since El Capitan, what Xcode/Terminal use by
    // default at small sizes) -- operator, 2026-09-11, on a cramped
    // 6-character grid-square field: "der raster schneidet fast die
    // buchstaben ab ... vielleicht auch eine andere font". Menlo stays
    // as the fallback for the rare system without SF Mono available.
    //
    // Windows and Linux have neither (CI, 2026-09-21: the rotor readout's
    // three columns no longer fit at any width there, Qt had fallen back
    // to a wide proportional face): Consolas/Cascadia on Windows, the
    // DejaVu/Liberation pair on Linux, all of them 0.55-0.6 em wide like
    // Menlo, so the readout's measured column budgets (RotorWidget.cpp,
    // kReadoutMinWidth) hold on every platform.
    f.setFamilies({QStringLiteral("SF Mono"), QStringLiteral("Menlo"), QStringLiteral("Consolas"),
                   QStringLiteral("Cascadia Mono"), QStringLiteral("DejaVu Sans Mono"),
                   QStringLiteral("Liberation Mono"), QStringLiteral("Courier New")});
    f.setStyleHint(QFont::Monospace);
    f.setFixedPitch(true);
    return f;
}

QString unknownDash()
{
    return QStringLiteral("——");
}

QString shiftL(const QColor& base, int deltaL)
{
    QColor c = base.toHsl();
    const int l = std::clamp(c.lightness() + deltaL, 0, 255);
    c.setHsl(c.hslHue(), c.hslSaturation(), l, c.alpha());
    return c.name();
}

QString raisedFill(const QString& baseHex, int lift, int drop)
{
    const QColor base = QColor(baseHex);
    return QStringLiteral(
        "qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 %1, stop:0.55 %2, stop:1 %3)")
        .arg(shiftL(base, lift), base.name(), shiftL(base, -drop));
}

QString sunkenFill(const QString& baseHex, int deepen, int lift)
{
    const QColor base = QColor(baseHex);
    return QStringLiteral(
        "qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 %1, stop:1 %2)")
        .arg(shiftL(base, -deepen), shiftL(base, lift));
}

QString appStyleSheet()
{
    // Colours/borders/radii only -- see the note on capsFont()/
    // monoFont() in StyleKit.h for why no rule here carries font-size.
    return QStringLiteral(
        "QWidget { background: %1; color: %2; }"
        "QMainWindow { background: %1; }"
        "QMainWindow::separator { background: %3; width: 6px; height: 6px; }"
        "QToolTip { background: %4; color: %2; border: 1px solid %3; padding: 4px; }"

        "QGroupBox { background: %4; border: 1px solid %3; border-radius: %10px;"
        "  margin-top: 10px; padding-top: 14px; color: %5; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"

        "QLabel { background: transparent; color: %2; }"

        "QLineEdit, QSpinBox, QDoubleSpinBox {"
        "  background: %6; border: 1px solid %7; border-radius: %11px;"
        "  color: %2; padding: 3px 6px; selection-background-color: %8;"
        "  selection-color: %9; }"
        "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus { border: 1px solid %8; }"
        "QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled { color: %12; }"

        "QComboBox { background: %4; border: 1px solid %3; border-radius: %11px;"
        "  color: %2; padding: 3px 6px; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView { background: %4; color: %2;"
        "  selection-background-color: %8; selection-color: %9; }"

        "QCheckBox { color: %2; spacing: 6px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; background: %6;"
        "  border: 1px solid %7; border-radius: 3px; }"
        "QCheckBox::indicator:checked { background: %8; border-color: %8; }"

        "QPushButton { background: %13; border: 1px solid %3; border-radius: %11px;"
        "  color: %2; padding: 4px 12px; }"
        "QPushButton:hover { background: %14; }"
        "QPushButton:pressed { background: %15; }"
        "QPushButton:checked { background: %16; border-color: %17; color: %9; }"
        "QPushButton:disabled { color: %12; }"

        "QTableView, QTableWidget { background: %4; alternate-background-color: %18;"
        "  color: %2; gridline-color: %3; border: 1px solid %3; border-radius: %10px;"
        "  selection-background-color: %8; selection-color: %9; }"
        "QHeaderView::section { background: %4; color: %5; border: none;"
        "  border-bottom: 1px solid %3; border-right: 1px solid %3; padding: 4px 6px; }"

        "QScrollBar:vertical { background: %1; width: 12px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %7; border-radius: 5px; min-height: 24px; }"
        "QScrollBar::handle:vertical:hover { background: %3; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { background: %1; height: 12px; margin: 0; }"
        "QScrollBar::handle:horizontal { background: %7; border-radius: 5px; min-width: 24px; }"
        "QScrollBar::handle:horizontal:hover { background: %3; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"

        "QMenuBar { background: %1; color: %2; border-bottom: 1px solid %3; }"
        "QMenuBar::item { background: transparent; padding: 4px 10px; }"
        "QMenuBar::item:selected { background: %14; }"
        "QMenu { background: %4; color: %2; border: 1px solid %3; }"
        "QMenu::item { padding: 4px 20px; }"
        "QMenu::item:selected { background: %8; color: %9; }"

        "QStatusBar { background: %19; border-top: 1px solid %3; color: %5; }"
        "QStatusBar::item { border: none; }"

        "QDialog { background: %1; color: %2; }"
        )
        // Chained single-value arg() calls, numbered contiguously %1..%19
        // with no gaps: QString::arg() always replaces the lowest-
        // numbered *remaining* placeholder, not the number written in
        // this comment, so a skipped number here would silently shift
        // every argument after it onto the wrong placeholder.
        .arg(kAppBg())                             // %1
        .arg(kTextPrimary())                       // %2
        .arg(kBorder())                             // %3
        .arg(kPanelBg())                            // %4
        .arg(kTextSecondary())                      // %5
        .arg(kInsetBg())                            // %6
        .arg(kInsetBorder())                        // %7
        .arg(kBlueBg())                             // %8
        .arg(kBlueText())                           // %9
        .arg(QString::number(kPanelRadius))        // %10
        .arg(QString::number(kRadius))             // %11
        .arg(kTextInactive())                       // %12
        .arg(raisedFill(kButtonBg()))               // %13
        .arg(raisedFill(kButtonHover(), 20, 10))    // %14
        .arg(sunkenFill(kButtonBg()))               // %15
        .arg(raisedFill(kBlueBg(), 18, 14))         // %16
        .arg(kBlueBorder())                         // %17
        .arg(shiftL(QColor(kPanelBg()), 2))        // %18 -- alternate row tint, a hair lighter than the panel
                                                    // itself (was a hardcoded "#0e0e10" literal -- correct only
                                                    // for Bernstein; found live 2026-09-12 rendering as a near-
                                                    // opaque black stripe with unreadable text under Gelb Hell,
                                                    // since a fixed near-black tint has nothing to do with a
                                                    // light theme's own panel colour)
        .arg(kStatusBarBg());                       // %19
}

void applyPanelFrameStyle(QWidget* widget)
{
    // Historical shared name -- see the two-argument overload below for
    // why a dockable panel calls that one directly with its own id
    // instead of going through this delegate.
    applyPanelFrameStyle(widget, QStringLiteral("contestPanelFrame"));
}

void applyPanelFrameStyle(QWidget* widget, const QString& objectName)
{
    if (!widget) {
        return;
    }
    // A shared-per-call objectName used purely as a QSS ID-selector hook
    // (Qt has no "class" attribute the way CSS does) -- every panel
    // wrapped this way gets the identical rule, scoped to exactly the
    // widget it's set on. See the declaration in StyleKit.h for why a
    // bare class selector doesn't work here.
    widget->setObjectName(objectName);
    // A plain QWidget's default paintEvent() does not draw its own
    // stylesheet's background/border at all (Qt requires this
    // attribute for that; QFrame and other widgets with their own
    // paintEvent() already do it, but setting the attribute on those
    // too is harmless) -- without it the "panel" background/border
    // below would silently never appear.
    widget->setAttribute(Qt::WA_StyledBackground, true);
    widget->setStyleSheet(QStringLiteral(
        "QWidget#%1 { background: %2; border: 1px solid %3; border-radius: %4px; }")
        .arg(objectName, kPanelBg(), kBorderSubtle())
        .arg(kPanelRadius));
}

QString badgeStyle(bool ok)
{
    return QStringLiteral(
        "QLabel { background: %1; color: %2; border-radius: %3px;"
        " padding: 2px 8px; }")
        .arg(ok ? kBadgeOkBg() : kBadgeOffBg(),
             ok ? kGreenText() : kTextInactive())
        .arg(kRadius - 1);
}

QString iconButtonStyle()
{
    // Ported verbatim (colour roles, not literal hex) from the real
    // Longpath GridCellWidget::buildCellButtons() `btnCss`
    // (~/Longpath/NereusSDR/src/gui/applets/GridCellWidget.cpp) -- a
    // flat, borderless button that only shows itself on hover, used
    // there for the detach/close/⚙ row. kTextScale/kButtonHover/
    // kTextPrimary are this codebase's own names for the same three
    // roles that precedent uses (kTextScale/kButtonHover/kTextPrimary
    // there too).
    return QStringLiteral(
        "QPushButton { background: transparent; border: none;"
        " color: %1; padding: 0; }"
        "QPushButton:hover { background: %2; color: %3; border-radius: 3px; }")
        .arg(kTextScale(), kButtonHover(), kTextPrimary());
}

QString lockBadgeStyle(bool locked)
{
    return QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: %4px; padding: 0px 4px; }"
        "QPushButton:hover { background: %5; }")
        .arg(locked ? kBadgeWarnBg() : kBadgeOffBg(),
             locked ? kAmberText() : kTextInactive(),
             locked ? kAmberBorder() : kBorderSubtle())
        .arg(kRadius - 1)
        .arg(locked ? kAmberDim() : kButtonHover());
}

} // namespace Contestprogramm::Style
