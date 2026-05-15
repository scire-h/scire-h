# Changelog

All notable changes to this project. The dates are when each phase
landed on the development branch.

## Phase 2.7 — Multi-output bus

* Added four optional stereo aux output buses (`Ch1 Out`…`Ch4 Out`)
  alongside the main mix bus, so each drum can be routed to its own
  DAW track in Logic Pro / Ableton / Bitwig.
* `processBlock` renders each voice into a small scratch buffer and
  routes it both to its aux bus (if enabled) and to the main mix bus.
* README documents the Multi-Output instrument-track creation flow
  and the side-chain piezo triggering workflow.

## Phase 2.6 — MULTI VCO cascade routing

* Re-implemented the PULL feature per the circuit research notes:
  triggering Ch_n with MULTI VCO engaged fires Ch_(n+1) at 80 %
  velocity, one hop, no infinite chain. Implemented in
  `PluginProcessor::fireChannel`.
* `Voice` is now strictly one VCO per channel (previously kept four
  oscillators for unison, which mismatched the panel semantics).

## Phase 2.5 — `LFOSchmitt`

* Op-amp Schmitt + RC integrator relaxation oscillator. Replaces the
  inline `std::sin(2π·phase)` LFO with the same triangle-core state
  machine as `TriangleCoreVCO`, exposed at 0.8–22 Hz on an
  exponential FREQUENCY slider taper.

## Phase 2.4 — `PiezoTrigger`

* Peak detector (one-pole fast attack, slow release) feeding a Schmitt
  trigger with configurable upper / lower thresholds and a retrigger
  hold window.
* Plug-in now exposes an optional stereo Trigger In side-chain bus;
  L → Ch1, R → Ch2 fire on hard hits without any MIDI.

## Phase 2.3 — `NoiseBPState` via `TPTSvf`

* `TPTSvf.h` adds a zero-delay-feedback state-variable filter
  (Zavalishin-style trapezoidal-rule discretisation), header-only.
* `NoiseVoice` now uses two TPT-SVFs (main BP + parallel peaking
  emphasis BP) with per-character Q values for CYMBAL / SNARE /
  NOISE. No per-block heap allocations, `tan()` only runs once every
  4 samples during a sweep.

## Phase 2.2 — `OTAVCA`

* CA3080 / LM13700-style tanh VCA model. Replaces `softSat(x · 1.4)`
  in `Voice` with `V_out = G_env · tanh(V_in · drive)`. Signal-level-
  dependent third-harmonic distortion that fades naturally as the
  envelope decays — the DS-4's "bloom" on heavy hits.

## Phase 2.1 — `TriangleCoreVCO`

* ICL8038-style triangle-core oscillator: one phase state + rising/
  falling boolean simulate the 8038 timing capacitor + current-source
  flip-flop.
* Sine output is a 3rd-order Bhaskara polynomial of the triangle that
  matches the 8038's odd-harmonic THD signature (H3 ≈ −38 dB ≈ 1.3 %
  THD, H2/H4 below −45 dB) measured by the unit test.
* Three drift layers retained from the previous `DriftedVCO`: per-hit
  ±3-cent detune, bounded slow random walk, continuous 0.15–0.4 Hz
  wobble.
* Sawtooth uses a separate PolyBLEP accumulator since the 8038 has
  no native saw output.

## Phase 1 — JUCE skeleton

* `juce_add_plugin` target builds Standalone, AU and VST3 from one
  `CMakeLists.txt`; JUCE itself is fetched by CMake (no submodule).
* `juce::AudioProcessorValueTreeState` parameter tree with 15
  parameters per channel + 2 global.
* Lock-free editor-to-DSP trigger FIFO (`juce::AbstractFifo`).
* MIDI input: notes C1..D#1 trigger Ch1..Ch4 (any octave wraps via
  modulo).
* Resizable panel UI mirroring the cyan / yellow strips of the DS-4M
  silk-screen, four channel strips + master strip + per-channel pad
  buttons.

## Phase 0 — HTML / Web Audio prototype

* Single-file `index.html` with no external dependencies. Drum-synth
  voice rendered via the Web Audio API, with the same panel layout
  as the JUCE plug-in.
* Reachable from any browser by opening the file directly; the
  initial click unlocks `AudioContext`.

## Reference research

* `ds4-native/docs/CIRCUIT_RESEARCH.md` — panel-to-circuit mapping,
  IC inventory (ICL8038 VCO, OTA-pair VCA, op-amp Schmitt LFO,
  peak-detector + Schmitt trigger conditioning), confidence levels
  per inferred block, and a Sources list of every public reference
  consulted.
* `ds4-native/docs/PHASE2_DESIGN.md` — DSP module breakdown that
  ties each circuit block to a JUCE class, including algorithm
  choices (TPT integrator, PolyBLEP, Bhaskara polynomial sine
  shaper) and unit-test targets.
