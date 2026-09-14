#include "ui/PanelLayoutManager.h"

#include "data/ContestDatabase.h"
#include "ui/PanelContainerWidget.h"

#include <QEvent>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace Contestprogramm {

namespace {

QString layoutKeyFor(const QString& id)
{
    return QStringLiteral("PanelLayout_%1").arg(id);
}

const QString kOrderKey = QStringLiteral("PanelLayoutOrder");

} // namespace

PanelLayoutManager::PanelLayoutManager(ContestDatabase& database, QWidget* canvasParent, QObject* parent)
    : QObject(parent)
    , m_database(database)
{
    m_canvas = new QWidget(canvasParent);
    m_canvas->setObjectName(QStringLiteral("panelCanvas"));
    // See clampPanelsToCanvas()'s own comment -- this is what actually
    // catches the canvas settling into its real on-screen size (and any
    // later live resize), the one moment a canvas-bounds clamp can be
    // applied correctly.
    m_canvas->installEventFilter(this);
}

bool PanelLayoutManager::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_canvas && event->type() == QEvent::Resize) {
        clampPanelsToCanvas();
    }
    return QObject::eventFilter(watched, event);
}

PanelContainerWidget* PanelLayoutManager::registerPanel(const QString& id, const QString& title, QWidget* content,
                                                         bool contentHasOwnChrome, const QRect& defaultGeometry)
{
    auto* container = new PanelContainerWidget(id, title, content, contentHasOwnChrome, m_canvas);

    // Scoped to `id` -- see saveLayout()'s own doc comment for why a
    // save triggered by ONE panel must not also re-persist every OTHER
    // panel's current geometry (which can be a transient
    // clampPanelsToCanvas() shrink, not a real edit).
    connect(container, &PanelContainerWidget::geometryEdited, this, [this, id]() { saveLayout(id); });
    connect(container, &PanelContainerWidget::lockedChanged, this, [this, id](bool) { saveLayout(id); });
    connect(container, &PanelContainerWidget::raiseRequested, this, &PanelLayoutManager::bumpZOrder);

    PanelEntry entry;
    entry.container = container;
    entry.defaultGeometry = defaultGeometry;
    m_panels.insert(id, entry);

    if (!m_zOrder.contains(id)) {
        m_zOrder.append(id);
    }

    loadLayoutForPanel(id, container, defaultGeometry);
    container->show();
    container->raise();
    return container;
}

PanelContainerWidget* PanelLayoutManager::panel(const QString& id) const
{
    const auto it = m_panels.constFind(id);
    return it == m_panels.constEnd() ? nullptr : it.value().container;
}

