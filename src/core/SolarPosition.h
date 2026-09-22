#pragma once

#include <QDateTime>

namespace Contestprogramm {

// Wo die Sonne gerade senkrecht steht -- der Unterpunkt der Sonne.
// Daraus ergibt sich die Graulinie: die Dämmerungszone ist der Kreis
// mit 10 008 km Abstand um den Gegenpunkt dieses Punktes (ein Viertel
// des Erdumfangs), und auf welcher Seite Tag ist, sagt der Unterpunkt
// selbst.
//
// Contestprogramm-original (2026-09-22), nach der Kurzformel des NOAA
// Solar Calculator (gml.noaa.gov/grad/solcalc) -- mittlere Länge,
// mittlere Anomalie, Mittelpunktsgleichung, Schiefe der Ekliptik,
// daraus Deklination und Rektaszension, und über die Sternzeit die
// Länge. Genauigkeit rund ein hundertstel Grad, also etwa ein
// Kilometer -- für eine Dämmerungslinie, die selbst ein paar hundert
// Kilometer breit ist, mehr als genug. Keine Nutation, keine
// Refraktion, keine Parallaxe.
struct SolarPoint {
    double latitudeDeg = 0.0;   // die Deklination der Sonne, -23,44 bis +23,44
    double longitudeDeg = 0.0;  // -180 bis 180, positiv nach Osten
};

// `utc` muss in UTC vorliegen (QDateTime::currentDateTimeUtc() oder
// eine Zeit mit Qt::UTC); eine Zeit in Ortszeit wird umgerechnet.
SolarPoint subsolarPoint(const QDateTime& utc);

// Ein Viertel des Erdumfangs: so weit ist die Dämmerungslinie vom
// Gegenpunkt der Sonne entfernt (Erdradius 6371 km, wie im ganzen
// Programm).
double terminatorRadiusKm();

} // namespace Contestprogramm
