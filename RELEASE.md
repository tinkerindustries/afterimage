# Releasing

A release is a signed, notarized DMG of the Mac app, attached to a GitHub release on `tinkerindustries/afterimage`. A person runs the steps below by hand. There is no release automation and no CI.

## Versioning

The version is `CFBundleShortVersionString` in `mac/Info.plist.in`. `tools/release.sh` names the DMG after it, so `Afterimage-<version>.dmg` carries whatever that string says. Tags have the form `vX.Y.Z`. Set the plist version to the same `X.Y.Z` as the tag so the file name and the tag agree.


## What gets published

| Artifact | Destination | Produced by |
|---|---|---|
| `Afterimage-<version>.dmg` | GitHub release assets | `tools/release.sh` |

The app is arm64 only, and `CMAKE_OSX_DEPLOYMENT_TARGET` in `CMakeLists.txt` sets the minimum macOS version.

## Before releasing

- [ ] The working tree is clean and `main` is pushed.
- [ ] `ctest --test-dir build` passes (see `TESTING.md`).
- [ ] The app launches and reports "system audio" in its window title.
- [ ] The version in `mac/Info.plist.in` is the version you are releasing.
- [ ] `xcrun notarytool history --keychain-profile afterimage-notary` lists submissions. If it errors, create the profile with `xcrun notarytool store-credentials afterimage-notary` first.

## Cutting a release

```bash
tools/release.sh
gh release create vX.Y.Z dist/Afterimage-<version>.dmg \
  --repo tinkerindustries/afterimage --target main \
  --title "Afterimage X.Y.Z" --notes-file <notes.md>
```

`tools/release.sh` runs these steps in order:

1. Configures and builds in `build/` with Release settings.
2. Signs the app with the Developer ID Application certificate and the hardened runtime.
3. Packs the app and an Applications link into `dist/Afterimage-<version>.dmg` and signs the DMG.
4. Submits the DMG to Apple's notary service and waits for the result.
5. Staples the ticket to the DMG and checks it with `spctl`.

`--no-notarize` stops after signing. Notarization takes a few minutes.

`gh release create` makes the tag `vX.Y.Z` at the target commit. Push any commits first so the tag points at what you built.

Verify with `gh release view vX.Y.Z --repo tinkerindustries/afterimage` and `xcrun stapler validate dist/Afterimage-<version>.dmg`.

### Release notes

Write them by hand. They need to state:

- the requirements: macOS 14.2 or later, Apple Silicon
- that the app ships with no presets, and how to add them (Settings, then the presets folder)
- that macOS asks for permission to record system audio on first launch, and that the app uses its synthesiser until the user allows it

## Changelog

There is no `CHANGELOG.md`. The release notes are the record.

## Permissions and secrets

- The signing identity is the Developer ID Application certificate in the login keychain. `SIGN_IDENTITY` overrides the default identity name in `release.sh`.
- Notarization uses the keychain profile named by `NOTARY_PROFILE` (default `afterimage-notary`). It holds an Apple ID and an app-specific password. Neither is stored in the repository.
- Creating the release needs write access to `tinkerindustries/afterimage` through `gh`.

## Hotfix

> **TODO:** no hotfix path is documented. Releases are cut from `main`, so a fix ships as the next patch version.

## If a release goes wrong

- `gh release delete vX.Y.Z --repo tinkerindustries/afterimage --cleanup-tag` removes the release and its tag. This stops new downloads. It does not recall copies already downloaded.
- A notarized build cannot be recalled by the team. Ship a fixed patch version and say so in its notes.
