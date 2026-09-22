#pragma once

#include "core/CountryPrefixIndex.h"

#include <QDateTime>
#include <QString>

namespace Contestprogramm {

// Was über die Gegenstation bekannt ist, während ihr Rufzeichen
// getippt wird: Land, Richtung, Entfernung, langer Weg, Ortszeit dort
// und wann dort die Sonne auf- und untergeht.
//
// Contestprogramm-original (2026-09-22). Dasselbe, was N1MM in seinem
// Info-Fenster zeigt (Land, kurzer und langer Weg, Entfernung,
// Sonnenauf-/untergang und Ortszeit am anderen Ende) und DXLog neben
// der Eingabe -- hier in einer Zeile, weil das Log-Panel eine hat.
//
// Die Herkunft der Position steht mit drin: ein getauschter Locator ist
// genau, ein Landesmittelpunkt ist es nicht (`approximate`), und die
// Zeile sagt das mit einer Tilde vor der Entfernung. Ohne geladene
// Länderliste und ohne Locator bleibt alles leer -- geraten wird nicht.
struct DxInfo {
    bool known = false;         // überhaupt etwas herausgefunden?
    bool approximate = false;   // Ort ist der Mittelpunkt eines Landes

    QString countryName;
    QString primaryPrefix;
    QString continent;
    int cqZone = 0;

    double bearingDeg = 0.0;
    double distanceKm = 0.0;
    double longPathBearingDeg = 0.0;
    double longPathKm = 0.0;

    QDateTime localTime;        // Ortszeit am anderen Ende (ohne Sommerzeit)
    QDateTime sunriseUtc;
    QDateTime sunsetUtc;
    bool polarDay = false;
    bool polarNight = false;

    // Die Zeile, wie sie unter der Eingabe steht. Leer, solange nichts
    // bekannt ist.
    QString statusLine() const;
};

// `grid` ist der getauschte Locator, soweit schon eingetippt (dann
// zählt er), sonst leer. `ownGrid` ist der eigene Standort; ohne ihn
// gibt es keine Richtung und keine Entfernung, aber weiterhin Land und
// Ortszeit.
DxInfo lookupDxInfo(const CountryPrefixIndex& countries,
                    const QString& ownGrid,
                    const QString& callsign,
                    const QString& grid,
                    const QDateTime& nowUtc);

} // namespace Contestprogramm
