#pragma once

class QCoreApplication;

namespace Contestprogramm {

// Qts eigene Beschriftungen auf Deutsch. Ohne das stehen die Knöpfe,
// die Qt selbst malt -- QDialogButtonBox (Save/Cancel/Ok/Reset) und
// QMessageBox (Yes/No) -- englisch mitten in einem deutschen Fenster,
// denn Qt installiert von sich aus keinen Übersetzer.
//
// Fest auf Deutsch, nicht auf die Systemsprache: dieses Programm ist
// deutsch, auch auf einem englisch eingestellten Rechner.
//
// Gesucht wird an drei Stellen -- dort, wo Qt seine Übersetzungen
// meldet, im Bundle neben dem Programm (macOS: Contents/Resources/
// translations, was scripts/package-macos.sh dort hinlegt) und direkt
// neben der ausführbaren Datei (Windows/Linux). Findet sich nichts,
// bleibt es bei Qts englischen Beschriftungen: dann fehlt die
// .qm-Datei im Paket, was kein Grund ist, den Start zu stören.
//
// Gibt zurück, ob ein Übersetzer installiert wurde -- der Prüfstand
// hängt daran, sonst niemand.
bool installGermanQtTranslations(QCoreApplication& app);

} // namespace Contestprogramm
