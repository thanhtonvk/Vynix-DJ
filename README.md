<p align="center">
  <img src="res/images/icons/logo.jpeg" width="320" alt="Vynix DJ">
</p>

<h1 align="center">Vynix DJ</h1>

<p align="center">
  Turn your DJ controller into an <b>all-in-one DJ system</b>.<br>
  Open-source DJ software for Android, Windows, Linux and macOS, built for touch screens
  and USB DJ controllers. Based on <a href="https://mixxx.org">Mixxx</a>.
</p>

<p align="center">
  <b>English</b> · <a href="README.vi.md">Tiếng Việt</a>
</p>

<p align="center">
  <img src="res/skins/VynixDJ/skin_preview.png" width="720" alt="Vynix DJ interface">
</p>

---

## What is Vynix DJ?

Standalone all-in-one units like the Pioneer XDJ series or Denon DJ Prime put the
decks, the mixer and a screen in one box, so you can play without a laptop. Vynix DJ
lets you build the same thing from a regular USB DJ controller: add a phone, a tablet,
a single-board computer with a touch screen or a small PC, and the controller becomes a
complete DJ system.

- **Made for modding.** The interface is designed for a screen mounted on or next to the
  controller: large touch targets, full-screen layout and controller-first browsing, so
  a whole set can be played from the controller alone.
- **Runs on what you have.** The same software and interface on Android, Windows, Linux
  (x64 and ARM64) and macOS. Pick the hardware that fits your build.
- **Plug and play controllers.** More than 140 controller mappings ship with Mixxx, and
  a mapping is picked automatically when a controller is plugged in.
- **Your existing library.** Play from local folders or from Rekordbox USB drives with
  their playlists, cues, loops and beatgrids.

> **Two decks.** Vynix DJ currently supports **2 decks**. Controllers with four decks
> work, but only decks 1 and 2 are shown and mixed in the interface.

## Build ideas

| Build | Platform | Notes |
|---|---|---|
| Controller + phone or tablet | Android | USB OTG cable or hub to the controller. The most compact build. |
| Controller + ARM64 board with a touch screen (e.g. Raspberry Pi) | Linux ARM64 | A fixed "XDJ style" unit, the board can be mounted in a case. |
| Controller + mini PC or old laptop | Windows / Linux x64 | The most processing power for large libraries. |
| Controller + Mac | macOS | Apple Silicon Macs. |

## Features

**Decks**
- Two decks with colored waveforms (RGB / Filtered / HSV), beatgrids, cues, hot cues
  and loops.
- Two-finger pinch zoom on the waveforms.
- Touch interface that scales to phone, tablet and desktop screens, truly full screen,
  including the camera cutout (notch) area on phones.

**Mixer** (MIXER tab)
- GAIN, tempo fader with pitch % and 0 % reset, channel faders, LOW / MID / HI EQ
  and FX faders, master VU meter, CUE / PLAY / SYNC and crossfader for both decks.
- **Sound Color FX** like on Pioneer XDJ mixers: FILTER, SPACE, DUB ECHO, SWEEP, NOISE
  and CRUSH choose the effect of the FX fader of both decks.
- Tap an EQ or FX fader twice to center it.

**Beat FX** (PLAY tab)
- Hardware-style effect unit: pick an effect from a full-screen picker, turn it on and
  set the BEAT and LEVEL knobs. LEVEL starts at 0 when an effect is chosen.
- Echo, Reverb, Flanger, Phaser, Filter, Trans, Pan, Glitch, Pitch, Bitcrusher and Noise.
- One FX unit (DDJ style) or two (Denon DJ / Numark style).

**Library**
- Music folders on the device or computer, with search and a track info panel: cover
  art, BPM, key, duration, overview waveform and LOAD 1 / LOAD 2 buttons.
- Rekordbox USB drives: playlists, cues, loops and beatgrids from drives exported with
  Rekordbox. Rekordbox and Serato are listed right after Tracks.

**Controllers**
- USB MIDI controllers, including through the Android MIDI API on phones and tablets.
- USB HID / Bulk controllers (some Traktor, Hercules, Denon, … models).
- Automatic mapping selection from the mappings that ship with Mixxx (Pioneer DDJ,
  Denon MC, Numark, Hercules, Roland, …).
- Import mappings from the community (`.xml`, `.js` or a `.zip` with both) right in
  the app, also on Android where the mapping folder cannot be opened.
- Controller-first browsing: turning the browse knob opens the library, loading a track
  returns to the decks.