void PanelLayoutManager::loadLayoutForPanel(const QString& id, PanelContainerWidget* container,
                                             const QRect& defaultGeometry)
{
    const QString raw = m_database.settingValue(layoutKeyFor(id));
    if (raw.isEmpty()) {
        container->trySetGeometry(defaultGeometry);
        return;
    }

    // x|y|w|h|locked -- same pipe-joined-fields shape ContainerWidget::
    // serialize() uses, scoped down to the fields this simpler system
    // actually has.
    const QStringList parts = raw.split(QLatin1Char('|'));
    if (parts.size() < 5) {
        container->trySetGeometry(defaultGeometry);
        return;
    }

    bool xOk = false;
    bool yOk = false;
    bool wOk = false;
    bool hOk = false;
    const int x = parts[0].toInt(&xOk);
    const int y = parts[1].toInt(&yOk);
    const int w = parts[2].toInt(&wOk);
    const int h = parts[3].toInt(&hOk);
    if (!xOk || !yOk || !wOk || !hOk) {
        container->trySetGeometry(defaultGeometry);
        return;
    }

    // Geometry first, then lock -- restoring a locked panel's geometry
    // through the same trySetGeometry() gate everything else uses would
    // otherwise be a no-op, exactly the same ordering restoreState()
    // uses for ContainerWidget::deserialize() + setLocked().
    container->trySetGeometry(QRect(x, y, w, h));
    container->setLocked(parts[4].compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
}

void PanelLayoutManager::finalizeInitialLayout()
{
    const QString orderRaw = m_database.settingValue(kOrderKey);
    if (orderRaw.isEmpty()) {
        return;
    }

    const QStringList savedOrder = orderRaw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    QStringList applied;
    for (const QString& id : savedOrder) {
        if (PanelContainerWidget* container = panel(id)) {
            container->raise();
            applied.append(id);
        }
    }
    // Panels the saved order didn't mention (registered this session
    // but never saved before) keep whatever stacking the loop above
    // already put them in relative to each other -- append them after
    // the saved ones, in registration order, so the very next
    // saveLayout() call persists the order actually on screen instead
    // of the pre-restore registration order still sitting in m_zOrder.
    for (const QString& id : std::as_const(m_zOrder)) {
        if (!applied.contains(id)) {
            applied.append(id);
        }
    }
    m_zOrder = applied;
}

void PanelLayoutManager::bumpZOrder(const QString& id)
{
    if (!m_zOrder.contains(id)) {
        return;
    }
    m_zOrder.removeAll(id);
    m_zOrder.append(id);
    // A raise never changes `id`'s own geometry -- scoped the same as
    // every other single-panel save (see saveLayout()'s doc comment);
    // the order itself is always written in full below regardless.
    saveLayout(id);
}

void PanelLayoutManager::resetToDefaultLayout()
{
    for (auto it = m_panels.constBegin(); it != m_panels.constEnd(); ++it) {
        PanelContainerWidget* container = it.value().container;
        // Unlock first -- trySetGeometry() is a no-op while locked, and
        // "reset" means the operator wants their panels back where they
        // started, not to stay stuck wherever they were locked.
        container->setLocked(false);
        container->trySetGeometry(it.value().defaultGeometry);
    }
    saveLayout();
}

void PanelLayoutManager::clampPanelsToCanvas()
{
    // A locked panel is frozen exactly where the operator put it --
    // trySetGeometry() already refuses to move a locked panel for any
    // other caller (see PanelContainerWidget::trySetGeometry()), so this
    // leaves locked panels alone too rather than silently unsticking
    // them out from under a lock.
    for (auto it = m_panels.constBegin(); it != m_panels.constEnd(); ++it) {
        PanelContainerWidget* container = it.value().container;
        if (!container || container->isLocked()) {
            continue;
        }
        const QRect current = container->geometry();
        QRect clamped = current;
        // Shrink first (mirrors PanelContainerWidget::updateResize()'s
        // own min-size floor), then reposition -- a panel that is both
        // too far right/down AND wider/taller than the canvas needs both
        // to end up fully on-canvas.
        clamped.setWidth(std::min(clamped.width(), std::max(PanelContainerWidget::kMinWidth, m_canvas->width())));
        clamped.setHeight(std::min(clamped.height(), std::max(PanelContainerWidget::kMinHeight, m_canvas->height())));
        const int maxX = std::max(0, m_canvas->width() - clamped.width());
        const int maxY = std::max(0, m_canvas->height() - clamped.height());
        clamped.moveLeft(std::clamp(clamped.left(), 0, maxX));
        clamped.moveTop(std::clamp(clamped.top(), 0, maxY));
        if (clamped != current) {
            // trySetGeometry() only (setGeometry() + the min-size floor
            // it already enforces) -- deliberately NOT geometryEdited(),
            // which would persist this on every single resize tick of a
            // live window drag. The clamp is idempotent and re-derives
            // itself from the live canvas size on every resize, so there
            // is nothing that needs to survive as a standalone saved
            // edit; the operator's own next real drag/resize still saves
            // normally through the usual endDrag()/endResize() path.
            container->trySetGeometry(clamped);
        }
    }
}

namespace {
QString serializeGeometry(const PanelContainerWidget* container)
{
    const QRect g = container->geometry();
    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(g.x())
        .arg(g.y())
        .arg(g.width())
        .arg(g.height())
        .arg(container->isLocked() ? QStringLiteral("true") : QStringLiteral("false"));
}
} // namespace

void PanelLayoutManager::saveLayout(const QString& onlyId)
{
    // Writing every panel's CURRENT geometry on every single edit used
    // to be the whole body of this method -- but "current" includes
    // whatever clampPanelsToCanvas() last squeezed an UNTOUCHED panel
    // down to while the canvas was temporarily smaller than its real
    // saved layout (a smaller-than-usual window at startup, another
    // panel's drag/resize briefly making room, etc.). clampPanelsToCanvas()
    // deliberately never itself calls saveLayout() (see its own doc
    // comment) precisely so a transient shrink stays visual-only -- but
    // that guarantee only holds if THIS method also never persists a
    // panel nobody actually edited. Confirmed live, 2026-09-12: adding
    // one new panel (and the z-order churn from registering it) alone
    // was enough to silently overwrite four unrelated, already-correct
    // saved panel geometries with their currently-clamped, much smaller
    // values. `onlyId` fixes that: every caller except
    // resetToDefaultLayout() (where every panel's geometry has
    // genuinely just changed) now passes the one id that was actually
    // edited/raised.
    if (onlyId.isEmpty()) {
        for (auto it = m_panels.constBegin(); it != m_panels.constEnd(); ++it) {
            m_database.setSettingValue(layoutKeyFor(it.key()), serializeGeometry(it.value().container));
        }
    } else if (const auto it = m_panels.constFind(onlyId); it != m_panels.constEnd()) {
        m_database.setSettingValue(layoutKeyFor(onlyId), serializeGeometry(it.value().container));
    }
    // The z-order is one global value, not per-panel -- always correct
    // to write in full regardless of which single panel triggered this.
    m_database.setSettingValue(kOrderKey, m_zOrder.join(QLatin1Char(',')));
}

} // namespace Contestprogramm
