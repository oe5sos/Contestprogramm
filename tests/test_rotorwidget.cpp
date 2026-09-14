#include <QtTest>

#include <QApplication>
#include <QFontMetrics>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QSignalSpy>

#include "app/ContestSettings.h"
#include "core/RotorDialStyle.h"
#include "core/terrain/LineOfSight.h"
#include "ui/RotorWidget.h"
#include "ui/SettingsDialog.h"
#include "ui/StyleKit.h"

using namespace Contestprogramm;

// RotorWidget::secondAntennaBearing() is a plain static function (no
// widget needs to be constructed to exercise it) -- it would otherwise
// stay a QTEST_APPLESS_MAIN pure-logic test like test_beamheading.cpp/
// test_maidenhead.cpp, but TestRotorWidgetBandLabel below constructs
// actual RotorWidget instances (a QWidget subclass, per the plan/
// class comment's Kern-Welle 2 rotor-slot work), which needs a live
// QApplication (same reasoning as test_grid_autofill.cpp's
// UnifiedLogWidget test) -- see the custom main() at the bottom that runs
// both test classes in this one executable.
class TestRotorWidgetSecondAntenna : public QObject
{
    Q_OBJECT

private slots:
    void offsetIsAddedToCurrentAzimuth();
    void resultWrapsAcross360();
    void resultWrapsForNegativeOffset();
    void zeroOffsetMatchesPrimaryNeedle();
};

void TestRotorWidgetSecondAntenna::offsetIsAddedToCurrentAzimuth()
{
    // Plan's own example: rotor points X, second antenna physically
    // points X+70.
    QCOMPARE(RotorWidget::secondAntennaBearing(100.0, 70.0), 170.0);
}

void TestRotorWidgetSecondAntenna::resultWrapsAcross360()
{
    QCOMPARE(RotorWidget::secondAntennaBearing(350.0, 70.0), 60.0);
}

void TestRotorWidgetSecondAntenna::resultWrapsForNegativeOffset()
{
    QCOMPARE(RotorWidget::secondAntennaBearing(10.0, -70.0), 300.0);
}

void TestRotorWidgetSecondAntenna::zeroOffsetMatchesPrimaryNeedle()
{
    QCOMPARE(RotorWidget::secondAntennaBearing(215.0, 0.0), 215.0);
}

// RotorWidget itself is a QWidget subclass, so exercising setBandLabel()
// needs a live QApplication (same reasoning as test_grid_autofill.cpp's
// UnifiedLogWidget test) -- combined with the QTEST_APPLESS_MAIN class
// above into one executable via a custom main() below, rather than a
// second test binary, since both cover the same class.
class TestRotorWidgetBandLabel : public QObject
{
    Q_OBJECT

private slots:
    void constructorSetsInitialLabel();
    void setBandLabelUpdatesLabel();
    void setBandLabelIsIdempotentForSameValue();
};

void TestRotorWidgetBandLabel::constructorSetsInitialLabel()
{
    RotorWidget widget(QStringLiteral("2m"));
    QCOMPARE(widget.bandLabel(), QStringLiteral("2m"));
}

void TestRotorWidgetBandLabel::setBandLabelUpdatesLabel()
{
    // The operator's own example from the task: repurposing a rotor slot
    // for a different band setup than the original two-mast VHF/UHF
    // station -- the label is free text, not a fixed band identity.
    RotorWidget widget(QStringLiteral("2m"));
    widget.setBandLabel(QStringLiteral("Kurzwelle"));
    QCOMPARE(widget.bandLabel(), QStringLiteral("Kurzwelle"));
}

void TestRotorWidgetBandLabel::setBandLabelIsIdempotentForSameValue()
{
    RotorWidget widget(QStringLiteral("70cm"));
    widget.setBandLabel(QStringLiteral("70cm"));
    QCOMPARE(widget.bandLabel(), QStringLiteral("70cm"));
}

