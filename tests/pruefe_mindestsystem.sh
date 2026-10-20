#!/bin/sh
# Prüft, mit welchem Mindestsystem eine macOS-Binärdatei gestempelt ist.
#
#   tests/pruefe_mindestsystem.sh <mach-o-datei> <höchstens-erlaubte-version>
#
# Hintergrund: CMAKE_OSX_DEPLOYMENT_TARGET wirkt nur, wenn es VOR
# project() gesetzt wird -- danach ist die Compiler-Erkennung gelaufen
# und die Zeile ist stillschweigend wirkungslos. Dann stempelt CMake die
# Version der Baumaschine ein, und die App startet auf älteren Macs
# nicht mehr ("erfordert eine neuere Version von macOS"). Das ist genau
# der Fehler, der am 2026-10-20 auf dem Mac mini 2018 aufgefallen ist.
#
# Deshalb wird hier die GEBAUTE Datei gefragt, nicht die CMake-Variable:
# eine wirkungslose Vorgabe und eine fehlende Vorgabe sehen in der
# Variable gleich aus (CMake füllt sie mit dem Hostwert), in der Datei
# aber nicht.
set -eu
BIN=${1:?Mach-O-Datei}
MAX=${2:?höchstens erlaubte Version, z.B. 12.0}

[ -f "$BIN" ] || { echo "nicht gefunden: $BIN"; exit 1; }

# vtool gehört zu den Command Line Tools; ohne es wird nicht stumm
# bestanden, sondern aufgeschlagen -- ein Prüfer, der bei Abwesenheit
# Erfolg meldet, ist kein Prüfer.
command -v vtool >/dev/null 2>&1 || { echo "vtool fehlt (Command Line Tools)"; exit 1; }

# Bei einer universellen Datei bringt vtool einen Block je Architektur.
MINOS=$(vtool -show-build "$BIN" | awk '/minos/ {print $2}' | sort -t. -k1,1n -k2,2n | tail -1)
[ -n "$MINOS" ] || { echo "kein minos in: $(vtool -show-build "$BIN")"; exit 1; }

# Versionsvergleich über eine Zahl (12.0 -> 1200, 11.3 -> 1103).
zahl() { echo "$1" | awk -F. '{printf "%d%02d", $1, ($2==""?0:$2)}'; }

if [ "$(zahl "$MINOS")" -gt "$(zahl "$MAX")" ]; then
    echo "FEHLER: $BIN verlangt macOS $MINOS, erlaubt sind höchstens $MAX."
    echo "        Steht set(CMAKE_OSX_DEPLOYMENT_TARGET ...) noch VOR project()?"
    exit 1
fi
echo "ok: Mindestsystem macOS $MINOS (erlaubt bis $MAX)"
