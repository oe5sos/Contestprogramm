#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

namespace Contestprogramm {

// Ein DXCC-Gebiet, so wie eine cty.dat-Zeile es beschreibt.
struct CountryEntry {
    QString name;           // "Austria"
    QString primaryPrefix;  // "OE"
    QString continent;      // "EU"
    int cqZone = 0;
    int ituZone = 0;
    double latitudeDeg = 0.0;   // Nord positiv
    double longitudeDeg = 0.0;  // OST positiv -- cty.dat führt West positiv, hier umgedreht
    // Die Zeitverschiebung des Gebietes gegen UTC, Ost positiv (Wien
    // +1). cty.dat führt auch dieses Feld andersherum, hier ebenfalls
    // umgedreht. Grobe Angabe je Gebiet, keine Sommerzeit -- sie sagt,
    // ob am anderen Ende gerade Nacht ist, nicht wann dort der Bus fährt.
    double utcOffsetHours = 0.0;
    bool isValid() const { return !primaryPrefix.isEmpty(); }
};

// Von welchem Rufzeichen kommt welches Land -- die Zuordnung, die auf
// Kurzwelle an die Stelle des Locators tritt: dort tauscht niemand
// einen, und ohne Ort gibt es weder Entfernung noch Richtung noch einen
// Punkt auf der Karte.
//
// Contestprogramm-original (2026-09-22). Die Daten kommen NICHT aus
// diesem Programm: es liest das verbreitete cty.dat-Format (AD1C,
// country-files.com), genau wie die Super-Check-Partial-Liste eine
// Datei ist, die der Bediener selbst lädt (Datei > SCP-Liste laden).
// Im Programm steckt die Mechanik, nicht die Länderliste -- so bleibt
// die Pflege dort, wo sie hingehört, und nichts Fremdes liegt im
// Quelltext.
//
// Format einer Aufzeichnung (durch ';' getrennt): eine Kopfzeile mit
// acht durch ':' getrennten Feldern -- Name, CQ-Zone, ITU-Zone,
// Kontinent, Breite, Länge (West positiv), Zeitverschiebung,
// Hauptpräfix -- und danach die Präfixe, durch Kommas getrennt. Ein
// Präfix darf Abweichungen tragen: "=RUFZEICHEN" für ein einzelnes
// Rufzeichen, "(n)" CQ-Zone, "[n]" ITU-Zone, "<lat/lon>" Ort, "{CC}"
// Kontinent, "~tz~" Zeitzone.
class CountryPrefixIndex {
public:
    // Liest eine cty.dat. Der bisherige Inhalt wird ersetzt -- nur bei
    // Erfolg; schlägt das Lesen fehl, bleibt die alte Liste stehen,
    // damit eine kaputte Datei nicht mitten im Contest alles wegnimmt.
    bool loadFromCty(const QByteArray& text, QString* errorOut = nullptr);

    bool isEmpty() const { return m_entries.isEmpty(); }
    int countryCount() const { return m_entries.size(); }
    int prefixCount() const { return m_byPrefix.size(); }

    // Das Land zu einem Rufzeichen: zuerst die Liste der einzeln
    // eingetragenen Rufzeichen (die "="-Einträge), dann der längste
    // passende Präfix. Ein Zusatz am Schrägstrich entscheidet mit --
    // DL/OE5SOS ist Deutschland, OE5SOS/P bleibt Österreich. Ohne
    // Treffer ein leerer Eintrag (isValid() == false); geraten wird
    // nicht.
    CountryEntry lookup(const QString& callsign) const;

    // Der Teil des Rufzeichens, mit dem gesucht wird -- öffentlich, weil
    // genau daran die Fälle hängen, die man prüfen will.
    static QString lookupKey(const QString& callsign);

private:
    QVector<CountryEntry> m_entries;
    QHash<QString, int> m_byPrefix;    // Präfix -> Index in m_entries
    QHash<QString, int> m_byExactCall; // "="-Einträge
};

} // namespace Contestprogramm
