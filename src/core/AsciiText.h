// =================================================================
// src/core/AsciiText.h  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original.
//
// Umlaute in eine Abgabedatei, die nur ASCII kennt.
//
// Beide Abgabeformate, die dieses Programm schreibt, erlauben keine
// Zeichen jenseits des 7-Bit-ASCII:
//
//   REG1TEST (EDI), Abschnitt "Characters": "Used characters are in
//   accordance with the 7-bit ASCII alphabet and only characters with
//   the following decimal number are allowed 10, 13, 32-127."
//
//   Cabrillo v3: "The Cabrillo file format is ASCII text."
//
// Nur steht in Martins Papieren "Österreich", "Musterstraße" und
// "10 m über Grund". Bisher schrieb das Programm diese Zeichen so
// hinaus, wie sie dastanden: in die EDI-Datei als Latin-1-Byte
// (0xD6, 0xDF -- jenseits von 127), in die Cabrillo-Datei als zwei
// UTF-8-Bytes. Ein Robot, der die Datei als ASCII oder als UTF-8
// liest, bekommt davon entweder Unsinn oder einen Fehler, und in
// welcher der beiden Lesarten er steht, erfährt man erst, wenn die
// Einreichung zurückkommt.
//
// Also umschreiben statt hinausschreiben, und zwar so, dass ein
// Mensch es noch lesen kann: ö -> oe, ß -> ss, é -> e. Ein
// Fragezeichen wäre die schlechtere Antwort -- man sieht ihm nicht an,
// was dort stand.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-10-16 — New: shared ASCII transliteration for the EDI and
//                 Cabrillo exporters. AI-assisted via Anthropic Claude
//                 Code, operator Ralph Martin Fischer.
// =================================================================

#pragma once

#include <QString>

namespace Contestprogramm {

// Gibt den Text als reines ASCII zurück: deutsche und nordische
// Umlaute ausgeschrieben (ö -> oe, Ö -> Oe bzw. OE vor Großbuchstaben,
// ß -> ss), alles andere Lateinische auf seinen Grundbuchstaben
// gebracht (é -> e, č -> c), Zeichen ohne lateinische Entsprechung
// fallen weg. Zeilenumbrüche und Tabulatoren werden zu Leerzeichen,
// sonstige Steuerzeichen fallen weg.
QString asciiOnly(const QString& text);

} // namespace Contestprogramm
