// Tests for core/ColorTheme.h's pure round-trip functions -- the
// storage-key <-> enum mapping ContestSettings::loadFrom/saveTo rely on
// for persisting the operator's chosen colour theme.

#include <QtTest>

#include "core/BandUtils.h"
#include "core/ColorTheme.h"
#include "ui/StyleKit.h"

#include <QColor>

using namespace Contestprogramm;

class TestColorTheme : public QObject
{
    Q_OBJECT

private slots:
    void everyThemeRoundTripsThroughItsStorageKey();
    void unrecognizedKeyFallsBackToBernstein();
    void emptyKeyFallsBackToBernstein();
    void storageKeyLookupIsCaseInsensitive();
    void removedThemesFallBackToBernstein();
    void bernsteinIsFirstInDisplayOrder();
    void everyThemeHasANonEmptyDisplayName();
    void bandColoursFollowTheChosenTheme();
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
    QVERIFY(colorThemeFromStorageKey(QStringLiteral("GRUEN")) == ColorTheme::Gruen);
}

void TestColorTheme::removedThemesFallBackToBernstein()
{
    // The keys the 2026-09-12/13 themes wrote into existing settings
    // tables -- an operator who had one of them selected must come up
    // in Bernstein after the 2026-09-21 reduction to two themes, not
    // crash or land on Gruen by accident.
    for (const char* removed : {"gelb_hell", "gelb_dunkel", "blau_hell", "blau_dunkel", "graphit"}) {
        QVERIFY2(colorThemeFromStorageKey(QString::fromLatin1(removed)) == ColorTheme::Bernstein, removed);
    }
}

void TestColorTheme::bernsteinIsFirstInDisplayOrder()
{
    const QVector<ColorTheme> themes = allColorThemes();
    QVERIFY(!themes.isEmpty());
    QCOMPARE(themes.first(), ColorTheme::Bernstein);
    // Exactly the two the operator asked for (2026-09-21), each once.
    QCOMPARE(themes.size(), 2);
    QCOMPARE(themes.last(), ColorTheme::Gruen);
}

void TestColorTheme::everyThemeHasANonEmptyDisplayName()
{
    for (ColorTheme theme : allColorThemes()) {
        QVERIFY(!colorThemeDisplayName(theme).isEmpty());
    }
}

// Die Bandfarben im Log hängen am Akzentton des Themas, nicht an einer
// festen Tabelle -- Martin, 2026-09-23: "man sollte auch die farbe und
// das design einfach umschalten können". Ein Thema zu wechseln muss
// also auch sie mitnehmen.
void TestColorTheme::bandColoursFollowTheChosenTheme()
{
    const QStringList bands = knownBands();
    QVERIFY(bands.size() > 5);

    QHash<ColorTheme, QStringList> perTheme;
    for (ColorTheme theme : allColorThemes()) {
        Style::setActiveTheme(theme);
        QStringList tints;
        for (const QString& band : bands) {
            const QString tint = Style::bandTint(band);
            if (tint.isEmpty()) {
                continue; // Mikrowellenbänder tragen keinen eigenen Ton
            }
            const QColor colour(tint);
            QVERIFY2(colour.isValid(), qPrintable(band + QStringLiteral(": ") + tint));
            // Lesbar auf den dunklen Flächen dieses Programms, in
            // jedem Thema: nicht fast schwarz und nicht fast weiß.
            QVERIFY2(colour.lightness() >= 120 && colour.lightness() <= 215,
                      qPrintable(QStringLiteral("%1 %2 L=%3").arg(band, tint).arg(colour.lightness())));
            tints << tint;
        }
        QVERIFY(tints.size() >= 6);
        // Benachbarte Bänder sind auseinanderzuhalten.
        for (int i = 1; i < tints.size(); ++i) {
            QVERIFY2(tints.at(i) != tints.at(i - 1), qPrintable(tints.at(i)));
        }
        perTheme.insert(theme, tints);
    }

    // Und das eigentliche: zwei Themen ergeben zwei Reihen.
    QVERIFY(perTheme.value(ColorTheme::Bernstein) != perTheme.value(ColorTheme::Gruen));
    qInfo() << "Bernstein:" << perTheme.value(ColorTheme::Bernstein);
    qInfo() << "Gruen:" << perTheme.value(ColorTheme::Gruen);

    // Ein Band ohne eigenen Ton behält die normale Textfarbe.
    Style::setActiveTheme(ColorTheme::Bernstein);
    QVERIFY(Style::bandTint(QStringLiteral("10368")).isEmpty());
    QVERIFY(Style::bandTint(QString()).isEmpty());
}

QTEST_APPLESS_MAIN(TestColorTheme)
#include "test_color_theme.moc"
