#pragma once

#include <QString>
#include <QStringList>

namespace Contestprogramm {

class ContestDatabase;
class ContestDefinition;
struct ContestSettings;

// The station-description part of an EDI header -- the P*/R*/M*/S*
// keys that say who operated from where with what. Deliberately NOT
// part of ContestSettings: nothing but this export reads any of it,
// and ContestSettings is the live operating state (rotors, feeds, CAT),
// not the once-per-contest submission paperwork. Persisted in the same
// `settings` table anyway, under "edi_*" keys, so it survives between
// contests the way N1MM+'s "Station Information" and DXLog.net's
// station info do -- filled in once, reused every time.
struct EdiStationInfo {
    QString section;       // PSect -- entry class as the organiser names it
    QString club;          // PClub
    QString locationLine1; // PAdr1 -- where the station physically stood
    QString locationLine2; // PAdr2
    QString operators;     // MOpe1 -- semicolon-separated calls (multi-op)
    QString name;          // RName -- responsible operator
    QString street;        // RAdr1
    QString postalCode;    // RPoCo
    QString city;          // RCity
    QString country;       // RCoun
    QString phone;         // RPhon
    QString email;         // RHBBS
    QString txEquipment;   // STXEq
    int powerWatts = 0;    // SPowe
    QString rxEquipment;   // SRXEq
    QString antenna;       // SAnte
    // SAntH (height above ground;above sea level) is NOT here -- it
    // comes from ContestSettings::antennaHeightM/ownElevationM, which
    // the terrain module already keeps accurate for the same site.

    void loadFrom(const ContestDatabase& database);
    void saveTo(ContestDatabase& database) const;
};

// Writes the IARU Region 1 "REG1TEST" EDI format (version 1) -- the
// submission format every IARU-R1 and ÖVSV VHF/UHF contest actually
// asks for. Cabrillo (see CabrilloExporter) is the ARRL/HF form; an EDI
// file is what the OE/DL/IARU-R1 robots ingest. Both N1MM+ and
// DXLog.net export this for their VHF contests; the layout here follows
// the published REG1TEST specification, not either program's output.
//
// One file per band, by definition of the format (PBand is a single
// value and the robots evaluate each band as its own entry), so the
// caller iterates bandsWithQsos() and writes exportBand() once each.
//
// Conventions, all deliberate:
//   - CRLF line endings and Latin-1 bytes are what the format's
//     Windows-era readers expect; the caller writes the QString with
//     toLatin1() (see EdiExportDialog).
//   - QSO points come from data/ContestScoring.h (the definition's
//     scoring rule, normally 1 point per whole km between locator
//     centres, at least 1 with a known locator) -- the same function
//     that feeds the live score panel, so the claimed C* header
//     values are exactly what the operator watched all night.
//   - A dupe (QsoRecord::isDupe) stays in the file, scores 0 and
//     carries the "D" flag; the organiser's robot wants to see it. A
//     QSO marked invalid (isInvalid) is left out entirely, same as
//     CabrilloExporter/AdifExporter.
//   - "New WWL" (the N flag) is per band, on the 4-character large
//     square, which is what "WWL" means in IARU-R1 scoring. "New DXCC"
//     is left blank: this project has no prefix table, and the flag is
//     informational -- the robot recomputes it anyway.
class EdiExporter {
public:
    explicit EdiExporter(ContestDatabase& database);

    // Bands (QsoRecord::band values, "144"/"432"/...) that hold at
    // least one non-invalid QSO for `contestId`, in
    // ContestDefinition::bands() order; bands the definition does not
    // list follow in first-seen order.
    QStringList bandsWithQsos(const QString& contestId, const ContestDefinition& definition) const;

    // The complete REG1TEST text for one band, CRLF-terminated lines.
    QString exportBand(const QString& contestId,
                       const QString& band,
                       const ContestDefinition& definition,
                       const ContestSettings& settings,
                       const EdiStationInfo& station) const;

    // "144" -> "144 MHz", "1296" -> "1,3 GHz": REG1TEST's own PBand
    // spellings (decimal comma included). Unknown bands come back as
    // "<band> MHz".
    static QString bandLabel(const QString& band);

    // REG1TEST mode codes: 1 SSB, 2 CW, 5 AM, 6 FM, 7 RTTY, 8 SSTV,
    // 9 ATV; 0 for anything else ("no mode information").
    static int modeCode(const QString& mode);

    // "OE5SOS_144MHz.edi" -- one file name per band, call first so a
    // folder of several entrants' files sorts by station.
    static QString suggestedFileName(const QString& ownCallsign, const QString& band);

private:
    ContestDatabase* m_database;
};

} // namespace Contestprogramm
