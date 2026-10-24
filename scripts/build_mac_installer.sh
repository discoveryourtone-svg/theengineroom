#!/usr/bin/env bash
# Builds "TheEngineRoom-<version>-macOS.pkg": a branded installer with three
# optional parts (VST3, Audio Unit, standalone app).
#
#   ./scripts/build_mac_installer.sh 1.0.0
#
# Optional signing/notarisation (set these env vars, e.g. from GitHub secrets):
#   MACOS_INSTALLER_ID   "Developer ID Installer: Your Name (TEAMID)"
#   APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD
set -euo pipefail

VERSION="${1:?usage: build_mac_installer.sh <version>}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ART="$ROOT/build/EngineRoom_artefacts/Release"
WORK="$ROOT/build/installer"
OUT="$ROOT/dist"
PKG="$OUT/TheEngineRoom-$VERSION-macOS.pkg"
ID="com.discoveryourtone.engineroom"

rm -rf "$WORK"
mkdir -p "$WORK/roots" "$WORK/pkgs" "$WORK/scripts-au" "$WORK/res" "$OUT"

stage () {   # stage <part> <bundle> <install dir>
  mkdir -p "$WORK/roots/$1$3"
  ditto "$2" "$WORK/roots/$1$3/$(basename "$2")"
}
stage vst3 "$ART/VST3/The Engine Room.vst3"       "/Library/Audio/Plug-Ins/VST3"
stage au   "$ART/AU/The Engine Room.component"    "/Library/Audio/Plug-Ins/Components"
stage app  "$ART/Standalone/The Engine Room.app"  "/Applications"

# Make Logic/GarageBand notice the new Audio Unit straight away.
cat > "$WORK/scripts-au/postinstall" <<'EOS'
#!/bin/sh
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
exit 0
EOS
chmod +x "$WORK/scripts-au/postinstall"

for part in vst3 au app; do
  plist="$WORK/$part.plist"
  pkgbuild --analyze --root "$WORK/roots/$part" "$plist" >/dev/null
  # Always install to the standard location, even if an old copy was moved.
  i=0
  while /usr/libexec/PlistBuddy -c "Print :$i" "$plist" >/dev/null 2>&1; do
    /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$plist" >/dev/null 2>&1 || true
    /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$plist"
    i=$((i + 1))
  done

  extra=()
  [ "$part" = au ] && extra=(--scripts "$WORK/scripts-au")
  pkgbuild --root "$WORK/roots/$part" \
           --component-plist "$plist" \
           --identifier "$ID.$part" \
           --version "$VERSION" \
           --install-location / \
           ${extra[@]+"${extra[@]}"} \
           "$WORK/pkgs/EngineRoom-$part.pkg"
done

cp "$ROOT/installer/mac/resources/"* "$WORK/res/"
cp "$ROOT/installer/EULA.txt" "$WORK/res/EULA.txt"

cat > "$WORK/distribution.xml" <<EOX
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>The Engine Room</title>
  <organization>com.discoveryourtone</organization>
  <domains enable_localSystem="true" enable_anywhere="false" enable_currentUserHome="false"/>
  <options customize="allow" require-scripts="false" rootVolumeOnly="true" hostArchitectures="arm64,x86_64"/>
  <allowed-os-versions><os-version min="11.0"/></allowed-os-versions>
  <welcome    file="welcome.html"    mime-type="text/html"/>
  <license    file="EULA.txt"        mime-type="text/plain"/>
  <conclusion file="conclusion.html" mime-type="text/html"/>
  <background file="background.png"  mime-type="image/png" alignment="bottomleft" scaling="none"/>

  <choices-outline>
    <line choice="vst3"/>
    <line choice="au"/>
    <line choice="app"/>
  </choices-outline>

  <choice id="vst3" title="VST3 plugin" description="For Ableton Live, Cubase, Reaper, Studio One, Bitwig and most DAWs.">
    <pkg-ref id="$ID.vst3"/>
  </choice>
  <choice id="au" title="Audio Unit" description="For Logic Pro and GarageBand.">
    <pkg-ref id="$ID.au"/>
  </choice>
  <choice id="app" title="The Engine Room app" description="Play straight through your audio interface, no DAW needed.">
    <pkg-ref id="$ID.app"/>
  </choice>

  <pkg-ref id="$ID.vst3" version="$VERSION" onConclusion="none">EngineRoom-vst3.pkg</pkg-ref>
  <pkg-ref id="$ID.au"   version="$VERSION" onConclusion="none">EngineRoom-au.pkg</pkg-ref>
  <pkg-ref id="$ID.app"  version="$VERSION" onConclusion="none">EngineRoom-app.pkg</pkg-ref>
</installer-gui-script>
EOX

sign=()
[ -n "${MACOS_INSTALLER_ID:-}" ] && sign=(--sign "$MACOS_INSTALLER_ID")
productbuild --distribution "$WORK/distribution.xml" \
             --resources "$WORK/res" \
             --package-path "$WORK/pkgs" \
             ${sign[@]+"${sign[@]}"} \
             "$PKG"

if [ -n "${MACOS_INSTALLER_ID:-}" ] && [ -n "${APPLE_ID:-}" ]; then
  xcrun notarytool submit "$PKG" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
        --password "$APPLE_APP_PASSWORD" --wait
  xcrun stapler staple "$PKG"
fi

echo "Built $PKG"
