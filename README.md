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
gewählt; ON4KST und Cluster wählen nach einem Fehlschlag mit
wachsendem Abstand (5 s … 60 s) neu.

Läuft das Programm schon, übergibt ein zweiter Start an die laufende
Instanz -- außer die Instanz ist ein älterer Bau als das gestartete Programm:
dann startet sie sich selbst neu, mit dem neuen Bau. Bauen und starten reicht
also, ⌘Q ist nicht nötig.

## Daten

Alles liegt in einer SQLite-Datei im Anwendungsdatenordner
(macOS: `~/Library/Application Support/Contestprogramm/Contestprogramm/contestprogramm.sqlite`):
Log, Einstellungen, Layout-Profile, importierte Locator-Liste.

- **Sicherung:** jede Minute eine konsistente Kopie nach
  `…/backups/contestprogramm-YYYYMMDD-HHMM.sqlite` (nur wenn seit der
  letzten Kopie ein QSO geschrieben wurde). Die letzten zwei Stunden
  bleiben minutenweise, ältere Kopien eine je zehn Minuten, höchstens
  400 Dateien. Manuell: *Datei › Log jetzt sichern (Kopie)* (⌘S). Zurück auf einen Stand:
  *Datei › Sicherung wiederherstellen…* (der jetzige Stand wird vorher
  gesichert, das Programm startet neu). *Datei › Zweiter Sicherungsordner…*
  kopiert jede Sicherung zusätzlich auf einen USB-Stick oder in einen
  Cloud-Ordner -- ein abgezogener Stick stört die Sicherung neben der
  Datenbank nicht, er wird nur in der Statuszeile gemeldet.
- **Isolierter Testlauf:** `CONTESTPROGRAMM_DATA_DIR=/pfad ./Contestprogramm`
  öffnet eine andere Datenbank statt der echten.
- **Nur eine Instanz je Datenordner:** ein zweiter Start holt das laufende
  Programm nach vorn und beendet sich (Sperrdatei `contestprogramm.lock`).

## Contest-Definitionen

`resources/contest_definitions/*.json`, eine Datei je Contest; Änderungen
über *Datei › Contest-Regeln…* landen als Override im Anwendungsdatenordner.
Mitgeliefert: IARU R1 VHF (144, September), UHF/Microwave (432 + 1296,
Oktober), Marconi Memorial (144 CW, November), die Subregionals
(144 + 432, März/Mai/Juli), ein ÖVSV-Contest und ein Kurzwellen-Übungslog
(RST + laufende Nummer, keine echte Ausschreibung – zum Mitloggen am
Gerät).

