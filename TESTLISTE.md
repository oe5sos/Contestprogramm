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

- [ ] **Panels einzeln in Longpath (2026-11-04, #72 + oe5sos/Longpath#386).**
      Longpath › Tools › Contest logbook panels: ein Panel anhaken, es
      muss als eigenes Fenster neben dem Longpath-Hauptfenster erscheinen,
      OHNE dass das Logbuch-Hauptfenster aufgeht; ✕ am Panel dockt es
      zurück (unsichtbar, weil das Logbuch zu ist); danach Tools ›
      Contest Logbook öffnen: dasselbe Logbuch, das Panel wieder drin.
      Zu sehen ist vor allem, was der offscreen-Prüfstand nicht kann: ob
      macOS ein Werkzeugfenster zeigt, dessen Elternfenster verborgen
      ist, und ob das Panel beim Minimieren von Longpath mit ins Dock geht.
      Prüfstände: `test_einbettung::panelsLassenSichVonAussenAbloesen`,
      Longpath `tst_contestfenster::einPanelOhneLogbuchFenster`.
- [ ] **Wasserfall gegen das echte Longpath — Ursache gefunden, am
      Gerät noch nicht nachgefahren.** Gefahren am 2026-11-02: Longpath
      protokolliert „Spektrum abonniert rx 0, 718 Punkte" und sendet
      **null Binärrahmen** (nackter Prüf-Client; das Logprogramm ist gegen
      ein Doppel mit 400 Rahmen belegt). Longpath-Seite, 2026-11-03: der
      Spektrum-Abgriff hing an der FFTEngine von Strom 0, und die rechnet
      bei der Flex nichts (Bild kommt fertig von der Maschine). Behoben in
      oe5sos/Longpath#378 (`RadioModel::spektrumBereit`, geräteunabhängig;
      Gegenprobe: alte Fassung 0 Rahmen, neue 7/7 grün). Dort steht jetzt
      auch die Zeile „erstes Spektrumbild gesendet" im Longpath-Protokoll —
      fehlt sie bei stehendem Abonnement, ist Longpath die Seite, nicht das
      Logprogramm. Am 2026-11-03 mit dem Bau aus #378 an der Flex
      gefahren: **65 Binärrahmen in 8 s** (vorher 0), „erstes
      Spektrumbild gesendet" 43 ms nach dem Abonnement, Spanne 200 kHz.
      Um 17:33 das Logprogramm aus /Applications dazu: abonniert 718
      Punkte, erstes Bild 7 ms später, **das Panel zeigt das Band**
      (Spektrumlinie mit Trägern, laufende Wasserfallzeilen). Seit #69
      hat das Panel eine Frequenzskala und die Marke der eigenen
      Sendefrequenz (aus `spectrum_centre`/`spectrum_span`/`tx_frequency`);
      am Prüfstand belegt, am echten Longpath (braucht #378) noch nicht
      gesehen. Offen bleibt: stimmt die Skala mit Longpaths Fenster
      überein, hält die Leitung über Stunden?

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
