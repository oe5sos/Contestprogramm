#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;

namespace Contestprogramm {

class UtcClockWidget;

// Panel header strip shared by every panel-like container in the app,
// per HAUSSTIL.md rule 3 ("Jedes Panel hat denselben Kopf: 3 px
// Akzentbalken ... Titel versal mit weiter Laufweite").
//
// The design mockups (Main.dc.html/Rotor-A.dc.html) also show a "⠿"
// drag-handle glyph next to the accent bar. Checked against the real
// Longpath chrome (~/Longpath/NereusSDR/src/gui/containers/
// ContainerWidget.cpp, the title-bar construction in its constructor):
// there is no such glyph anywhere in the real header. The accent bar
// itself -- a plain 3px flat-coloured QLabel (kAmberText there) --
// *is* the drag handle (SizeAllCursor + "Drag to move this container"
// tooltip); "⠿" is a mockup invention, not real chrome. Contestprogramm
// has no docking/floating/dragging at all, so this widget renders just
// the accent bar as a static visual accent, with no glyph and no drag
// semantics grafted on for something that cannot actually be dragged.
class PanelHeaderBar : public QWidget {
    Q_OBJECT

public:
    explicit PanelHeaderBar(const QString& title, QWidget* parent = nullptr);

    void setTitle(const QString& title);

    // Dockable-panel lock affordance (movable/resizable/dockable panel
    // wave). Built here but hidden by default, so every existing
    // wrapInPanel()/PanelHeaderBar call site that never opts in (the
    // vast majority) is completely unaffected -- a hidden widget in a
    // QHBoxLayout takes no space. PanelContainerWidget is the one
    // caller that turns this on. A small bordered capsule (Style::
    // lockBadgeStyle(), see its own comment) rather than a bare icon,
    // per HAUSSTIL rule 5 ("Zustand steht als umrandete Kapsel") and
    // this wave's own design note: a lock icon should follow that same
    // restrained visual language, not a decorative addition.
    void setLockAffordanceEnabled(bool enabled);
    void setLocked(bool locked);
    bool isLocked() const { return m_locked; }

    // Per-panel quick-options affordance (⚙), the same reusable pattern
    // as the lock affordance above: built here, hidden by default, so
    // every existing wrapInPanel()/PanelHeaderBar call site that never
    // opts in stays completely unaffected. Modeled on the real Longpath
    // GridCellWidget precedent (~/Longpath/NereusSDR/src/gui/applets/
    // GridCellWidget.cpp buildCellButtons()): identical glyph (U+2699),
    // identical flat hover-highlight styling (Style::iconButtonStyle(),
    // ported from that file's own `btnCss`), same "only visible when
    // there is something to open" posture -- there gated on
    // AppletWidget::hasExtendedSettings(), here left to the caller
    // (setOptionsAffordanceEnabled()) since PanelHeaderBar has no
    // notion of what its panel's content can show. PanelHeaderBar only
    // raises the click as optionsRequested(); what opens (a popup, a
    // dialog, an inline flyout) is entirely the caller's choice -- see
    // RotorWidget's own self-painted-header equivalent for one concrete
    // answer (a QMenu), since RotorWidget cannot use this class
    // directly (it paints its own panel chrome, see its class comment).
    void setOptionsAffordanceEnabled(bool enabled);

    // A live UTC readout inside this header, opt-in and hidden by
    // default like the two affordances above. Added for the Log panel
    // specifically (operator, 2026-09-11: "uhrzeit fehlt, muss fix beim
    // logfenster sein" -- DXLog.net's own "Contest recorder" window
    // shows the clock fixed inside the log window itself, not only in a
    // separate global title bar). Reuses UtcClockWidget rather than a
    // second clock implementation; its countdown row is left off here
    // (setCountdownVisible defaults to false) -- this is a quick
    // always-there time reference beside the log, not a second copy of
    // the top bar's countdown.
    void setClockVisible(bool visible);

signals:
    // Emitted only when the operator actually clicks the lock button --
    // PanelContainerWidget connects this to its own setLocked() (single
    // source of truth), not the other way around, so setLocked() calls
    // driven by e.g. layout restore never loop back into a click.
    void lockToggled(bool locked);

    // Emitted only when the operator actually clicks the options
    // button -- same "click only" contract as lockToggled() above. The
    // caller owns what happens next (there is no PanelHeaderBar-side
    // state to keep in sync, unlike the lock's on/off badge).
    void optionsRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void updateLockButtonAppearance();

    QLabel* m_titleLabel;
    UtcClockWidget* m_clockWidget;
    QPushButton* m_lockButton;
    QPushButton* m_optionsButton;
    bool m_locked = false;
};

} // namespace Contestprogramm