Über *Datei › Contest-Regeln…* lässt sich **jede** dieser Regeln selbst
setzen – Name, Bänder, Exchange-Felder, Wertung, Nummernkreis,
Multiplikator, Dupe-Regel, erlaubte Betriebsarten, Cabrillo-Name – und
mit *Neuer Contest…* eine Ausschreibung von Null anlegen. Die Kennung
wird aus dem Namen abgeleitet und ändert sich danach nicht mehr: sie
steht in jedem geloggten QSO. *Zurücksetzen* wirft die eigenen
Einstellungen wieder weg.

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
001, IARU-R1-Regel) oder `contest` (eine Folge über den ganzen Contest).
`multiplier_field`: `grid` (Standard – Locator-Großfeld), `prefix`
(WPX-Regel, rechnet sich allein aus dem Rufzeichen), `dxcc` (Land,
braucht eine geladene Länderliste) oder `none`. `cabrillo_name`
(optional): der Name, unter dem der Robot den Contest kennt
(`CQ-WW-CW`); ohne ihn steht die interne Kennung im Kopf der Datei. `dupe_scope`: bei IARU R1/ÖVSV
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
| Loggen | Eingabezeile mit Contest-Exchange, Auto-Seriennummer, Dupe-Check, Run/S&P, Locator-Vorbelegung aus eigenem Log (auch aus früheren Contests; die empfangene Nummer wird nie vorbelegt), Locator-Liste, QRZ/HamQTH. Enter loggt aus jedem Feld; fehlt Nummer oder Locator, springt das erste Enter ins leere Feld, ein zweites Enter loggt trotzdem. Geloggte Dupes tragen „DUPE“, ungültige „UNGÜLTIG“; bei einem Dupe sagt die Statuszeile des Panels sofort, als welche Nummer und um wie viel Uhr (UTC) die Station schon im Log steht, und markiert die Zeile. Nummern stehen überall dreistellig (001); der Fokus liegt beim Start im Rufzeichenfeld, das neueste QSO bleibt sichtbar | Panel „Log" |
| Vor/nach dem Contest | *Datei › Neues Log beginnen (altes archivieren)* (Neustart bei 001, alte QSOs bleiben fürs Locator-Gedächtnis); Locator aus alten EDI/ADIF-Logs anderer Programme übernehmen | *Datei › Log abschließen…*, *Datei › Locator aus alten Logs übernehmen…* |
| Korrigieren | Call, Nr./Grid und Zeit direkt in der Log-Zeile (Doppelklick/Enter; in Nr./Grid zählt, was ein Wert ist -- „JN58SD“ allein bleibt der Locator, „12“ allein die Nummer); statt Löschen „ungültig" markieren (zählt dann nirgends mehr mit). Nach jeder Korrektur werden die Dupe-Markierungen des Logs neu berechnet (N1MM „Rescore") | Panel „Log" |
| Wertung | QSOs, Punkte (km je Band, Σ), 10 min/Stunde mit Trend und bester Stunde, ODX, Großfelder -- als Instrument, das der Panelgröße folgt: niedrig und breit die Zählerleiste mit Balken je Band und Sechs-Stunden-Sparkline, sonst Kacheln (bei 270×130 die vier wichtigsten, größer alle sechs mit Unterzeile) | Panel „Rate"; *Fenster › Statistik…* (je Band, je Stunde, längste QSOs) |
| Multiplikatoren | gearbeitete/offene Multiplikatoren je Band -- Locator-Großfelder, WPX-Präfixe oder Länder, je nach Contest-Regel; das Fenster heißt nach dem, was drinsteht | *Fenster › Locator-Felder…* |
| Kurzwelle | Bänder 1,8 bis 28 MHz, Band folgt dem Funkgerät; Multiplikator wahlweise WPX-Präfix (rechnet sich aus dem Rufzeichen) oder Land; Karte bis 20 000 km mit Graulinie; Cabrillo mit kHz und Kategorien. Ohne getauschten Locator setzt die **Länderliste** (cty.dat von country-files.com, selbst geladen) die Station auf den Mittelpunkt ihres Landes -- gepunkteter Hof heißt „ungefähr", ins Log kommt dieser Ort nie | *Datei › Länderliste laden (cty.dat)…* |
| Check Partial | Rufzeichen-Vorschläge beim Tippen aus Log, Locator-Liste, gehörten Stationen, SCP-Liste; N+1 ab vier Zeichen; Klick übernimmt Call+Locator | Panel „Check"; *Datei › SCP-Liste laden…* |
| Bandmap | Spots (KST/Cluster) auf der Frequenzachse, eigene Frequenz, gearbeitet gedimmt; Klick = QSY | Panel „Bandmap" |
| Skeds | Verabredungen mit Zeitleiste der nächsten Stunde; Eingabe von Hand oder als Vorschlag aus einer KST-Nachricht an dich; Klick = QSY + Rotor + Eingabezeile; Alarm 2 min vorher; ein QSO schließt den Sked | Panel „Skeds" |
| CW | Makro-Zeile, F1–F6 als Tasten (Tastung über `rigctld`), Esc stoppt, Alt+W leert die Eingabe; ESM: Enter sendet, was der QSO-Stand verlangt, und loggt erst am Ende | Checkbox „CW-Makros anzeigen" / „ESM"; *Datei › ESM-Texte…* |
| Feeds | ON4KST-Chat und DX-Cluster, geografisch gefiltert (Radius, Terrain), Nächstes-Ziel-Vorschlag mit Nachrichtenentwurf | Panele „Log" (Kandidaten), „Nächstes Ziel" |
| Karte / Rotoren | Panel „Karte / Verbindungen" als **Radar**: Scheibe mit Ringen, Peilung, Stationen als Punkte, Rotor als Lichtkegel je Antenne, Horizont als dunkler Rand, Zahlen rechts. Ebenen (Ringe, Peilung, Horizont, Rotoren, Zweitantenne je Rotor, Öffnungswinkel, Altern, Füllen; Grenzen, Städte, Raster, gearbeitete Felder) im ⚙-Menü rechts oben im Panelkopf, bleiben gespeichert. Klick auf eine Station = QSY + Rotor; Klick auf „Offen in Richtung" funkt die nächste offene Station im Beam an (weiteste zuerst, reihum). Horizont aus den SRTM-Daten um den Standort (genaue Position aus den Einstellungen, sonst Locator-Mitte). Rotorskalen mit dem Öffnungswinkel als Kegel je Antenne (der Winkel aus dem ⚙-Menü der Karte) und der Ablesung Aktuell/Ziel/Entfernung in Glaszellen -- in einem niedrigen Panel weicht die Ablesung zeilenweise, in einem schmalen erst auf die kleine Schrift und dann ganz, die Skala bleibt und wird nie abgeschnitten; Rotoren über `rotctld`, Standortvergleich per Horizont | Panele „Karte", „Rotoren"; *Datei › Standortvergleich…* |
| Log prüfen | Was der Auswerter beanstanden würde, vorher: Fehler (kein/kurzer Locator, keine empfangene Nummer, Zeit außerhalb des Contests, verbotene Betriebsart, doppelt gesendete Nummer, eigenes Rufzeichen), Warnungen (RST-Form, seltsames Rufzeichen, ein Call mit zwei Locatoren, > 1500 km, Frequenz ≠ Band, unmarkiertes Dupe, Nummer außer der Reihe), Hinweise (Lücken, Dupes, ungültige). Doppelklick springt zum QSO; der EDI-Export zeigt das Ergebnis und fragt bei Fehlern | *Datei › Log prüfen…* |
| Startcheck | Vor dem ersten CQ alles auf einen Blick: Station (Rufzeichen, Locator und ob der exakte Standort im selben Feld liegt, Höhen), Contest (Definition, Zeitfenster: Start in …/läuft/vorbei, Log leer oder QSOs vor dem Start), Uhrzeit gegen einen Zeitserver (Warnung ab 5 s, Fehler ab 1 min Abweichung), Transverter (eingerichtet? an?), Verbindungen (CAT, Rotoren samt rotctld-Störung, ON4KST, Cluster), Daten (Sicherung und zweiter Sicherungsordner, Geländedaten, Locator-Liste); „Bereit" ohne Fehler, alle 5 s neu geprüft | *Datei › Startcheck (bereit?)…* |
| Transverter | ZF-Band des Funkgeräts → Band auf der Antenne mit Offset (aus den Bändern vorgeschlagen: 1296 − 144 = 1152 MHz, überschreibbar); der Schalter in der Kopfzeile sagt, ob er dran ist. Band im Log, Rotor-Zuordnung, Bandmap und QSY rechnen dann mit der Antennenfrequenz; ein Band außerhalb des Contests (Funkgerät auf KW geparkt) verstellt das Log-Band nicht mehr | *Datei › Transverter…*, Schalter „Transverter" oben |
| Tastenkürzel | Alle Tasten und Griffe auf einer Seite (Eingabezeile, CW, Korrekturen, Klicks auf Station/Spot/Radar, Rotor-Ziel, Panels) | *Hilfe › Tastenkürzel…* |
| Über | Version, Commit und Baudatum (bei jedem Bauen erzeugt), Qt, Datenbank- und Sicherungspfade, „Datenordner zeigen" | *Hilfe › Über Contestprogramm…* |
| Contest wählen | Liste der Definitionen; der eigene Locator wird jedes Mal mit abgefragt (vorbelegt, OK nur mit gültigem Locator, „Exakter Standort: JN67UT übernehmen" wenn die Einstellungen woanders liegen) | *Datei › Contest wählen…* |
| Abgabe | **EDI/REG1TEST** (eine Datei je Band, das Format der IARU-R1/ÖVSV-Roboter), Cabrillo (fragt vorher nach Bediener, Hilfsmitteln, Leistung, Sender, Station, Club, E-Mail und merkt sich die Angaben), ADIF | *Datei › EDI exportieren…* usw. |
| Scoreboard | Contest-Online-Score-XML per HTTP POST, aus bis konfiguriert | *Datei › Online-Scoreboard…* |
| Farbthema | Zwei Varianten: **Bernstein** (Standard, die Longpath-Farben) und **Grün** (dunkles Anthrazit, helle Schrift, grüner Akzent für Messwerte und Kopfzeilen, Bernstein nur noch für Warnungen); wird beim nächsten Start wirksam | *Datei › Einstellungen › Farbthema* |

Neue Panele (Check, Bandmap, Skeds) sind in bereits gespeicherten Layout-Profilen
zunächst ausgeblendet – *Fenster › Panels* schaltet sie ein; die Position
bleibt dann im Profil. Ein frischer Start (und *Fenster › Fenster zurücksetzen*)
wählt die Anordnung nach der Fensterfläche: die große für Flächen ab
1440×982, sonst die kompakte für ein 13"-Notebook (Rotoren und Karte oben,
Nächstes Ziel und Rate darunter, Log über die Breite, Bandmap daneben;
Check und Skeds ausgeblendet), auf größeren Flächen gestreckt.
In einem schmalen Log-Panel rücken die Spalten zusammen; reicht das nicht,
weichen zuerst die Peilung und dann der eigene Exchange, bevor ein
Rollbalken kommt -- Zeit, Call, empfangener Exchange, km und Status bleiben.

