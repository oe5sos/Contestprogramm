#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace Contestprogramm {

// One DX spot or chat-derived candidate coming out of On4kstClient, per
// the plan's core/SpotCandidate.h ("POD: callsign, grid, rawLine,
// timestamp, source"). `grid` is empty when it could not be determined
// (e.g. a plain chat line without an embedded locator) -- GeoFilter
// treats an empty/invalid grid as "unknown distance", not as "out of
// range" (see GeoFilter.h).
struct SpotCandidate {
    QString callsign;
    QString grid;
    QString rawLine;
    // Der lesbare Teil: bei einer ON4KST-Chatzeile das, was die Station
    // wirklich geschrieben hat (Feld 6 der CH/CR-Zeile), bei einem Spot
    // die Zeile selbst. rawLine bleibt daneben stehen -- für die
    // Rohansicht und zum Nachsehen, wenn eine Zerlegung einmal danebengeht.
    //
    // Ohne das stand im Chat-Panel die ganze Protokollzeile:
    // "CR|2|1790589376|SP2ERZ|..." statt dessen, was er geschrieben hat.
    // Martin, 2026-09-28, im laufenden Betrieb gesehen.
    QString message;
    QDateTime timestampUtc;
    QString source; // e.g. "on4kst"
    qint64 freqHz = 0; // 0 when not known (e.g. a chat line, not a DL| spot)
};

} // namespace Contestprogramm

// Needed for QSignalSpy / QVariant to carry SpotCandidate values --
// On4kstClient::spotReceived / chatLineReceived pass it as a signal
// argument, and tests/test_on4kstprotocol.cpp inspects captured
// arguments via QSignalSpy.
Q_DECLARE_METATYPE(Contestprogramm::SpotCandidate)