// Martin's request this session, after comparing the live app against
// the design mockups ("da hatten wir andere") and proposing the fix
// himself ("vielleicht mehrere Möglichkeiten, dann kann individuell
// ausgewählt werden"): RotorWidget gets three selectable paint styles
// (RotorDialStyle, core/RotorDialStyle.h) instead of only the original
// full-compass rendering. Covers the property round trip, that a style
// switch leaves every other bit of widget state untouched, that a
// repaint in each style actually runs without crashing (QWidget::grab()
// forces a real paintEvent() even though this test never shows/exposes
// the widget), and SettingsDialog's own round trip for the new combo --
// same pattern as test_utc_countdown.cpp's contest-end tests.
class TestRotorWidgetDialStyle : public QObject
{
    Q_OBJECT

private slots:
    void defaultStyleIsFullCompass();
    void setDialStyleRoundTrips();
    void setDialStyleIsIdempotentForSameValue();
    void styleSwitchPreservesOtherStateAndRepaintsCleanly();
    void settingsDialogRoundTripsDefaultDialStyle();
    void settingsDialogRoundTripsChangedDialStyle();
    void settingsDialogRoundTripsDigitalDialStyle();
    void partialArcSecondAntennaDottedTreatmentPaintsCleanly();
    void terrainSectorWashPaintsCleanlyAcrossDialStyles();
};

void TestRotorWidgetDialStyle::defaultStyleIsFullCompass()
{
    // FullCompass is RotorWidget's only rendering before this pass --
    // a freshly constructed widget must keep behaving that way until
    // something actually asks for a different style.
    RotorWidget widget(QStringLiteral("2m"));
    QCOMPARE(widget.dialStyle(), RotorDialStyle::FullCompass);
}

void TestRotorWidgetDialStyle::setDialStyleRoundTrips()
{
    RotorWidget widget(QStringLiteral("2m"));
    widget.setDialStyle(RotorDialStyle::LinearScale);
    QCOMPARE(widget.dialStyle(), RotorDialStyle::LinearScale);
    widget.setDialStyle(RotorDialStyle::PartialArc);
    QCOMPARE(widget.dialStyle(), RotorDialStyle::PartialArc);
    widget.setDialStyle(RotorDialStyle::Digital);
    QCOMPARE(widget.dialStyle(), RotorDialStyle::Digital);
    widget.setDialStyle(RotorDialStyle::FullCompass);
    QCOMPARE(widget.dialStyle(), RotorDialStyle::FullCompass);
}

void TestRotorWidgetDialStyle::setDialStyleIsIdempotentForSameValue()
{
    // Matches setBandLabel()/setExtraBandBadge()'s existing
    // idempotent-no-op convention (see RotorWidget::setDialStyle()).
    RotorWidget widget(QStringLiteral("2m"));
    widget.setDialStyle(RotorDialStyle::PartialArc);
    widget.setDialStyle(RotorDialStyle::PartialArc);
    QCOMPARE(widget.dialStyle(), RotorDialStyle::PartialArc);
}

void TestRotorWidgetDialStyle::styleSwitchPreservesOtherStateAndRepaintsCleanly()
{
    RotorWidget widget(QStringLiteral("2m"));
    widget.resize(240, 320);
    widget.setAzimuthDeg(214.0);
    widget.setConnected(true);
    widget.setTargetBearing(72.0, 471.0, QStringLiteral("SP9XYZ"), QStringLiteral("JO90"));
    widget.setSecondAntenna(true, 70.0);
    widget.setExtraBandBadge(QStringLiteral("+23cm"));

    const QList<RotorDialStyle> styles = {
        RotorDialStyle::FullCompass,
        RotorDialStyle::LinearScale,
        RotorDialStyle::PartialArc,
        RotorDialStyle::Digital,
        RotorDialStyle::FullCompass, // and back again
    };
    for (RotorDialStyle style : styles) {
        widget.setDialStyle(style);
        QCOMPARE(widget.dialStyle(), style);

        // Every field the three paint paths share must survive the
        // switch untouched -- setDialStyle() only ever assigns
        // m_dialStyle and calls update(), so this also guards against a
        // future refactor accidentally coupling style selection to
        // widget state.
        QCOMPARE(widget.bandLabel(), QStringLiteral("2m"));
        QCOMPARE(widget.azimuthDeg(), 214.0);
        QVERIFY(widget.hasTargetBearing());
        QVERIFY(widget.secondAntennaEnabled());
        QCOMPARE(widget.secondAntennaOffsetDeg(), 70.0);

        // Force a real paintEvent() (grab() renders even an
        // unshown/unexposed widget) -- confirms each of the three
        // paint paths actually runs end to end with a target, a
        // connected second antenna, and an extra-band badge all set,
        // not just that the setter itself doesn't crash.
        const QPixmap pixmap = widget.grab();
        QVERIFY(!pixmap.isNull());
    }
}