## Ablauf am Contest-Wochenende

Vorher, zu Hause:

1. *Datei › Contest wählen…*: den Contest (`IARU_R1_UHF` für den
   UHF/Mikrowellen-Contest im Oktober) — und den **Locator des
   Contest-Standorts**, der dort jedes Mal abgefragt wird (nicht der
   Heim-Locator; liegt der exakte Standort aus den Einstellungen in einem
   anderen Feld, bietet ein Knopf ihn an). Höhe und Antennenhöhe unter
   *Datei › Einstellungen*.
2. Rotoren: Gerät/Modell/Baud je Slot, dann startet das Programm `rotctld`
   selbst; oder `rotctld` von Hand. Band-Zuordnung (welcher Rotor für
   welches Band).
3. *Datei › Transverter…*, falls ein Band über einen Transverter läuft;
   der Schalter oben bleibt aus, bis er wirklich dranhängt.
4. *Datei › Neues Log beginnen (altes archivieren)*, damit die Nummern bei 001
   beginnen; *Datei › Locator aus alten Logs übernehmen* für die
   Locator-Vorschläge.
5. *Datei › Zweiter Sicherungsordner…* auf einen USB-Stick.
6. Einmal mit Internet starten, damit die Geländedaten um den Standort
   im Cache liegen (Startcheck: „Geländedaten“).

