#pragma once

#include "data/ContestDefinition.h"

#include <QDialog>
#include <QVector>

class QComboBox;
class QPushButton;
class QTableWidget;

namespace Contestprogramm {

// "individuell auswählen können, was genau der Contest für Rules hat...
// Änderung zusätzlich möglich wenn falsch" -- the plan's
// ContestRulesEditor item. Lets the operator view/add/remove/reorder/
// edit a contest's exchange_fields (the same JSON shape
// ContestDefinition::loadFromJson reads -- see its class comment), then
// saves the result as a user-writable override
// (ContestDefinition::overrideFilePath) that AppController::
// loadAvailableContestDefinitions() prefers over the shipped
// resources/contest_definitions/*.json from then on. A "configure
// before you start" action, so modal fits better than a persistent
// window like MultiplierWindow.
//
// Only exchange_fields is editable here -- id/name/bands/dupe_scope/
// multiplier_field carry over unchanged from whichever shipped/already-
// overridden ContestDefinition the operator selects (see
// ContestDefinition::withExchangeFields). Broader per-contest editing
// (bands, dupe scope, a brand new contest from scratch) is out of this
// pass's scope, per the plan's own framing ("Serial an/aus, Locator
// an/aus, Name/RS(T)/Power/Kategorie an/aus, plus ein freies
// Zusatzfeld" -- all exchange-field composition, nothing about bands).
class ContestRulesEditor : public QDialog {
    Q_OBJECT

public:
    // `availableContests` supplies the id/name/bands/dupe_scope/
    // multiplier_field every saved override keeps unchanged, and the
    // exchange_fields each contest starts out with (the currently
    // active override if AppController already loaded one, else the
    // shipped default -- loadAvailableContestDefinitions() already
    // prefers the override, so this dialog does not need to know the
    // difference). `initialContestId` preselects the contest combo
    // (MainWindow passes the currently active contest).
    ContestRulesEditor(const QVector<ContestDefinition>& availableContests,
                        const QString& initialContestId,
                        QWidget* parent = nullptr);

private slots:
    void onContestSelectionChanged(int index);
    void onAddField();
    void onRemoveField();
    void onMoveFieldUp();
    void onMoveFieldDown();
    void onSave();

private:
    void loadFieldsIntoTable(const QVector<ContestDefinition::ExchangeField>& fields);
    QVector<ContestDefinition::ExchangeField> fieldsFromTable() const;
    const ContestDefinition* selectedDefinition() const;

    QVector<ContestDefinition> m_availableContests;
    QComboBox* m_contestCombo;
    QTableWidget* m_fieldsTable;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
    QPushButton* m_moveUpButton;
    QPushButton* m_moveDownButton;
};

} // namespace Contestprogramm
