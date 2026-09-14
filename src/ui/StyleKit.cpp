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
// from). GelbHell/GelbDunkel/BlauHell/BlauDunkel: operator-approved
// 2026-09-12 ("DIE 4 BITTE") after a live side-by-side mockup review --
// each derived from a small set of hand-picked seed colors (background,
// panel, text, the three semantic accents) with the remaining
// structural roles (button/badge/inset/glow shades) computed from those
// seeds using the same lightness-shift relationships Bernstein's own
// values already exhibit, so every theme stays internally consistent
// the way Bernstein already was, not just individually pretty.
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

constexpr Palette kPaletteGelbHell = {
    // Mid/TitleBorder added 2026-09-14 -- interpolated between this
    // theme's existing (operator-approved) Top/Bot rather than reusing
    // Bernstein's Longpath literals, which would be the wrong hue here.
    "#e8d24a", "#f4ecac", "#ecdd7c", "#e4d26e", "#e0cc5c", "#dac13a", "#b89a2e", "#cdb64a", "#eae2a2", "#e0d898",
    "#171408", "#3a3214", "#483f19", "#5c5220", "#8a7c40",
    "#2a5fc4", "#1a4a9c", "#f4f0e4",
    "#8a4a10", "#c48a34", "#6b3a0c", "#f0c888", "#c48a34",
    "#cfe0b0", "#2a5c34", "#7ba05c",
    "#e8ac98", "#4c1810", "#8a2418",
    "#cbe2bf", "#eae2a2", "#f1c78e",
    "#f0da52", "#d1ba4e", "#cdb64a",
    "#d8c030", "#cdb64a",
    "#171408", "#756a26", "#a99936",
};

constexpr Palette kPaletteGelbDunkel = {
    "#080704", "#0e0c08", "#181408", "#141107", "#100e06", "#0b0906", "#3a3212", "#221e0c", "#181612", "#22201c",
    "#ede0b0", "#b8ac78", "#a69a68", "#8a8050", "#5c5432",
    "#3576e0", "#2a5fbe", "#f8f4e0",
    "#f0d040", "#6b5a18", "#c0a428", "#332a08", "#6b5a18",
    "#2a3a1c", "#8fae5a", "#4c5c2c",
    "#7a2c22", "#f0d8ce", "#d66a54",
    "#1e2814", "#13110d", "#282107",
    "#050401", "#262210", "#221e0c",
    "#060502", "#221e0c",
    "#f4ecc0", "#8a856b", "#4f4c3c",
};

constexpr Palette kPaletteBlauHell = {
    "#c8dcf0", "#e8f0fa", "#d4e4f4", "#cbdfef", "#c0d8ec", "#afcee7", "#7ca4cc", "#a8c4e0", "#dee6f0", "#d4dce6",
    "#101820", "#30404c", "#3e505e", "#546878", "#8098a8",
    "#1a4a9c", "#123a7c", "#e8f0fa",
    "#a05a10", "#c89848", "#7c440c", "#f0d4a0", "#c89848",
    "#c0e0cc", "#245c40", "#6ca884",
    "#f0b8b0", "#4c1410", "#a02824",
    "#bfe1c7", "#dee6f0", "#f4d398",
    "#d0e4f8", "#acc8e4", "#a8c4e0",
    "#a8c8e8", "#a8c4e0",
    "#101820", "#63707e", "#91a1b2",
};

constexpr Palette kPaletteBlauDunkel = {
    "#050a10", "#081018", "#101c28", "#0e1924", "#0c1620", "#0d1823", "#243c50", "#182a38", "#121a22", "#1c242c",
    "#d8e6f2", "#9cb4c8", "#89a1b5", "#6c8598", "#465c6c",
    "#4a8fe8", "#3a76c4", "#eef6fc",
    "#5fc4e0", "#245868", "#3f96ac", "#0c2a33", "#245868",
    "#1c3a2f", "#6fa384", "#2c5c4c",
    "#7a2c2e", "#f0dcda", "#d6746b",
    "#142924", "#0d151d", "#0a222a",
    "#02070d", "#1c2e3c", "#182a38",
    "#030608", "#182a38",
    "#e8f2f7", "#828a8f", "#495055",
};

// Graphit -- operator, 2026-09-13: "mache c", the Design-C ("Fokus-
// Karte") mockup direction made real. Unlike Gelb/Blau (Bernstein's own
// hue with a different background lightness), Graphit reassigns what
// the "blue" and "amber" slots MEAN: blue -> cyan (still the
// interactive/touchable family HAUSSTIL describes, just this theme's
// hue for it), amber -> coral (still the warm/measured-highlight
// family, again a different hue). Green/red keep their usual role,
// re-tuned for contrast against this darker graphite background, the
// same way every other theme here already re-tunes them rather than
// reusing Bernstein's literal values.
constexpr Palette kPaletteGraphit = {
    "#0a0e12", "#10151a", "#171e24", "#151a20", "#12171c", "#13181d", "#232b32", "#1a2026", "#161c22", "#1f272e",
    "#e4eef2", "#9fb0b8", "#7d8f98", "#63747c", "#4a5960",
    "#082226", "#0f4a52", "#e8fbff",
    "#ef6b8e", "#5c2c3a", "#a84a63", "#2c1620", "#8a4358",
    "#16261e", "#7fd9a8", "#2f6b4a",
    "#3a1620", "#ffd6de", "#b25468",
    "#182620", "#14181c", "#2e1a22",
    "#050708", "#1e262c", "#1a2026",
    "#0d1216", "#1a2026",
    "#eef5f7", "#6fd7e8", "#2c3a40",
};

const Palette& paletteFor(ColorTheme theme)
{
    switch (theme) {
    case ColorTheme::GelbHell:
        return kPaletteGelbHell;
    case ColorTheme::GelbDunkel:
        return kPaletteGelbDunkel;
    case ColorTheme::BlauHell:
        return kPaletteBlauHell;
    case ColorTheme::BlauDunkel:
        return kPaletteBlauDunkel;
    case ColorTheme::Graphit:
        return kPaletteGraphit;
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
    f.setFamilies({QStringLiteral("SF Mono"), QStringLiteral("Menlo")});
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
