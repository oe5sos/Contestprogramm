#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

namespace Contestprogramm {

class ContestDatabase;
class ContestDefinition;

// Per-band worked/needed multiplier tracking, per the plan's
// "Multiplier-Tracking + Worked/Needed-Raster pro Band" nachziehen item.
//
// Zwei Grundlagen (ContestDefinition::multiplierField()): "grid", das
// Locator-Großfeld der UKW-Contests, und seit 2026-09-22 "prefix", die
// WPX-Regel für Kurzwelle (core/CallsignPrefix.h) -- beide rechnen
// sich allein aus dem, was ohnehin im Log steht, ohne fremde Tabelle.
// Jede andere Angabe (etwa ein Land, wofür es eine Präfix-Tabelle
// bräuchte) lässt die Listen leer, statt eine Grundlage zu erfinden.
class MultiplierTracker {
public:
    explicit MultiplierTracker(ContestDatabase& database);

    // Re-derives worked-multiplier sets for `contestId` from `database`,
    // per `definition`'s bands() and multiplierField(). Eine Grundlage,
    // die dieser Zähler nicht kennt, leert ihn, statt zu raten -- siehe
    // Klassenkommentar.
    void recompute(const QString& contestId, const ContestDefinition& definition);

    const QStringList& bands() const { return m_bands; }
    QSet<QString> workedMultipliers(const QString& band) const;
    int totalMultiplierCount() const; // union of every band's worked set

    // Der Schlüssel, unter dem ein Kontakt zählt -- je nach Grundlage
    // aus dem Locator oder aus dem Rufzeichen. Leer, wenn die nötige
    // Angabe fehlt (auf Kurzwelle wird kein Locator getauscht) oder die
    // Grundlage keine ist.
    QString multiplierKeyFor(const QString& grid, const QString& callsign) const;

    // Ist dieser Kontakt auf diesem Band noch nicht gearbeitet?
    // Used by ChatFeedModel's importance scoring (core/ChatImportanceScorer.h)
    // to boost a still-needed multiplier over one already worked.
    bool isNeededMultiplier(const QString& band, const QString& grid, const QString& callsign) const;

    // VHF/UHF contest convention: the multiplier is the 4-character
    // Maidenhead field+square (e.g. "JN77"), not the full 6-character
    // locator -- two stations a few km apart in the same JN77 square
    // are the same multiplier. A grid shorter than 4 characters is
    // returned upper-cased and otherwise unchanged (defensive; the
    // schema does not enforce a minimum length).
    static QString multiplierKeyForGrid(const QString& grid);

private:
    ContestDatabase& m_database;
    QStringList m_bands;
    QString m_basis = QStringLiteral("grid");
    QHash<QString, QSet<QString>> m_workedByBand;
};

} // namespace Contestprogramm
