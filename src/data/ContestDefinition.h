#pragma once

#include "data/ContestSchedule.h"

#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Contestprogramm {

// Loads/validates the generic exchange-schema JSON described in the
// project plan (§ "ContestDefinition (generisches Exchange-Schema)").
// One instance per contest_definitions/*.json file.
class ContestDefinition {
public:
    struct ExchangeField {
        QString key;
        QString label;
        QString type;           // e.g. "int", "grid6"
        bool autoIncrement = false;
    };

    // Parses `path` as a ContestDefinition JSON file. On success returns
    // a populated, isValid() definition and clears `errorOut`; on
    // failure returns a default-constructed (isValid() == false)
    // definition and fills `errorOut` with a human-readable reason.
    static ContestDefinition loadFromFile(const QString& path, QString* errorOut = nullptr);
    static ContestDefinition loadFromJson(const QByteArray& json, QString* errorOut = nullptr);

    // Directory for user-saved exchange-field overrides (see
    // ui/ContestRulesEditor.h), in the same app-data location main.cpp
    // already uses for the SQLite database (QStandardPaths::
    // AppDataLocation) -- writable at runtime even once
    // resources/contest_definitions/ ships read-only inside a packaged
    // app bundle, unlike the source-tree/beside-binary locations
    // AppController::findContestDefinitionsDir() searches. One JSON
    // file per contest id (see overrideFilePath), not per shipped
    // filename -- ContestRulesEditor edits by id, and a contest may not
    // even have a shipped file (a future "define your own contest").
    static QString overrideDirectory();
    static QString overrideFilePath(const QString& contestId);

    bool isValid() const { return m_valid; }

    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    const QStringList& bands() const { return m_bands; }
    const QStringList& dupeScope() const { return m_dupeScope; }
    const QVector<ExchangeField>& exchangeFields() const { return m_exchangeFields; }

    // What MultiplierTracker (data/MultiplierTracker.h) counts as a
    // multiplier for this contest -- "grid" (Maidenhead square) for
    // both shipped definitions, per the plan's own confirmation that
    // Serial+Locator is the common VHF/UHF form. Optional JSON key
    // ("multiplier_field"); defaults to "grid" when absent so the two
    // existing contest_definitions/*.json files need no change.
    const QString& multiplierField() const { return m_multiplierField; }

    // How a valid QSO scores -- see data/ContestScoring.h. "distance_km"
    // (1 point per km, the IARU-R1/ÖVSV/DARC VHF-UHF rule and the
    // default when the JSON key "scoring" is absent) or "qso_count"
    // (1 point per QSO).
    const QString& scoring() const { return m_scoring; }

    // Where the sent serial number counts: "band" (the default -- IARU
    // Region 1 rule: "a serial number commencing with 001 for the first
    // contact on each band", ÖVSV likewise) or "contest" (one sequence
    // across all bands). JSON key "serial_scope".
    const QString& serialScope() const { return m_serialScope; }

    // When the contest runs, as a rule the program turns into dates for
    // any year (see data/ContestSchedule.h). Optional JSON key
    // "schedule"; a definition without one has an invalid schedule and
    // the countdown/log check then rely on ContestSettings::
    // contestEndUtc alone.
    const ContestSchedule& schedule() const { return m_schedule; }

    // The contest's name in a Cabrillo header ("CQ-WW-CW",
    // "IARU-HF") -- what the receiving robot matches on, which is
    // almost never this file's own id(). Optional JSON key
    // "cabrillo_name"; empty (the default) makes CabrilloExporter fall
    // back to id(), exactly as it did before this key existed.
    const QString& cabrilloName() const { return m_cabrilloName; }

    // Modes the rules allow ("CW", "SSB", ...), upper-cased; empty (the
    // default) means any. Optional JSON key "modes" -- the Marconi
    // Memorial is CW only, and a QSO logged in SSB there is one the log
    // check flags (data/LogCheck.h).
    const QStringList& modes() const { return m_modes; }

    // Every rule of a contest that the operator may set, in one place --
    // what ui/ContestRulesEditor.h edits and what withRules()/
    // fromRules() write back. Deliberately NOT id() (a contest keeps
    // its identity; the log rows point at it) and not schedule(), which
    // no dialog offers yet and which withRules() therefore carries over
    // untouched.
    struct Rules {
        QString name;
        QStringList bands;
        QStringList dupeScope;
        QVector<ExchangeField> exchangeFields;
        QString multiplierField = QStringLiteral("grid");
        QString scoring = QStringLiteral("distance_km");
        QString serialScope = QStringLiteral("band");
        QString cabrilloName;
        QStringList modes;
    };

    Rules rules() const;

    // Checks what the JSON loader would check, before anything is
    // written: a name, at least one band, at least one exchange field,
    // no duplicate field keys, "callsign" in the dupe scope, and known
    // values for scoring/serial_scope/multiplier_field. Returns false
    // and fills errorOut with a sentence for the operator.
    static bool validateRules(const Rules& rules, QString* errorOut = nullptr);

    // A copy of this definition with `rules` applied -- id() and
    // schedule() stay. Returns an invalid definition (and fills
    // errorOut) when validateRules() says no, so a bad edit can never
    // be written to disk.
    ContestDefinition withRules(const Rules& rules, QString* errorOut = nullptr) const;

    // A contest the operator invented, from nothing. `id` must be
    // non-empty and carry only A-Z, 0-9 and underscore -- it becomes a
    // file name (overrideFilePath) and the qsos.contest_id of every QSO
    // logged under it.
    static ContestDefinition fromRules(const QString& id, const Rules& rules, QString* errorOut = nullptr);

    // Known values, in the order a dialog should offer them, each with
    // the wording the operator reads.
    static QVector<QPair<QString, QString>> scoringChoices();
    static QVector<QPair<QString, QString>> serialScopeChoices();
    static QVector<QPair<QString, QString>> multiplierChoices();

    // Returns a copy of this definition with exchangeFields() replaced
    // by `fields` -- id/name/bands/dupe_scope/multiplier_field stay
    // unchanged. Used by ContestRulesEditor to build the definition it
    // then writes via saveToFile(); never mutates `this` in place, so
    // an in-progress edit cannot be mistaken for the still-active one.
    ContestDefinition withExchangeFields(const QVector<ExchangeField>& fields) const;

    // Writes this definition back out in the same JSON shape
    // loadFromJson() reads (see the class comment's schema reference).
    // The mechanism behind ContestRulesEditor's save and, per the plan,
    // "Änderung zusätzlich möglich wenn falsch" -- loading a
    // ContestDefinition prefers a file at overrideFilePath(id()) over
    // the shipped one whenever both exist (see
    // AppController::loadAvailableContestDefinitions). Returns false
    // and fills `errorOut` on failure (e.g. the target directory could
    // not be created).
    bool saveToFile(const QString& path, QString* errorOut = nullptr) const;

private:
    QString m_id;
    QString m_name;
    QStringList m_bands;
    QStringList m_dupeScope;
    QVector<ExchangeField> m_exchangeFields;
    QString m_multiplierField = QStringLiteral("grid");
    QString m_scoring = QStringLiteral("distance_km");
    QString m_serialScope = QStringLiteral("band");
    QString m_cabrilloName;
    ContestSchedule m_schedule;
    QStringList m_modes;
    bool m_valid = false;
};

} // namespace Contestprogramm
