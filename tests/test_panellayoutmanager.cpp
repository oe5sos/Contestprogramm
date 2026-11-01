#include <QtTest>
#include <QSignalSpy>

#include <QApplication>
#include <QLabel>
#include <QResizeEvent>
#include <QTemporaryDir>

#include "data/ContestDatabase.h"
#include "ui/PanelContainerWidget.h"
#include "ui/LayoutProfileManager.h"
#include "ui/PanelLayoutManager.h"

using namespace Contestprogramm;

namespace {

// None of this file's widgets are ever show()n (see the class comment's
// own "no live QMouseEvent simulation" reasoning) -- and a QWidget that
// has never been shown/created at the platform level does not dispatch a
// real QEvent::Resize from a plain resize() call (confirmed empirically:
// resize() still updates the widget's own cached size, but no event
// reaches an installed eventFilter). PanelLayoutManager::
// clampPanelsToCanvas() is wired to canvas()'s QEvent::Resize (see its
// own comment for why it has to react to a real resize rather than
// running inside trySetGeometry() itself), so exercising it here needs
// resize() (to make canvas()->width()/height() actually report the new
// size clampPanelsToCanvas() reads) PLUS an explicitly sent QResizeEvent
// (to make the eventFilter that calls it actually run) -- the same
// "drive the tested entry point directly rather than simulate the real
// OS-level trigger" approach this file's own class comment already
// establishes for geometryEdited()/raiseRequested().
void resizeCanvas(PanelLayoutManager& manager, const QSize& newSize)
{
    const QSize oldSize = manager.canvas()->size();
    manager.canvas()->resize(newSize);
    QResizeEvent event(newSize, oldSize);
    QCoreApplication::sendEvent(manager.canvas(), &event);
}

} // namespace

// PanelLayoutManager owns real QWidgets (PanelContainerWidget instances,
// a canvas), so this needs a live QApplication -- same reasoning as
// test_mainwindow_rotor_toggle.cpp. Covers the port's persistence
// contract: geometry/locked-state/z-order round-trip through
// ContestDatabase's `settings` key/value table (the same store
// ContestSettings::loadFrom/saveTo already use -- see
// PanelLayoutManager.h's class comment for why, matching the real
// ContainerManager's own AppSettings-key/value-store mechanism) across
// separate ContestDatabase instances opened against the same file, the
// same way a real second launch would see it. Signals that are normally
// only emitted from real mouse-driven drag/resize/click interaction
// (geometryEdited/raiseRequested) are invoked directly here rather than
// simulated via QMouseEvent sequences -- the same "drive the tested
// state-changing entry point directly, not the UI chrome that triggers
// it" approach test_mainwindow_rotor_toggle.cpp already uses for
// MainWindow::applyRotorWidgetSettings(); PanelContainerWidget's own
// eventFilter wiring (that a real drag/click actually reaches these
// signals) is straightforward enough to review directly against the
// real Longpath ContainerWidget it was ported from.
class TestPanelLayoutManager : public QObject
{
    Q_OBJECT

private slots:
    void registerPanelUsesDefaultGeometryOnFirstRun();
    void editedGeometryRoundTripsThroughPersistence();
    void lockedStateRoundTripsThroughPersistenceAndStillGatesMoves();
    void resetToDefaultLayoutRestoresDefaultsAndPersistsThem();
    void zOrderRoundTripsThroughPersistence();
    void canvasShrinkClampsOffCanvasPanelBackOnScreen();
    void canvasResizeLeavesLockedPanelAloneEvenIfOffCanvas();
    void canvasResizeLeavesOnCanvasPanelUntouched();
    void editingOnePanelDoesNotPersistAnotherPanelsClampedGeometry();
    void compactDesignFitsASmallCanvas();
    void freshInstallPlacesPanelsByTheFittingDesignOnce();
    void profileSnapshotCountsUnshownPanelsAsVisible();
    void revealedPanelIsPulledOntoTheCanvas();
    void clampLeavesPanelsAloneWhileTheCanvasHasNoRealSize();
    void panelEditsSurviveARestartWithProfiles();
};

namespace {

QWidget* makeContent()
{
    return new QLabel(QStringLiteral("content"));
}

} // namespace

