#include "ui/PanelContainerWidget.h"

#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace Contestprogramm {

PanelContainerWidget::PanelContainerWidget(const QString& id, const QString& title, QWidget* content,
                                            bool contentHasOwnChrome, QWidget* parent)
    : QWidget(parent)
    , m_id(id)
    , m_contentHasOwnChrome(contentHasOwnChrome)
    , m_content(content)
{
    // Kept stable across the panel's lifetime -- the same purpose the
    // pre-docking cwMacroRow objectName already served (a test hook),
    // now also PanelLayoutManager's persistence key. Set via the
    // two-argument Style::applyPanelFrameStyle() overload below in
    // header mode (which needs this exact name as its QSS ID selector,
    // not the shared "contestPanelFrame" one), or directly here in
    // chromeless mode, which draws no frame of its own.
    setObjectName(id);
    setToolTip(title);
    content->setParent(this);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(0);

    if (contentHasOwnChrome) {
        // See the class comment: MapWidget already paints its own full
        // panel chrome including a header band -- no frame, no
        // PanelHeaderBar here, content fills the container completely.
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(content, 1);
        content->installEventFilter(this);
        buildChromelessOverlay();
    } else {
        // Same structure MainWindow's wrapInPanel() free function
        // already builds (QFrame-style panel + PanelHeaderBar + 1px
        // margin so the rounded border stays visible all the way
        // around), just as this stateful widget's own chrome instead
        // of a helper called once. See StyleKit.h for why the
        // two-argument overload (this panel's own id, not the shared
        // "contestPanelFrame" name) is required here.
        Style::applyPanelFrameStyle(this, id);
        layout->setContentsMargins(1, 1, 1, 1);
        m_headerBar = new PanelHeaderBar(title, this);
        m_headerBar->setLockAffordanceEnabled(true);
        m_headerBar->installEventFilter(this);
        connect(m_headerBar, &PanelHeaderBar::lockToggled, this, &PanelContainerWidget::setLocked);
        layout->addWidget(m_headerBar);
        layout->addWidget(content, 1);
    }

    m_resizeGrip = new QWidget(this);
    m_resizeGrip->setFixedSize(kResizeGripSize, kResizeGripSize);
    m_resizeGrip->setCursor(Qt::SizeFDiagCursor);
    m_resizeGrip->setToolTip(QStringLiteral("Ziehen zum Verändern der Größe"));
    m_resizeGrip->setStyleSheet(QStringLiteral("background: %1; border-radius: %2px;")
                                     .arg(Style::kTextInactive())
                                     .arg(kResizeGripSize / 2));
    m_resizeGrip->installEventFilter(this);

    setMinimumSize(kMinWidth, kMinHeight);
    layoutOverlayWidgets();
}

void PanelContainerWidget::buildChromelessOverlay()
{
    m_overlayLockButton = new QPushButton(this);
    m_overlayLockButton->setFixedSize(24, 18);
    m_overlayLockButton->setCursor(Qt::ArrowCursor);
    connect(m_overlayLockButton, &QPushButton::clicked, this, [this]() {
        setLocked(!m_locked);
    });
    updateChromelessLockBadge();
}

void PanelContainerWidget::updateChromelessLockBadge()
{
    if (!m_overlayLockButton) {
        return;
    }
    m_overlayLockButton->setText(m_locked ? QString::fromUtf8("\U0001F512")
                                           : QString::fromUtf8("\U0001F513"));
    m_overlayLockButton->setToolTip(m_locked
        ? QStringLiteral("Gesperrt -- klicken zum Entsperren")
        : QStringLiteral("Panel sperren"));
    m_overlayLockButton->setStyleSheet(Style::lockBadgeStyle(m_locked));
}

void PanelContainerWidget::setLocked(bool locked)
{
    if (m_locked == locked) {
        return;
    }
    m_locked = locked;
    if (m_headerBar) {
        m_headerBar->setLocked(locked);
    }
    updateChromelessLockBadge();
    m_resizeGrip->setCursor(locked ? Qt::ArrowCursor : Qt::SizeFDiagCursor);
    emit lockedChanged(locked);
}