void TestRotorWidgetDialStyle::settingsDialogRoundTripsDefaultDialStyle()
{
    ContestSettings initial;
    initial.ownCallsign = QStringLiteral("OE5SOS");
    QCOMPARE(initial.rotorDialStyle, RotorDialStyle::FullCompass);

    SettingsDialog dialog(initial, {});
    QCOMPARE(dialog.settings().rotorDialStyle, RotorDialStyle::FullCompass);
}

void TestRotorWidgetDialStyle::settingsDialogRoundTripsChangedDialStyle()
{
    ContestSettings initial;
    initial.ownCallsign = QStringLiteral("OE5SOS");
    initial.rotorDialStyle = RotorDialStyle::PartialArc;

    SettingsDialog dialog(initial, {});
    QCOMPARE(dialog.settings().rotorDialStyle, RotorDialStyle::PartialArc);
}

void TestRotorWidgetDialStyle::settingsDialogRoundTripsDigitalDialStyle()
{
    // The fourth style added this session (2026-09-11), alongside
    // Klassisch/Bogen-Präzision's in-place refinements of FullCompass/
    // PartialArc above -- Digital is the one genuinely new enum value,
    // so it gets the same SettingsDialog round-trip coverage
    // settingsDialogRoundTripsChangedDialStyle() already gives
    // PartialArc.
    ContestSettings initial;
    initial.ownCallsign = QStringLiteral("OE5SOS");
    initial.rotorDialStyle = RotorDialStyle::Digital;

    SettingsDialog dialog(initial, {});
    QCOMPARE(dialog.settings().rotorDialStyle, RotorDialStyle::Digital);
}

void TestRotorWidgetDialStyle::partialArcSecondAntennaDottedTreatmentPaintsCleanly()
{
    // Bogen-Präzision's own new code path this pass added: the second
    // antenna's needle switches from the dashed/amber treatment every
    // other style uses to a dotted instrument-face line ending in a
    // hollow ring-edge circle, specific to PartialArc (see
    // RotorWidget::drawNeedle()'s PartialArc-only branch,
    // Design3.dc.html). Exercises it both with the second antenna's own
    // bearing outside AND actually inside the mechanical stop zone (the
    // dotted line's own blocked/red colour override) -- each needle is
    // judged on its own bearing, per drawNeedlesAndTarget()'s existing
    // per-needle inGap() check.
    RotorWidget widget(QStringLiteral("2m"));
    widget.resize(240, 380);
    widget.setDialStyle(RotorDialStyle::PartialArc);
    widget.setConnected(true);
    widget.setSecondAntenna(true, 90.0);

    widget.setAzimuthDeg(10.0); // second antenna at 100° -- outside the 150..210 gap
    QVERIFY(!widget.grab().isNull());

    widget.setAzimuthDeg(90.0); // second antenna at 180°, dead centre of the gap -- blocked/red override
    QVERIFY(!widget.grab().isNull());
}

// Phase 2 terrain sectors (2026-09-12) -- setTerrainSectors() is new
// API this pass added (see RotorWidget.h's own doc comment); exercised
// with a real 360-entry sweep containing both a Blocked and a Marginal
// run, across all four dial styles, with the azimuth sitting right
// inside the Blocked run so the needle/caption/readout warning-red
// override (isCurrentAzimuthTerrainBlocked()) is exercised too, not
// just the ring wash itself (drawTerrainSectorWash(), FullCompass/
// PartialArc only -- LinearScale/Digital have no ring to wash, but
// must still paint cleanly with sectors set).
void TestRotorWidgetDialStyle::terrainSectorWashPaintsCleanlyAcrossDialStyles()
{
    QVector<LineOfSightClass> sectors(360, LineOfSightClass::Clear);
    for (int deg = 80; deg < 100; ++deg) {
        sectors[deg] = LineOfSightClass::Blocked;
    }
    for (int deg = 200; deg < 220; ++deg) {
        sectors[deg] = LineOfSightClass::Marginal;
    }

    for (RotorDialStyle style : {RotorDialStyle::FullCompass, RotorDialStyle::PartialArc, RotorDialStyle::Digital,
                                  RotorDialStyle::LinearScale}) {
        RotorWidget widget(QStringLiteral("2m"));
        widget.resize(240, 380);
        widget.setDialStyle(style);
        widget.setConnected(true);
        widget.setTerrainSectors(sectors);
        widget.setAzimuthDeg(90.0); // inside the injected Blocked run
        QVERIFY(!widget.grab().isNull());
    }
}

