# Synth Drum 4 — Pollard Syndrum circuit research

Goal: a **digitally exact recreation of one channel of the Pollard
Syndrum** (Quad 477 / 478 generation). One pad for now; every tuning
parameter, every function and the oscillator voice itself are
reproduced. The project name is **Synth Drum 4** (SD-4).

## 1. Which hardware are we recreating?

| Model | Year | Notes |
| ----- | ---- | ----- |
| Syndrum One (177) | 1976 | first unit, single channel |
| Syndrum Quad 477  | 1977 | the famous one; 4 channels, SNARE type switch |
| Syndrum Quad 478  | 1978 | reworked circuit (RDSI era); adds SNARE SUSTAIN fader |
| Twin 278 / 178    | 1978+ | same 478-generation boards, fewer channels |

We model the **478-generation channel** because (a) the public
schematic scan covers 178/278/478, and (b) it is the superset of the
477 panel (SNARE SUSTAIN added). A single SD-4 channel therefore also
covers the Syndrum One use-case.

## 2. Channel signal path (from the public 178/278/478 schematic)

```
PAD (piezo, velocity-sensitive)
  └─ trigger conditioner ── SENSITIVITY pot
       ├─► SWEEP EG  (RC discharge; depth = RANGE × velocity,
       │              return time = SWEEP pot, polarity = SWEEP switch)
       ├─► TONE EG   (RC discharge; time = TONE SUSTAIN × velocity)
       └─► SNARE EG  (RC discharge; time = SNARE SUSTAIN,
                      level/length preset by SNARE switch 1/2)

VCO (function generator; SINE / TRIANGLE / SQUARE via TONE switch)
  pitch = TUNE + SWEEP EG ± + VIBRATO LFO
VIBRATO LFO: SQUARE / TRIANGLE / RAMP, RATE 0.5 Hz – 250 Hz
NOISE source ─ band shaping ─ SNARE EG VCA ──┐
VCO ─────────── TONE EG VCA ─────────────────┤
                                             └─ mix ─ VOLUME ─ out
```

Key facts and where they come from:

