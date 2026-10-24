#!/usr/bin/env bash
# Wraps the macOS .pkg installer in a branded disk image:
#   dist/TheEngineRoom-<version>-macOS.dmg
# Opening it shows a DYT-branded window with one "Install The Engine Room" icon.
#
#   ./scripts/build_mac_dmg.sh 1.0.0      (run after build_mac_installer.sh)
#
# Optional signing/notarisation env vars (same as the other scripts):
#   MACOS_DEV_ID, APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD
set -euo pipefail

VERSION="${1:?usage: build_mac_dmg.sh <version>}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG="$ROOT/dist/TheEngineRoom-$VERSION-macOS.pkg"
DMG="$ROOT/dist/TheEngineRoom-$VERSION-macOS.dmg"
STAGE="$ROOT/build/dmg"
VOL="The Engine Room"

[ -f "$PKG" ] || { echo "Missing $PKG. Run build_mac_installer.sh first."; exit 1; }

rm -rf "$STAGE" "$DMG"
mkdir -p "$STAGE"
cp "$PKG" "$STAGE/Install The Engine Room.pkg"
cp "$ROOT/installer/EULA.txt" "$STAGE/Licence Agreement.txt"

# Retina-ready background (1x + 2x combined into one TIFF)
tiffutil -cathidpicheck "$ROOT/installer/mac/dmg-background.png" \
                        "$ROOT/installer/mac/dmg-background@2x.png" \
                        -out "$ROOT/build/dmg-background.tiff"

# Branded window via create-dmg. Finder styling can fail on headless CI runners,
# so fall back to a plain (still working) disk image rather than failing the build.
command -v create-dmg >/dev/null || brew install create-dmg >/dev/null
if ! create-dmg \
      --volname "$VOL" \
      --background "$ROOT/build/dmg-background.tiff" \
      --window-pos 200 120 \
      --window-size 640 422 \
      --icon-size 96 \
      --text-size 13 \
      --icon "Install The Engine Room.pkg" 320 205 \
      --icon "Licence Agreement.txt" 560 350 \
      --hide-extension "Install The Engine Room.pkg" \
      --hide-extension "Licence Agreement.txt" \
      --no-internet-enable \
      "$DMG" "$STAGE"; then
  echo "::warning::Styled DMG failed; building a plain one instead."
  rm -f "$DMG"
  hdiutil create -volname "$VOL" -srcfolder "$STAGE" -ov -format UDZO "$DMG"
fi

if [ -n "${MACOS_DEV_ID:-}" ]; then
  codesign --force --timestamp --sign "$MACOS_DEV_ID" "$DMG"
  if [ -n "${APPLE_ID:-}" ]; then
    xcrun notarytool submit "$DMG" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
          --password "$APPLE_APP_PASSWORD" --wait
    xcrun stapler staple "$DMG"
  fi
fi

hdiutil verify "$DMG"
echo "Built $DMG"
