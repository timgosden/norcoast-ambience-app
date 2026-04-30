# Norcoast Ambience — Plugin

A JUCE-based port of the standalone Norcoast Ambience synth, building to
**AU**, **AUv3** (TODO), **VST3**, **CLAP**, and **Standalone** from one
C++ codebase.

This lives alongside the standalone web app in this repo as a **fully
parallel experiment**. The web app under `/public/` is unaffected.

## Status: phase 6 — + master HPF/LPF (pre-FX, log fader curves)

All FX have been moved off hard-coded constants and onto host-visible
`AudioParameterFloat`s, exposed both to the host (for automation) and
to a slider grid in the editor. The processor caches atom pointers
(`std::atomic<float>*`) for realtime-safe reads, recomputes IIR
coefficients only when the underlying dB value changes, and uses a
`SmoothedValue` master gain to avoid pops on big fader moves.

New master DSP added in this phase:
  - **Width LFO** (0.3 Hz, depth `widthMod * 0.5`) — pans the master
    image left/right via the same equation as Web Audio's
    StereoPanner.
  - **Saturation** — `tanh(x * amt*3.5) / tanh(amt*3.5)` with a
    `1 - amt*0.3` makeup-gain compensation. Verbatim port of the
    standalone's `buildSatCurve`.
  - **Master volume** — `juce::SmoothedValue` ramped over 50 ms.

GUI is a 4-column slider grid (Layers / FX / EQ / Master) with the
existing on-screen keyboard + Latch + All-Off + MIDI/Audio settings
buttons below.

Both pad layers from `PAD_LAYERS` in the standalone now run in
parallel as separate `juce::Synthesiser`s sharing the same MIDI
input. `PadVoice` is layer-agnostic — driven by a `LayerConfig`
struct (`Source/LayerConfig.h`) so adding more layers later is just
a matter of defining another config.

Foundation (15 oscs/voice, octave −1, `fBase 200`, `fRate 0.1`) and
Pads (30 oscs/voice across two octaves, `fBase 1200`, `fRate 0.042`,
4 timbres: lush + warm + choir + celestial) are sonically
interchangeable with the web app's first two layers.

LFOs run at block rate (one tick per `processBlock`) — at sub-Hz
rates the per-block step is sub-percent so audio stays smooth.

Master FX chain (signal flow): synth → chorus → delay → reverb → EQ.

- **Chorus**: Juno-style stereo. Two modulated delays — dL at
  7 ms ± 2.5 ms @ 0.55 Hz panned −0.65, dR at 9 ms ± 3 ms @ 0.73 Hz
  panned +0.65. Wet mix 0.175 (`chorusMix * 0.5`).
- **Delay**: 60/70 s (≈857 ms, quarter @ 70 BPM), feedback 0.57,
  wet send 0.322, feedback path low-passed at 3 kHz, wet send
  brightened with a +12 dB high-shelf at 1200 Hz. Matches the
  standalone's `delayNode → dLP → delayFB / delayGainN → dToneNode`
  topology.
- **Reverb**: Dattorro plate (`Source/DattorroReverb.h`) — direct
  C++ port of `public/dattorro-reverb-worklet.js`. 12 delay lines,
  pre-delay buffer, 3 one-pole LPFs, two cubic-interpolated
  excursion taps, 14 stereo output taps. Defaults match the
  standalone (`reverbSize 0.92` → decay 0.829, `reverbMod 0.74`
  → excursion 1.558 Hz / 1.11 ms). Wet/dry mix at 0.71/0.29.
- **EQ**: 4-band master, lowshelf 100 Hz / peak 350 Hz Q 0.7 / peak
  2.5 kHz Q 0.9 / highshelf 8 kHz at the standalone's default gains
  (0, −0.6, −2.0, +1.4 dB).

Texture (granular) and shimmer in phase 3f+.

Test: open the Standalone, click a chord on the on-screen
keyboard. Compare against the standalone with Texture muted. Should
sound nearly identical — the FX chain (reverb / delay / chorus /
shimmer) is the remaining gap.

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
