#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace Contestprogramm {

class ContestDatabase;
class PanelLayoutManager;

// Named, switchable panel visibility/geometry snapshots -- a scoped
// port of Longpath's real LayoutProfiles
// (~/Longpath/NereusSDR/src/gui/LayoutProfiles.h), backing the
// left-side ProfileRail. Longpath's version also tracks applet order,
// floating-window geometry, and band/mode bindings -- none of which
// this project has an equivalent of; what carries over is the one
// part the operator actually asked for: which panels are visible and
// where, switchable by clicking a rail badge, with a brand-new profile
// starting EMPTY rather than as a copy of whatever was already on
// screen (Longpath's own explicit choice, LayoutProfiles.h: "wenn ich
// links ein neues profil öffne, sollte dieses leer sein" -- a separate
// "duplicate" action exists for when a copy is what's actually wanted).
//
// Deliberately scoped to the five canvas panels passed in at
// construction (unifiedlog/rotorrow/map/suggestion/ratemeter in
// MainWindow), NOT cwMacroRow: that panel's visibility is already an
// independent, explicitly-checkboxed setting
// (ContestSettings::cwMacroPanelVisible) -- letting profiles also
// drive it would just fight that checkbox over which one is the real
// source of truth.
class LayoutProfileManager : public QObject {
    Q_OBJECT

public:
    LayoutProfileManager(ContestDatabase& database, PanelLayoutManager& panels, QStringList panelIds,
                          QObject* parent = nullptr);

    QStringList profileNames() const { return m_order; }
    QString activeProfile() const { return m_active; }
    // No profile existed when this was constructed: profile "1" was
    // just made from what was on screen.
    bool isFirstLaunch() const { return m_firstLaunch; }
    // Snapshots the active profile from the panels as they are now --
    // MainWindow calls it once the fresh install's design has been
    // placed for the canvas's real size (PanelLayoutManager::
    // initialDesignApplied), replacing the constructor's snapshot of
    // the not-yet-shown window.
    void saveActiveProfileState();

    // No-op if `name` is already active or unknown. Snapshots the
    // outgoing profile's current on-screen state first, so switching
    // away and back reproduces exactly what was left visible.
    void switchTo(const QString& name);
    // Saves the outgoing (currently active) profile's live state, then
    // creates and switches to a brand-new, empty (every panel hidden)
    // profile. Returns its generated name.
    QString createProfile();
    // Copies `sourceName`'s stored state (its LIVE state, if it happens
    // to be the active profile) into a new profile and switches to it.
    void duplicateProfile(const QString& sourceName);
    void renameProfile(const QString& oldName, const QString& newName);
    // Refuses (returns false, no other effect) if `name` is the last
    // remaining profile -- the rail always needs at least one to fall
    // back to.
    bool removeProfile(const QString& name);

signals:
    // Fired after any change to the profile list or the active one --
    // ProfileRail::setProfiles() is the intended slot.
    void profilesChanged();

private:
    QString serializeCurrentState() const;
    void applyState(const QString& serialized);
    void persistOrderAndActive();
    QString generateProfileName() const;

    ContestDatabase& m_database;
    PanelLayoutManager& m_panels;
    QStringList m_panelIds;
    QStringList m_order;
    QString m_active;
    bool m_firstLaunch = false;
};

} // namespace Contestprogramm