**Settings**
- In-app settings pages for touch screens, plus the full Preferences dialog with large
  text and swipe scrolling.

## Requirements

- **Android:** Android 9 (API 28) or later, **arm64-v8a** CPU. **All files access**
  permission to read your music, and a **USB OTG** cable or adapter for the controller.
- **Windows:** Windows 10 or later, 64-bit.
- **Linux:** Debian / Ubuntu based distributions, x86_64 or ARM64.
- **macOS:** macOS 11 or later, Apple Silicon (M1 or later).

## Installation

Download the package for your platform from the
[Releases](https://github.com/thanhtonvk/Vynix-DJ/releases) page:

| Platform | File |
|---|---|
| Android | `VynixDJ-<version>-android-arm64.apk` |
| Windows | `VynixDJ-<version>-windows-x64.zip` |
| Linux (PC) | `VynixDJ-<version>-linux-x64.deb` |
| Linux (ARM64, e.g. Raspberry Pi) | `VynixDJ-<version>-linux-arm64.deb` |
| macOS (Apple Silicon) | `VynixDJ-<version>-macos-arm64.dmg` |

**Android:** open the APK on the device and allow installing apps from unknown sources,
or install it from a computer with `adb install VynixDJ-<version>-android-arm64.apk`.
Launch Vynix DJ and grant file access when asked.

**Windows:** unzip the archive and run `VynixDJ.exe`.

**Linux:** `sudo apt install ./VynixDJ-<version>-linux-x64.deb` (or the ARM64 package),
then start Vynix DJ from the applications menu.

**macOS:** open the DMG and drag **VynixDJ** to **Applications**. The app is not signed
with an Apple Developer ID, so macOS blocks it the first time: right-click the app →
**Open** → **Open**, or run `xattr -dr com.apple.quarantine /Applications/VynixDJ.app`.

## Quick start

**DJ controller**

- Plug in the controller **before** launching Vynix DJ. Controllers with a bundled
  mapping are enabled automatically.
- If you plug one in while the app is running, a notice asks you to restart the app.
- To choose a mapping manually: **SETTINGS → DEVICE → DJ controllers → OPEN**.
- To use a mapping that does not ship with Mixxx, e.g. from the
  [Mixxx forum](https://mixxx.discourse.group/c/controller-mappings/10): download it, then
  **Import...** next to **Load Mapping** and pick the `.zip` or the `.xml` and `.js` files.
  Select the imported mapping and press **Apply**.

**Music**

- On Android, copy music into the `Music` or `Download` folder; both are scanned on
  first launch. On computers, your `Music` folder is used.
- To add another folder: **SETTINGS → DEVICE → Music library → OPEN → Add**.
- Open **BROWSE → TRACKS** (or turn the browse knob), select a track and press
  **LOAD 1** / **LOAD 2**, double-tap it, or use the load buttons of the controller.

**Mixing** (MIXER tab)

- Tempo, volume, LOW / MID / HI and FX faders, GAIN knobs, CUE / PLAY / SYNC and the
  crossfader for both decks.
- The buttons in the middle choose the effect of the FX faders. The fader is off in the
  center; FILTER cuts the highs or lows, the other effects grow to both sides.

**Beat FX** (PLAY tab, right panel)

1. Tap the effect name to open the picker, then choose an effect.
2. Select **Deck 1** / **Deck 2** to apply it to.
3. Press **FX ON**, then turn up **LEVEL** and set **BEAT**.
4. One or two FX units: **SETTINGS → GENERAL → FX units**.

**Audio**

- To choose the sound card, latency or sample rate: **SETTINGS → AUDIO → OPEN**. On
  Android the default output is the device speaker or headphones, via Oboe/AAudio.
- Master, booth and headphone levels and the master EQ are in **SETTINGS → AUDIO**.
- Sample players are in **SETTINGS → SAMPLER**.

## Building from source

### Windows, Linux and macOS

The Windows (x64), Linux (x64, ARM64) and macOS (arm64) packages are built by GitHub
Actions, see [`.github/workflows`](.github/workflows). Pushing a `v*` tag builds all of
them and attaches them to the release. See [CONTRIBUTING.md](CONTRIBUTING.md) for
building the desktop version locally.

### Android

Requirements: Linux x86_64 (or WSL 2 on Windows), CMake ≥ 3.22, Ninja, JDK 17,
Android SDK (platform 35, build-tools 35.0.0) and NDK 27.2.12479018.

```bash
# 1. Install the Android SDK/NDK and download the prebuilt (vcpkg) libraries for arm64-android
source tools/android_buildenv.sh setup

# 2. Create an APK signing key (only once)
keytool -genkey -keystore vynixdj.keystore -alias vynixdj -keyalg RSA \
        -keysize 2048 -validity 10000
export QT_ANDROID_KEYSTORE_PATH=$PWD/vynixdj.keystore
export QT_ANDROID_KEYSTORE_ALIAS=vynixdj
export QT_ANDROID_KEYSTORE_STORE_PASS=<password>
export QT_ANDROID_KEYSTORE_KEY_PASS=<password>

# 3. Configure and build
mkdir -p build && cd build
cmake -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=$MIXXX_VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_SYSTEM_NAME=Android \
      -DVCPKG_TARGET_TRIPLET=arm64-android \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DQT6=ON -DQML=ON -DHID=ON -DBULK=ON \
      -DBUILD_TESTING=OFF -DBUILD_BENCH=OFF \
      -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
      ..
cmake --build . --parallel

# The APK is written to:
# build/android-build/build/outputs/apk/release/android-build-release-signed.apk
```

The `setup` step needs `sudo` to install packages and downloads about 15 GB of
libraries. `-DCMAKE_CXX_SCAN_FOR_MODULES=OFF` is needed with CMake 3.28 and later, the
NDK has no `clang-scan-deps`.

The build is arm64 only. x86_64 Android emulator images running **Android 11 or later**
can still run it through ARM translation, but slower than a real device:

```bash
sdkmanager "system-images;android-34;google_apis;x86_64" "emulator"
avdmanager create avd -n vynix -k "system-images;android-34;google_apis;x86_64" -d pixel_8
emulator -avd vynix -gpu host &
adb install -r build/android-build/build/outputs/apk/release/android-build-release-signed.apk
```

## Differences from Mixxx

| Component | Main changes |
|---|---|
| `res/skins/VynixDJ/` | Touch skin: MIXER tab with Sound Color FX, Beat FX picker, track info panel |
| `src/controllers/controllermanager.cpp` | Auto-enables new controllers and picks a mapping by name |
| `src/controllers/controllermappingimport.*` | Import mapping files and ZIP archives into the user mapping folder |
| `src/controllers/midi/androidmidi*` | Android MIDI backend (`android.media.midi`) |
| `src/library/` | Controller-first browsing, search from the sidebar, Rekordbox / Serato first |
| `src/effects/` | Beat FX knobs, `beat_period` control, BQ EQ as the default deck EQ |
| `src/widget/` | Two-finger pinch zoom on waveforms, double-tap to center faders |
| `src/preferences/dialog/dlgpreferences.cpp` | Large-text Preferences with swipe scrolling |
| `src/soundio/` | Detects the built-in speaker on Android, selects Oboe and a default output |
| `packaging/android/src/com/tondz/vynixdj/` | `MidiBridge` (MIDI), `StorageVolumes` (USB drives), `MainActivity` (full screen) |
| `src/util/androidlibccompat.c` | libc functions missing on Android 9–13 |

It also includes several fixes for Mixxx on Android: a crash on launch on Android 9–13,
empty waveforms, no audio output, Rekordbox USB drives not found, and effects and fonts
not loading from the APK assets.

## Known limitations

- Only **2 decks** are supported.
- Controllers with a built-in sound card (e.g. Denon MC7000) may only play the Master
  output on Android; separate headphone cueing is not supported there yet.
- Controllers plugged in while the app is running are only detected after a restart.

## Contributing

Vynix DJ is an independent project and is not part of the Mixxx development team.
Please report bugs and feature requests in [Issues](https://github.com/thanhtonvk/Vynix-DJ/issues),
**not** on Mixxx channels. Photos and descriptions of your all-in-one builds are welcome
there too.

General fixes that are not tied to the Vynix DJ interface may be contributed back to
Mixxx following the process in [CONTRIBUTING.md](CONTRIBUTING.md).

## Credits

- **[Mixxx](https://mixxx.org)**: audio engine, library, controller mappings and the
  whole foundation. © Mixxx Development Team.

"Mixxx" is a trademark of the Mixxx project. Vynix DJ is an independent derivative and
is not endorsed or sponsored by the Mixxx project.

## License

Vynix DJ is released under the **GNU General Public License, version 2 or later**,
like Mixxx. See [LICENSE](LICENSE) and [COPYING](COPYING). The license and copyright
notices of the VynixDJ skin are in [`res/skins/VynixDJ/LICENSE`](res/skins/VynixDJ/LICENSE).