// Martin's second request this session (2026-09-10), generalizing the
// rotor-style pass above: "ich will das design von den rotoren als
// option in der taskleiste ändern können" -- RotorWidget now carries
// its own ⚙ options affordance (m_optionsButton) that opens a small
// QMenu offering the same styles directly from the panel's own header
// (three at the time of that request, now four since the Digital style
// was added 2026-09-11 -- see addStyleAction() calls below), instead of
// only through SettingsDialog's "Rotor-Anzeige" combo. showOptionsPopup()
// shows the menu via QMenu::popup() (matching the real Longpath
// TxApplet::showFinePopup() precedent's own non-blocking
// QWidget(Qt::Popup)::show()) rather than the blocking exec(), which is
// what makes this directly testable: popup() returns immediately, so
// the test can find the still-open QMenu as a child of the widget and
// trigger one of its actions synchronously, with no QTimer/nested-
// event-loop choreography needed.
class TestRotorWidgetOptionsPopup : public QObject
{
    Q_OBJECT

private slots:
    void clickingOptionsButtonOpensMenuWithFourStyleEntries();
    void currentStyleStartsCheckedInTheMenu();
    void currentStyleStartsCheckedInTheMenuForDigital();
    void pickingAnEntryChangesDialStyleImmediately();
    void pickingAnEntryEmitsDialStyleRequestedWithThatStyle();
    void pickingDigitalEntryChangesDialStyleImmediately();
    void pickingDigitalEntryEmitsDialStyleRequestedWithThatStyle();
};

namespace {

QPushButton* findRotorOptionsButton(RotorWidget& widget)
{
    return widget.findChild<QPushButton*>(QStringLiteral("rotorWidgetOptionsButton"));
}

// Clicking the button calls showOptionsPopup(), which shows the menu
// via the non-blocking QMenu::popup() -- by the time click() returns,
// the (still open) QMenu is a QObject child of `widget`, findable the
// same way findRotorOptionsButton() above finds the button.
QMenu* openOptionsMenu(RotorWidget& widget)
{
    QPushButton* button = findRotorOptionsButton(widget);
    if (!button) {
        return nullptr;
    }
    button->click();
    return widget.findChild<QMenu*>();
}

} // namespace

void TestRotorWidgetOptionsPopup::clickingOptionsButtonOpensMenuWithFourStyleEntries()
{
    RotorWidget widget(QStringLiteral("2m"));
    QMenu* menu = openOptionsMenu(widget);
    QVERIFY(menu);
    QCOMPARE(menu->actions().size(), 4);
    // Same labels as SettingsDialog's own "Rotor-Anzeige" combo, in the
    // same order (RotorDialStyle::FullCompass/LinearScale/PartialArc/
    // Digital).
    QCOMPARE(menu->actions().at(0)->text(), QStringLiteral("Kompass (360°)"));
    QCOMPARE(menu->actions().at(1)->text(), QStringLiteral("Skala (linear)"));
    QCOMPARE(menu->actions().at(2)->text(), QStringLiteral("Rotor-Box (Bogen)"));
    QCOMPARE(menu->actions().at(3)->text(), QStringLiteral("Digital (Zahlen)"));
}

void TestRotorWidgetOptionsPopup::currentStyleStartsCheckedInTheMenu()
{
    RotorWidget widget(QStringLiteral("2m"));
    widget.setDialStyle(RotorDialStyle::PartialArc);

    QMenu* menu = openOptionsMenu(widget);
    QVERIFY(menu);
    QVERIFY(!menu->actions().at(0)->isChecked()); // Kompass
    QVERIFY(!menu->actions().at(1)->isChecked()); // Skala
    QVERIFY(menu->actions().at(2)->isChecked());  // Rotor-Box -- the current style
    QVERIFY(!menu->actions().at(3)->isChecked()); // Digital
}

void TestRotorWidgetOptionsPopup::currentStyleStartsCheckedInTheMenuForDigital()
{
    RotorWidget widget(QStringLiteral("2m"));
    widget.setDialStyle(RotorDialStyle::Digital);

    QMenu* menu = openOptionsMenu(widget);
    QVERIFY(menu);
    QVERIFY(!menu->actions().at(0)->isChecked()); // Kompass
    QVERIFY(!menu->actions().at(1)->isChecked()); // Skala
    QVERIFY(!menu->actions().at(2)->isChecked()); // Rotor-Box
    QVERIFY(menu->actions().at(3)->isChecked());  // Digital -- the current style
}

