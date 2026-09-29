#pragma once

#include <QString>

namespace Contestprogramm {

struct QsoRecord;
class ContestSettings;

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

    QString pfad() const { return m_pfad; }
    QString letzterFehler() const { return m_letzterFehler; }

private:
    QString m_pfad;
    QString m_letzterFehler;
};

} // namespace Contestprogramm
