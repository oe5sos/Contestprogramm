#pragma once

#include <QString>

namespace Contestprogramm {

// Das Datenverzeichnis des Contestprogramms -- EIN Ort fuer beide
// Betriebsarten.
//
// Eigenstaendig ist es QStandardPaths::AppDataLocation, also
// <GenericData>/Contestprogramm/Contestprogramm (macOS: ~/Library/
// Application Support/Contestprogramm/Contestprogramm). Eingebettet in
// Longpath (seit 2026-11-04) zeigt AppDataLocation aber auf LONGPATHS
// Ordner; damit das Log dasselbe bleibt, ob das Fenster in Longpath oder
// das Programm allein laeuft, wird der Pfad hier aus GenericDataLocation
// zusammengesetzt -- fuer beide gleich.
//
// CONTESTPROGRAMM_DATA_DIR (Umgebung) geht vor, wie in main.cpp seit je:
// Pruefstaende und Probelaeufe arbeiten so in einem eigenen Ordner, ohne
// das echte Log zu beruehren.
QString standardDatenverzeichnis();

// Die Datenbankdatei in diesem Verzeichnis.
QString datenbankPfad(const QString& datenverzeichnis);

} // namespace Contestprogramm