void TestPanelLayoutManager::registerPanelUsesDefaultGeometryOnFirstRun()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("plm_default.sqlite")), QStringLiteral("plm_default")));

    QWidget canvasParent;
    PanelLayoutManager manager(db, &canvasParent);
    const QRect defaultRect(10, 20, 300, 200);
    PanelContainerWidget* panel =
        manager.registerPanel(QStringLiteral("p"), QStringLiteral("P"), makeContent(), false, defaultRect);
    QCOMPARE(panel->geometry(), defaultRect);
}

void TestPanelLayoutManager::editedGeometryRoundTripsThroughPersistence()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("plm_edit.sqlite"));

    ContestDatabase db1;
    QVERIFY(db1.open(dbPath, QStringLiteral("plm_edit_1")));
    {
        QWidget canvasParent;
        PanelLayoutManager manager(db1, &canvasParent);
        PanelContainerWidget* panel = manager.registerPanel(QStringLiteral("p"), QStringLiteral("P"), makeContent(),
                                                              false, QRect(0, 0, 300, 200));
        QVERIFY(panel->trySetGeometry(QRect(40, 60, 500, 260)));
        emit panel->geometryEdited(); // as if a real drag/resize just completed
    }
    db1.close();

    ContestDatabase db2;
    QVERIFY(db2.open(dbPath, QStringLiteral("plm_edit_2")));
    QWidget canvasParent2;
    PanelLayoutManager manager2(db2, &canvasParent2);
    // A different default this time -- proves the restored geometry came
    // from the persisted edit, not from falling back to the default.
    PanelContainerWidget* panel2 =
        manager2.registerPanel(QStringLiteral("p"), QStringLiteral("P"), makeContent(), false, QRect(0, 0, 100, 100));
    QCOMPARE(panel2->geometry(), QRect(40, 60, 500, 260));
}

void TestPanelLayoutManager::lockedStateRoundTripsThroughPersistenceAndStillGatesMoves()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("plm_lock.sqlite"));

    ContestDatabase db1;
    QVERIFY(db1.open(dbPath, QStringLiteral("plm_lock_1")));
    {
        QWidget canvasParent;
        PanelLayoutManager manager(db1, &canvasParent);
        PanelContainerWidget* panel = manager.registerPanel(QStringLiteral("p"), QStringLiteral("P"), makeContent(),
                                                              false, QRect(0, 0, 300, 200));
        panel->setLocked(true); // lockedChanged -> manager saves immediately
    }
    db1.close();

    ContestDatabase db2;
    QVERIFY(db2.open(dbPath, QStringLiteral("plm_lock_2")));
    QWidget canvasParent2;
    PanelLayoutManager manager2(db2, &canvasParent2);
    PanelContainerWidget* panel2 = manager2.registerPanel(QStringLiteral("p"), QStringLiteral("P"), makeContent(),
                                                            false, QRect(0, 0, 300, 200));
    QVERIFY(panel2->isLocked());
    // A restored-locked panel must actually reject further moves too,
    // not just report isLocked() true.
    QVERIFY(!panel2->trySetGeometry(QRect(999, 999, 300, 200)));
}

void TestPanelLayoutManager::resetToDefaultLayoutRestoresDefaultsAndPersistsThem()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("plm_reset.sqlite"));
    const QRect defaultRect(5, 5, 300, 200);

    ContestDatabase db1;
    QVERIFY(db1.open(dbPath, QStringLiteral("plm_reset_1")));
    {
        QWidget canvasParent;
        PanelLayoutManager manager(db1, &canvasParent);
        PanelContainerWidget* panel =
            manager.registerPanel(QStringLiteral("p"), QStringLiteral("P"), makeContent(), false, defaultRect);
        QVERIFY(panel->trySetGeometry(QRect(400, 400, 350, 260)));
        panel->setLocked(true);

        manager.resetToDefaultLayout();
        // "Fenster zurücksetzen" unlocks too -- a reset that left the
        // panel stuck locked wherever it happened to be would not be a
        // reset at all.
        QVERIFY(!panel->isLocked());
        QCOMPARE(panel->geometry(), defaultRect);
    }
    db1.close();

    // And the reset was actually persisted, not just applied in memory.
    ContestDatabase db2;
    QVERIFY(db2.open(dbPath, QStringLiteral("plm_reset_2")));
    QWidget canvasParent2;
    PanelLayoutManager manager2(db2, &canvasParent2);
    PanelContainerWidget* panel2 = manager2.registerPanel(QStringLiteral("p"), QStringLiteral("P"), makeContent(),
                                                            false, QRect(999, 999, 300, 200));
    QCOMPARE(panel2->geometry(), defaultRect);
    QVERIFY(!panel2->isLocked());
}

