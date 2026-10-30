#!/bin/bash
# Builds Afterimage, signs it with the Developer ID certificate, packs it into a DMG, notarizes the
# DMG and staples the ticket to it. The DMG is written to dist/.
#
#   tools/release.sh                 sign, notarize and staple
#   tools/release.sh --no-notarize   sign only
#
# Notarization reads the keychain profile named by NOTARY_PROFILE, created once with
# `xcrun notarytool store-credentials`.
set -euo pipefail

IDENTITY="${SIGN_IDENTITY:-Developer ID Application: Geoffrey Rich (RFZCAME3XT)}"
PROFILE="${NOTARY_PROFILE:-afterimage-notary}"
root="$(cd "$(dirname "$0")/.." && pwd)"
version="$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$root/mac/Info.plist.in")"
app="$root/build/Afterimage.app"
dmg="$root/dist/Afterimage-$version.dmg"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

cmake -S "$root" -B "$root/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$root/build"

codesign --force --options runtime --timestamp --sign "$IDENTITY" "$app"
codesign --verify --strict --verbose=2 "$app"

mkdir -p "$root/dist"
rm -f "$dmg"
cp -R "$app" "$stage/"
ln -s /Applications "$stage/Applications"
hdiutil create -volname "Afterimage $version" -srcfolder "$stage" -ov -format UDZO "$dmg"
codesign --force --timestamp --sign "$IDENTITY" "$dmg"

if [ "${1:-}" != "--no-notarize" ]; then
  xcrun notarytool submit "$dmg" --keychain-profile "$PROFILE" --wait
  xcrun stapler staple "$dmg"
  spctl --assess --type open --context context:primary-signature --verbose=2 "$dmg"
fi
echo "$dmg"
