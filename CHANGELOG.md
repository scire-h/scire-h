# Changelog

All notable changes to this project. The dates are when each phase
landed on the development branch.

## ZURE — the name, and 1992

* The machine is now called **ZURE** (`zure.html`; the ずれ is the
  instrument). Tests, Makefile (`make zure`, `poly` kept as an alias),
  workflow names, project/recording file names and the MIDI-map
  localStorage key follow (the old key is still read as a fallback).
* Reskin: a 1992 personal computer, not a game console. Paper white +
  ink black, 2x2 checkerboard dither in place of every gradient,
  bitmap-font stack (MS Gothic / Osaka-Mono / monospace), zero border
  radius, hard offset shadows, System-7 striped title bars, buttons
  that press into their own shadow, blinking dithered REC.
* The phase scope now rasterises at 92x92 and upscales through
  `image-rendering: pixelated` — real chunky dots, no glow.
* Header BPM stepper for the selected track: left/right buttons per
  digit (±0.01 / 0.1 / 1 / 10), hold for auto-repeat, letter button
  cycles the selection, live target readout. All through the glide.
* Echo gains a manual mode: a TIME knob (20 ms - 2 s) plus FREE in the
  division row; turning the knob claims the delay from tempo sync.

## POLYTEMPO — FX, fill, memories, performance controls, Platinum

* Per-track FX: send into a shared convolver reverb (generated
  exponential-noise IR) plus a per-track ping-pong L/R echo whose time
  is a note value against that track's own tempo — updated every tick,
  so echoes sweep with glides and catch-ups. Stems record dry.
* FILL button / `F` key: every unmuted drum track plays one bar of
  randomly generated fill (six template families + jitter) from its
  own next bar head, then returns to its pattern automatically.
* Drum tracks: four pattern memories (P1-P4), instant switch, saved in
  the project JSON.
* Step grids paint with press-and-slide (pointer capture; the first
  cell decides on/off, `touch-action: pan-y` keeps page scroll alive).
* Header now carries A-D track on/off toggles next to PLAY (muting
  also chokes sustained voices) and a FILL button.
* Track selection (click a card or Numpad 1-4) + numpad BPM nudge:
  +/- = 1, Shift = 0.1, * and / = 10 BPM — all through the glide.
* Mac OS 8 Platinum reskin: deliberate monochrome, hard bevels,
  striped title bars, hard offset shadows; top panels compacted into
  one row (page is roughly half as tall to the first track).

## POLYTEMPO — voice editing

* Every synth voice is now editable per track (VOICE EDIT panel):
  waveform (AUTO / sine / triangle / square / saw), pitch (cutoff for
  the chord voice), decay and sustain. AUTO keeps the library's stock
  shape, so untouched tracks sound as before.
* Rows became mono voices with choke: a new hit releases the previous
  one through a dedicated choke gain (no envelope cancellation, no
  cancelAndHoldAtTime portability issues), closed and open hat choke
  each other, and one chord hit chokes the previous chord. This is
  what makes SUSTAIN musical in a step sequencer: a sustained voice
  holds until its row speaks again (8 s safety cap, released on STOP).
* Envelopes moved from fixed AD to AD(S): decay approaches
  sus x peak via setTargetAtTime; sus=0 reproduces the old one-shots.
* Chord decay is a knob now instead of silently tracking tempo.
* Edit values survive 808/909 switches, are MIDI-learnable, and are
  saved in the project JSON (older files load with stock defaults).
* test_voices.js rewritten for the new contract: 296 assertions total.

## POLYTEMPO — recorder, BPM glide, Aqua look

* **Recorder**: hi-res capture to WAV (16 / 24 / 32-bit float) or AIFF
  (16 / 24-bit) at 44.1 / 48 / 88.2 / 96 kHz. Captures the master bus
  (post soft-clip, pre monitor volume) and, with PARA on, every unmuted
  track's dry post-gain signal in parallel. Raw Float32 capture via an
  AudioWorklet driven by explicit start/end frame numbers, so all stems
  cover the identical frame range and are sample-aligned. Worklet module
  loads via data: URL with blob: fallback (Chromium rejects blob:
  worklets on file:// pages). Encoders are pure functions, including a
  hand-rolled IEEE 754 80-bit extended writer for the AIFF sample-rate
  field.
* **BPM changes glide**: turning a BPM/TUNE knob, typing a value or
  sending MIDI CC no longer jumps the tempo — it glides from the current
  playing tempo at the console's CATCH RATE / CURVE, re-planning when
  re-aimed mid-flight. CATCH RATE range extended to 120 s. Instant while
  stopped.
* **Non-blocking BPM entry**: the double-click editor is now an inline
  input; the old prompt() froze the main thread and stalled the audio.
* Samples load with BASE BPM = 120 (audio files carry no tempo
  metadata); FIT LOOP derives it from the loop length instead of the
  previous silent auto-guess.
* **Aqua reskin**: Mac OS X Tiger-era look — brushed metal, gel
  buttons, recessed wells, engraved labels, Lucida Grande.
* Tests: 57 new assertions (glide behaviour + WAV/AIFF writers
  re-parsed with independent readers), 233 total.

## POLYTEMPO — four-track polytempo loop machine

* New single-file app `polytempo.html`: four loops, each with its own
  continuously-variable BPM, and a catch-up operation that merges one
  into another.
* Lookahead scheduler on the Web Audio clock. Step length is found by
  integrating the tempo curve with Simpson's rule under fixed-point
  iteration, so steps stay exact while the tempo is moving — measured
  drift is under a microsecond per bar.
* **CATCH RATE** knob sets the merge time (0.25 s – 60 s) and is live:
  turning it mid-catch re-plans the remaining travel from that instant,
  so a merge can be hurried or stretched while it is audibly in flight.
* **BPM SYNC** matches tempo only, leaving the bar heads to drift.
  **PHASE SYNC** additionally walks the bar head into place with a
  raised-cosine BPM swell whose integral equals the measured phase
  error — it overshoots the target audibly and lands exactly on it.
  Residual is re-measured and re-corrected until under 15 ms.
* LINEAR / EXP merge curves; AHEAD / NEAREST / BEHIND phase direction.
* 808 and 909 voice libraries (kick, snare, closed/open hat, chord
  tone), switchable while running. Chord tracks play scale degrees
  I / IV / V / vi over KEY + OCTAVE + FINE.
* Sample tracks run tape-style: `playbackRate = BPM / BASE BPM`, so
  tempo moves pitch. Their TUNE knob writes BPM directly — tuning is
  tempo.
* Master chain: 20 Hz high-pass → limiter → tanh soft clip, 12 dB of
  per-track headroom, quiet default volume, peak meter with hold and
  clip LED.
* Phase scope, Web MIDI Learn (receive), JSON project save/load.
* New `tests/web/` suite: 176 assertions run under plain Node with no
  browser and no npm install. Both suites slice the code they test out
  of `polytempo.html`, so there is no second copy to drift. Wired into
  `make test` and a new `Web tests` GitHub Actions workflow.

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
