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
// Both shipped ContestDefinitions (OE_VHF_UHF, IARU_R1_VHF_UHF) score
// grid squares as the multiplier, so that is the only basis implemented
// here -- see ContestDefinition::multiplierField().
class MultiplierTracker {
public:
    explicit MultiplierTracker(ContestDatabase& database);

    // Re-derives worked-multiplier sets for `contestId` from `database`,
    // per `definition`'s bands() and multiplierField(). Any
    // multiplierField() other than "grid" clears the tracker to empty
    // rather than guessing at an unimplemented basis (e.g. DXCC) -- see
    // the class comment.
    void recompute(const QString& contestId, const ContestDefinition& definition);

    const QStringList& bands() const { return m_bands; }
    QSet<QString> workedMultipliers(const QString& band) const;
    int totalMultiplierCount() const; // union of every band's worked set

    // Is `grid`'s multiplier key NOT yet in workedMultipliers(band)?
    // Used by ChatFeedModel's importance scoring (core/ChatImportanceScorer.h)
    // to boost a still-needed multiplier over one already worked.
    bool isNeededMultiplier(const QString& band, const QString& grid) const;

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
    QHash<QString, QSet<QString>> m_workedByBand;
};

} // namespace Contestprogramm
