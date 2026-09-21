#pragma once

#include <QMap>
#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>

class QEvent;
class QWidget;

namespace Contestprogramm {

class ContestDatabase;
class PanelContainerWidget;

// Port of Longpath's real ContainerManager (~/Longpath/NereusSDR/src/gui/
// containers/ContainerManager.h/.cpp): owns the set of docked panels,
// their geometry/locked-state/z-order, and persists that layout the same
// way the real ContainerManager does -- a per-panel serialized string
// (ContainerWidget::serialize()'s own pipe-joined-fields shape) under its
// own key, plus a comma-joined id list, both through a plain key/value
// store (Longpath: AppSettings, an XML file under ~/.config/Longpath/;
// Contestprogramm: this project's own ContestDatabase `settings` table,
// via settingValue()/setSettingValue() -- the same store ContestSettings::
// loadFrom/saveTo already use, not a new file).
//
// Scoped to the one dock mode Contestprogramm actually needs -- see
// PanelContainerWidget's class comment for the panel-chrome side of the
// same scoping decision. Longpath's other two dock modes (PanelDocked in
// a QSplitter, Floating as a separate top-level window via
// FloatingContainer) and its axis-lock/tab/MMIO machinery are out of
// scope: this manager only ever places panels absolutely within one
// canvas widget, exactly like Longpath's OverlayDocked mode.
class PanelLayoutManager : public QObject {
    Q_OBJECT

public:
    // `database` is a non-owning reference; the caller (MainWindow, via
    // AppController::database()) keeps it alive for as long as this
    // manager exists -- the same non-owning-pointer pattern
    // RateMeterWidget::setSource() already uses for ContestDatabase.
    // `canvasParent` becomes the parent of the canvas widget this
    // manager creates; canvas() hands that widget back to MainWindow to
    // place in its own central layout.
    explicit PanelLayoutManager(ContestDatabase& database, QWidget* canvasParent, QObject* parent = nullptr);

    QWidget* canvas() const { return m_canvas; }

    // Wraps `content` in a new PanelContainerWidget parented to
    // canvas(), positions/sizes it from the saved layout if one exists
    // for `id`, or from `defaultGeometry` on first run, and wires it
    // into this manager's persistence (geometryEdited/lockedChanged ->
    // an immediate save; raiseRequested -> a z-order bump, also saved
    // immediately -- there is no debounce: every edit is already a
    // single drag-end/resize-end/click event, not a per-pixel stream,
    // so saving right away is both simpler and safer against a crash
    // or force-quit losing the last edit). `contentHasOwnChrome` is
    // forwarded to PanelContainerWidget -- see its class comment.
    PanelContainerWidget* registerPanel(const QString& id, const QString& title, QWidget* content,
                                         bool contentHasOwnChrome, const QRect& defaultGeometry,
                                         const QRect& compactGeometry = QRect());

    // Two default designs. `defaultGeometry` (registerPanel) is the
    // large one, laid out for a canvas of kLargeDesignCanvas; a canvas
    // that cannot hold it -- a 13" MacBook's is 1372×692 -- gets the
    // compact one (`compactGeometry`), and a panel registered without
    // a compact rect stays hidden there. Found 2026-09-21: a fresh
    // install on the operator's own MacBook Air showed the large design
    // clamped into a canvas 290 px too low, a heap of overlapping
    // panels, which the first profile then saved as its layout.
    static constexpr QSize kLargeDesignCanvas{1440, 982};
    static constexpr QSize kCompactDesignCanvas{1372, 692};
    static bool canvasFitsLargeDesign(const QSize& canvasSize);
    // A compact-design rect for a canvas larger than kCompactDesignCanvas
    // (stretched to fill, never shrunk).
    static QRect scaledCompactRect(const QRect& rect, const QSize& canvasSize);
    // The width the design in force is laid out for -- the large one's
    // when the canvas holds it, else the compact one's.
    int designWidth() const;

    // Whether any panel had a saved geometry when it was registered.
    // MainWindow declares the install fresh (setFreshInstall(true)) when
    // neither that nor a layout profile existed; only then does the
    // first real canvas size place the panels by the fitting design.
    bool hadSavedLayout() const { return m_sawSavedLayout; }
    void setFreshInstall(bool fresh) { m_freshInstall = fresh; }