void TestPanelLayoutManager::zOrderRoundTripsThroughPersistence()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("plm_zorder.sqlite"));

    ContestDatabase db1;
    QVERIFY(db1.open(dbPath, QStringLiteral("plm_zorder_1")));
    {
        QWidget canvasParent;
        PanelLayoutManager manager(db1, &canvasParent);
        PanelContainerWidget* a = manager.registerPanel(QStringLiteral("a"), QStringLiteral("A"), makeContent(),
                                                          false, QRect(0, 0, 200, 120));
        manager.registerPanel(QStringLiteral("b"), QStringLiteral("B"), makeContent(), false, QRect(0, 130, 200, 120));
        manager.registerPanel(QStringLiteral("c"), QStringLiteral("C"), makeContent(), false, QRect(0, 260, 200, 120));
        // No saved order yet on a first run -- a no-op, matching what
        // MainWindow's constructor actually does at startup.
        manager.finalizeInitialLayout();

        // "a" gets clicked/dragged to the front -- same signal
        // PanelContainerWidget's own eventFilter emits on a real mouse
        // press (see the class comment above for why this is invoked
        // directly rather than simulated).
        emit a->raiseRequested(QStringLiteral("a"));
    }
    db1.close();

    ContestDatabase db2;
    QVERIFY(db2.open(dbPath, QStringLiteral("plm_zorder_2")));
    // Registration order is a/b/c again -- MainWindow always registers
    // panels in the same fixed source-code order every launch;
    // finalizeInitialLayout() is what has to reconcile that back to the
    // persisted "b,c,a" stacking, not registration order by itself.
    QCOMPARE(db2.settingValue(QStringLiteral("PanelLayoutOrder")), QStringLiteral("b,c,a"));

    QWidget canvasParent2;
    PanelLayoutManager manager2(db2, &canvasParent2);
    manager2.registerPanel(QStringLiteral("a"), QStringLiteral("A"), makeContent(), false, QRect(0, 0, 200, 120));
    manager2.registerPanel(QStringLiteral("b"), QStringLiteral("B"), makeContent(), false, QRect(0, 130, 200, 120));
    manager2.registerPanel(QStringLiteral("c"), QStringLiteral("C"), makeContent(), false, QRect(0, 260, 200, 120));
    manager2.finalizeInitialLayout();

    // Saving again right after finalizeInitialLayout() must reproduce
    // the same order it just restored -- proves it actually rebuilt the
    // in-memory z-order list, not just raise()d widgets on screen
    // without updating what a later saveLayout() would write back out.
    manager2.saveLayout();
    QCOMPARE(db2.settingValue(QStringLiteral("PanelLayoutOrder")), QStringLiteral("b,c,a"));
}

