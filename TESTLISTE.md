# Offene Punkte zum Testen und Bauen

Gemeinsame Liste. Martin, 2026-09-30: „schreibe es in die liste für
später mal zu testen." Was hier steht, ist NICHT vergessen, sondern
bewusst aufgeschoben.

## Martin selbst (geht nur am Gerät / vor Ort)

- [ ] **Zweiter Sicherungsordner setzen.** USB-Stick anstecken,
      *Datei › Sicherung › Zweiter Sicherungsordner…*. Ohne das liegt
      jede Sicherung nur auf derselben Platte wie das Log.
- [ ] **Alltagsdurchgang vor dem Contest**: ein paar QSOs loggen, eines
      korrigieren, eines löschen, eines ungültig setzen, Band wechseln.
- [ ] **Chatraum-Nummern prüfen.** Belegt ist nur 2 (144/432). Ob
      1 = 50/70, 3 = Mikrowelle, 4 = EME, 5 = Kurzwelle wirklich
      stimmen, zeigt nach einem Wechsel der Chatkopf: dort steht, aus
      welchem Raum die Zeilen tatsächlich kommen.

## Claude (gebaut, aber noch nicht am echten Gerät geprüft)

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

## Zwei Fallen, die schon zugeschlagen haben

- Nach `git merge` **CMake neu einlesen**, sonst läuft ein neu
  hinzugekommener Prüfstand nicht mit und die Zahl „108/108" ist grün,
  ohne ihn ausgeführt zu haben (2026-09-30 genau so passiert).
- **Nichts installieren, bevor die volle Suite durch ist.** Am
  2026-09-29 hing das Programm bei 99 % CPU, weil ich eine Fassung
  kopiert habe, während die Suite noch lief -- sie hätte den Fehler
  gefunden.
