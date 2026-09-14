#pragma once

#include <QString>

namespace Contestprogramm {

class ContestDatabase;
class ContestDefinition;
struct ContestSettings;

// Builds a Cabrillo v3.0 log for one contest, per the project plan's
// "Cabrillo-Export (Phase 1)" section. Reads `qsos WHERE contest_id=?`,
// builds the header from ContestSettings + ContestDefinition.
//
// Deviation from the plan: CATEGORY-POWER has no backing field anywhere
// in the schema/ContestSettings for this phase (nothing in the plan's
// Phase-1 scope tracks TX power), so it is an explicit parameter here
// with a "LOW" default rather than an invented persisted field.
// CATEGORY-BAND / CATEGORY-MODE are derived from the logged QSOs
// (single value if the log is single-band/single-mode, else
// "ALL" / "MIXED").
class CabrilloExporter {
public:
    explicit CabrilloExporter(ContestDatabase& database);

    QString exportContest(const QString& contestId,
                          const ContestDefinition& definition,
                          const ContestSettings& settings,
                          const QString& categoryPower = QStringLiteral("LOW")) const;

private:
    ContestDatabase* m_database;
};

} // namespace Contestprogramm
