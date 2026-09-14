#pragma once

#include <QString>
#include <QVector>

namespace Contestprogramm {

// Selectable UI colour theme -- operator, 2026-09-12: "gelber
// Hintergrund, schwarze Schrift, ev auch was in blau, hell und
// dunkel... DIE 4 BITTE", after a live side-by-side mockup review.
// Bernstein (the original amber-on-near-black look) stays the default.
//
// Lives in its own core/ header, not nested inside app/ContestSettings.h
// or declared in ui/StyleKit.h, so both can use the same unqualified
// `ColorTheme` type without either layer including the other -- the
// same reasoning core/RotorDialStyle.h already documents for itself
// (this codebase's app/core/data layer never includes from ui/). The
// actual colour values live in ui/StyleKit.cpp, which includes this
// header rather than the other way around.
enum class ColorTheme {
    Bernstein,
    GelbHell,
    GelbDunkel,
    BlauHell,
    BlauDunkel,
    // Graphit -- operator, 2026-09-13: "mache c", following a live
    // 3-direction mockup review (Design C, "Fokus-Karte": Karte
    // dominant, Cyan/Koralle statt Amber auf Graphit). Dark-only, no
    // Hell/Dunkel pair -- the design itself is a single committed
    // direction, not a light/dark family the way Gelb/Blau are.
    Graphit,
};

// In display order (SettingsDialog's theme combo iterates this) --
// Bernstein first as the familiar original.
QVector<ColorTheme> allColorThemes();
QString colorThemeDisplayName(ColorTheme theme);

// Round-trips through ContestSettings' persisted string field --
// unrecognized text falls back to Bernstein rather than erroring, the
// same "unknown value degrades to the original default" rule
// rotorDialStyleFromString() already applies in ContestSettings.cpp.
QString colorThemeStorageKey(ColorTheme theme);
ColorTheme colorThemeFromStorageKey(const QString& key);

} // namespace Contestprogramm
