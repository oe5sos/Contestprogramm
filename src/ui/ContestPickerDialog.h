#pragma once

#include "data/ContestDefinition.h"

#include <QDialog>
#include <QVector>

class QLineEdit;
class QPushButton;
class QTableWidget;

namespace Contestprogramm {

// Dedicated "Contest wählen..." picker window, per the operator's
// explicit request for a proper contest-selection window instead of only
// the combo box buried in SettingsDialog. NOT the green "NEU"/"neu" badge
// in the entry row -- that is UnifiedLogWidget::setDupeIndicator's
// dupe-status indicator, a separate, already-correct feature (green =
// not a dupe) that this task must not touch.
//
// Scope: a *local* picker over the already-known ContestDefinitions
// (currently the two shipped resources/contest_definitions/*.json plus
// whatever ContestRulesEditor overrides exist -- see
// AppController::availableContestDefinitions()), not a live external
// contest-calendar integration -- that would need its own data-source
// research (like the ON4KST protocol did) and is explicitly out of scope
// for this pass.
//
// This dialog only reports back which contest id was picked
// (selectedContestId()) and which own locator the operator confirmed
// (ownGrid()); it does not itself flip ContestSettings or rebuild
// anything. MainWindow::openContestPicker() applies the result through
// the exact same AppController::setSettings() + applyActiveContest-
// Definition() (+ the same follow-up refreshes) SettingsDialog's own
// contest combo already triggers on accept -- one propagation path,
// not two.
//
// The locator is asked every time (operator, 2026-09-21: "der locator
// sollte immer mit eingabe ... nachgefragt werden"): a contest is
// picked shortly before it starts, often at a portable site, and the
// home locator left in the settings would go out with every exchange.
// The field comes prefilled with the current one; when the exact
// position from the settings lies in another square, a button offers
// that square.
class ContestPickerDialog : public QDialog {
    Q_OBJECT

public:
    ContestPickerDialog(const QVector<ContestDefinition>& availableContests,
                         const QString& initialContestId,
                         QWidget* parent = nullptr);
    // The own locator the dialog asks for: prefilled with `currentGrid`;
    // `exactLocationGrid` (may be empty) is the square the settings'
    // exact position lies in, offered when it differs.
    void setOwnGrid(const QString& currentGrid, const QString& exactLocationGrid = QString());
    // What the operator confirmed, upper-cased; valid whenever exec()
    // returned Accepted (OK stays disabled on an invalid locator).
    QString ownGrid() const;

    // The chosen contest's id, or an empty string if nothing was
    // selectable (m_availableContests was empty). Only meaningful after
    // exec() == QDialog::Accepted; MainWindow does not call this
    // otherwise.
    QString selectedContestId() const;

private:
    void refreshGridState();

    QVector<ContestDefinition> m_availableContests;
    QTableWidget* m_table;
    QLineEdit* m_gridEdit = nullptr;
    QPushButton* m_exactGridButton = nullptr;
    QPushButton* m_okButton = nullptr;
    QString m_exactLocationGrid;
};

} // namespace Contestprogramm
