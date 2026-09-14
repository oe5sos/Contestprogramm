// Tests for core/ColorTheme.h's pure round-trip functions -- the
// storage-key <-> enum mapping ContestSettings::loadFrom/saveTo rely on
// for persisting the operator's chosen colour theme.

#include <QtTest>

#include "core/ColorTheme.h"

using namespace Contestprogramm;

class TestColorTheme : public QObject
{
    Q_OBJECT

private slots:
    void everyThemeRoundTripsThroughItsStorageKey();
    void unrecognizedKeyFallsBackToBernstein();
    void emptyKeyFallsBackToBernstein();
    void storageKeyLookupIsCaseInsensitive();
    void bernsteinIsFirstInDisplayOrder();
    void everyThemeHasANonEmptyDisplayName();
};

void TestColorTheme::everyThemeRoundTripsThroughItsStorageKey()
{
    for (ColorTheme theme : allColorThemes()) {
        const QString key = colorThemeStorageKey(theme);
        QVERIFY(!key.isEmpty());
        QVERIFY(colorThemeFromStorageKey(key) == theme);
    }
}

void TestColorTheme::unrecognizedKeyFallsBackToBernstein()
{
    QVERIFY(colorThemeFromStorageKey(QStringLiteral("no_such_theme")) == ColorTheme::Bernstein);
}

void TestColorTheme::emptyKeyFallsBackToBernstein()
{
    // The state a fresh install's settings table is in before any
    // "color_theme" row has ever been written -- must degrade to the
    // original palette, not an arbitrary one.
    QVERIFY(colorThemeFromStorageKey(QString()) == ColorTheme::Bernstein);
}

void TestColorTheme::storageKeyLookupIsCaseInsensitive()
{
    QVERIFY(colorThemeFromStorageKey(QStringLiteral("GELB_HELL")) == ColorTheme::GelbHell);
}

void TestColorTheme::bernsteinIsFirstInDisplayOrder()
{
    const QVector<ColorTheme> themes = allColorThemes();
    QVERIFY(!themes.isEmpty());
    QCOMPARE(themes.first(), ColorTheme::Bernstein);
    // And every theme appears exactly once.
    QCOMPARE(themes.size(), 6);
}

void TestColorTheme::everyThemeHasANonEmptyDisplayName()
{
    for (ColorTheme theme : allColorThemes()) {
        QVERIFY(!colorThemeDisplayName(theme).isEmpty());
    }
}

QTEST_APPLESS_MAIN(TestColorTheme)
#include "test_color_theme.moc"
