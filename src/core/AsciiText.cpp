// =================================================================
// src/core/AsciiText.cpp  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original. Siehe AsciiText.h.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-10-16 — New. AI-assisted via Anthropic Claude Code, operator
//                 Ralph Martin Fischer.
// =================================================================

#include "core/AsciiText.h"

#include <QChar>

namespace Contestprogramm {

namespace {

// Die Zeichen, die sich nicht zerlegen lassen und trotzdem eine feste
// Schreibweise haben. Die Umlaute stehen hier mit "e", weil das die
// deutsche Ersatzschreibung ist -- ein zerlegtes "ö" wäre nur "o" und
// aus "Österreich" würde "Osterreich".
struct Replacement {
    ushort code;
    const char* lower;   // vor Kleinbuchstaben oder am Wortende
    const char* upper;   // nur für Großbuchstaben, vor Großbuchstaben
};

const Replacement kReplacements[] = {
    {0x00C4, "Ae", "AE"}, {0x00D6, "Oe", "OE"}, {0x00DC, "Ue", "UE"},
    {0x00E4, "ae", "ae"}, {0x00F6, "oe", "oe"}, {0x00FC, "ue", "ue"},
    {0x00DF, "ss", "ss"}, {0x1E9E, "SS", "SS"},   // ß und das große ẞ
    {0x00C6, "Ae", "AE"}, {0x00E6, "ae", "ae"},   // Æ æ
    {0x00D8, "Oe", "OE"}, {0x00F8, "oe", "oe"},   // Ø ø
    {0x00C5, "Aa", "AA"}, {0x00E5, "aa", "aa"},   // Å å
    {0x0152, "Oe", "OE"}, {0x0153, "oe", "oe"},   // Œ œ
    {0x00D0, "D",  "D"},  {0x00F0, "d",  "d"},    // Ð ð
    {0x00DE, "Th", "TH"}, {0x00FE, "th", "th"},   // Þ þ
    {0x0141, "L",  "L"},  {0x0142, "l",  "l"},    // Ł ł
    {0x00B5, "u",  "u"},                           // µ (Mikro)
};

const Replacement* replacementFor(QChar c)
{
    for (const Replacement& entry : kReplacements) {
        if (entry.code == c.unicode()) { return &entry; }
    }
    return nullptr;
}

} // namespace

QString asciiOnly(const QString& text)
{
    QString out;
    out.reserve(text.size() + 8);

    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        const ushort code = c.unicode();

        if (code >= 32 && code <= 126) {
            out += c;
            continue;
        }
        if (code == '\n' || code == '\r' || code == '\t') {
            // Eine Abgabedatei ist zeilenweise aufgebaut; ein
            // Zeilenumbruch mitten in einem Feld teilte die Zeile.
            out += QLatin1Char(' ');
            continue;
        }
        if (code < 32 || code == 127) {
            continue;   // Steuerzeichen, nichts davon gehört in die Datei
        }

        if (const Replacement* entry = replacementFor(c)) {
            // Groß geschrieben und es folgt ein weiterer Großbuchstabe
            // (oder das Wort endet): "ÖSTERREICH" soll nicht
            // "OeSTERREICH" werden.
            bool allCaps = false;
            if (c.isUpper()) {
                allCaps = true;   // bis das Gegenteil dasteht
                for (int j = i + 1; j < text.size(); ++j) {
                    const QChar next = text.at(j);
                    if (!next.isLetter()) { break; }
                    allCaps = !next.isLower();
                    break;
                }
            }
            out += QLatin1String(allCaps ? entry->upper : entry->lower);
            continue;
        }

        // Alles übrige Lateinische zerlegen und die Akzente abwerfen:
        // "é" wird zu "e", "č" zu "c". Was danach noch jenseits von 126
        // liegt, hat keine lateinische Entsprechung und fällt weg --
        // ein Fragezeichen verschwiege, dass dort etwas stand, und ein
        // Umschriftwerk für Kyrillisch oder Griechisch gehört nicht in
        // ein Contestlogbuch.
        const QString decomposed = QString(c).normalized(QString::NormalizationForm_KD);
        for (const QChar& part : decomposed) {
            if (part.unicode() >= 32 && part.unicode() <= 126) {
                out += part;
            }
        }
    }
    return out;
}

} // namespace Contestprogramm
