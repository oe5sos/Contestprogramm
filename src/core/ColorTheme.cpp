#include "core/ColorTheme.h"

namespace Contestprogramm {

QVector<ColorTheme> allColorThemes()
{
    return {ColorTheme::Bernstein, ColorTheme::GelbHell, ColorTheme::GelbDunkel, ColorTheme::BlauHell,
            ColorTheme::BlauDunkel, ColorTheme::Graphit};
}

QString colorThemeDisplayName(ColorTheme theme)
{
    switch (theme) {
    case ColorTheme::Bernstein:
        return QStringLiteral("Bernstein (Standard)");
    case ColorTheme::GelbHell:
        return QStringLiteral("Gelb Hell");
    case ColorTheme::GelbDunkel:
        return QStringLiteral("Gelb Dunkel");
    case ColorTheme::BlauHell:
        return QStringLiteral("Blau Hell");
    case ColorTheme::BlauDunkel:
        return QStringLiteral("Blau Dunkel");
    case ColorTheme::Graphit:
        return QStringLiteral("Graphit (Cyan/Koralle)");
    }
    return QStringLiteral("Bernstein (Standard)");
}

QString colorThemeStorageKey(ColorTheme theme)
{
    switch (theme) {
    case ColorTheme::Bernstein:
        return QStringLiteral("bernstein");
    case ColorTheme::GelbHell:
        return QStringLiteral("gelb_hell");
    case ColorTheme::GelbDunkel:
        return QStringLiteral("gelb_dunkel");
    case ColorTheme::BlauHell:
        return QStringLiteral("blau_hell");
    case ColorTheme::BlauDunkel:
        return QStringLiteral("blau_dunkel");
    case ColorTheme::Graphit:
        return QStringLiteral("graphit");
    }
    return QStringLiteral("bernstein");
}

ColorTheme colorThemeFromStorageKey(const QString& key)
{
    for (ColorTheme theme : allColorThemes()) {
        if (colorThemeStorageKey(theme).compare(key, Qt::CaseInsensitive) == 0) {
            return theme;
        }
    }
    return ColorTheme::Bernstein; // unrecognized/empty -- the original default, not an error
}

} // namespace Contestprogramm