void TestPanelLayoutManager::canvasShrinkClampsOffCanvasPanelBackOnScreen()
{
    // Reproduces this session's actual rotor-second-antenna bug at its
    // real root. The bug report: RotorWidget's second-antenna needle +
    // numbered mark rendered correctly for m_rotor1Widget but never for
    // m_rotor2Widget, regardless of its own settings. RotorWidget and
    // MainWindow's rotor code turned out to be byte-for-byte symmetric
    // between the two slots (confirmed by re-reading them, then by
    // qDebug()-ing actual runtime values -- both widgets got identical
    // per-instance geometry/center/radius and correctly distinct
    // second-antenna bearings). The real cause: PanelContainerWidget's
    // own class comment documents this dock mode as "absolute position
    // within a parent canvas, clamped to it" -- but that clamp only ran
    // for an interactive drag/resize (PanelContainerWidget::
    // updateDrag()/updateResize()), never for a geometry that was
    // simply registered/loaded (trySetGeometry(), at construction time,
    // before canvas() even has its real size -- see
    // clampPanelsToCanvas()'s own comment) and then left in place while
    // the canvas itself later resized smaller. In the live app, the
    // "rotorrow" panel's saved position (presumably valid against an
    // earlier, wider canvas) ended up hanging 155px off the actual
    // canvas's right edge; m_rotor2Widget, as the SECOND (rightmost)
    // child in that panel's QHBoxLayout, was the one whose right-hand
    // paint content -- exactly where an angled second needle/numbered
    // mark point -- fell into the overhanging, invisible region. The
    // widget painted correctly the whole time; the panel container just
    // never showed part of it.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("plm_clamp.sqlite")), QStringLiteral("plm_clamp")));

    QWidget canvasParent;
    PanelLayoutManager manager(db, &canvasParent);
    // Generously large to start with -- mirrors a canvas that was once
    // wide enough for this panel's saved position.
    manager.canvas()->resize(2000, 2000);

    PanelContainerWidget* panel = manager.registerPanel(QStringLiteral("p"), QStringLiteral("P"), makeContent(),
                                                          false, QRect(1500, 1500, 400, 300));
    QCOMPARE(panel->geometry(), QRect(1500, 1500, 400, 300));

    // The canvas then shrinks -- e.g. the real MainWindow's own window
    // being narrower on this particular launch than when the layout was
    // saved.
    resizeCanvas(manager, QSize(800, 600));

    const QRect after = panel->geometry();
    QVERIFY(after.x() >= 0);
    QVERIFY(after.y() >= 0);
    QVERIFY(after.x() + after.width() <= manager.canvas()->width());
    QVERIFY(after.y() + after.height() <= manager.canvas()->height());
    // The panel's own size is untouched -- the shrunken canvas (800x600)
    // is still comfortably big enough to hold 400x300 somewhere on
    // screen, it just needed to move, not shrink. Shrinking instead
    // would reintroduce the exact same class of bug one level down,
    // squeezing the panel's own content below whatever minimum size ITS
    // children need (exactly what would happen to the two RotorWidget
    // instances inside "rotorrow" if it were shrunk instead of moved).
    QCOMPARE(after.size(), QSize(400, 300));
}

void TestPanelLayoutManager::canvasResizeLeavesLockedPanelAloneEvenIfOffCanvas()
{
    // trySetGeometry() already refuses to move a locked panel for every
    // other caller (see lockedStateRoundTripsThroughPersistenceAndStillGatesMoves
    // above) -- the canvas-resize clamp respects the same rule rather
    // than silently unsticking a panel the operator deliberately locked
    // in place, even if that leaves it off-canvas.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("plm_clamp_locked.sqlite")), QStringLiteral("plm_clamp_locked")));

    QWidget canvasParent;
    PanelLayoutManager manager(db, &canvasParent);
    manager.canvas()->resize(2000, 2000);

    PanelContainerWidget* panel = manager.registerPanel(QStringLiteral("p"), QStringLiteral("P"), makeContent(),
                                                          false, QRect(1500, 1500, 400, 300));
    panel->setLocked(true);

    resizeCanvas(manager, QSize(800, 600));

    QCOMPARE(panel->geometry(), QRect(1500, 1500, 400, 300));
}

void TestPanelLayoutManager::canvasResizeLeavesOnCanvasPanelUntouched()
{
    // A panel that already comfortably fits must not be nudged or
    // resized by an unrelated canvas resize -- guards against an
    // overzealous clamp moving/shrinking panels that were never actually
    // hanging off the canvas.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("plm_clamp_fine.sqlite")), QStringLiteral("plm_clamp_fine")));

    QWidget canvasParent;
    PanelLayoutManager manager(db, &canvasParent);
    manager.canvas()->resize(2000, 2000);

    PanelContainerWidget* panel = manager.registerPanel(QStringLiteral("p"), QStringLiteral("P"), makeContent(),
                                                          false, QRect(10, 10, 300, 200));
    resizeCanvas(manager, QSize(800, 600)); // still plenty of room for (10,10,300x200)

    QCOMPARE(panel->geometry(), QRect(10, 10, 300, 200));
}

