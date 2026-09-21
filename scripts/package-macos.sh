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
BUILD_DIR=${1:?build-dir}
OUT_DIR=${2:?ausgabe-dir}
MACDEPLOYQT=${3:-/opt/homebrew/opt/qt/bin/macdeployqt}
APP="$BUILD_DIR/Contestprogramm.app"
[[ -d "$APP" ]] || { echo "kein $APP"; exit 1; }
VERSION=$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$APP/Contents/Info.plist")
ARCH=$(uname -m); [[ "$ARCH" == "arm64" ]] && ARCH_LABEL=apple-silicon || ARCH_LABEL=intel

"$MACDEPLOYQT" "$APP" -verbose=0 2>/dev/null || true   # die rpath-Fehler betreffen genau die Teile, die gleich entfernt werden

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

codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"

mkdir -p "$OUT_DIR"
STAGE=$(mktemp -d)
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
DMG="$OUT_DIR/Contestprogramm-$VERSION-macOS-$ARCH_LABEL.dmg"
rm -f "$DMG"
hdiutil create -volname "Contestprogramm $VERSION" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null
rm -rf "$STAGE"
(cd "$OUT_DIR" && shasum -a 256 "$(basename "$DMG")" > "$(basename "$DMG").sha256")
echo "fertig: $DMG ($(du -h "$DMG" | cut -f1))"
