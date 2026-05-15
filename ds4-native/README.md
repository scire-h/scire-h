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
| `TriangleCoreVCO.cpp` | **Phase 2.1**: ICL8038-style triangle-core VCO. One phase state + rising/falling boolean simulate the 8038's timing capacitor + current-source flip-flop. Sine output is a 3rd-order Bhaskara polynomial of the triangle (matches the 8038 sine output's odd-harmonic THD signature to within a couple of dB per harmonic). Square comes free from the comparator state; sawtooth uses a PolyBLEP accumulator at the same control current. Carries the three drift layers (per-hit, slow walk, continuous wobble). |
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

### Multi-output routing (one DAW track per drum)

The plug-in exposes **five output buses**: the main stereo mix and
four optional stereo aux buses (`Ch1 Out`...`Ch4 Out`, each disabled
by default). To route each drum to its own Logic track:

1. In Logic, create the instrument track as **Multi-Output**
   (`+ → Software Instrument → Multi-Output → AU Instruments →
   ULT-SOUND Clone → ULT-SOUND DS-4M`).
2. Each enabled aux bus shows up under the track header. Click the
   small `+` icon underneath the track to add an aux track for each
   channel; choose `Ch1 Out`...`Ch4 Out` from the dropdown.
3. CYMBAL / CYMBAL / SNARE / NOISE now have their own faders, sends,
   inserts and routing destinations in the mixer.
4. The main stereo bus still carries the full mix, so leave it
   armed if you want a pre-mixed stem -- otherwise mute it.

### Side-chain piezo triggering

Route any percussive audio source (a contact mic, a drum-loop track,
a sampled hit) into the plug-in's `Trigger In` side-chain bus. The
internal `PiezoTrigger` (peak detector + Schmitt trigger with
configurable hysteresis and re-trigger hold) converts the audio into
discrete trigger events:

* mono input  -> Ch1 only
* stereo input -> L = Ch1, R = Ch2

Channels 3 and 4 stay MIDI / GUI-only since JUCE only exposes one
side-chain bus per plug-in.

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
* [x] **Phase 2** — Component-level analog model. Detailed design in [`docs/CIRCUIT_RESEARCH.md`](docs/CIRCUIT_RESEARCH.md) (panel-to-circuit mapping + IC inventory) and [`docs/PHASE2_DESIGN.md`](docs/PHASE2_DESIGN.md) (DSP module breakdown). Status:
  * [x] **2.1 `TriangleCoreVCO`** — ICL8038-style triangle-core + 3rd-order Bhaskara sine shaper. Verified: H3 = -38 dB (≈ 1.3 % THD), H2/H4 below -45 dB.
  * [x] **2.2 `OTAVCA`** — CA3080 / LM13700 tanh VCA. Replaces the inline `softSat` in `Voice`. Signal-level-dependent third-harmonic distortion that fades naturally as the envelope decays.
  * [x] **2.3 `NoiseBPState` via `TPTSvf`** — Zavalishin-style zero-delay-feedback state-variable filter as a header-only helper. `NoiseVoice` now uses two TPT-SVFs (main BP + parallel peaking emphasis) with no per-block heap allocations.
  * [x] **2.4 `PiezoTrigger`** — peak-detector + Schmitt-trigger front-end. Plugin now exposes an optional stereo *Trigger In* side-chain bus; L → Ch1, R → Ch2 fire on hard hits.
  * [x] **2.5 `LFOSchmitt`** — op-amp Schmitt + RC integrator relaxation LFO. Same triangle-core state machine as `TriangleCoreVCO`, exponential 0.8-22 Hz FREQUENCY taper.
  * [x] **2.6 MULTI VCO cascade** — corrected: triggering Ch_n with PULL engaged also fires Ch_(n+1) at 80 % velocity, exactly one hop. Implemented in `PluginProcessor::fireChannel`.
  * [x] **DSP unit tests** — five JUCE-independent test binaries under `tests/` (17 assertions total, all passing under g++ -O2). Run `tests/run_all.sh` or use the `tests/CMakeLists.txt` ctest target.
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