void TestPanelLayoutManager::editingOnePanelDoesNotPersistAnotherPanelsClampedGeometry()
{
    // Reproduces a real data-loss bug found live, 2026-09-12: registering
    // a brand-new "suggestion" panel (and the z-order churn that came
    // with it) alone was enough to silently overwrite four completely
    // unrelated, already-correct saved panel geometries with the much
    // smaller values clampPanelsToCanvas() had visually (not
    // persistently) squeezed them down to while the window happened to
    // be smaller than usual at that moment. Root cause: saveLayout()
    // used to write EVERY registered panel's CURRENT geometry on ANY
    // single panel's geometryEdited/lockedChanged/raiseRequested --
    // including panels nobody touched. This test pins the fix: editing
    // panel "b" must never cause panel "a" -- clamped by an unrelated
    // canvas shrink, but never itself edited -- to get a saved row at
    // all, let alone one holding its clamped geometry.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("plm_no_collateral_save.sqlite"));
    const QRect aOriginalDefault(1500, 1500, 400, 300);

    ContestDatabase db1;
    QVERIFY(db1.open(dbPath, QStringLiteral("plm_no_collateral_1")));
    {
        QWidget canvasParent;
        PanelLayoutManager manager(db1, &canvasParent);
        manager.canvas()->resize(2000, 2000);

        PanelContainerWidget* a =
            manager.registerPanel(QStringLiteral("a"), QStringLiteral("A"), makeContent(), false, aOriginalDefault);
        PanelContainerWidget* b = manager.registerPanel(QStringLiteral("b"), QStringLiteral("B"), makeContent(),
                                                          false, QRect(0, 0, 200, 120));

        // The window shrinks -- "a" gets visually clamped back on
        // screen; "b" already comfortably fits and is left untouched.
        resizeCanvas(manager, QSize(800, 600));
        QVERIFY(a->geometry() != aOriginalDefault); // sanity: the clamp actually moved it

        // The operator then drags/resizes "b" -- the one and only panel
        // actually edited this session.
        QVERIFY(b->trySetGeometry(QRect(10, 10, 250, 150)));
        emit b->geometryEdited();
    }
    db1.close();

    // "a" must never have gotten a saved row at all: a fresh open with a
    // DIFFERENT default for "a" must fall back to that new default, not
    // to whatever clamped geometry was on screen when "b" was saved.
    ContestDatabase db2;
    QVERIFY(db2.open(dbPath, QStringLiteral("plm_no_collateral_2")));
    // No row at all for "a" -- if the old bug were still present, this
    // key would hold "a"'s clamped, on-screen-at-the-time geometry.
    QVERIFY(db2.settingValue(QStringLiteral("PanelLayout_a")).isEmpty());
    // At least PanelContainerWidget::kMinWidth/kMinHeight so
    // trySetGeometry() doesn't itself bump this up to the min-size floor
    // and obscure what this test is actually checking.
    const QRect aFreshDefault(3, 3, 200, 100);
    QWidget canvasParent2;
    PanelLayoutManager manager2(db2, &canvasParent2);
    PanelContainerWidget* a2 =
        manager2.registerPanel(QStringLiteral("a"), QStringLiteral("A"), makeContent(), false, aFreshDefault);
    PanelContainerWidget* b2 = manager2.registerPanel(QStringLiteral("b"), QStringLiteral("B"), makeContent(), false,
                                                        QRect(0, 0, 200, 120));
    QCOMPARE(a2->geometry(), aFreshDefault);
    // "b"'s own real edit did round-trip correctly -- this fix is about
    // scoping the save, not about breaking the one save that should
    // still happen.
    QCOMPARE(b2->geometry(), QRect(10, 10, 250, 150));
}