void TestRotorWidgetOptionsPopup::pickingAnEntryChangesDialStyleImmediately()
{
    RotorWidget widget(QStringLiteral("2m"));
    QCOMPARE(widget.dialStyle(), RotorDialStyle::FullCompass);

    QMenu* menu = openOptionsMenu(widget);
    QVERIFY(menu);
    menu->actions().at(1)->trigger(); // "Skala (linear)" -> LinearScale

    QCOMPARE(widget.dialStyle(), RotorDialStyle::LinearScale);
}

void TestRotorWidgetOptionsPopup::pickingAnEntryEmitsDialStyleRequestedWithThatStyle()
{
    RotorWidget widget(QStringLiteral("2m"));
    QSignalSpy spy(&widget, &RotorWidget::dialStyleRequested);

    QMenu* menu = openOptionsMenu(widget);
    QVERIFY(menu);
    menu->actions().at(2)->trigger(); // "Rotor-Box (Bogen)" -> PartialArc

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.constFirst().constFirst().value<RotorDialStyle>(), RotorDialStyle::PartialArc);
}

void TestRotorWidgetOptionsPopup::pickingDigitalEntryChangesDialStyleImmediately()
{
    RotorWidget widget(QStringLiteral("2m"));
    QCOMPARE(widget.dialStyle(), RotorDialStyle::FullCompass);

    QMenu* menu = openOptionsMenu(widget);
    QVERIFY(menu);
    menu->actions().at(3)->trigger(); // "Digital (Zahlen)" -> Digital

    QCOMPARE(widget.dialStyle(), RotorDialStyle::Digital);
}

void TestRotorWidgetOptionsPopup::pickingDigitalEntryEmitsDialStyleRequestedWithThatStyle()
{
    RotorWidget widget(QStringLiteral("2m"));
    QSignalSpy spy(&widget, &RotorWidget::dialStyleRequested);

    QMenu* menu = openOptionsMenu(widget);
    QVERIFY(menu);
    menu->actions().at(3)->trigger(); // "Digital (Zahlen)" -> Digital

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.constFirst().constFirst().value<RotorDialStyle>(), RotorDialStyle::Digital);
}

// Bug report this session (2026-09-10): the second needle/numbered mark
// rendered correctly for m_rotor1Widget but never for m_rotor2Widget
// ("regardless of its own settings" -- confirmed by swapping which slot
// was enabled). Reading RotorWidget/MainWindow found both slots'
// creation, wiring, and paint code byte-for-byte symmetric; runtime
// qDebug() during a live repro then proved both widgets' state,
// center/radius, and computed second-antenna bearing were ALSO already
// identical/correct at paint time. The actual cause turned out to live
// entirely outside RotorWidget: m_rotor2Widget is the SECOND (rightmost)
// child in m_rotorRow's QHBoxLayout, so it was the one whose right-hand
// content (where an angled second needle/mark point) fell inside the
// region PanelLayoutManager's "rotorrow" panel container was hanging off
// the canvas's right edge -- silently clipped away by that ancestor, not
// by anything RotorWidget itself computed wrong. See
// PanelLayoutManager::clampPanelsToCanvas() for the actual fix, and
// TestPanelLayoutManager::canvasShrinkClampsOffCanvasPanelBackOnScreen in
// test_panellayoutmanager.cpp for the test that actually reproduces and
// guards it -- this class only covers what IS reachable through
// RotorWidget's own public API (that two independently constructed
// instances keep fully independent second-antenna state), which the two
// widgets already had, before and after the fix; it deliberately cannot
// catch the real bug, since RotorWidget has no way to know about, or
// account for, an ancestor's clipping region.
class TestRotorWidgetTwoInstances : public QObject
{
    Q_OBJECT

private slots:
    void secondWidgetSecondAntennaIsIndependentOfFirst();
};

