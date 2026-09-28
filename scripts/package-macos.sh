#!/bin/zsh
# Packt das gebaute Contestprogramm.app zu einem verteilbaren DMG.
#
#   scripts/package-macos.sh <build-dir> <ausgabe-dir> [macdeployqt]
#
# Erwartet ein Release-Bau-Verzeichnis (cmake -DCMAKE_BUILD_TYPE=Release)
# mit Contestprogramm.app. Schritte, jeder einzeln nachvollziehbar:
#   1. macdeployqt kopiert die Qt-Frameworks und Plug-ins ins Bundle.
#   2. Homebrew-Qt zieht dabei QtQml/QtQuick (Virtual Keyboard) und
#      Bildformat-Plug-ins mit unauflösbaren Abhängigkeiten (QtPdf, QtSvg,
#      WebP) mit -- alles, was dieses Programm nicht braucht. Weg damit,
#      danach jede nicht mehr referenzierte Bibliothek ebenfalls.
#   3. Die Icon-Quellen (resources/mac, resources/branding) haben im
#      Bundle nichts verloren; ein .iconset-Ordner unter Contents/MacOS
#      lässt codesign sogar scheitern.
#   4. Ad-hoc-Signatur (ohne Developer ID): läuft auf Apple Silicon, beim
#      ersten Start Rechtsklick › Öffnen.
#   5. DMG mit Applications-Verknüpfung, SHA256 daneben.
set -euo pipefail
setopt null_glob   # ein Bundle aus aqt-Qt hat keine losen .dylib -- leere Globs sind dann kein Fehler
BUILD_DIR=${1:?build-dir}
OUT_DIR=${2:?ausgabe-dir}
MACDEPLOYQT=${3:-/opt/homebrew/opt/qt/bin/macdeployqt}
APP="$BUILD_DIR/Contestprogramm.app"
[[ -d "$APP" ]] || { echo "kein $APP"; exit 1; }
VERSION=$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$APP/Contents/Info.plist")
ARCH=$(uname -m); [[ "$ARCH" == "arm64" ]] && ARCH_LABEL=apple-silicon || ARCH_LABEL=intel

"$MACDEPLOYQT" "$APP" -verbose=0 2>/dev/null || true   # die rpath-Fehler betreffen genau die Teile, die gleich entfernt werden

# Qts deutsche Knopfbeschriftungen ins Bundle (app/AppLanguage.h sucht
# hier). macdeployqt bringt keine Übersetzungen mit; ohne die Datei
# stünde im fertigen Programm "Save"/"Cancel". Homebrew legt sie unter
# share/qt/translations ab, ein aqt-Qt direkt unter translations.
QT_BIN_DIR=${MACDEPLOYQT:h}
for qm in "$QT_BIN_DIR/../share/qt/translations/qtbase_de.qm" "$QT_BIN_DIR/../translations/qtbase_de.qm"; do
    if [[ -f "$qm" ]]; then
        mkdir -p "$APP/Contents/Resources/translations"
        cp "$qm" "$APP/Contents/Resources/translations/"
        break
    fi
done

FW="$APP/Contents/Frameworks"; PL="$APP/Contents/PlugIns"
for f in QtQml QtQmlMeta QtQmlModels QtQmlWorkerScript QtQuick QtOpenGL QtVirtualKeyboard QtVirtualKeyboardQml QtSvg QtPdf; do
    rm -rf "$FW/$f.framework"
done
rm -rf "$PL/platforminputcontexts" "$PL/iconengines/libqsvgicon.dylib"
for p in libqpdf libqwebp libqmng libqjp2 libqtga libqwbmp libqtiff libqmacheif; do rm -f "$PL/imageformats/$p.dylib"; done
rm -rf "$APP/Contents/MacOS/resources/mac" "$APP/Contents/MacOS/resources/branding"

# Nicht mehr referenzierte Bibliotheken entfernen (Fixpunkt über otool -L).
python3 - "$APP" <<'PY'
import subprocess, os, sys
app = sys.argv[1]; fw = os.path.join(app, 'Contents/Frameworks')
def deps(p):
    return [l.strip().split(' (')[0] for l in subprocess.run(['otool','-L',p],capture_output=True,text=True).stdout.splitlines()[1:]]
roots = [os.path.join(app, 'Contents/MacOS/Contestprogramm')]
for r, d, fs in os.walk(os.path.join(app, 'Contents/PlugIns')):
    roots += [os.path.join(r, f) for f in fs if f.endswith('.dylib')]
for d in os.listdir(fw):
    if d.endswith('.framework'):
        roots.append(os.path.join(fw, d, 'Versions/A', d[:-len('.framework')]))