// Two designs: the large one for a canvas of at least 1440×982, the
// compact one for anything smaller (a 13" MacBook's 1372×692), where a
// panel without a compact rect stays hidden. "Fenster zurücksetzen"
// picks by the canvas as it is.
void TestPanelLayoutManager::compactDesignFitsASmallCanvas()
{
    QVERIFY(PanelLayoutManager::canvasFitsLargeDesign(QSize(1440, 982)));
    QVERIFY(PanelLayoutManager::canvasFitsLargeDesign(QSize(1900, 1100)));
    QVERIFY(!PanelLayoutManager::canvasFitsLargeDesign(QSize(1372, 692)));
    QVERIFY(!PanelLayoutManager::canvasFitsLargeDesign(QSize(1440, 981)));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("plm_compact.sqlite")), QStringLiteral("plm_compact")));
    QWidget canvasParent;
    PanelLayoutManager manager(db, &canvasParent);
    manager.canvas()->resize(1372, 692);
    PanelContainerWidget* rotors = manager.registerPanel(QStringLiteral("rotorrow"), QStringLiteral("R"), makeContent(),
                                                           false, QRect(0, 78, 620, 365), QRect(0, 0, 620, 250));
    PanelContainerWidget* skeds = manager.registerPanel(QStringLiteral("skeds"), QStringLiteral("S"), makeContent(),
                                                          false, QRect(0, 720, 620, 262));
    QCOMPARE(rotors->geometry(), QRect(0, 78, 620, 365)); // registration alone: the large default
    QCOMPARE(manager.designWidth(), PanelLayoutManager::kCompactDesignCanvas.width());

    rotors->setLocked(true);
    manager.resetToDefaultLayout();
    QCOMPARE(rotors->geometry(), QRect(0, 0, 620, 250));
    QVERIFY(!rotors->isLocked());
    QVERIFY(skeds->isHidden()); // no place in the compact design

    // A canvas between the two designs (a 1080p monitor): the compact
    // design stretched to fill it.
    QCOMPARE(PanelLayoutManager::scaledCompactRect(QRect(0, 0, 620, 250), QSize(1372, 692)), QRect(0, 0, 620, 250));
    QCOMPARE(PanelLayoutManager::scaledCompactRect(QRect(630, 0, 742, 442), QSize(2744, 1384)), QRect(1260, 0, 1484, 884));
    QCOMPARE(PanelLayoutManager::scaledCompactRect(QRect(0, 258, 310, 184), QSize(1000, 500)), QRect(0, 258, 310, 184)); // never shrunk
    manager.canvas()->resize(1852, 900);
    manager.resetToDefaultLayout();
    QCOMPARE(rotors->geometry(), QRect(0, 0, qRound(620 * 1852 / 1372.0), qRound(250 * 900 / 692.0)));

    // A big canvas: the large design, and the panel is not hidden by it.
    manager.canvas()->resize(1600, 1100);
    QCOMPARE(manager.designWidth(), PanelLayoutManager::kLargeDesignCanvas.width());
    skeds->setVisible(true);
    manager.resetToDefaultLayout();
    QCOMPARE(rotors->geometry(), QRect(0, 78, 620, 365));
    QCOMPARE(skeds->geometry(), QRect(0, 720, 620, 262));
    QVERIFY(!skeds->isHidden());
}

void TestPanelLayoutManager::freshInstallPlacesPanelsByTheFittingDesignOnce()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("plm_fresh.sqlite")), QStringLiteral("plm_fresh")));
    QWidget canvasParent;
    PanelLayoutManager manager(db, &canvasParent);
    PanelContainerWidget* rotors = manager.registerPanel(QStringLiteral("rotorrow"), QStringLiteral("R"), makeContent(),
                                                           false, QRect(0, 78, 620, 365), QRect(0, 0, 620, 250));
    QVERIFY(!manager.hadSavedLayout());
    QSignalSpy applied(&manager, &PanelLayoutManager::initialDesignApplied);

    // Not declared fresh: a resize only clamps, as before.
    resizeCanvas(manager, QSize(1372, 692));
    QCOMPARE(applied.count(), 0);
    QCOMPARE(rotors->geometry(), QRect(0, 78, 620, 365));

    // Declared fresh: the first real canvas size places the compact
    // design; a resize within the settle time places it again (the
    // window shown large, then cut down to the screen), one after the
    // settle time does not.
    manager.setFreshInstall(true);
    manager.setDesignSettleMs(300);
    resizeCanvas(manager, QSize(1372, 821));
    QCOMPARE(applied.count(), 1);
    QCOMPARE(rotors->geometry(), QRect(0, 0, 620, qRound(250 * 821 / 692.0)));
    resizeCanvas(manager, QSize(1372, 692));
    QCOMPARE(applied.count(), 2);
    QCOMPARE(rotors->geometry(), QRect(0, 0, 620, 250));
    // Deutlich länger als die 300 ms Nachlaufzeit: ein einmaliger
    // QTimer feuert auf einem überlasteten Rechner später, als er
    // soll, und 400 ms liegen dafür zu knapp daran. Auf dem
    // macOS-Läufer von GitHub ist genau das am 2026-09-23 passiert --
    // die Nachlaufzeit war noch nicht abgelaufen, die dritte
    // Größenänderung zählte mit, und der Prüfstand sah drei statt
    // zwei. Gemessen wird hier "nach Ablauf der Nachlaufzeit", nicht
    // "nach 400 ms".
    QTest::qWait(1200);
    rotors->trySetGeometry(QRect(40, 40, 620, 250));
    resizeCanvas(manager, QSize(1380, 700));
    QCOMPARE(applied.count(), 2);
    QCOMPARE(rotors->geometry(), QRect(40, 40, 620, 250));

    // A database with a saved panel is never fresh.
    db.setSettingValue(QStringLiteral("PanelLayout_rotorrow"), QStringLiteral("5|5|620|250|false"));
    QWidget canvasParent2;
    PanelLayoutManager manager2(db, &canvasParent2);
    manager2.registerPanel(QStringLiteral("rotorrow"), QStringLiteral("R"), makeContent(), false, QRect(0, 78, 620, 365),
                           QRect(0, 0, 620, 250));
    QVERIFY(manager2.hadSavedLayout());
}