void TestRotorWidgetTwoInstances::secondWidgetSecondAntennaIsIndependentOfFirst()
{
    // Mirrors MainWindow::applyRotorWidgetSettings()'s own construction
    // order: m_rotor1Widget built (and, per applyRotorSlot(), told its
    // settings) first, m_rotor2Widget second.
    RotorWidget rotor1(QStringLiteral("2m"));
    RotorWidget rotor2(QStringLiteral("70cm"));

    // The exact "rotor1 disabled, rotor2 enabled at a distinct offset"
    // swap from the live bug's own Test B -- chosen specifically to rule
    // out "it's really about whichever offset value" rather than "it's
    // really about whichever slot".
    rotor1.setSecondAntenna(false, 0.0);
    rotor2.setSecondAntenna(true, 90.0);

    QVERIFY(!rotor1.secondAntennaEnabled());
    QVERIFY(rotor2.secondAntennaEnabled());
    QCOMPARE(rotor2.secondAntennaOffsetDeg(), 90.0);

    rotor1.setAzimuthDeg(0.0);
    rotor2.setAzimuthDeg(0.0);
    QCOMPARE(RotorWidget::secondAntennaBearing(rotor2.azimuthDeg(), rotor2.secondAntennaOffsetDeg()), 90.0);

    // Both widgets repaint without crashing -- confirms RotorWidget's own
    // paint path runs cleanly end to end with this exact per-instance
    // state. Each widget is unparented/top-level here, so this cannot
    // exercise the actual bug (an ancestor container clipping the
    // second-in-a-row widget's right-hand content) -- see this class's
    // own comment above.
    rotor1.resize(220, 326);
    rotor2.resize(220, 326);
    QVERIFY(!rotor1.grab().isNull());
    QVERIFY(!rotor2.grab().isNull());
}

// Martin's third request this session (2026-09-10): comparing the live
// app against the design mockup ("Idea A", Rotor-A.dc.html/
// Rotor-Dual.dc.html) found RotorWidget's readout missing the mockup's
// own three-column AKTUELL/ZIEL/ENTFERNUNG block and target-station
// caption line entirely -- drawReadout() only ever painted a single
// small "AZ <value>" line plus a callsign/grid line with no distance
// alongside it. RotorWidget is a hand-painted QWidget with no QLabel
// tree beneath the dial (see this file's own TextSegment/
// drawCenteredSegments precedent in RotorWidget.cpp), so there is no
// text layer to assert against directly -- these tests instead cover
// what IS reachable through the public API: that the new
// targetBearingDeg()/targetDistanceKm()/targetCallsign()/targetGrid()
// accessors (added alongside this pass, same reasoning
// secondAntennaEnabled()/secondAntennaOffsetDeg() already gave) return
// exactly what setTargetBearing() was given, and that grab() -- which
// forces a real paintEvent() -- runs the new readout's every branch
// (target set with a known station identity, target set with no
// identity at all, and no target whatsoever, i.e. the Style::
// unknownDash() path) without crashing, across all three dial styles.
class TestRotorWidgetReadout : public QObject
{
    Q_OBJECT

private slots:
    void targetAccessorsReturnWhatSetTargetBearingWasGiven();
    void targetAccessorsDefaultEmptyBeforeAnyTargetIsSet();
    void readoutPaintsCleanlyWithAKnownStationIdentity();
    void digitalGlassReadoutPaintsCleanlyAcrossConnectionAndTargetStates();
    void readoutPaintsCleanlyWithATargetButNoStationIdentity();
    void readoutPaintsCleanlyWithNoTargetAtAll();
    void minimumSizeHintFitsTheThreeColumnReadoutBlock();
    void minimumSizeHintAccountsForDigitalGlassPanels();
};

void TestRotorWidgetReadout::targetAccessorsReturnWhatSetTargetBearingWasGiven()
{
    // The Rotor-Dual.dc.html mockup's own worked example.
    RotorWidget widget(QStringLiteral("2m"));
    widget.setTargetBearing(72.0, 471.0, QStringLiteral("SP9XYZ"), QStringLiteral("JO90"));

    QVERIFY(widget.hasTargetBearing());
    QCOMPARE(widget.targetBearingDeg(), 72.0);
    QCOMPARE(widget.targetDistanceKm(), 471.0);
    QCOMPARE(widget.targetCallsign(), QStringLiteral("SP9XYZ"));
    QCOMPARE(widget.targetGrid(), QStringLiteral("JO90"));
}