* The trigger is conditioned and routed to **separate envelope
  generators for amplitude and pitch sweep** (Clacktronics drumlab
  notes on the 178/278/478 schematic; same fact already used in this
  repo's `ds4-native/docs/CIRCUIT_RESEARCH.md`).
* Velocity is not just volume: **the harder you hit, the farther the
  sweep is driven and the longer the sustain runs** (Cherry Audio's
  Mark-Barton-endorsed recreation documents this trigger behaviour).
* The audio path is built on **RC4136 quad op-amps**; the schematic
  header lists the channel pots: `SENSITIVITY · VOLUME · SWEEP ·
  RANGE` (5 kΩ), plus TUNE and the sustain faders.
* All envelopes are **RC discharge curves** (true exponentials), not
  linear ramps.

## 3. Panel controls reproduced in SD-4

| Control | Hardware behaviour | SD-4 implementation | Confidence |
| ------- | ------------------ | ------------------- | ---------- |
| SENSITIVITY | pad input gain into the trigger conditioner | velocity scaling ahead of all three EGs | **High** (panel + schematic pot list) |
| VOLUME | channel output level | post-mix gain | **High** |
| TUNE | VCO base pitch | 30 – 800 Hz exponential | **High** (panel) |
| TONE switch | SINE / TRIANGLE / SQUARE wave select | three oscillator shapes; sine carries the function-generator H3 colour | **High** (panel, Cherry Audio docs) |
| SWEEP switch | UP / OFF / DOWN. DOWN = pitch jumps *up* on the hit then decays down (the signature); UP = mirror image | start-offset polarity on the sweep EG | **High** (Cherry Audio docs, demos) |
| RANGE | how far the pitch bends, scaled by velocity | 0 – 36 semitones × velocity | **High** (panel + manual description) |
| SWEEP (pot) | pitch return time | RC time constant of the sweep EG, 30 ms – 2 s | **Medium** — the pot exists in the schematic header; its taper/extents are inferred from recordings |
| TONE SUSTAIN | decay length of the tone, up to ~20 s, velocity-scaled | RC discharge τ, 50 ms – 20 s | **High** (manual: "up to 20 seconds") |
| VIBRATO RATE | LFO speed **0.5 Hz – 250 Hz** — runs well into audio rate, giving the Syndrum its FM/ring-mod "UFO" voices | exponential slider over the same range, true audio-rate FM of the VCO | **High** (manual spec) |
| VIBRATO waveform | SQUARE / TRIANGLE / RAMP switch | three LFO shapes | **High** (Cherry Audio docs) |
| VIBRATO DEPTH | modulation amount | 0 – 100 % of VCO pitch | **Medium** — a depth control is present on recreations; slider extents inferred |
| SNARE switch | OFF / 1 (quiet, shorter) / 2 (loud, longer) noise burst mixed with the tone | noise → band shaping → own EG; two preset level/length pairs | **High** (Cherry Audio docs) |
| SNARE SUSTAIN | 478 addition: fader for the noise decay length | scales the snare EG τ | **High** (model history) |

Master section on the Quad console (master VOLUME + SENSITIVITY) is
trivial once more channels exist; with one pad the channel controls
cover it.

### Performance extensions (marked `+` in the UI)

These knobs are **not on the original 478 panel** — their defaults
reproduce the hardware, and they only depart from it when moved:

| Control | Default (= hardware) | What it adds | Basis |
| ------- | -------------------- | ------------ | ----- |
| ATTACK | 1.5 ms (≈ instant) | amp-envelope rise time, 0.2 – 80 ms | the DS-4 / ULT-SOUND branch of the family exposes a long/short attack on its amp envelope; the Pollard hit is otherwise fixed-instant |
| VIB DELAY | 0 s | vibrato fades in over 0 – 1.5 s after the hit | standard synth vibrato-delay; lets the LFO swell in on long sustains |
| DRIVE | 1.25× | pre-gain into the OTA-VCA `tanh`, 0.5 – 6× | exposes the CA3080/LM13700 "bloom" that is otherwise a fixed amount; harder drive = more third-harmonic grit |

## 4. Digital modelling choices (Phase 0, Web Audio)

* **Envelopes** — every amplitude EG uses `setTargetAtTime`, which is
  exactly the RC-discharge exponential of the hardware.
* **Pitch sweep** — modelled the way the circuit actually behaves: the
  sweep EG is an RC-discharge *voltage*, and a 1 V/oct VCO turns that
  into a pitch that decays exponentially **in octaves**, not linearly
  in Hz. We render that curve with `setValueCurveAtTime`
  (`f(t) = f0·2^(±range/12 · e^{-t/τ})`), τ set so the pitch settles
  ≈99 % by the SWEEP TIME value. A naïve `setTargetAtTime` on the
  frequency param would decay linearly in Hz and sound wrong on big
  sweeps.
* **Analog VCO drift** — two layers, matching the `ds4-native`
  `TriangleCoreVCO` model: a per-hit ±3-cent detune on the base pitch,
  and a continuous slow wobble (0.15–0.4 Hz, ≈2.5 cents) summed onto
  `vco.frequency`. Stops the recreation sounding digitally static.
* **Sine shape** — the function-generator sine of the era is not pure;
  we use a `PeriodicWave` with a −38 dB 3rd harmonic (≈1.3 % THD),
  matching the measurement already validated for the ICL8038 model in
  `ds4-native` (`TriangleCoreVCO` unit test).
* **Audio-rate vibrato** — the LFO is a real `OscillatorNode` patched
  into `vco.frequency`, so at 250 Hz it produces genuine FM sidebands
  like the hardware, not a control-rate approximation.
* **Snare noise** — two parallel band-passes (a body band + a higher
  emphasis band for the metallic sizzle) per snare type, mirroring the
  hardware noise voicing rather than a single biquad.
* **Velocity law** — one velocity sample feeds three places, like the
  conditioned trigger pulse: amplitude (`v^1.3`), sweep depth
  (`0.3 + 0.7 v`), sustain time (`0.35 + 0.65 v`).
* **Monophonic retrigger** — a Syndrum channel is one voice; a new hit
  chokes the previous one (5 ms fade) and restarts the envelopes.
* Remaining Phase-0 simplification, to revisit in the native phase:
  `OscillatorNode` square/triangle are band-limited rather than
  component-level VCO waveshaper models. (The earlier single-biquad
  snare and linear-Hz sweep simplifications are now resolved above.)

## 5. Sources

* [Pollard Syndrum 178/278/478 schematic scan — Clacktronics drumlab](http://clacktronics.co.uk/research/drumlab/pollard-syndrum/Pollard_Syndrum_178-278-478_Schematic.pdf) — RC4136 op-amps, pot list (SENSITIVITY / VOLUME / SWEEP / RANGE), trigger → dual-EG routing.
* [Pollard Syndrum Schematic Redraw — ZEN Instruments](http://zeninstruments.blogspot.com/2020/06/pollard-syndrum-schematic-redraw.html) — legible re-draw of the same schematic.
* [Syndrum module — Cherry Audio Store](https://store.cherryaudio.com/modules/syndrum) — Mark-Barton-endorsed recreation; documents SWEEP switch semantics, SNARE 1/2 behaviour, velocity → sweep/sustain coupling, vibrato SQR/TRI/RAMP switch.
* [The Syndrum Saga — Cherry Audio](https://cherryaudio.com/news/the-syndrum-saga) — design history straight from co-inventor Mark Barton.
* [Pollard Syndrum — Wikipedia](https://en.wikipedia.org/wiki/Pollard_Syndrum) — model timeline, invention history.
* [Syndrums Owner's Manual (ISBN 9798346417422)](https://www.amazon.com/Syndrums-Owners-Manual-Pollard/dp/B0DZ5H2QS9) — collected 477/478/178/278/179 manuals + service docs (commercial; spec quotes of "sustain up to 20 s", "vibrato 0.5–250 Hz" surfaced via search).
* `ds4-native/docs/CIRCUIT_RESEARCH.md` in this repo — prior Syndrum-family research (ICL8038 VCO identification, EG topology).

Patent status: the 1976–77 Pollard patents expired in the 1990s; the
circuit is free to recreate. *Syndrum* as a name may still be claimed
as a trademark, which is why this project ships as **Synth Drum 4**.
