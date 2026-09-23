#pragma once

#include "core/ColorTheme.h"

#include <QColor>
#include <QFont>
#include <QString>
#include <QVector>

class QWidget;

namespace Contestprogramm::Style {

// Palette, font ladder and helper functions ported verbatim from the
// real Longpath source (~/Longpath/NereusSDR/src/gui/StyleConstants.h,
// docs/design/HAUSSTIL.md) -- Contestprogramm is a standalone sibling
// project (its own repo, its own CMake), so these values are copied
// rather than #included across repos. Every Bernstein hex value below
// is the exact Longpath value, not a re-guess; the reasoning for each
// one (why this grey, why this amber) lives in Longpath's own
// StyleConstants.h and is not repeated here.
//
// Selectable colour themes (see core/ColorTheme.h for the ColorTheme
// enum itself and why it lives there, not here) -- every Style::kXxx()
// color function below reads from whichever theme is currently active
// (setActiveTheme()), so this is a genuine runtime switch, not just a
// compile-time palette. The font ladder/radius/tracking constants
// further down are NOT theme-dependent -- sizing and spacing are the
// same across every theme, only colour changes.

// Switches every color function below to `theme`'s palette, effective
// immediately for any code that calls them from this point on. Does
// NOT itself repaint anything already on screen -- see MainWindow::
// applyColorTheme() for the "make the already-built UI actually show
// it" half (re-applies appStyleSheet() to qApp, then update()s the
// handful of custom-painted widgets that read these colors directly in
// their own paintEvent() rather than through a stylesheet, since a
// stylesheet repolish alone does not reach those).
void setActiveTheme(ColorTheme theme);
ColorTheme activeTheme();

// ── Surfaces ────────────────────────────────────────────────────────
QString kAppBg();
QString kPanelBg();
QString kPanelHeadTop();
QString kPanelHeadMid();
QString kPanelHeadBot();
// The panel-header's own bottom rule, distinct from kBorderSubtle --
// Longpath's titleBarStyle() gives its title chrome a darker seam
// (kTitleBorder, #0d0d0f) than the ordinary panel border, because the
// header sits AUFGESETZT (raised) above the panel body and needs a
// harder edge where it meets it. Reusing kBorderSubtle there was an
// approximation; this is the exact matching role.
QString kTitleBorder();
QString kBorder();
QString kBorderSubtle();
QString kButtonBg();
QString kButtonHover();

// ── Text ────────────────────────────────────────────────────────────
QString kTextPrimary();
QString kTextSecondary();
QString kTextTertiary();
QString kTextScale();
QString kTextInactive();

// ── Meaning: blue = interactive/commanded, warm = measured ─────────
QString kBlueBg();
QString kBlueBorder();
QString kBlueText();

QString kAmberText();

// Der Farbton für ein Band in der Bandzelle des Logs -- leer für ein
// Band ohne eigenen Ton, dann bleibt der normale Text.
//
// Die Töne sind NICHT fest verdrahtet, sondern hängen am Akzentton des
// gewählten Farbthemas: in Bernstein läuft die Reihe von Bernstein
// aus, in Grün von Grün aus, und ein späteres Thema bringt seine eigene
// Reihe mit, ohne dass hier etwas nachgezogen werden muss (Martin,
// 2026-09-23: "man sollte auch die farbe und das design einfach
// umschalten können"). Sättigung und Helligkeit kommen ebenfalls aus
// dem Thema, damit die Zelle nicht im einen Thema leuchtet und im
// anderen murmelt.
QString bandTint(const QString& band);
QString kAmberDim();
QString kAmberWarn();
QString kAmberBg();
QString kAmberBorder();

QString kGreenBg();
QString kGreenText();
QString kGreenBorder();

// Genuine warning red. Not kTxRed (Longpath's stronger #c25a5c) -- that
// one is reserved for MOX/TX in Longpath and has no equivalent state
// here.
QString kRedBg();
QString kRedText();
QString kRedBorder();

// Named badge grund/text pairs (StyleConstants.h kBadge*Bg) rather than
// an invented opacity over an unknown background.
QString kBadgeOkBg();
QString kBadgeOffBg();
QString kBadgeWarnBg();

// Versenkt (sunken) fields -- deliberately darker than the panel, not
// the same value as kAppBg (see StyleConstants.h's kInsetBg note).
QString kInsetBg();
QString kInsetBorder();
QString kGroove();

// Status bar, per StyleConstants.h -- a shade darker than the panel,
// not the same value as kAppBg (two different roles that happen to
// look close).
QString kStatusBarBg();
QString kStatusBarBorder();

// Instrument needles/ticks: cream-white (or, in a light theme, near-
// black) -- never the theme's own accent colour, per HAUSSTIL "Zeiger
// und Teilung cremeweiss -- nie farbig", generalized here to "always
// the theme's own face/ink colour, never its accent".
QString kInstrumentFace();
QString kInstrumentGlowHi();
QString kInstrumentGlowLo();

// ── Six-step font ladder (StyleConstants.h) ─────────────────────────
constexpr int kFontCaption = 9;
constexpr int kFontSmall   = 11;
constexpr int kFontBody    = 13;
constexpr int kFontSub     = 16;
constexpr int kFontReading = 22;
constexpr int kFontDisplay = 38;

// Letter-tracking for a caps line, as a fraction of the pixel size.
// HAUSSTIL.md: ".18em". MUST be applied via QFont::setLetterSpacing --
// Qt stylesheets silently drop letter-spacing (see capsFont() below and
// the "Falle" note in HAUSSTIL.md).
constexpr double kCapsTracking = 0.18;

// "Nie Radius 3" -- HAUSSTIL.md. Qt's own default is 3px and reads as
// unstyled the moment it appears next to anything deliberately chosen.
constexpr int kRadius      = 7;
constexpr int kPanelRadius = 8;

// A caps label: small, demi-bold, all-uppercase (via QFont
// capitalization, not by rewriting the string), tracked. Every group
// label, panel title and column header in this pass goes through this
// helper rather than an ad-hoc font-size in a stylesheet -- see
// HAUSSTIL.md's "Die Falle, die einen Monat Typografie unsichtbar
// gemacht hat": a font-size set via setStyleSheet() on a *container*
// cascades to every descendant and silently wins over their own
// setFont() calls. Sizing in this pass therefore always goes through
// QFont (this function or monoFont()), never through a stylesheet.
QFont capsFont(const QFont& base, int px = kFontCaption);

// A value that changes and gets compared -- HAUSSTIL rule 4. Time,
// frequency, distance, bearing, rate, callsign.
QFont monoFont(const QFont& base, int px, QFont::Weight weight = QFont::Normal);

// Dieselbe Familienliste als CSS-Wert, fuer die wenigen Stellen, die
// eine Schrift in Rich Text setzen muessen und deshalb kein QFont
// setzen koennen. Nie eine einzelne Familie dort hinschreiben: "Menlo"
// allein gibt es auf Windows und Linux nicht.
QString monoFontFamilyCss();

// "Unbekannt ist ein Strich, keine Null" -- HAUSSTIL rule 7. Two
// em dashes, matching the design mockups this pass follows.
QString unknownDash();

// AUFGESETZT (raised) / VERSENKT (sunken) vertical gradients, computed
// from a base colour rather than typed as a second, driftable literal
// -- see StyleConstants.h's raisedFill()/sunkenFill() for the full
// reasoning (an added-lightness step on a near-black ground, not a
// multiplicative lighter()).
QString shiftL(const QColor& base, int deltaL);
// `baseHex` takes the QString a Style::kXxx() color function now
// returns directly (was `const char*`, back when those were compile-
// time literals).
QString raisedFill(const QString& baseHex, int lift = 16, int drop = 12);
QString sunkenFill(const QString& baseHex, int deepen = 10, int lift = 12);

// The app-wide base stylesheet: palette + borders + radii only. No
// font-size anywhere in here -- see capsFont()/monoFont() above for
// why sizing never goes through a stylesheet in this codebase.
QString appStyleSheet();

// A plain bordered, rounded panel with no header bar -- matches the
// design mockups' bare `.panel` (entry bar, rate meter row).
//
// Sets `widget`'s objectName and stylesheet together, using an ID
// selector (`QWidget#<name> { ... }`) rather than a bare class selector
// (`QWidget { ... }` / `QFrame { ... }`): a class selector set via
// setStyleSheet() on a container either doesn't match at all (if the
// container isn't literally a QFrame) or, worse, cascades to every
// child QWidget inside it (background/border/radius included) --
// giving every label inside its own little bordered box. An ID
// selector matches only the one widget carrying that exact
// objectName, which is what "wrap this one container in a panel"
// actually means.
void applyPanelFrameStyle(QWidget* widget);

// Same rule, bound to a caller-chosen objectName instead of the shared
// "contestPanelFrame" the no-argument overload above hardcodes. Added
// for PanelContainerWidget (dockable-panel wave): a docked panel's
// objectName IS its panel id (e.g. "cwMacroRow", kept for
// findChild<QWidget*>() test lookups that predate docking), so it
// cannot also be overwritten to the shared frame name -- Qt's ID
// selectors are matched against a widget's *current* objectName each
// repaint, not frozen at setStyleSheet()-call time, so setting the
// style first and renaming the widget after would silently detach the
// rule. Calling this with the panel's own id up front avoids that
// order-of-operations trap entirely; the no-arg overload just delegates
// here with the historical shared name.
void applyPanelFrameStyle(QWidget* widget, const QString& objectName);

// Small connect/disconnected badge pill: kBadgeOkBg/kGreenText when
// `ok`, kBadgeOffBg/kTextInactive otherwise -- the named pairs from
// StyleConstants.h, not an invented colour.
QString badgeStyle(bool ok);

// Small transparent icon button -- text colour kTextScale, no border,
// a soft kButtonHover/kTextPrimary highlight on hover, per the real
// Longpath GridCellWidget::buildCellButtons() precedent
// (~/Longpath/NereusSDR/src/gui/applets/GridCellWidget.cpp, its
// `btnCss`), which uses exactly this look for its detach/close/⚙
// row. Shared by PanelHeaderBar's and RotorWidget's own ⚙ options
// affordance so both match that precedent and each other.
QString iconButtonStyle();

// Lock-state capsule for dockable panels (PanelContainerWidget), per
// HAUSSTIL rule 5 ("Zustand steht als umrandete Kapsel"). Deliberately
// NOT badgeStyle()'s green/grey connect pair -- a locked panel is a
// restriction the operator chose, not a healthy-link indicator -- so
// this pairs kBadgeWarnBg/kAmberText (locked) with kBadgeOffBg/
// kTextInactive (unlocked), the same named-pair convention badgeStyle()
// already established, plus a thin border (kAmberBorder/kBorderSubtle)
// so the capsule actually reads as "umrandet" rather than a flat fill.
QString lockBadgeStyle(bool locked);

} // namespace Contestprogramm::Style
