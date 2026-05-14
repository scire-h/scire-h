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
* [ ] **Phase 2** — Component-level analog model. Detailed design in [`docs/CIRCUIT_RESEARCH.md`](docs/CIRCUIT_RESEARCH.md) (panel-to-circuit mapping + IC inventory) and [`docs/PHASE2_DESIGN.md`](docs/PHASE2_DESIGN.md) (DSP module breakdown). Headline changes:
  * `TriangleCoreVCO` — ICL8038-style triangle-core with PolyBLAMP-anti-aliased ramps and a piecewise-linear sine shaper that matches the 8038's ~1 % THD signature.
  * `PiezoTrigger` — peak-detector + Schmitt-trigger so the plug-in can be driven by an actual piezo on a side-chain audio input.
  * `OTAVCA` — CA3080 / LM13700-style tanh VCA, replacing the plain `tanh(x · 1.4)` hack.
  * `NoiseBPState` — TPT state-variable band-pass + peaking, no per-block heap allocations.
  * `LFOSchmitt` — relaxation-oscillator LFO matching the panel topology.
  * `MultiVCOPullRouter` — the cascade-trigger feature (PULL switches → fire neighbour channel's pitch envelope without its amp envelope).
* [ ] **Phase 3** — Validation against a real DS-4M / DS-1 (record A/B clips, fit `sineShaper` and `NoiseBPState` Q values to ±2 dB spectral match across 0-10 kHz).
* [ ] **Phase 4** — Skinned panel UI rendered from SVG, matching the silk-screen of the original case down to the screws.

## Licence

This is a homage to a long-discontinued instrument from a defunct
manufacturer. The DSP and code are MIT licensed. ULT-SOUND, Toyo Gakki
and DS-4M are trademarks of their respective owners; this project is
not affiliated with or endorsed by either.