bool PanelContainerWidget::trySetGeometry(const QRect& rect)
{
    if (m_locked) {
        return false;
    }
    QRect clamped = rect;
    // Bench-found live, 2026-09-13 (operator screenshot): kMinWidth
    // (160) is a generic floor for whatever ends up hosted here, not a
    // promise that any given content widget can still draw legibly at
    // that width -- MapWidget's own minimumSizeHint() is 300px, and a
    // fresh/default-geometry launch squeezed its panel down to a sliver
    // well under that. A header bar spans the container's full width
    // regardless of chrome mode, so the content's minimum width IS the
    // container's real minimum width -- unlike height, where header-mode
    // panels add their own header strip on top of the content, this
    // needs no such offset. Widgets with no meaningful minimumSizeHint()
    // (the common case) return (0, 0)/invalid here, so std::max leaves
    // kMinWidth as the effective floor for them, unchanged from before.
    const int contentMinWidth = m_content ? m_content->minimumSizeHint().width() : 0;
    clamped.setWidth(std::max({kMinWidth, contentMinWidth, clamped.width()}));
    clamped.setHeight(std::max(kMinHeight, clamped.height()));
    setGeometry(clamped);
    return true;
}

void PanelContainerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutOverlayWidgets();
}

void PanelContainerWidget::layoutOverlayWidgets()
{
    if (m_resizeGrip) {
        m_resizeGrip->move(width() - kResizeGripSize - 2, height() - kResizeGripSize - 2);
        m_resizeGrip->raise();
    }
    if (m_overlayLockButton) {
        m_overlayLockButton->move(width() - m_overlayLockButton->width() - 6, 6);
        m_overlayLockButton->raise();
    }
}

void PanelContainerWidget::beginDrag(const QPoint& globalPos)
{
    m_dragging = true;
    raise();
    m_dragStartOffset = globalPos - pos();
}

void PanelContainerWidget::updateDrag(const QPoint& globalPos)
{
    if (!m_dragging) {
        return;
    }
    QPoint newPos = globalPos - m_dragStartOffset;
    // Clamped to the parent canvas -- same overlay-docked behavior as
    // ContainerWidget::updateDrag()'s non-floating branch.
    if (parentWidget()) {
        const int maxX = std::max(0, parentWidget()->width() - width());
        const int maxY = std::max(0, parentWidget()->height() - height());
        newPos.setX(std::clamp(newPos.x(), 0, maxX));
        newPos.setY(std::clamp(newPos.y(), 0, maxY));
    }
    if (pos() != newPos) {
        move(newPos);
    }
}

void PanelContainerWidget::endDrag()
{
    if (!m_dragging) {
        return;
    }
    m_dragging = false;
    emit geometryEdited();
}

void PanelContainerWidget::beginResize(const QPoint& globalPos)
{
    m_resizing = true;
    raise();
    m_resizeStartGlobal = globalPos;
    m_resizeStartSize = size();
}

void PanelContainerWidget::updateResize(const QPoint& globalPos)
{
    if (!m_resizing) {
        return;
    }
    const int dx = globalPos.x() - m_resizeStartGlobal.x();
    const int dy = globalPos.y() - m_resizeStartGlobal.y();
    int newW = std::max(kMinWidth, m_resizeStartSize.width() + dx);
    int newH = std::max(kMinHeight, m_resizeStartSize.height() + dy);
    if (parentWidget()) {
        newW = std::min(newW, std::max(kMinWidth, parentWidget()->width() - x()));
        newH = std::min(newH, std::max(kMinHeight, parentWidget()->height() - y()));
    }
    if (QSize(newW, newH) != size()) {
        resize(newW, newH);
    }
}

void PanelContainerWidget::endResize()
{
    if (!m_resizing) {
        return;
    }
    m_resizing = false;
    emit geometryEdited();
}

bool PanelContainerWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_resizeGrip) {
        if (m_locked) {
            return QWidget::eventFilter(watched, event);
        }
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                emit raiseRequested(m_id);
                beginResize(me->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_resizing) {
            updateResize(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_resizing) {
            endResize();
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }

    // Drag surface: the header bar in header mode, or the content
    // widget's own self-painted header band in chromeless mode --
    // mirrors ContainerWidget's own eventFilter on m_titleBar/
    // m_titleLabel/m_grip, gated on "!m_locked" the same way.
    const bool isDragSurface = (watched == m_headerBar) || (m_contentHasOwnChrome && watched == m_content);
    if (isDragSurface && !m_locked) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() != Qt::LeftButton) {
                return QWidget::eventFilter(watched, event);
            }
            if (m_contentHasOwnChrome && me->position().y() >= kChromelessDragBandHeight) {
                // Below the content's own painted header band -- leave
                // it alone so e.g. MapWidget's own buttons still work.
                return QWidget::eventFilter(watched, event);
            }
            emit raiseRequested(m_id);
            beginDrag(me->globalPosition().toPoint());
            return true;
        }
        if (event->type() == QEvent::MouseMove && m_dragging) {
            updateDrag(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
            endDrag();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

} // namespace Contestprogramm
