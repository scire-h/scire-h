# ULT-SOUND DS-4M — Native (JUCE) build

Component-level model of the ULT-SOUND / Toyo Gakki DS-4M analog drum
synthesiser, built as a JUCE 8 plugin. Builds as **Standalone**, **AU**
(for Logic Pro / GarageBand / MainStage) and **VST3**.

## Architecture

```
        VCO1 ─┐
        VCO2 ─┤   (multi-VCO: +7 / -7 / neighbour-channel detune)
        VCO3 ─┼─► sum ─► Ladder LPF (24 dB/oct) ─► tanh VCA ─► stereo out
   NoiseVoice ┘            ▲                              ▲
                           │                              │
                       Filter EG (RC)                  Amp EG (RC)
```

Key DSP modules:

| File              | What it does                                                                   |
| ----------------- | ------------------------------------------------------------------------------ |
| `DriftedVCO.cpp`  | Anti-aliased oscillator (PolyBLEP for sqr/saw, integrated PolyBLEP triangle) with per-hit detune, slow random walk, and continuous wobble. |
| `NoiseVoice.cpp`  | Per-channel band-passed noise with resonant peaking. Switches between CYMBAL / SNARE / NOISE voicings.                                     |
| `ExpEnvelope.cpp` | RC-discharge style envelope generator (true exponential decay, matches an analog op-amp + cap).                                            |
| `Voice.cpp`       | Ties it all together. Reads APVTS parameters per block, handles trigger, sweeps pitch + filter, sums voices into the stereo bus.           |
| `PluginProcessor` | `juce::AudioProcessor` with the full APVTS parameter tree, MIDI input (C1..D#1 → Ch1..Ch4), and an editor-to-DSP lock-free trigger FIFO.   |
| `PluginEditor`    | Resizable panel UI matching the DS-4M silk-screen.                                                                                          |

## Build (macOS)

Requires **CMake 3.22+** and **Xcode** (or just the Command Line Tools).
JUCE itself is fetched automatically by CMake (no submodules required).

```bash
cd ds4-native
cmake -B build -G Xcode
cmake --build build --config Release
```

If you prefer a Makefiles generator instead of Xcode:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

When the build finishes, `COPY_PLUGIN_AFTER_BUILD` will install the
plug-ins for you:

* **AU**         → `~/Library/Audio/Plug-Ins/Components/ULT-SOUND DS-4M.component`
* **VST3**       → `~/Library/Audio/Plug-Ins/VST3/ULT-SOUND DS-4M.vst3`
* **Standalone** → `build/DS4M_artefacts/Release/Standalone/ULT-SOUND DS-4M.app`

### Loading in Logic Pro

1. Quit Logic Pro.
2. Build with `cmake --build build --config Release`.
3. macOS validates the AU on the next Logic launch. The first time it
   may take 10-30 s to scan.
4. In Logic, open the Plug-in Manager (Logic Pro → Settings → Plug-In
   Manager…) and verify **ULT-SOUND DS-4M** under *ULT-SOUND Clone* is
   marked **passed**.
5. Insert an Instrument track and choose **AU Instruments → ULT-SOUND
   Clone → ULT-SOUND DS-4M**. Send MIDI notes C1..D#1 to trigger the
   four channels.

### Code-signing for distribution

By default the build is ad-hoc signed which is fine for personal use.
For redistribution sign + notarise the bundle:

```bash
codesign --deep --force --options runtime --timestamp \
  --sign "Developer ID Application: <YOUR NAME>" \
  "~/Library/Audio/Plug-Ins/Components/ULT-SOUND DS-4M.component"
```

## Development roadmap

* [x] **Phase 1** — JUCE skeleton, working subtractive synth voice, parameter tree, basic UI, AU/VST3/Standalone targets.
* [ ] **Phase 2** — Replace the JUCE Ladder Filter with a hand-rolled zero-delay-feedback Moog ladder using the Newton iteration described in *Stilson & Smith 1996*.
* [ ] **Phase 3** — Component-level VCO: model the Pollard Syndrum's triangle-core oscillator (current-mode integrator + comparator) using TPT integrators, then derive sine via a polynomial waveshaper of the triangle output (the same trick the Curtis CEM3340 chips use).
* [ ] **Phase 4** — Voltage-controlled-amplifier non-linearity (OTA-style); 2nd-order temperature drift on the exponential converter.
* [ ] **Phase 5** — Trigger-input conditioning circuit (peak detector → Schmitt trigger), allowing real piezo input from an audio input bus.
* [ ] **Phase 6** — Skinned panel UI rendered from SVG, matching the silk-screen of the original case down to the screws.

## Licence

This is a homage to a long-discontinued instrument from a defunct
manufacturer. The DSP and code are MIT licensed. ULT-SOUND, Toyo Gakki
and DS-4M are trademarks of their respective owners; this project is
not affiliated with or endorsed by either.
