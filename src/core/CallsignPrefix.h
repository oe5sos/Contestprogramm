#pragma once

#include <QString>

namespace Contestprogramm {

// Der Präfix eines Rufzeichens nach der WPX-Regel -- OE5SOS -> "OE5",
// DL1ABC -> "DL1", 9A1A -> "9A1". Auf Kurzwelle ist das der
// Multiplikator, den man ohne jede fremde Datei ausrechnen kann: er
// steckt vollständig im Rufzeichen. (Für Land, Kontinent oder Zone
// bräuchte es eine Präfix-Tabelle -- die gibt es hier bewusst nicht.)
//
// Contestprogramm-original (2026-09-22), nach den Regeln des CQ WPX
// Contest (cqwpx.com/rules.htm), Abschnitt "Prefixes":
//
//   * Der Präfix ist der Teil des Rufzeichens bis einschließlich der
//     letzten Ziffer.
//   * Ein Rufzeichen ohne Ziffer bekommt eine 0 hinter die ersten
//     beiden Zeichen: RAEM -> "RA0".
//   * Bei einem Zusatz vor oder hinter dem Schrägstrich zählt der
//     Zusatz: KH6/N8BJQ und N8BJQ/KH6 ergeben beide "KH6"; ein Zusatz
//     ohne Ziffer bekommt wieder die 0: PA/N8BJQ -> "PA0".
//   * Ein Zusatz, der nur eine Ziffer ist, ersetzt die Ziffer:
//     N8BJQ/9 -> "N9".
//   * Betriebszusätze (/P, /M, /MM, /AM, /QRP, /A, /LH ...) zählen
//     nicht: OE5SOS/P -> "OE5".
//
// Welcher Teil bei zwei Rufzeichenteilen der Zusatz ist, entscheidet
// die Länge (der kürzere; bei Gleichstand der erste) -- dieselbe
// Faustregel, die die gängigen Contestlogs benutzen. Ein leeres oder
// unbrauchbares Rufzeichen ergibt einen leeren Präfix.
QString wpxPrefix(const QString& callsign);

// Das Grundrufzeichen fuer die Dupe-Pruefung, IARU R1 VHF+-Regeln
// (GC 2023) 1.2: "Added prefix and/or suffix do not generate different
// call sign (i.e. S50AAA/p or DL/S50AAA are the same call sign as
// S50AAA)." Betriebszusaetze weg (wie bei wpxPrefix), von den uebrigen
// Teilen der laengste -- bei Gleichstand der erste:
//   S50AAA/P -> S50AAA, DL/S50AAA -> S50AAA, 9A/OE5SOS/P -> OE5SOS,
//   N8BJQ/9 -> N8BJQ.
// Nur zum VERGLEICHEN. Geloggt und exportiert wird immer das Rufzeichen,
// wie es ueber Funk ausgetauscht wurde (Regel 1.9.1).
QString baseCallsign(const QString& callsign);

} // namespace Contestprogramm
