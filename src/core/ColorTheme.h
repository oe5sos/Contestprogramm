#pragma once

#include <QString>
#include <QVector>

namespace Contestprogramm {

// Selectable UI colour theme. Two, deliberately: operator, 2026-09-21
// ("alles bisheriger zusätzlich löschen und nur 2 varianten anbieten.
// die jetziger und als option siehe fotos") -- the original Bernstein
// look stays the default, and the one alternative is Gruen, modelled
// on a screenshot he sent (charcoal surfaces, near-white text, one
// green accent, amber kept for markers/warnings). The earlier
// GelbHell/GelbDunkel/BlauHell/BlauDunkel/Graphit themes (2026-09-12/13)
// were removed at his request; their stored keys fall back to
// Bernstein via colorThemeFromStorageKey().
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
    Gruen,
};

// In display order (SettingsDialog's theme combo iterates this) --
// Bernstein first as the familiar original.
QVector<ColorTheme> allColorThemes();
QString colorThemeDisplayName(ColorTheme theme);

// Round-trips through ContestSettings' persisted string field --
// unrecognized text (including the keys of the removed themes) falls
// back to Bernstein rather than erroring, the same "unknown value
// degrades to the original default" rule rotorDialStyleFromString()
// already applies in ContestSettings.cpp.
QString colorThemeStorageKey(ColorTheme theme);
ColorTheme colorThemeFromStorageKey(const QString& key);

} // namespace Contestprogramm