void TestRotorWidgetReadout::targetAccessorsDefaultEmptyBeforeAnyTargetIsSet()
{
    // A freshly constructed widget has no target at all -- the readout's
    // ZIEL/ENTFERNUNG columns and the caption row must all fall back to
    // Style::unknownDash() (HAUSSTIL rule 7), never a fabricated 0/empty
    // value that only happens to look plausible.
    RotorWidget widget(QStringLiteral("2m"));
    QVERIFY(!widget.hasTargetBearing());
    QVERIFY(widget.targetCallsign().isEmpty());
    QVERIFY(widget.targetGrid().isEmpty());
}

void TestRotorWidgetReadout::readoutPaintsCleanlyWithAKnownStationIdentity()
{
    // Exercises the caption row's "callsign · grid · distance" branch
    // (drawReadout(): m_hasTarget && station identity known) across
    // three of the four dial styles, which all share this one
    // drawReadout() call -- Digital paints its own analogous but
    // structurally different glass-panel readout instead
    // (drawDigitalReadout(), same computeCaptionLine() logic), covered
    // separately by digitalGlassReadoutPaintsCleanlyAcrossConnectionAnd
    // TargetStates() below.
    RotorWidget widget(QStringLiteral("2m"));
    widget.resize(240, 380);
    widget.setConnected(true);
    widget.setAzimuthDeg(300.0);
    widget.setTargetBearing(60.0, 471.0, QStringLiteral("SP9XYZ"), QStringLiteral("JO90"));

    for (RotorDialStyle style : {RotorDialStyle::FullCompass, RotorDialStyle::LinearScale, RotorDialStyle::PartialArc}) {
        widget.setDialStyle(style);
        QVERIFY(!widget.grab().isNull());
    }
}

void TestRotorWidgetReadout::digitalGlassReadoutPaintsCleanlyAcrossConnectionAndTargetStates()
{
    // Digital's own new readout (drawDigitalReadout()/drawGlassPanel())
    // REPLACES drawReadout() for this style, so it needs the same
    // paint-clean coverage across connected/disconnected and
    // target-set/no-target that the other three styles already get via
    // drawReadout() (readoutPaintsCleanlyWithAKnownStationIdentity()/
    // readoutPaintsCleanlyWithNoTargetAtAll() above/below).
    RotorWidget widget(QStringLiteral("2m"));
    widget.resize(280, 460);
    widget.setDialStyle(RotorDialStyle::Digital);
    widget.setSecondAntenna(true, 70.0);

    // Connected, with a full target (known station identity) -- the
    // "everything lit up" case, all three glass panels showing a real
    // measured/commanded value.
    widget.setConnected(true);
    widget.setAzimuthDeg(214.0);
    widget.setTargetBearing(72.0, 471.0, QStringLiteral("SP9XYZ"), QStringLiteral("JO90"));
    QVERIFY(!widget.grab().isNull());

    // Disconnected, no target at all -- every glass panel falls back to
    // Style::unknownDash() (HAUSSTIL rule 7), never a fabricated value.
    widget.setConnected(false);
    widget.clearTargetBearing();
    QVERIFY(!widget.grab().isNull());
}

void TestRotorWidgetReadout::readoutPaintsCleanlyWithATargetButNoStationIdentity()
{
    // A target bearing set without a callsign/grid (setTargetBearing()'s
    // last two parameters default to QString()) -- per this task's own
    // "never fabricate a placeholder when nothing is set" rule, the
    // caption row must stay blank rather than drawing a misleading dash
    // (that would suggest the bearing/distance above are ALSO unknown,
    // when they are not) or a fabricated "· ·" with nothing between the
    // separators. This only confirms the paint path survives that branch
    // -- see this test class's own comment for why RotorWidget's paint
    // output cannot be asserted on more directly than that.
    RotorWidget widget(QStringLiteral("2m"));
    widget.resize(240, 380);
    widget.setTargetBearing(90.0, 120.0);

    QVERIFY(widget.hasTargetBearing());
    QVERIFY(widget.targetCallsign().isEmpty());
    QVERIFY(widget.targetGrid().isEmpty());
    QVERIFY(!widget.grab().isNull());
}

