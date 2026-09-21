# Contestprogramm

Ein eigenständiges Logprogramm für 24-Stunden-VHF/UHF-Conteste (2 m / 70 cm)
mit Echtzeit-Unterstützung: gefilterter ON4KST-Chat und DX-Cluster,
Vorschlag fürs nächste Ziel, Distanz/Peilung je Kontakt, Rotorsteuerung,
Terrain-Sichtlinien vom eigenen Standort. Qt 6 / C++20 / CMake, SQLite als
Log; macOS, Windows und Linux. Geschwisterprojekt zu Longpath, aber nicht
hineingebaut.

## Bauen und starten

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build
./build/Contestprogramm.app/Contents/MacOS/Contestprogramm      # macOS
cd build && ctest -j8                                            # Tests
```

Voraussetzungen: Qt 6 (Core, Widgets, Sql, Network, Test), zlib, für
CAT/Rotor Hamlib (`rigctld`/`rotctld`). `rotctld` startet das Programm
selbst, wenn ein Rotor auf `127.0.0.1` zeigt und in den Einstellungen
ein Gerät (Modell, Port, Baud) genannt ist -- ein bereits laufendes
`rotctld` hat Vorrang, ein selbst gestartetes endet mit dem Programm,
seine Fehlermeldung landet in der Statuszeile. Ein `rigctld`/`rotctld`,
das erst nach dem Programm hochkommt, wird alle drei Sekunden neu
gewählt.

## Daten

Alles liegt in einer SQLite-Datei im Anwendungsdatenordner
(macOS: `~/Library/Application Support/Contestprogramm/Contestprogramm/contestprogramm.sqlite`):
Log, Einstellungen, Layout-Profile, importierte Locator-Liste.

- **Sicherung:** alle fünf Minuten eine konsistente Kopie nach
  `…/backups/contestprogramm-YYYYMMDD-HHMM.sqlite` (nur wenn seit der
  letzten Kopie ein QSO geschrieben wurde; die ältesten fliegen ab 300
  Dateien). Manuell: *Datei › Log jetzt sichern*. Zurück auf einen Stand:
  *Datei › Sicherung wiederherstellen…* (der jetzige Stand wird vorher
  gesichert, das Programm startet neu).
- **Isolierter Testlauf:** `CONTESTPROGRAMM_DATA_DIR=/pfad ./Contestprogramm`
  öffnet eine andere Datenbank statt der echten.
- **Nur eine Instanz je Datenordner:** ein zweiter Start holt das laufende
  Programm nach vorn und beendet sich (Sperrdatei `contestprogramm.lock`).

## Contest-Definitionen

`resources/contest_definitions/*.json`, eine Datei je Contest; Änderungen
über *Datei › Contest-Regeln…* landen als Override im Anwendungsdatenordner.
Mitgeliefert: IARU R1 VHF (144, September), UHF/Microwave (432 + 1296,
Oktober), Marconi Memorial (144 CW, November), die Subregionals
(144 + 432, März/Mai/Juli) und ein ÖVSV-Contest.

```json
{
  "id": "IARU_R1_VHF_UHF",
  "name": "IARU Region 1 VHF/UHF Contest",
  "bands": ["144", "432"],
  "dupe_scope": ["callsign", "band", "mode"],
  "scoring": "distance_km",
  "serial_scope": "band",
  "modes": ["CW"],
  "schedule": { "month": 11, "weekend": 1, "day": "sat", "start": "14:00", "hours": 24 },
  "exchange_fields": [
    { "key": "rst",    "label": "RST",  "type": "rst" },
    { "key": "serial", "label": "Nr.", "type": "int", "auto_increment": true },
    { "key": "grid",   "label": "Grid", "type": "grid6" }
  ]
}
```

`scoring`: `distance_km` (Standard – Distanz zwischen den Locator-Mitten,
abgerundet auf ganze km plus 1, Summe je Band, keine Multiplikatoren; so
werten IARU R1, ÖVSV und DARC) oder `qso_count` (1 Punkt je QSO).
`serial_scope`: `band` (Standard – Seriennummer beginnt auf jedem Band bei
001, IARU-R1-Regel) oder `contest`. `dupe_scope`: bei IARU R1/ÖVSV
`["callsign", "band"]` – einmal je Band, unabhängig von der Betriebsart.
`modes` (optional): erlaubte Betriebsarten, sonst alle; bei genau einer
startet das Programm ohne CAT in dieser Betriebsart (Marconi: CW/599). `schedule`
(optional): wann der Contest läuft – „n-tes volles Wochenende im Monat,
Sa 14:00 UTC, 24 h" ist die IARU-R1-Regel, das Programm rechnet die Termine
für jedes Jahr selbst aus (Countdown in der Kopfzeile, Zeitprüfung); ein von
Hand gesetztes Contest-Ende in den Einstellungen hat Vorrang.

## Funktionen

| Bereich | Was | Wo |
|---|---|---|
| Loggen | Eingabezeile mit Contest-Exchange, Auto-Seriennummer, Dupe-Check, Run/S&P, Locator-Vorbelegung aus eigenem Log (auch aus früheren Contests), Locator-Liste, QRZ/HamQTH | Panel „Log" |
| Vor/nach dem Contest | Log abschließen und archivieren (Neustart bei 001, alte QSOs bleiben fürs Locator-Gedächtnis); Locator aus alten EDI/ADIF-Logs anderer Programme übernehmen | *Datei › Log abschließen…*, *Datei › Locator aus alten Logs übernehmen…* |
| Korrigieren | Call, Nr./Grid und Zeit direkt in der Log-Zeile (Doppelklick/Enter); statt Löschen „ungültig" markieren. Nach jeder Korrektur werden die Dupe-Markierungen des Logs neu berechnet (N1MM „Rescore") | Panel „Log" |
| Wertung | QSOs, Punkte (km je Band, Σ), 10 min/Stunde mit Trend und bester Stunde, ODX, Großfelder -- als Instrument, das der Panelgröße folgt: niedrig und breit die Zählerleiste mit Balken je Band und Sechs-Stunden-Sparkline, sonst Kacheln (bei 270×130 die vier wichtigsten, größer alle sechs mit Unterzeile) | Panel „Rate"; *Fenster › Statistik…* (je Band, je Stunde, längste QSOs) |
| Locator-Felder | gearbeitete/offene Großfelder je Band | *Fenster › Locator-Felder…* |
| Check Partial | Rufzeichen-Vorschläge beim Tippen aus Log, Locator-Liste, gehörten Stationen, SCP-Liste; N+1 ab vier Zeichen; Klick übernimmt Call+Locator | Panel „Check"; *Datei › SCP-Liste laden…* |
| Bandmap | Spots (KST/Cluster) auf der Frequenzachse, eigene Frequenz, gearbeitet gedimmt; Klick = QSY | Panel „Bandmap" |
| Skeds | Verabredungen mit Zeitleiste der nächsten Stunde; Eingabe von Hand oder als Vorschlag aus einer KST-Nachricht an dich; Klick = QSY + Rotor + Eingabezeile; Alarm 2 min vorher; ein QSO schließt den Sked | Panel „Skeds" |
| CW | Makro-Zeile, F1–F6 als Tasten (Tastung über `rigctld`), Esc stoppt, Alt+W leert die Eingabe; ESM: Enter sendet, was der QSO-Stand verlangt, und loggt erst am Ende | Checkbox „CW-Makros anzeigen" / „ESM"; *Datei › ESM-Texte…* |
| Feeds | ON4KST-Chat und DX-Cluster, geografisch gefiltert (Radius, Terrain), Nächstes-Ziel-Vorschlag mit Nachrichtenentwurf | Panele „Log" (Kandidaten), „Nächstes Ziel" |
| Karte / Rotoren | Panel „Karte / Verbindungen" in zwei Ansichten: **Radar** (Scheibe mit Ringen, Peilung, Stationen als Punkte, Rotor als Lichtkegel je Antenne, Horizont als dunkler Rand, Zahlen rechts) und **Karte + Horizont** (ruhige Landkarte, darunter die Skyline 0–360° mit jeder Station als Strich). Ebenen (Ringe, Peilung, Horizont, Rotoren, Zweitantenne je Rotor, Öffnungswinkel, Altern, Füllen; Grenzen, Städte, Raster) im ⚙-Menü rechts oben im Panelkopf, Ansicht und Ebenen bleiben gespeichert. Klick auf eine Station = QSY + Rotor; Klick auf „Offen in Richtung" funkt die nächste offene Station im Beam an (weiteste zuerst, reihum). Horizont aus den SRTM-Daten um den Standort (genaue Position aus den Einstellungen, sonst Locator-Mitte). Rotorskalen mit dem Öffnungswinkel als Kegel je Antenne (der Winkel aus dem ⚙-Menü der Karte) und der Ablesung Aktuell/Ziel/Entfernung in Glaszellen; Rotoren über `rotctld`, Standortvergleich per Horizont | Panele „Karte", „Rotoren"; *Datei › Standortvergleich…* |
| Log prüfen | Was der Auswerter beanstanden würde, vorher: Fehler (kein/kurzer Locator, keine empfangene Nummer, Zeit außerhalb des Contests, verbotene Betriebsart, doppelt gesendete Nummer, eigenes Rufzeichen), Warnungen (RST-Form, seltsames Rufzeichen, ein Call mit zwei Locatoren, > 1500 km, Frequenz ≠ Band, unmarkiertes Dupe, Nummer außer der Reihe), Hinweise (Lücken, Dupes, ungültige). Doppelklick springt zum QSO; der EDI-Export zeigt das Ergebnis und fragt bei Fehlern | *Datei › Log prüfen…* |
| Startcheck | Vor dem ersten CQ alles auf einen Blick: Station (Rufzeichen, Locator und ob der exakte Standort im selben Feld liegt, Höhen), Contest (Definition, Zeitfenster: Start in …/läuft/vorbei, Log leer oder QSOs vor dem Start), Verbindungen (CAT, Rotoren samt rotctld-Störung, ON4KST, Cluster), Daten (Sicherung, Geländedaten, Locator-Liste); „Bereit" ohne Fehler, alle 5 s neu geprüft | *Datei › Startcheck (bereit?)…* |
| Abgabe | **EDI/REG1TEST** (eine Datei je Band, das Format der IARU-R1/ÖVSV-Roboter), Cabrillo, ADIF | *Datei › EDI exportieren…* usw. |
| Scoreboard | Contest-Online-Score-XML per HTTP POST, aus bis konfiguriert | *Datei › Online-Scoreboard…* |

Neue Panele (Check, Bandmap, Skeds) sind in bereits gespeicherten Layout-Profilen
zunächst ausgeblendet – *Fenster › Panels* schaltet sie ein; die Position
bleibt dann im Profil.

## Abgleich mit N1MM Logger+ und DXLog.net

Beide sind Closed Source; übernommen wurden Verhalten und Dateiformate
(EDI, Cabrillo, ADIF, Call-History-Datei, `master.scp`, Contest-Online-
Score-XML), kein Code. Für einen 2 m/70 cm-Einzelop-Contest bewusst nicht
gebaut: hunderte KW-Regelwerke, SO2R, RTTY/PSK-Engines, WinKey/LPT,
Skimmer/RBN, Netzwerk-Multi-Op, Voice-Keyer.

## Lizenz und Herkunft

Siehe `NOTICE.md` (Attribution für portierten Locator-Code aus
freedv-gui/Longpath); die Projektlizenz ist noch nicht festgelegt.
