#!/usr/bin/env bash
# Packages the macOS application the way NyxMap and ISFLab do: Qt deployed into the bundle, every binary and
# bundle signed ad hoc, then a zip made with ditto. Without a valid signature macOS on Apple Silicon reports the
# downloaded app as "damaged"; with the ad-hoc one it only asks for the usual first-launch confirmation.
#
#   scripts/package-macos.sh build/application.json build AtemRemoteOSC-macos-v5.0.7.zip
set -euo pipefail

JSON="$1"; BUILD="$2"; ZIP="$3"
NAME="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["name"])' "$JSON")"
APP="$BUILD/$NAME.app"
test -d "$APP" || { echo "No application at $APP" >&2; exit 1; }

# MACDEPLOYQT_ARGS: for a Homebrew Qt, -libpath="$(brew --prefix)/lib" (the official Qt of the CI needs nothing)
macdeployqt "$APP" ${MACDEPLOYQT_ARGS:-}
mkdir -p "$APP/Contents/Resources"

cat > "$APP/Contents/Resources/README-macOS-First-Launch.txt" <<TXT
$NAME, first launch

This build is signed ad hoc, not with an Apple Developer ID certificate, and it is not notarized.
macOS may ask for a confirmation the first time.

First try: in Finder, Control-click the application, choose Open, then confirm Open.
If macOS still says the app is damaged, and only if you trust this download, open Terminal and run:
  xattr -dr com.apple.quarantine "/path/to/$NAME.app"
then open it again. This removes the download quarantine flag; it does not repair a corrupted application.
TXT

# Every Mach-O file first, then the bundles from the inside out, then the application
while IFS= read -r -d '' file; do
    file "$file" | grep -q Mach-O || continue
    codesign --force --sign - "$file"
done < <(find "$APP/Contents" -type f -print0)
while IFS= read -r -d '' bundle; do
    codesign --force --sign - "$bundle"
done < <(find "$APP/Contents" -depth -type d \( -name '*.framework' -o -name '*.bundle' -o -name '*.app' \) -print0)
codesign --force --sign - "$APP"
codesign --verify --deep --strict "$APP"

rm -f "$ZIP"
ditto -c -k --keepParent "$APP" "$ZIP"
echo "$ZIP"