void TestPanelLayoutManager::profileSnapshotCountsUnshownPanelsAsVisible()
{
    // The first profile is snapshotted in MainWindow's constructor,
    // before anything is shown: a panel that was never hidden on
    // purpose must be recorded as visible, or the next start opens an
    // empty canvas.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("plm_profile.sqlite")), QStringLiteral("plm_profile")));
    QWidget canvasParent;
    PanelLayoutManager manager(db, &canvasParent);
    manager.registerPanel(QStringLiteral("rotorrow"), QStringLiteral("R"), makeContent(), false, QRect(0, 78, 620, 365));
    PanelContainerWidget* skeds = manager.registerPanel(QStringLiteral("skeds"), QStringLiteral("S"), makeContent(),
                                                          false, QRect(0, 720, 620, 262));
    skeds->setVisible(false);
    LayoutProfileManager profiles(db, manager, {QStringLiteral("rotorrow"), QStringLiteral("skeds")});
    QVERIFY(profiles.isFirstLaunch());
    const QString stored = db.settingValue(QStringLiteral("LayoutProfile_1"));
    QVERIFY2(stored.contains(QStringLiteral("rotorrow:1:0:78:620:365")), qPrintable(stored));
    QVERIFY2(stored.contains(QStringLiteral("skeds:0:0:720:620:262")), qPrintable(stored));

    // The same database again: not a first launch, the state restored
    // (on a canvas big enough to hold it -- applying a profile clamps).
    QWidget canvasParent2;
    PanelLayoutManager manager2(db, &canvasParent2);
    manager2.canvas()->resize(1440, 982);
    PanelContainerWidget* rotors2 = manager2.registerPanel(QStringLiteral("rotorrow"), QStringLiteral("R"), makeContent(),
                                                             false, QRect(0, 0, 100, 100));
    PanelContainerWidget* skeds2 = manager2.registerPanel(QStringLiteral("skeds"), QStringLiteral("S"), makeContent(),
                                                            false, QRect(0, 0, 100, 100));
    LayoutProfileManager profiles2(db, manager2, {QStringLiteral("rotorrow"), QStringLiteral("skeds")});
    QVERIFY(!profiles2.isFirstLaunch());
    QCOMPARE(rotors2->geometry(), QRect(0, 78, 620, 365));
    QVERIFY(!rotors2->isHidden());
    QVERIFY(skeds2->isHidden());
}

void TestPanelLayoutManager::revealedPanelIsPulledOntoTheCanvas()
{
    // Skeds hidden by the compact design keeps its large-design place
    // (y=720) -- switched on from Fenster > Panels on a 692 px canvas
    // it must land inside the canvas, in front.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("plm_reveal.sqlite")), QStringLiteral("plm_reveal")));
    QWidget canvasParent;
    PanelLayoutManager manager(db, &canvasParent);
    manager.canvas()->resize(1372, 692);
    PanelContainerWidget* skeds = manager.registerPanel(QStringLiteral("skeds"), QStringLiteral("S"), makeContent(),
                                                          false, QRect(0, 720, 620, 262));
    skeds->setVisible(false);
    QCOMPARE(skeds->geometry().top(), 720);
    skeds->setVisible(true);
    manager.revealPanel(QStringLiteral("skeds"));
    QCOMPARE(skeds->geometry(), QRect(0, 692 - 262, 620, 262));

    // A profile made on a bigger screen, applied here: clamped too.
    db.setSettingValue(QStringLiteral("LayoutProfileOrder"), QStringLiteral("1"));
    db.setSettingValue(QStringLiteral("LayoutProfileActive"), QStringLiteral("1"));
    db.setSettingValue(QStringLiteral("LayoutProfile_1"), QStringLiteral("skeds:1:900:800:620:262"));
    QWidget canvasParent2;
    PanelLayoutManager manager2(db, &canvasParent2);
    manager2.canvas()->resize(1372, 692);
    PanelContainerWidget* skeds2 = manager2.registerPanel(QStringLiteral("skeds"), QStringLiteral("S"), makeContent(),
                                                            false, QRect(0, 720, 620, 262));
    LayoutProfileManager profiles(db, manager2, {QStringLiteral("skeds")});
    QVERIFY(!profiles.isFirstLaunch());
    QCOMPARE(skeds2->geometry(), QRect(1372 - 620, 692 - 262, 620, 262));
}

