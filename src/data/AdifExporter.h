#pragma once

#include <QString>

namespace Contestprogramm {

class ContestDatabase;
struct ContestSettings;

// General-format export alongside CabrilloExporter, per the plan's
// Cabrillo-Export section: "Cabrillo... aber qsos.freq_hz... wird im
// ADIF-Export als echtes FREQ-Feld mitgeschrieben -- rein im
// Hintergrund, keine eigene Anzeige/Eingabe in der UI dafür nötig."
// Cabrillo only ever gets the band code; this is the one place the
// exact logged frequency (Hz, from CAT/RigctldClient) becomes a real
// ADIF FREQ value (MHz, per the ADIF convention), for import into
// "normal" logging programs (Log4OM, DXKeeper, ...).
//
// Record format ported from the pattern in NereusSDR's
// src/models/LogEntry.cpp::toAdifRecord() / src/core/AdifLog.cpp
// (byte-length-prefixed <NAME:LEN>value tokens, <EOR> terminator,
// <ADIF_VER>/<PROGRAMID>/<EOH> header) -- the pattern only, not the
// code, since this exports QsoRecord (a different schema entirely, no
// NAME/QTH/POTA/etc.), not LogEntry.
class AdifExporter {
public:
    explicit AdifExporter(ContestDatabase& database);

    // `settings` liefert die Felder, die kein QSO beantworten kann:
    // eigenes Rufzeichen und eigener Locator (STATION_CALLSIGN /
    // OPERATOR / MY_GRIDSQUARE). Dieselbe Aufteilung wie beim
    // Cabrillo-Export.
    QString exportContest(const QString& contestId, const ContestSettings& settings) const;

private:
    ContestDatabase* m_database;
};

} // namespace Contestprogramm
