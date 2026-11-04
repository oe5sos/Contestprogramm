# Offene Punkte zum Testen und Bauen

Gemeinsame Liste. Martin, 2026-09-30: „schreibe es in die liste für
später mal zu testen." Was hier steht, ist NICHT vergessen, sondern
bewusst aufgeschoben.

## Martin selbst (geht nur am Gerät / vor Ort)

- [ ] **Zweiter Sicherungsordner setzen.** USB-Stick anstecken,
      *Datei › Sicherung › Zweiter Sicherungsordner…*. Ohne das liegt
      jede Sicherung nur auf derselben Platte wie das Log.
- [ ] **Eine EDI-Datei wirklich einreichen.** Das Programm hält jetzt
      die REG1TEST-Spezifikation Feld für Feld ein (Zeichensatz,
      Zeilenlänge, Spaltenlängen -- `tests/test_edi_spezifikation.cpp`),
      aber ob der Robot des jeweiligen Contests sie annimmt, zeigt nur
      die Einreichung selbst. Am besten vorab mit einem Übungslog.
- [ ] **Alltagsdurchgang vor dem Contest**: ein paar QSOs loggen, eines
      korrigieren, eines löschen, eines ungültig setzen, Band wechseln.
- [ ] **Chatraum-Nummern prüfen.** Belegt ist nur 2 (144/432). Ob
      1 = 50/70, 3 = Mikrowelle, 4 = EME, 5 = Kurzwelle wirklich
      stimmen, zeigt nach einem Wechsel der Chatkopf: dort steht, aus
      welchem Raum die Zeilen tatsächlich kommen.

## Claude (gebaut, aber noch nicht am echten Gerät geprüft)

- [ ] **Wasserfall gegen das echte Longpath.** Gebaut und belegt ist der
      Rahmenleser (`tests/test_wasserfall.cpp`, samt Gegenprobe mit einem
      Rahmen, der 8192 Punkte verspricht und 10 mitbringt) und die Leitung
      gegen ein WebSocket-Doppel (`tests/test_wasserfall_leitung.cpp`),
      dazu ein Bild aus dem laufenden Fenster. **Noch offen:** ein Lauf
      gegen das wirkliche Longpath mit einem Gerät daran -- stimmt die
      Breite des Bildes mit dem, was Longpath zeigt, und hält die Leitung
      über Stunden? *Datei › Einstellungen › Wasserfall*, Longpath-Host
      und -Port.

- [ ] **Fassungspruefung beim Start, aus einem echten Paket.** Im
      Pruefstand belegt sind die beiden Lagen, die ein Entwicklungsbau
      ueberhaupt erreichen kann: Haken aus („Pruefung aus“) und Haken
      an ohne App-Paket („Eigenbau“), dazu die Anzeige der neuen
      Fassung (`tests/test_auto_pruefung.cpp`). **Noch offen:** ein
      Start aus dem installierten `Contestprogramm.app`, wenn auf
      GitHub eine neuere Fassung liegt -- dann muss in der Statuszeile
      „v0.1.7 · 0.1.8 verfuegbar“ stehen und ein Klick darauf den
      Updater oeffnen. Das geht erst mit dem naechsten Release.

- [ ] **Kartenkegel folgt dem Rotorzeiger** — im Prüfstand belegt
      (31°/39°), am echten Rotor noch nicht nachgefahren.
- [ ] **Magenta im Chat** — Prüfstand und Bild vorhanden; live erst
      sichtbar, wenn dich jemand anspricht.
- [ ] **Raumwechsel im Chat** — am Prüfstand-Server bewiesen, am echten
      ON4KST noch nicht durchgeführt.

## Noch zu bauen

- [ ] **Platte läuft voll** — was passiert beim Loggen, wenn kein Platz
      mehr ist? Das Journal schreibt dann ebenfalls nicht; ein
      Prüfstand dafür fehlt.
- [ ] **QSO-Bearbeitungsfenster.** Änderbar sind bisher nur Rufzeichen,
      Nr./Locator und Zeit. Band, Modus und RST kommt man nicht an --
      N1MM und DXLog öffnen dafür ein Fenster mit allen Feldern.
- [ ] **Dauerlauf mit echten Netzdiensten** über Stunden (ON4KST und
      Cluster verbunden), nicht nur gegen den Prüfstand-Server.

## Fallen, die schon zugeschlagen haben

- Nach `git merge` **CMake neu einlesen**, sonst läuft ein neu
  hinzugekommener Prüfstand nicht mit und die Zahl „108/108" ist grün,
  ohne ihn ausgeführt zu haben (2026-09-30 genau so passiert).
- **Nichts installieren, bevor die volle Suite durch ist.** Am
  2026-09-29 hing das Programm bei 99 % CPU, weil ich eine Fassung
  kopiert habe, während die Suite noch lief -- sie hätte den Fehler
  gefunden.
- **Neuer Code muss von Anfang an auf allen drei Plattformen gedacht
  werden**, nicht von der CI nachgereicht bekommen. Am 2026-09-30 zwei
  Windows-Fehler hintereinander aus EINER neuen Datei (QsoJournal):
  `<unistd.h>`/`fsync()` gibt es dort nicht (`_commit()` aus `<io.h>`),
  und eine Vorwärtsdeklaration als `class` statt `struct` lässt den
  MSVC-Linker ein Symbol suchen, das so nie erzeugt wurde -- clang und
  gcc ist der Unterschied egal.
