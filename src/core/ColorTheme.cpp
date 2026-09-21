#include "core/ColorTheme.h"

namespace Contestprogramm {

QVector<ColorTheme> allColorThemes()
{
    return {ColorTheme::Bernstein, ColorTheme::Gruen};
}

QString colorThemeDisplayName(ColorTheme theme)
{
    switch (theme) {
    case ColorTheme::Bernstein:
        return QStringLiteral("Bernstein (Standard)");
    case ColorTheme::Gruen:
        return QStringLiteral("Grün");
    }
    return QStringLiteral("Bernstein (Standard)");
}

QString colorThemeStorageKey(ColorTheme theme)
{
    switch (theme) {
    case ColorTheme::Bernstein:
        return QStringLiteral("bernstein");
    case ColorTheme::Gruen:
        return QStringLiteral("gruen");
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
    return ColorTheme::Bernstein; // unrecognized/empty/removed theme -- the original default, not an error
}

} // namespace Contestprogramm
