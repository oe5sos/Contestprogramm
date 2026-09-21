#include "ui/LayoutProfileManager.h"

#include "data/ContestDatabase.h"
#include "ui/PanelContainerWidget.h"
#include "ui/PanelLayoutManager.h"

#include <QMap>
#include <QPair>
#include <QRect>

namespace Contestprogramm {

namespace {
const QString kOrderKey = QStringLiteral("LayoutProfileOrder");
const QString kActiveKey = QStringLiteral("LayoutProfileActive");

QString profileKey(const QString& name)
{
    return QStringLiteral("LayoutProfile_%1").arg(name);
}
} // namespace

LayoutProfileManager::LayoutProfileManager(ContestDatabase& database, PanelLayoutManager& panels,
                                            QStringList panelIds, QObject* parent)
    : QObject(parent)
    , m_database(database)
    , m_panels(panels)
    , m_panelIds(std::move(panelIds))
{
    const QString orderRaw = m_database.settingValue(kOrderKey);
    if (orderRaw.isEmpty()) {
        // First launch on this database, or an upgrade from before
        // profiles existed: profile "1" becomes whatever is already on
        // screen right now (PanelLayoutManager has already restored
        // each panel from the legacy single-layout PanelLayout_* keys
        // by the time MainWindow constructs this), so nothing visually
        // changes for an operator who already had a working layout.
        m_order = {QStringLiteral("1")};
        m_active = QStringLiteral("1");
        m_firstLaunch = true;
        m_database.setSettingValue(profileKey(m_active), serializeCurrentState());
        persistOrderAndActive();
        return;
    }

    m_order = orderRaw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (m_order.isEmpty()) {
        m_order = {QStringLiteral("1")};
    }
    m_active = m_database.settingValue(kActiveKey);
    if (m_active.isEmpty() || !m_order.contains(m_active)) {
        m_active = m_order.first();
    }
    // The restored active profile's own stored visibility/geometry is
    // now the source of truth, overriding whichever state
    // PanelLayoutManager's own legacy per-panel restore left the
    // canvas in.
    applyState(m_database.settingValue(profileKey(m_active)));
}

QString LayoutProfileManager::serializeCurrentState() const
{
    QStringList parts;
    for (const QString& id : m_panelIds) {
        PanelContainerWidget* container = m_panels.panel(id);
        if (container == nullptr) {
            continue;
        }
        const QRect g = container->geometry();
        // isHidden(), not isVisible(): a panel is part of the layout
        // unless it was hidden on purpose. isVisible() is false for
        // every panel before the window is shown -- and this snapshot
        // is taken in MainWindow's constructor on a first launch, so
        // the first profile recorded every panel as hidden and a
        // second start after a crash or kill opened an empty canvas
        // (found 2026-09-21).
        parts << QStringLiteral("%1:%2:%3:%4:%5:%6")
                     .arg(id, container->isHidden() ? QStringLiteral("0") : QStringLiteral("1"))
                     .arg(g.x())
                     .arg(g.y())
                     .arg(g.width())
                     .arg(g.height());
    }
    return parts.join(QLatin1Char(';'));
}

void LayoutProfileManager::applyState(const QString& serialized)
{
    QMap<QString, QPair<bool, QRect>> parsed;
    for (const QString& entry : serialized.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const QStringList fields = entry.split(QLatin1Char(':'));
        if (fields.size() != 6) {
            continue;
        }
        bool xOk = false, yOk = false, wOk = false, hOk = false;
        const int x = fields[2].toInt(&xOk);
        const int y = fields[3].toInt(&yOk);
        const int w = fields[4].toInt(&wOk);
        const int h = fields[5].toInt(&hOk);
        if (!xOk || !yOk || !wOk || !hOk) {
            continue;
        }
        parsed.insert(fields[0], {fields[1] == QLatin1String("1"), QRect(x, y, w, h)});
    }

    for (const QString& id : m_panelIds) {
        PanelContainerWidget* container = m_panels.panel(id);
        if (container == nullptr) {
            continue;
        }
        // A panel this profile never saved a row for (a brand-new,
        // empty profile is the common case) starts hidden -- that is
        // the whole point of "leer": the operator shows and places
        // only what this profile is actually for.
        const auto it = parsed.constFind(id);
        if (it == parsed.constEnd()) {
            container->setVisible(false);
            continue;
        }
        if (it->first) {
            container->trySetGeometry(it->second);
        }
        container->setVisible(it->first);
    }
}

void LayoutProfileManager::saveActiveProfileState()
{
    m_database.setSettingValue(profileKey(m_active), serializeCurrentState());
}

void LayoutProfileManager::persistOrderAndActive()
{
    m_database.setSettingValue(kOrderKey, m_order.join(QLatin1Char(',')));
    m_database.setSettingValue(kActiveKey, m_active);
}

QString LayoutProfileManager::generateProfileName() const
{
    int n = 1;
    QString candidate;
    do {
        candidate = QString::number(n);
        ++n;
    } while (m_order.contains(candidate));
    return candidate;
}

void LayoutProfileManager::switchTo(const QString& name)
{
    if (name == m_active || !m_order.contains(name)) {
        return;
    }
    saveActiveProfileState();
    m_active = name;
    applyState(m_database.settingValue(profileKey(m_active)));
    persistOrderAndActive();
    emit profilesChanged();
}

QString LayoutProfileManager::createProfile()
{
    saveActiveProfileState();
    const QString name = generateProfileName();
    m_order.append(name);
    m_active = name;
    m_database.setSettingValue(profileKey(name), QString());
    applyState(QString());
    persistOrderAndActive();
    emit profilesChanged();
    return name;
}

void LayoutProfileManager::duplicateProfile(const QString& sourceName)
{
    if (!m_order.contains(sourceName)) {
        return;
    }
    // Snapshot whichever profile is live right now before navigating
    // away from it -- covers both "duplicate the one I'm looking at"
    // and "duplicate some other, already-saved profile".
    saveActiveProfileState();
    const QString raw = (sourceName == m_active) ? serializeCurrentState()
                                                  : m_database.settingValue(profileKey(sourceName));
    const QString name = generateProfileName();
    m_order.append(name);
    m_database.setSettingValue(profileKey(name), raw);
    m_active = name;
    applyState(raw);
    persistOrderAndActive();
    emit profilesChanged();
}

void LayoutProfileManager::renameProfile(const QString& oldName, const QString& newName)
{
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || trimmed == oldName || !m_order.contains(oldName) || m_order.contains(trimmed)) {
        return;
    }
    // The old LayoutProfile_<oldName> row is left in place, orphaned --
    // same call PanelLayoutManager's own doc comment already makes for
    // a stale PanelLayout_* row: harmless, never looked up again once
    // nothing in m_order names it.
    m_database.setSettingValue(profileKey(trimmed), m_database.settingValue(profileKey(oldName)));
    const int idx = m_order.indexOf(oldName);
    m_order[idx] = trimmed;
    if (m_active == oldName) {
        m_active = trimmed;
    }
    persistOrderAndActive();
    emit profilesChanged();
}

bool LayoutProfileManager::removeProfile(const QString& name)
{
    if (m_order.size() <= 1 || !m_order.contains(name)) {
        return false;
    }
    m_order.removeAll(name);
    if (m_active == name) {
        m_active = m_order.first();
        applyState(m_database.settingValue(profileKey(m_active)));
    }
    persistOrderAndActive();
    emit profilesChanged();
    return true;
}

} // namespace Contestprogramm