void TestPanelLayoutManager::clampLeavesPanelsAloneWhileTheCanvasHasNoRealSize()
{
    // Before the window is laid out the canvas reports Qt's placeholder
    // size; a clamp into that would shrink every panel to its minimum
    // in the corner for good. Applying a profile in the constructor
    // must therefore not clamp yet (2026-09-21).
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("plm_placeholder.sqlite")), QStringLiteral("plm_placeholder")));
    db.setSettingValue(QStringLiteral("LayoutProfileOrder"), QStringLiteral("1"));
    db.setSettingValue(QStringLiteral("LayoutProfileActive"), QStringLiteral("1"));
    db.setSettingValue(QStringLiteral("LayoutProfile_1"), QStringLiteral("map:1:630:0:742:442"));
    QWidget canvasParent;
    PanelLayoutManager manager(db, &canvasParent);
    QVERIFY(manager.canvas()->width() < 300); // never laid out
    PanelContainerWidget* map = manager.registerPanel(QStringLiteral("map"), QStringLiteral("M"), makeContent(), false,
                                                        QRect(910, 78, 530, 501), QRect(630, 0, 742, 442));
    LayoutProfileManager profiles(db, manager, {QStringLiteral("map")});
    QCOMPARE(map->geometry(), QRect(630, 0, 742, 442));
    manager.clampPanelsToCanvas();
    manager.revealPanel(QStringLiteral("map"));
    QCOMPARE(map->geometry(), QRect(630, 0, 742, 442));

    // With a real size the clamp works as before.
    resizeCanvas(manager, QSize(1000, 400));
    QCOMPARE(map->geometry(), QRect(1000 - 742, 0, 742, 400));
}

void TestPanelLayoutManager::panelEditsSurviveARestartWithProfiles()
{
    // A dragged panel is persisted per panel (saveLayout) -- but the
    // next start applies the active profile on top, so the profile has
    // to follow every edit or the drag is undone at the next launch
    // (the case until 2026-09-21).
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("plm_persist.sqlite")), QStringLiteral("plm_persist")));
    {
        QWidget canvasParent;
        PanelLayoutManager manager(db, &canvasParent);
        manager.canvas()->resize(1440, 982);
        PanelContainerWidget* map = manager.registerPanel(QStringLiteral("map"), QStringLiteral("M"), makeContent(),
                                                            false, QRect(910, 78, 530, 501));
        LayoutProfileManager profiles(db, manager, {QStringLiteral("map")});
        map->trySetGeometry(QRect(100, 100, 530, 501));
        manager.saveLayout(QStringLiteral("map")); // what a drag's geometryEdited() does
        QVERIFY2(db.settingValue(QStringLiteral("LayoutProfile_1")).contains(QStringLiteral("map:1:100:100:530:501")),
                 qPrintable(db.settingValue(QStringLiteral("LayoutProfile_1"))));
    }
    QWidget canvasParent2;
    PanelLayoutManager manager2(db, &canvasParent2);
    manager2.canvas()->resize(1440, 982);
    PanelContainerWidget* map2 = manager2.registerPanel(QStringLiteral("map"), QStringLiteral("M"), makeContent(), false,
                                                          QRect(910, 78, 530, 501));
    LayoutProfileManager profiles2(db, manager2, {QStringLiteral("map")});
    QCOMPARE(map2->geometry(), QRect(100, 100, 530, 501));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestPanelLayoutManager tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_panellayoutmanager.moc"
