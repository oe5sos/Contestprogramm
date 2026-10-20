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
#
# Das Ziel darf eine einzelne Mach-O-Datei ODER ein .app-Paket sein. Beim
# Paket wird JEDE ausführbare Datei darin geprüft -- die App ist nur so
# alt wie die neueste Datei in ihrem Paket. Ein mit Mindestsystem 12
# gebautes Programm startet trotzdem nicht auf Monterey, wenn
# macdeployqt Qt-Frameworks hineinkopiert hat, die für ein neueres
# System gebaut sind.
set -eu
BIN=${1:?Mach-O-Datei oder .app-Paket}
MAX=${2:?höchstens erlaubte Version, z.B. 12.0}

[ -e "$BIN" ] || { echo "nicht gefunden: $BIN"; exit 1; }

# vtool gehört zu den Command Line Tools; ohne es wird nicht stumm
# bestanden, sondern aufgeschlagen -- ein Prüfer, der bei Abwesenheit
# Erfolg meldet, ist kein Prüfer.
command -v vtool >/dev/null 2>&1 || { echo "vtool fehlt (Command Line Tools)"; exit 1; }

# Bei einer universellen Datei bringt vtool einen Block je Architektur;
# beim Paket kommen die Blöcke aller enthaltenen Dateien dazu. Gesucht
# ist in beiden Fällen der HÖCHSTE Wert -- er entscheidet, ob die App
# startet.
if [ -d "$BIN" ]; then
    DATEIEN=$(find "$BIN" -type f -perm -u+x -o -type f -name '*.dylib' | sort -u)
    # Jede Datei einzeln fragen und den schlimmsten Wert behalten; `file`
    # siebt Skripte und Datendateien aus, die vtool nur anmeckern würde.
    MINOS=""; SCHULDIG=""
    for f in $DATEIEN; do
        case "$(file -b "$f")" in *Mach-O*) ;; *) continue ;; esac
        m=$(vtool -show-build "$f" 2>/dev/null | awk '/minos/ {print $2}' \
            | sort -t. -k1,1n -k2,2n | tail -1)
        [ -n "$m" ] || continue
        if [ -z "$MINOS" ] || [ "$(echo "$m $MINOS" | awk -F'[. ]' '{print ($1*100+$2 > $3*100+$4)}')" = "1" ]; then
            MINOS=$m; SCHULDIG=$f
        fi
    done
    [ -n "$MINOS" ] || { echo "keine Mach-O-Datei in $BIN gefunden"; exit 1; }
else
    MINOS=$(vtool -show-build "$BIN" | awk '/minos/ {print $2}' | sort -t. -k1,1n -k2,2n | tail -1)
    SCHULDIG=$BIN
    [ -n "$MINOS" ] || { echo "kein minos in: $(vtool -show-build "$BIN")"; exit 1; }
fi

# Versionsvergleich über eine Zahl (12.0 -> 1200, 11.3 -> 1103).
zahl() { echo "$1" | awk -F. '{printf "%d%02d", $1, ($2==""?0:$2)}'; }

if [ "$(zahl "$MINOS")" -gt "$(zahl "$MAX")" ]; then
    echo "FEHLER: verlangt macOS $MINOS, erlaubt sind höchstens $MAX."
    echo "        Die höchste Marke trägt: $SCHULDIG"
    echo "        Ist es das Programm selbst, steht set(CMAKE_OSX_DEPLOYMENT_TARGET ...)"
    echo "        vermutlich nicht mehr VOR project(). Ist es ein Qt-Framework,"
    echo "        wurde mit einem Qt gebaut, das selbst ein neueres System verlangt"
    echo "        (Homebrew-Qt ist immer für das System der Baumaschine gebaut --"
    echo "        verteilbare Pakete brauchen das offizielle Qt-Paket)."
    exit 1
fi
echo "ok: Mindestsystem macOS $MINOS (erlaubt bis $MAX), höchste Marke: $SCHULDIG"
