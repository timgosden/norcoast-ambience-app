# Norcoast Ambience — Plugin

A JUCE-based port of the standalone Norcoast Ambience synth, building to
**AU**, **AUv3** (TODO), **VST3**, **CLAP**, and **Standalone** from one
C++ codebase.

This lives alongside the standalone web app in this repo as a **fully
parallel experiment**. The web app under `/public/` is unaffected.

## Status: phase 2c — Foundation with full LFO modulation

`PadVoice` now matches the standalone Foundation layer including
modulation:

- Filter LFO 1: `fRate = 0.1 Hz`, depth `fMod = 30 Hz` modulates LP cutoff
- Filter LFO 2: ~0.027–0.040 Hz incommensurate, depth `fMod * 0.45`, random phase
- Amp LFO: `aRate = 0.12 Hz`, depth `aMod = 0.03` (±3% gain)
- Breath LFO: ~0.011–0.017 Hz, depth 0.03 (~80 s breathing period)
- Per-voice detune LFOs: 0.01–0.06 Hz, depth ~0.1–0.94 cents

LFOs run at block rate (one tick per `processBlock`) — at sub-Hz
rates the per-block step is sub-percent so audio stays smooth.

Pads / Texture / FX in phase 3.

Test: open the Standalone, click a low key on the on-screen
keyboard. Compare against the standalone Foundation layer (load the
web app, mute Pads + Texture, raise Foundation). Should now feel
alive — subtle filter movement, gentle amp wobble, slow breathing
in the background.

## Phases

1. **Skeleton (this commit)** — empty plugin, builds to AU + VST3 + CLAP + Standalone.
2. **Foundation pad layer** — first DSP, oscillators + LFOs + envelope. A/B against the web app.
3. **Rest of the synth** — Pads layer, Texture (granular), reverb, delay, chorus, shimmer, EQ, drums, arp, click, sequencer.
4. **Parameter exposure** — every slider as `AudioProcessorParameter`, save/restore, MIDI learn.
5. **MIDI + tempo sync** — host tempo, key changes, MIDI clock.
6. **UI** — either a native JUCE Component or `WebBrowserComponent` reusing the existing HTML GUI talking to the native engine.

## Build

Requires CMake ≥ 3.22 and a C++17 compiler. JUCE 8 and the CLAP
extension are pulled at configure time via `FetchContent` — nothing
to install separately.

### macOS (AU + VST3 + CLAP + Standalone)

```bash
cd plugin
cmake -B build -G Xcode
cmake --build build --config Release
```

Built artefacts land in `plugin/build/NorcoastAmbience_artefacts/Release/`:

| Format     | Path                                       | Install location                                  |
|------------|--------------------------------------------|---------------------------------------------------|
| AU         | `AU/Norcoast Ambience.component`           | `~/Library/Audio/Plug-Ins/Components/`            |
| VST3       | `VST3/Norcoast Ambience.vst3`              | `~/Library/Audio/Plug-Ins/VST3/`                  |
| CLAP       | `CLAP/Norcoast Ambience.clap`              | `~/Library/Audio/Plug-Ins/CLAP/`                  |
| Standalone | `Standalone/Norcoast Ambience.app`         | anywhere                                          |

After installing the AU, refresh validation:

```bash
auval -a            # list AUs
auval -v aumu NcAm NCAU   # validate ours specifically
```

### Windows (VST3 + CLAP + Standalone)

```cmd
cd plugin
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

VST3 install location: `%CommonProgramFiles%\VST3\`
CLAP install location: `%CommonProgramFiles%\CLAP\`

### Linux (VST3 + CLAP + Standalone)

```bash
cd plugin
cmake -B build
cmake --build build --config Release -j
```

## License notes

JUCE is dual-licensed: GPL or commercial. If you intend to ship a
**closed-source** plugin you need a commercial JUCE license
(<https://juce.com/get-juce>). The CMake setup pulls JUCE via
`FetchContent` at build time; no JUCE source lives in this repo.

CLAP itself (and `clap-juce-extensions`) is MIT-licensed, no
restriction.
