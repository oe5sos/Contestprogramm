#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class QVBoxLayout;
class QPushButton;

namespace Contestprogramm {

// Left-side, full-height strip of round profile badges, one per saved
// layout profile, plus a dashed "+" at the bottom to add a new one --
// ported in spirit from Longpath's real ProfileRail
// (~/Longpath/NereusSDR/src/gui/widgets/ProfileRail.h, "Profilschiene
// ganz links über die volle Höhe, wie bei Zeus"), adapted to this
// project's own panel model (LayoutProfileManager here, LayoutProfiles
// there). Operator, 2026-09-14: "mache zusätzlich wie bei longpath auf
// der linken seite eine leiste, dass ich ein zweites profil anlegen
// kann um zb die karte dort alleine zu platzieren".
//
// This widget only renders/emits requests; LayoutProfileManager owns
// the actual profile data and decides what switching/creating/
// duplicating/removing means for the panels themselves.
class ProfileRail : public QWidget {
    Q_OBJECT

public:
    static constexpr int kWidth = 44;

    explicit ProfileRail(QWidget* parent = nullptr);

    // `names` in display (top-to-bottom) order; `activeName` gets the
    // highlighted ring. Safe to call repeatedly (e.g. on every
    // LayoutProfileManager::profilesChanged()) -- rebuilds the badge
    // column from scratch, which is fine at the "a handful of
    // profiles" scale this is meant for.
    void setProfiles(const QStringList& names, const QString& activeName);

signals:
    void profileActivated(const QString& name);
    void newProfileRequested();
    void renameRequested(const QString& name);
    void duplicateRequested(const QString& name);
    void removeRequested(const QString& name);

private:
    void rebuildBadges();
    QPushButton* makeBadge(const QString& name, bool active);

    QVBoxLayout* m_badgeColumn = nullptr;
    QStringList m_names;
    QString m_activeName;
};

} // namespace Contestprogramm
