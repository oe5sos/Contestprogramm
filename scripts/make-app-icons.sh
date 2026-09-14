#!/bin/sh
# =================================================================
# scripts/make-app-icons.sh  (Contestprogramm)
# =================================================================
#
# Erzeugt aus EINER SVG-Quelle das macOS-Programmsymbol, nach exakt
# demselben Verfahren wie das Geschwisterprojekt Longpath (siehe
# ~/Longpath/NereusSDR/scripts/make-app-icons.sh).
# Quelle: resources/branding/contestprogramm-appicon.svg
#
# Aufruf aus dem Wurzelverzeichnis:  sh scripts/make-app-icons.sh
#
# Nur macOS: qlmanage und iconutil sind Apple-Werkzeuge.
# =================================================================

set -e
SRC=resources/branding/contestprogramm-appicon.svg
OUT=resources/mac
TMP=$(mktemp -d)

# Aus der SVG-Quelle in PNG. qlmanage kann SVG auf macOS, rsvg-convert
# waere schoener, ist aber nicht installiert. Deshalb der Umweg ueber
# eine grosse PNG und sips zum Verkleinern -- sips rechnet sauber
# herunter, aber nicht aus SVG herauf.
qlmanage -t -s 1024 -o "$TMP" "$SRC" >/dev/null 2>&1
BIG="$TMP/$(basename $SRC).png"
[ -f "$BIG" ] || { echo "qlmanage hat nichts erzeugt"; exit 1; }

mkdir -p "$OUT/AppIcon.iconset"
for s in 16 32 64 128 256 512 1024; do
  sips -z $s $s "$BIG" --out "$TMP/icon_${s}.png" >/dev/null
done
cp "$TMP/icon_16.png"   "$OUT/AppIcon.iconset/icon_16x16.png"
cp "$TMP/icon_32.png"   "$OUT/AppIcon.iconset/icon_16x16@2x.png"
cp "$TMP/icon_32.png"   "$OUT/AppIcon.iconset/icon_32x32.png"
cp "$TMP/icon_64.png"   "$OUT/AppIcon.iconset/icon_32x32@2x.png"
cp "$TMP/icon_128.png"  "$OUT/AppIcon.iconset/icon_128x128.png"
cp "$TMP/icon_256.png"  "$OUT/AppIcon.iconset/icon_128x128@2x.png"
cp "$TMP/icon_256.png"  "$OUT/AppIcon.iconset/icon_256x256.png"
cp "$TMP/icon_512.png"  "$OUT/AppIcon.iconset/icon_256x256@2x.png"
cp "$TMP/icon_512.png"  "$OUT/AppIcon.iconset/icon_512x512.png"
cp "$TMP/icon_1024.png" "$OUT/AppIcon.iconset/icon_512x512@2x.png"

iconutil -c icns "$OUT/AppIcon.iconset" -o "$OUT/AppIcon.icns"
cp "$TMP/icon_512.png" "$OUT/AppIcon.png"
echo "icns + png erzeugt"
rm -rf "$TMP"