void TestRotorWidgetReadout::readoutPaintsCleanlyWithNoTargetAtAll()
{
    // No target ever set -- the ZIEL/ENTFERNUNG columns and the caption
    // row all take the Style::unknownDash() branch. Covers both
    // m_connected states, since the AKTUELL column's own dash/value
    // choice is independent of m_hasTarget.
    RotorWidget widget(QStringLiteral("2m"));
    widget.resize(240, 380);

    for (bool connected : {false, true}) {
        widget.setConnected(connected);
        for (RotorDialStyle style : {RotorDialStyle::FullCompass, RotorDialStyle::LinearScale,
                                      RotorDialStyle::PartialArc, RotorDialStyle::Digital}) {
            widget.setDialStyle(style);
            QVERIFY(!widget.grab().isNull());
        }
    }
}

void TestRotorWidgetReadout::minimumSizeHintFitsTheThreeColumnReadoutBlock()
{
    // Guards the panel-geometry re-tuning this pass also made
    // (MainWindow.cpp's "rotorrow" default QRect height, 236 -> 272):
    // if a future change shrinks the readout block back down without
    // updating that default, this is the test that should catch it
    // rather than leaving the panel silently too short again, per this
    // session's own "always re-tune the default when content outgrows
    // it" practice (see MainWindow.cpp's own comment on that QRect).
    RotorWidget widget(QStringLiteral("2m"));
    QVERIFY(widget.minimumSizeHint().height() >= 360);

    // Width regression guard for a real bug this pass's own live-app
    // verification screenshot caught: at the widget's OWN declared
    // minimumSizeHint() width, the three big AKTUELL/ZIEL/ENTFERNUNG
    // numbers (kFontDisplay, 38px) visibly overlapped each other --
    // 220px (the dial's own old minimum, before this readout existed)
    // is not wide enough for three ~90px-wide columns. Ties the
    // assertion to the actual measured font width (the same
    // Style::monoFont()/kFontDisplay drawReadout() itself paints with)
    // rather than a bare magic number, so a future font-ladder change
    // cannot silently reintroduce the overlap without this test
    // noticing.
    const QFontMetrics fmDisplay(Style::monoFont(widget.font(), Style::kFontDisplay));
    const int widestColumnContent = fmDisplay.horizontalAdvance(QStringLiteral("300°"));
    QVERIFY(widget.minimumSizeHint().width() >= widestColumnContent * 3);
}

void TestRotorWidgetReadout::minimumSizeHintAccountsForDigitalGlassPanels()
{
    // Corrected 2026-09-11: an earlier version of Digital gave
    // minimumSizeHint() its own, taller branch (small secondary ring +
    // a much taller glass-panel readout with a third Entfernung panel).
    // The operator asked for the ring back at the same size as the
    // other three styles ("rotor gleich groß wie die anderen") and the
    // Entfernung panel gone ("entfernung muss weg") -- Digital now
    // shares the exact same dial-area/text-area split as every other
    // style, so minimumSizeHint() is style-agnostic again and this test
    // guards THAT (no more special Digital branch), while still
    // confirming Digital paints cleanly with nothing clipped at that
    // one shared minimum -- the tightest size any real container is
    // allowed to give it.
    RotorWidget widget(QStringLiteral("2m"));
    widget.setDialStyle(RotorDialStyle::Digital);
    const QSize digitalMin = widget.minimumSizeHint();
    const QSize defaultMin = RotorWidget(QStringLiteral("2m")).minimumSizeHint();
    QCOMPARE(digitalMin, defaultMin);

    widget.resize(digitalMin);
    widget.setConnected(true);
    widget.setAzimuthDeg(214.0);
    widget.setTargetBearing(72.0, 471.0, QStringLiteral("SP9XYZ"), QStringLiteral("JO90"));
    widget.setSecondAntenna(true, 70.0);
    QVERIFY(!widget.grab().isNull());
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int status = 0;

    TestRotorWidgetSecondAntenna secondAntennaTest;
    status |= QTest::qExec(&secondAntennaTest, argc, argv);

    TestRotorWidgetBandLabel bandLabelTest;
    status |= QTest::qExec(&bandLabelTest, argc, argv);

    TestRotorWidgetDialStyle dialStyleTest;
    status |= QTest::qExec(&dialStyleTest, argc, argv);

    TestRotorWidgetOptionsPopup optionsPopupTest;
    status |= QTest::qExec(&optionsPopupTest, argc, argv);

    TestRotorWidgetTwoInstances twoInstancesTest;
    status |= QTest::qExec(&twoInstancesTest, argc, argv);

    TestRotorWidgetReadout readoutTest;
    status |= QTest::qExec(&readoutTest, argc, argv);

    return status;
}
#include "test_rotorwidget.moc"
