#include <QtTest>

#include <QApplication>
#include <QLabel>
#include <QResizeEvent>
#include <QTemporaryDir>

#include "data/ContestDatabase.h"
#include "ui/PanelContainerWidget.h"
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

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestPanelLayoutManager tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_panellayoutmanager.moc"
