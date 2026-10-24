# The Engine Room — plugin

**Overdrive, distortion and fuzz you can see.** The digital twin of the Discover Your Tone Engine Room pedal.

Formats: **VST3** (Mac + Windows), **AU** (Mac, for Logic/GarageBand), and a **Standalone app** (both).
Mac builds are Universal (Apple Silicon + Intel).

## What's in it

| Control | What it does |
|---|---|
| **Boost gauge** | Live needle showing how hard you're pushing the circuit. VAC = clean headroom, BOOST = breaking up. |
| **OD / DIST / FUZZ** | Three circuits. Overdrive keeps your clean signal mixed in (mid-hump, soft clipping). Distortion is tight with hard diode clipping. Fuzz is lopsided, gated and sputters as notes die. |
| **DRIVE** | How hard you push it. The blue band is the sweet spot for the current mode. |
| **TONE** | Dark to bright. |
| **LEVEL** | 5 = same volume as bypassed. Auto level-matching keeps this true as you turn up DRIVE. |
| **Footswitch + LED** | On/off, with a click-free, latency-matched crossfade. |
| **Hint strip** | Hover over anything and the pedal explains itself in plain English. |
| **Starter tones** | Six ready-made settings (First Crunch, Boost the Amp, Rock Riff, Wall of Sound, Fuzz Fight, Sputter). Also exposed as host presets. |
| **How it works →** | Opens discoveryourtone.com (point it at the lesson page when it exists). |
| **Licence chip** | Bottom-left: `FOUNDER #007 · SAM LEE`, or trial days left. Click to activate. |

DSP: 4x oversampled clipping (alias-free), parameter smoothing, 4 samples of latency (reported to the host), ~1.3% of one CPU core.
Window is resizable from 70% to 200%.

## Get the Mac and Windows installers (no Mac or PC dev setup needed)

1. Create a **private** GitHub repo and upload this folder. The `.gitignore` keeps out build files, your signing key and buyer data.
2. GitHub runs `.github/workflows/build.yml` on a Mac and a Windows machine: build → pluginval stress test → installer.
3. Open the repo's **Actions** tab → latest run → download:
   - `TheEngineRoom-<version>-macOS.dmg` — branded disk image; double-click the installer inside to add VST3, Audio Unit and the app (each optional)
   - `TheEngineRoom-<version>-Windows-Setup.exe` — installs VST3 and the app (each optional)
4. To publish a version: bump `VERSION` in `CMakeLists.txt`, then push a tag like `v1.0.0`. The installers land in a draft GitHub Release.

Both installers are branded, show the licence agreement (`installer/EULA.txt`), and end with "click ACTIVATE" instructions.

**Unsigned builds** work, but Mac shows "unidentified developer" (right-click → Open) and Windows shows SmartScreen. For public sale, add signing (below).

## Licence keys and pre-sale

See **[licence/README.md](licence/README.md)**. In short: create your signing key once in `licence/Licence-Desk.html`, drop in your pre-sale CSV, and send each buyer their key and founder number. Keys work offline; 14-day trial without one.

## Build locally

Needs CMake 3.22+ and Xcode (Mac) or Visual Studio 2022 (Windows). JUCE downloads itself.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Offline test tool (renders audio for every starter tone, checks bypass, measures CPU, snapshots the UI):

```bash
cmake -B build -DER_BUILD_TESTS=ON && cmake --build build --target EngineRoomRender
./build/EngineRoomRender_artefacts/Release/EngineRoomRender renders/ Tests/fixtures/dev_ledger.csv
```

The second argument runs the licence tests. The fixture keys are signed with the development key, so that part only passes until you swap in your own key.

## Before selling it

- **JUCE licence.** JUCE is free under AGPLv3 (you'd have to publish your source) or under a commercial licence. Check juce.com for the current tiers; there's usually a free tier for small revenue.
- **Mac signing + notarisation.** Needs an Apple Developer account. Add these repo secrets and the workflow does the rest automatically: `MACOS_CERT_P12` (Developer ID Application cert, base64), `MACOS_CERT_PASSWORD`, `MACOS_DEV_ID`, `MACOS_INSTALLER_ID` (Developer ID Installer), `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD`.
- **Windows signing.** Optional but avoids SmartScreen warnings. Add `WIN_CERT_PFX` (base64) and `WIN_CERT_PASSWORD`.
- **Your signing key.** Releases are blocked until you replace the development licence key (see licence/README.md).
- **VST logo.** Using the VST3 logo on your website requires agreeing to Steinberg's VST licensing terms.

## Project layout

```
Source/DriveEngine.*      The three circuits (all the sound lives here)
Source/PluginProcessor.*  Parameters, presets, bypass, state
Source/PluginEditor.*     Gauge, mode switch, knobs, footswitch, hint strip
Source/DytLookAndFeel.*   DYT brand system as code (colours, Jost, wave motif)
Source/Licence.*          Key verification, trial, storage
Source/ActivationPanel.*  Activation screen + licence chip
licence/                  Licence Desk (issue keys) + command-line tool
installer/                Mac pkg resources, Windows Inno Setup script, EULA
scripts/                  Mac installer + signing scripts
Resources/                Jost typeface (SIL Open Font License, OFL.txt)
Tests/RenderMain.cpp      Offline render + snapshot tool
```