dylibs = {d for d in os.listdir(fw) if d.endswith('.dylib')}
needed, frontier, seen = set(), list(roots), set()
while frontier:
    m = frontier.pop()
    if m in seen or not os.path.exists(m): continue
    seen.add(m)
    for dep in deps(m):
        b = os.path.basename(dep)
        if b in dylibs and b not in needed:
            needed.add(b); frontier.append(os.path.join(fw, b))
for extra in sorted(dylibs - needed):
    os.remove(os.path.join(fw, extra))
print(f"Bibliotheken: {len(needed)} behalten, {len(dylibs - needed)} entfernt")
PY
for lib in "$FW"/*.dylib; do
    install_name_tool -id "@executable_path/../Frameworks/$(basename "$lib")" "$lib" 2>/dev/null || true
done
for fwdir in "$FW"/*.framework; do
    name=$(basename "$fwdir" .framework)
    install_name_tool -id "@executable_path/../Frameworks/$name.framework/Versions/A/$name" "$fwdir/Versions/A/$name" 2>/dev/null || true
done

# Jeden @rpath-Verweis INNERHALB des Bundles auf einen festen Pfad
# umschreiben, und den Bau-rpath entfernen. Gefunden 2026-09-28: das
# ausgelieferte Programm trug als einzigen rpath /opt/homebrew/opt/qt/lib
# -- den Pfad der Bau-Maschine. libbrotlidec sucht ihre Partnerbibliothek
# über @rpath, und die liegt zwar IM Bundle, wurde dort aber nie gesucht.
# Auf dem Rechner des Entwicklers fällt das nicht auf (dort gibt es
# Homebrew), auf dem eines Empfängers schon. Die Schleifen oben setzen
# nur die IDs der Bibliotheken, nicht die Verweise untereinander.
fix_rpath_refs() {
    local bin="$1"
    otool -L "$bin" 2>/dev/null | awk '/@rpath\//{print $1}' | while read -r ref; do
        install_name_tool -change "$ref" "@executable_path/../Frameworks/${ref#@rpath/}" "$bin" 2>/dev/null || true
    done
}
for lib in "$FW"/*.dylib; do
    fix_rpath_refs "$lib"
done
for fwdir in "$FW"/*.framework; do
    name=$(basename "$fwdir" .framework)
    fix_rpath_refs "$fwdir/Versions/A/$name"
done
for plug in $(find "$APP/Contents/PlugIns" -name '*.dylib' 2>/dev/null); do
    fix_rpath_refs "$plug"
done
fix_rpath_refs "$APP/Contents/MacOS/Contestprogramm"

# Und die rpaths selbst: alles, was auf die Bau-Maschine zeigt, raus --
# an seine Stelle der Ordner im Bundle, damit ein @rpath, den ein
# künftiges Qt neu einführt, auch dort landet und nicht im Nichts.
otool -l "$APP/Contents/MacOS/Contestprogramm" \
    | awk '/LC_RPATH/{f=1} f&&/path /{print $2; f=0}' \
    | while read -r rp; do
        [ "$rp" = "@executable_path/../Frameworks" ] && continue
        install_name_tool -delete_rpath "$rp" "$APP/Contents/MacOS/Contestprogramm" 2>/dev/null || true
    done
install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP/Contents/MacOS/Contestprogramm" 2>/dev/null || true

codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"

mkdir -p "$OUT_DIR"
STAGE=$(mktemp -d)
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
DMG="$OUT_DIR/Contestprogramm-$VERSION-macOS-$ARCH_LABEL.dmg"
rm -f "$DMG"
# Mehrere Anläufe: "hdiutil: create failed - Resource busy" ist auf den
# macOS-Läufern der CI ein bekannter Aussetzer -- irgendein anderer
# Prozess (Spotlight, ein voriger diskimages-helper) hält den Ordner
# gerade fest. Erlebt am 2026-09-28, Lauf 36402539443: Bau und alle
# Prüfstände grün, nur das Abbild scheiterte. Ohne Wiederholung fällt
# damit irgendwann ein Release-Lauf aus, an dem sonst nichts falsch ist.
dmg_versuch=1
until hdiutil create -volname "Contestprogramm $VERSION" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null 2>&1; do
    if [ "$dmg_versuch" -ge 3 ]; then
        echo "hdiutil hat dreimal nicht gewollt -- hier die letzte Meldung:" >&2
        hdiutil create -volname "Contestprogramm $VERSION" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >&2
        exit 1
    fi
    echo "hdiutil war belegt, Versuch $dmg_versuch -- in 5 s noch einmal" >&2
    dmg_versuch=$((dmg_versuch + 1))
    sleep 5
done
rm -rf "$STAGE"
(cd "$OUT_DIR" && shasum -a 256 "$(basename "$DMG")" > "$(basename "$DMG").sha256")
echo "fertig: $DMG ($(du -h "$DMG" | cut -f1))"
