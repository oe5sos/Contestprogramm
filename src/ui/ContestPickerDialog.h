#pragma once

#include "data/ContestDefinition.h"

#include <QDialog>
#include <QVector>

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
// (selectedContestId()); it does not itself flip ContestSettings::
// activeContestId or rebuild anything. MainWindow::openContestPicker()
// applies the result through the exact same AppController::setSettings()
// + applyActiveContestDefinition() (+ the same follow-up refreshes)
// SettingsDialog's own contest combo already triggers on accept -- one
// propagation path, not two.
class ContestPickerDialog : public QDialog {
    Q_OBJECT

public:
    ContestPickerDialog(const QVector<ContestDefinition>& availableContests,
                         const QString& initialContestId,
                         QWidget* parent = nullptr);

    // The chosen contest's id, or an empty string if nothing was
    // selectable (m_availableContests was empty). Only meaningful after
    // exec() == QDialog::Accepted; MainWindow does not call this
    // otherwise.
    QString selectedContestId() const;

private:
    QVector<ContestDefinition> m_availableContests;
    QTableWidget* m_table;
};

} // namespace Contestprogramm