    // Puts every panel where the design for the canvas's current size
    // says (and unlocks it); the compact design also hides the panels
    // it has no place for.
    void applyDesignDefaults();

signals:
    // A fresh install's panels were just placed by the design fitting
    // the canvas's real size (the first time it had one) -- the moment
    // to snapshot the first profile.
    void initialDesignApplied();
    // saveLayout() just persisted a panel edit (drag, resize, lock) or
    // a reset -- LayoutProfileManager keeps the active profile in step
    // with it (found 2026-09-21: the profile, applied on every start,
    // otherwise undid every drag at the next launch).
    void layoutSaved();

public:
    PanelContainerWidget* panel(const QString& id) const;

    // Call once, after every registerPanel() call for this session has
    // been made (MainWindow's constructor does this at the very end of
    // panel setup): reconciles the initial on-screen stacking order
    // with the persisted z-order, if any was saved. Split out from
    // registerPanel() itself because the full saved order can only be
    // meaningfully applied once every panel it might mention actually
    // exists.
    void finalizeInitialLayout();

    // Restores every registered panel to the `defaultGeometry` it was
    // registered with, and unlocks it -- the "Fenster zurücksetzen" menu
    // action's implementation. Deliberately leaves the current z-order
    // alone (an operator asking to reset position/size is not also
    // asking to lose the panel they most recently brought to front).
    void resetToDefaultLayout();

    // Writes one registered panel's current geometry/locked-state to the
    // settings store, plus the current z-order, right now. `onlyId`
    // empty (the default) means "every registered panel" -- used only by
    // resetToDefaultLayout(), where every panel's geometry has genuinely
    // just changed. Every other caller (a single panel's geometryEdited/
    // lockedChanged/raiseRequested) passes that one panel's own id --
    // see this method's own .cpp doc comment for why writing every OTHER
    // panel's CURRENT geometry on every single-panel edit is actively
    // wrong, not just wasteful.
    void saveLayout(const QString& onlyId = QString());

    // Brings `id` to the front and records it in the persisted z-order
    // -- what a header click does, for callers without a mouse (the
    // Panels menu switching a hidden panel on).
    void raisePanel(const QString& id);
    // A panel switched on from Fenster > Panels: to the front, and
    // pulled onto the canvas if its remembered place lies outside it
    // (the large design's spot on a small screen).
    void revealPanel(const QString& id);
    // Re-clamps every unlocked panel into the canvas -- run on every
    // canvas resize (see eventFilter), and by LayoutProfileManager after
    // a profile made on a bigger screen was applied.
    void clampPanelsToCanvas();

protected:
    // Watches canvas() for QEvent::Resize -- see clampPanelsToCanvas()'s
    // own comment for why the clamp has to live here rather than in
    // PanelContainerWidget::trySetGeometry() itself.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct PanelEntry {
        PanelContainerWidget* container = nullptr;
        QRect defaultGeometry;
        QRect compactGeometry; // null: not part of the compact design
    };

    void loadLayoutForPanel(const QString& id, PanelContainerWidget* container, const QRect& defaultGeometry);
    void bumpZOrder(const QString& id);
    void clampPanelToCanvas(PanelContainerWidget* container);
    // False while the canvas still has Qt's pre-layout placeholder size.
    bool canvasHasRealSize() const;
    // clampPanelsToCanvas() re-clamps every registered, unlocked panel
    // to stay fully within canvas()'s current bounds -- called whenever
    // the canvas actually changes size (see eventFilter() above). PanelContainerWidget's own
    // class comment documents this dock mode as "absolute position
    // within a parent canvas, clamped to it", but that clamp previously
    // only ran for an interactive drag/resize (PanelContainerWidget::
    // updateDrag()/updateResize()) -- never for a geometry that was
    // loaded from a save (or the hardcoded default) via trySetGeometry()
    // and then simply left in place while the canvas itself later
    // resized. The fix can't live inside trySetGeometry() itself: that
    // is also the path registerPanel()/loadLayoutForPanel() call during
    // MainWindow's constructor, before canvas() has its real on-screen
    // size -- clamping there would silently shrink every panel's default
    // position down to whatever placeholder size a not-yet-shown QWidget
    // reports (see test_panellayoutmanager.cpp's own never-shown
    // canvasParent for a concrete case that would break). Reacting to an
    // actual QEvent::Resize sidesteps that: it only ever fires once
    // canvas() has a real, current size, whether that's the first
    // real layout pass at startup or a later live window resize.

    ContestDatabase& m_database;
    QWidget* m_canvas = nullptr;
    bool m_sawSavedLayout = false;
    bool m_freshInstall = false;
    bool m_initialDesignApplied = false;
    QMap<QString, PanelEntry> m_panels;
    QStringList m_zOrder; // bottom to top; persisted as the id list itself, mirrors ContainerIdList.
};

} // namespace Contestprogramm
