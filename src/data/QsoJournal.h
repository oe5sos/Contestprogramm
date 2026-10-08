#pragma once

#include <QString>

namespace Contestprogramm {

struct QsoRecord;
// STRUCT, nicht class: ContestSettings ist ein struct (siehe
// app/ContestSettings.h). MSVC nimmt das Schlüsselwort mit ins
// Namens-Mangling, clang und gcc nicht -- deshalb baute es hier
// anstandslos und der Windows-Linker fand schreibe() nicht
// ("unresolved external symbol", 2026-09-30).
struct ContestSettings;

// Die zweite Spur für jedes geloggte QSO.
//
// Martin, 2026-09-29: "primäres ziel und das muss zu 100 % passen sind
// meine logs, die ich eingebe. die dürfen nicht weg sein."
//
// Die Datenbank ist die erste Spur und seit heute gegen Stromausfall
// gesichert (PRAGMA synchronous = FULL, siehe ContestDatabase). Eine
// Datenbank kann aber auch auf andere Weise versagen: Datei gesperrt,
// Platte voll, Schema kaputt, ein Fehler in diesem Programm. Dann
// steht das QSO in der Eingabezeile und sonst nirgends.
//
// Dieses Journal hängt jedes QSO als ADIF-Zeile an eine einfache
// Textdatei an -- BEVOR die Datenbank es zu sehen bekommt, mit flush
// und fsync. Es ist bewusst das dümmste denkbare Verfahren: kein
// Schema, keine Transaktion, kein Index, nichts, was kaputtgehen
// könnte. Eine Zeile, angehängt, auf Platte.
//
// Wiederherstellen heißt dann: die Datei ist gültiges ADIF und lässt
// sich in dieses Programm (oder jedes andere Logbuch) importieren.
class QsoJournal {
public:
    // `pfad` ist die Journaldatei, üblicherweise neben der Datenbank.
    explicit QsoJournal(const QString& pfad);

    // Eine Zeile anhängen. Gibt false zurück, wenn das nicht gelingt --
    // der Aufrufer soll das sehen, statt es zu verschlucken.
    bool schreibe(const QsoRecord& record, const ContestSettings& settings);

    // Was mit einem schon geloggten QSO passiert ist -- gelöscht,
    // zurückgeholt, ungültig gesetzt -- als Zeile mit Zeitstempel in
    // eine zweite Datei neben dem Journal ("...-vorgaenge.log").
    //
    // Das Journal selbst bleibt reines ADIF und wird nie verändert: was
    // einmal drinsteht, bleibt. Es beantwortet damit aber nicht die
    // Frage, die am 2026-10-08 offenblieb, als bei einem Prüflauf zwei
    // QSOs im Papierkorb lagen und niemand sagen konnte, wodurch --
    // die Datenbank merkt sich nur den Zustand, nicht den Weg dorthin.
    // Diese Datei merkt sich den Weg.
    bool vermerke(const QString& vorgang);

    QString vorgangsPfad() const;

    QString pfad() const { return m_pfad; }
    QString letzterFehler() const { return m_letzterFehler; }

private:
    QString m_pfad;
    QString m_letzterFehler;
};

} // namespace Contestprogramm
