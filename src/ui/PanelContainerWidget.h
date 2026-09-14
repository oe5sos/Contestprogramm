#pragma once

#include <QPoint>
#include <QSize>
#include <QString>
#include <QWidget>

class QPushButton;

namespace Contestprogramm {

class PanelHeaderBar;

// Dockable/movable/resizable/lockable panel chrome -- a scoped port of
// Longpath's real ContainerWidget (~/Longpath/NereusSDR/src/gui/
// containers/ContainerWidget.h/.cpp). Longpath's ContainerWidget supports
// three dock modes (PanelDocked-in-a-QSplitter, OverlayDocked-absolute-
// position-over-the-central-widget, Floating-separate-top-level-window),
// axis-lock repositioning, tab-merging, and MMIO -- none of which
// Contestprogramm needs. This class ports exactly the one mode the
// operator actually asked for ("die windows müssen sich verschieben und
// anordnen lassen, layout speicherbar, mit schloss versperrbar"):
// Longpath's OverlayDocked behaviour -- absolute position within a parent
// canvas, clamped to it, dragged via the header, resized via a single
// bottom-right corner grip, lockable. See PanelLayoutManager's class
// comment for the layout-manager side of the same scoping decision, and
// this task's own report for why floating (separate top-level windows)
// was left out: real ContainerWidget's floating mode needs a whole
// second class (FloatingContainer, native-window reparenting) for a
// "nice-to-have", not a hard requirement.
//
// Two chrome modes, both already established in this codebase rather
// than invented here:
//
//  - Header mode (contentHasOwnChrome = false): builds the same
//    QFrame-style panel + PanelHeaderBar structure MainWindow's own
//    wrapInPanel() helper already built for the log/chat panels, just
//    as a stateful QWidget instead of a one-shot free function, plus
//    the header's own lock affordance (PanelHeaderBar::
//    setLockAffordanceEnabled). Used for UnifiedLogWidget (entry row +
//    log history + spot/chat candidates, one merged panel -- see ui/
//    UnifiedLogWidget.h), RateMeterWidget, the CW macro row, and the
//    rotor compass row (m_rotorRow itself paints nothing -- it is a
//    bare QHBoxLayout holder; its *children*, the individual
//    RotorWidgets, each paint their own smaller header, which reads as
//    "one instrument group, each instrument still labelled" rather
//    than a doubled header).
//
//  - Chromeless mode (contentHasOwnChrome = true): RotorWidget and
//    MapWidget already paint their own full panel chrome -- background,
//    border, and a PanelHeaderBar-style 3px accent-bar header with a
//    title -- directly in their own paintEvent() (see
//    RotorWidget::drawPanelHeader/MapWidget::drawPanelHeader). Wrapping
//    one of those in a second PanelHeaderBar would draw two stacked
//    headers. This mode adds only the *interaction* affordances --
//    a drag zone over the content's own painted header band, a corner
//    resize grip, and a small overlaid lock badge -- with no extra
//    painted chrome of its own. This is exactly the real
//    ContainerWidget's own m_noControls concept (Thetis ucMeter.cs
//    "_no_controls ... no title or resize grabber"), used for the same
//    reason: content that already draws its own controls should not
//    get a second set drawn over it. Used for MapWidget only --
//    Contestprogramm has no other self-chroming content widget wired
//    into the dockable set (RotorWidget instances live *inside* the
//    rotor row's header-mode container, they are not registered as
//    their own panels).
class PanelContainerWidget : public QWidget {
    Q_OBJECT

public:
    static constexpr int kMinWidth = 160;
    static constexpr int kMinHeight = 72;
    static constexpr int kResizeGripSize = 14;
    // Matches RotorWidget/MapWidget's own kHeaderHeight (28px) exactly --
    // see the chromeless-mode class comment above. A click below this
    // band in chromeless mode is left alone (so MapWidget's own zoom
    // buttons etc. still work) rather than starting a drag.
    static constexpr int kChromelessDragBandHeight = 28;

    // `content` becomes this container's child (reparented in the
    // constructor); `id` becomes both this widget's objectName (kept
    // stable for findChild<QWidget*>() lookups, the same purpose the
    // pre-docking m_cwRow objectName already served) and the key
    // PanelLayoutManager persists this panel's geometry/lock state
    // under.
    PanelContainerWidget(const QString& id, const QString& title, QWidget* content,
                          bool contentHasOwnChrome, QWidget* parent = nullptr);

    QString id() const { return m_id; }

    bool isLocked() const { return m_locked; }
    void setLocked(bool locked);

    // Applies `rect` (clamped to the minimum size above) unless this
    // panel is locked, in which case it is a no-op and this returns
    // false -- the one gate every drag/resize/programmatic move funnels
    // through (mirrors ContainerWidget's own eventFilter "!m_locked"
    // guard) and the entry point PanelLayoutManager/tests use to verify
    // lock behavior without simulating real QMouseEvents.
    bool trySetGeometry(const QRect& rect);

    QWidget* content() const { return m_content; }

    // Null in chromeless mode (contentHasOwnChrome=true -- see the class
    // comment), since there is no PanelHeaderBar there to return. Added
    // so a caller can opt a specific panel into PanelHeaderBar's own
    // per-instance affordances (e.g. setClockVisible()) without
    // PanelContainerWidget needing to forward every such method itself.
    PanelHeaderBar* headerBar() const { return m_headerBar; }

signals:
    // Emitted once, after a user-driven drag or resize completes (NOT
    // for a programmatic trySetGeometry() call, e.g. from layout
    // restore) -- PanelLayoutManager saves the layout on this.
    void geometryEdited();
    void lockedChanged(bool locked);
    // Emitted on the mouse press that starts a drag or resize --
    // PanelLayoutManager bumps this panel to the front of its persisted
    // z-order on this signal. The container already raise()s itself
    // immediately regardless of whether anything is listening.
    void raiseRequested(const QString& id);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildChromelessOverlay();
    void updateChromelessLockBadge();
    void layoutOverlayWidgets();

    void beginDrag(const QPoint& globalPos);
    void updateDrag(const QPoint& globalPos);
    void endDrag();
    void beginResize(const QPoint& globalPos);
    void updateResize(const QPoint& globalPos);
    void endResize();

    QString m_id;
    bool m_locked = false;
    bool m_contentHasOwnChrome;
    QWidget* m_content = nullptr;

    // Header mode only.
    PanelHeaderBar* m_headerBar = nullptr;

    // Chromeless mode only -- a small overlay lock badge raised above
    // `content`, since there is no PanelHeaderBar to host one.
    QPushButton* m_overlayLockButton = nullptr;

    // Both modes -- overlaid via raise()/manual geometry (resizeEvent),
    // not part of the layout, matching the real ContainerWidget's own
    // single bottom-right resize grip.
    QWidget* m_resizeGrip = nullptr;

    bool m_dragging = false;
    QPoint m_dragStartOffset; // globalPos - pos() at drag start
    bool m_resizing = false;
    QPoint m_resizeStartGlobal;
    QSize m_resizeStartSize;
};

} // namespace Contestprogramm