Am Standort, vor dem ersten CQ: *Datei › Startcheck (bereit?)…* — alles
grün oder bewusst gelb (CAT, Rotoren, ON4KST, Uhrzeit, Sicherung).

Danach: *Datei › Log prüfen…*, dann *Datei › EDI exportieren…* (eine Datei
je Band); der ÖVSV-Roboter nimmt die Logs etwa vier Tage nach Contestende
an.

## Abgleich mit N1MM Logger+ und DXLog.net

Beide sind Closed Source; übernommen wurden Verhalten und Dateiformate
(EDI, Cabrillo, ADIF, Call-History-Datei, `master.scp`, Contest-Online-
Score-XML), kein Code. Für einen 2 m/70 cm-Einzelop-Contest bewusst nicht
gebaut: hunderte KW-Regelwerke, SO2R, RTTY/PSK-Engines, WinKey/LPT,
Skimmer/RBN, Netzwerk-Multi-Op, Voice-Keyer.

## Download

Fertige Pakete liegen bei den [GitHub-Releases](https://github.com/oe5sos/Contestprogramm/releases):
macOS (Apple Silicon) als DMG. Das Programm ist nicht bei Apple beglaubigt --
beim ersten Start Rechtsklick › Öffnen. Windows- und Linux-Pakete folgen
über die CI. Die Seite dazu: <https://www.longpath.at/contest/>.

Ein laufendes Programm holt sich die nächste Version selbst: **Hilfe ›
Auf neueste Version aktualisieren…** fragt die GitHub-Releases ab, lädt das
Paket für den eigenen Rechner (DMG, portables ZIP oder AppImage), prüft die
SHA-256-Summe, setzt es an die Stelle der laufenden Kopie und startet neu.
Ein Entwicklungsbau (nicht als App-Paket/AppImage gestartet) meldet nur,
dass es eine neuere Version gibt.

## Lizenz und Herkunft

GNU General Public License v3.0 oder später, siehe `LICENSE`. Attribution
für portierten Locator-Code (freedv-gui, über Longpath) in `NOTICE.md`.
