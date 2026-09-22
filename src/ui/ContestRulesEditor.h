#pragma once

#include "data/ContestDefinition.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace Contestprogramm {

// "individuell auswählen können, was genau der Contest für Rules hat...
// Änderung zusätzlich möglich wenn falsch" -- und, 2026-09-22: "man
// soll das auch alles manuell eingeben können, sprich selbst auswählen
// können, was der contest will".
//
// Hier steht jede Regel, die eine Ausschreibung setzen kann: Name,
// Bänder, Exchange-Felder, Wertung, Nummernkreis, Dupe-Regel,
// Multiplikator, Betriebsarten, Cabrillo-Name -- und "Neuer Contest",
// eine Ausschreibung von Null, ohne dass jemand eine JSON-Datei
// anfasst. Gespeichert wird als benutzereigene Datei
// (ContestDefinition::overrideFilePath), die
// AppController::loadAvailableContestDefinitions() der ausgelieferten
// vorzieht -- und die, wenn es gar keine ausgelieferte gibt, den
// Contest allein trägt.
//
// Nicht änderbar ist die Kennung (id): sie steht in jedem geloggten
// QSO (qsos.contest_id). Ein neuer Contest bekommt seine Kennung aus
// dem Namen abgeleitet und zu sehen, bevor er angelegt wird.
//
// "Zurücksetzen" wirft die eigene Datei weg: bei einer ausgelieferten
// Ausschreibung gilt danach wieder deren Original, bei einem selbst
// angelegten Contest ist er weg -- deshalb dort mit Rückfrage.
//
// Ein "vor dem Start einstellen"-Fenster, darum modal.
class ContestRulesEditor : public QDialog {
    Q_OBJECT

public:
    ContestRulesEditor(const QVector<ContestDefinition>& availableContests,
                       const QString& initialContestId,
                       QWidget* parent = nullptr);

    // Die Kennung, die zuletzt gespeichert (oder angelegt) wurde --
    // leer, wenn nichts geschrieben wurde. MainWindow schaltet einen
    // neu angelegten Contest damit gleich aktiv.
    QString savedContestId() const { return m_savedContestId; }
    // true, wenn "Zurücksetzen" einen selbst angelegten Contest
    // gelöscht hat: der Aufrufer muss dann einen anderen aktiv
    // schalten.
    bool deletedSelectedContest() const { return m_deletedContest; }

    // Legt einen neuen Contest mit diesem Namen an, mit den Regeln des
    // gerade gewählten als Ausgangspunkt, und wählt ihn aus -- auf die
    // Platte kommt er erst beim Speichern. Gibt die vergebene Kennung
    // zurück, leer wenn der Name nicht taugt. Öffentlich, weil der
    // Knopf daneben nur die Namensabfrage davorsetzt und ein Prüfstand
    // an einem modalen QInputDialog nicht vorbeikommt.
    QString createDraftContest(const QString& name);

private slots:
    void onContestSelectionChanged(int index);
    void onNewContest();
    void onAddField();
    void onRemoveField();
    void onMoveFieldUp();
    void onMoveFieldDown();
    void onResetToShipped();
    void onSave();

private:
    void loadRulesIntoForm(const ContestDefinition& definition);
    ContestDefinition::Rules rulesFromForm() const;
    void loadFieldsIntoTable(const QVector<ContestDefinition::ExchangeField>& fields);
    QVector<ContestDefinition::ExchangeField> fieldsFromTable() const;
    const ContestDefinition* selectedDefinition() const;
    // Aus "Kurzwelle Übung" wird "KURZWELLE_UEBUNG"; bei Kollision mit
    // einer schon vorhandenen Kennung mit _2, _3 ... weiter.
    QString proposeIdFor(const QString& name) const;

    QVector<ContestDefinition> m_availableContests;
    QString m_savedContestId;
    bool m_deletedContest = false;

    QComboBox* m_contestCombo;
    QPushButton* m_newButton;
    QLabel* m_idLabel;
    QLineEdit* m_nameEdit;
    QVector<QCheckBox*> m_bandChecks;
    QTableWidget* m_fieldsTable;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
    QPushButton* m_moveUpButton;
    QPushButton* m_moveDownButton;
    QComboBox* m_scoringCombo;
    QComboBox* m_serialScopeCombo;
    QCheckBox* m_dupeBandCheck;
    QCheckBox* m_dupeModeCheck;
    QComboBox* m_multiplierCombo;
    QVector<QCheckBox*> m_modeChecks;
    QLineEdit* m_cabrilloEdit;
    QPushButton* m_resetButton;
};

} // namespace Contestprogramm
