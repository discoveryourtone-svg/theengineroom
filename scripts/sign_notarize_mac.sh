#!/usr/bin/env bash
# Signs and notarises the macOS builds so they open without security warnings.
# Requires an Apple Developer account ($99/year) and the secrets listed in build.yml.
set -euo pipefail

ART="build/EngineRoom_artefacts/Release"
KEYCHAIN=build.keychain

echo "$MACOS_CERT_P12" | base64 --decode > cert.p12
security create-keychain -p ci "$KEYCHAIN"
security default-keychain -s "$KEYCHAIN"
security unlock-keychain -p ci "$KEYCHAIN"
security import cert.p12 -k "$KEYCHAIN" -P "$MACOS_CERT_PASSWORD" -T /usr/bin/codesign
security set-key-partition-list -S apple-tool:,apple: -s -k ci "$KEYCHAIN"
rm cert.p12

for b in "$ART/VST3/The Engine Room.vst3" "$ART/AU/The Engine Room.component" "$ART/Standalone/The Engine Room.app"; do
  codesign --force --deep --options runtime --timestamp --sign "$MACOS_DEV_ID" "$b"
  ditto -c -k --keepParent "$b" notarize.zip
  xcrun notarytool submit notarize.zip --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
        --password "$APPLE_APP_PASSWORD" --wait
  xcrun stapler staple "$b"
  rm notarize.zip
done
