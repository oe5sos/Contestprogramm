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
                                         bool contentHasOwnChrome, const QRect& defaultGeometry);

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

protected:
    // Watches canvas() for QEvent::Resize -- see clampPanelsToCanvas()'s
    // own comment for why the clamp has to live here rather than in
    // PanelContainerWidget::trySetGeometry() itself.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct PanelEntry {
        PanelContainerWidget* container = nullptr;
        QRect defaultGeometry;
    };

    void loadLayoutForPanel(const QString& id, PanelContainerWidget* container, const QRect& defaultGeometry);
    void bumpZOrder(const QString& id);
    // Re-clamps every registered, unlocked panel to stay fully within
    // canvas()'s current bounds -- called whenever the canvas actually
    // changes size (see eventFilter() above). PanelContainerWidget's own
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
    void clampPanelsToCanvas();

    ContestDatabase& m_database;
    QWidget* m_canvas = nullptr;
    QMap<QString, PanelEntry> m_panels;
    QStringList m_zOrder; // bottom to top; persisted as the id list itself, mirrors ContainerIdList.
};

} // namespace Contestprogramm
