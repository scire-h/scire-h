# scire-h — ULT-SOUND DS-4M clones

Two implementations of the **ULT-SOUND DS-4M** (Toyo Gakki, 1978),
a four-channel analog drum synthesiser in the Pollard Syndrum family.

```
scire-h/
├── index.html        ← single-file HTML / Web Audio clone
└── ds4-native/       ← JUCE 8 C++ plug-in (Standalone + AU + VST3)
```

## 1. HTML / Web Audio clone (`index.html`)

A single self-contained `index.html` (no external dependencies) that
reproduces the DS-4M panel and synthesis in the browser. Drop it onto
any browser to play.

Highlights:

* Faithful panel — cyan / yellow strips, note-ring VCO knob,
  silk-screened pills, blue *ULT-SOUND* drum-pad cans with
  curved-text rims.
* Subtractive synthesis per channel: VCO + Noise → resonant LPF →
  saturating VCA → master.
* Four waveforms (sine / triangle / square / sawtooth), per-hit and
  continuous analog drift, three-position pitch-sweep direction,
  band-passed noise voicings (CYMBAL / SNARE / NOISE).
* Pad clicks, keyboard `1`-`4`, demo pattern, presets, oscilloscope.

Open directly:

```
xdg-open  index.html   # Linux
open      index.html   # macOS
start     index.html   # Windows
```

## 2. JUCE plug-in (`ds4-native/`)

A C++ JUCE 8 project that builds the same instrument as a native
Standalone app, an AudioUnit (Logic Pro / GarageBand / MainStage) and
a VST3. The DSP is a component-level model based on the analog
circuit research in
[`ds4-native/docs/CIRCUIT_RESEARCH.md`](ds4-native/docs/CIRCUIT_RESEARCH.md).

```
ds4-native/
├── CMakeLists.txt     ← JUCE fetched via CMake, no submodule needed
├── Source/
│   ├── PluginProcessor + PluginEditor      ← APVTS, MIDI, side-chain
│   ├── TriangleCoreVCO   ← ICL8038-style triangle core + sine shaper
│   ├── NoiseVoice + TPTSvf  ← TPT state-variable BP + peaking
│   ├── OTAVCA            ← CA3080 / LM13700 tanh VCA
│   ├── LFOSchmitt        ← op-amp relaxation LFO
│   ├── PiezoTrigger      ← peak detector + Schmitt for piezo input
│   ├── ExpEnvelope       ← RC-discharge amp / pitch / filter EGs
│   ├── Presets           ← seven factory patches
│   ├── ChannelStrip + MasterStrip + PanelLookAndFeel  ← UI
│   └── Parameters.h      ← shared parameter ID constants
├── tests/                ← 5 unit-test binaries, 17 assertions, all
│   │                       pass under g++ -O2 -Wall -Wextra
│   ├── run_all.sh        ← shell driver, no JUCE required
│   ├── CMakeLists.txt    ← optional ctest target
│   └── test_*.cpp
└── docs/
    ├── CIRCUIT_RESEARCH.md  ← panel-to-circuit mapping, IC inventory,
    │                          sources
    └── PHASE2_DESIGN.md     ← DSP module breakdown
```

### Build (macOS)

```bash
cd ds4-native
cmake -B build -G Xcode
cmake --build build --config Release
```

`COPY_PLUGIN_AFTER_BUILD` installs:

* AU         → `~/Library/Audio/Plug-Ins/Components/ULT-SOUND DS-4M.component`
* VST3       → `~/Library/Audio/Plug-Ins/VST3/ULT-SOUND DS-4M.vst3`
* Standalone → `build/DS4M_artefacts/Release/Standalone/ULT-SOUND DS-4M.app`

Restart Logic Pro and load **AU Instruments → ULT-SOUND Clone →
ULT-SOUND DS-4M** on an instrument track. MIDI notes `C1..D#1`
trigger Ch1..Ch4. Optionally route a piezo or any percussive audio
into the plug-in's side-chain (L → Ch1, R → Ch2) for trigger-from-
audio behaviour.

### Run the DSP unit tests (no JUCE required)

```bash
cd ds4-native/tests
./run_all.sh
```

## 3. Vinyl noise generator (`vinyl.html`)

A single self-contained `vinyl.html` (no external dependencies, no
sample files) that procedurally generates the noise of an analog
record — the *プチプチ / パチパチ* crackle — and loops it forever for
an ambient listening experience. Drop it onto any browser.

**The model is cyclostationary noise — the noise of a *rotating body*.**
Real record noise is not stationary randomness: it is random texture
carried on a periodic clock, the rotation. The platter turns at a fixed
angular velocity, so every physical imperfection on the disc is read out
*once per revolution* — the same *パチッ* returns every 1.8 s (33⅓ rpm
→ 0.556 Hz). The generator therefore layers two things:

1. **A stationary floor** — the three frequency layers the sound
   decomposes into:
   * **低音部 / rumble** — brown noise → 95 Hz low-pass (bearing/motor/floor).
   * **中音域 / surface + hum + pop** — band-passed pink noise, 50/60 Hz
     mains hum (+2nd harmonic), and stochastic mid-band pops.
   * **高音 / hiss + crackle** — high-passed white noise + dense fine
     *チリチリ* crackle.
2. **Rotation-locked, period-stationary components** — the part that
   makes it a *record*:
   * **周回ノイズ / periodic defects** — a per-disc "fingerprint" of clicks
     pinned to fixed rotational phases, recurring every revolution (with
     per-pass jitter/dropout so it breathes, not ticks like a clock).
   * **偏心ワウ / eccentricity wow** — a 0.556 Hz pitch waver (an
     off-centre hole heard as pitch).
   * **反り / warp** — a 0.556 Hz infrasonic woofer-pump on the low bus.
   * **triboelectric buildup** — spinning slowly charges the disc, so
     crackle density *rises the longer it plays*; **静電気を拭く** (wipe)
     resets it and presses a fresh disc (new fingerprint).

Both the stochastic crackle and the rotation-locked defects are produced
by `AudioWorklet` processors that generate impulses sample-by-sample in
the audio thread (with `ScriptProcessor` fallbacks) — sample-accurate,
jitter-free and endless, without any recorded audio. The **RPM toggle**
(33⅓ / 45) retunes wow, warp and the defect period together; the disc
visual spins at the true period and flashes once per revolution so you
can *see* the 1.8 s pulse. Warmth, a generated convolution reverb, four
wear presets (クリーン / 埃っぽい / 年代物 / 深いアンビエント) and a
high-band spark visual round it out.

### Material model (物性 → ノイズ)

Orthogonal to the wear presets, a **material selector** derives the
timbre from real physical properties rather than arbitrary settings.
Each substrate carries a coefficient of (kinetic) friction μ, an
effective surface grain size, Mohs hardness, density and an internal
loss factor; these map monotonically onto the synthesis parameters
(friction-floor level ∝ μ, click length ∝ grain, brightness ∝ hardness,
ring Q ∝ 1/loss, low-end weight ∝ density, heavy-tail pops ∝
brittleness):

| Material | μ (kinetic) | character |
|---|---|---|
| **ビニール / PVC** | ≈ 0.30 | soft, smooth, damped — the quiet reference |
| **SP盤 / shellac** | ≈ 0.50 | abrasive mineral filler → "frying" surface noise |
| **御影石 / polished granite** | ≈ 0.55 | hard, dense, low-loss → metallic ring |
| **花崗岩 / coarse granite** | ≈ 0.70 | coarse crystals → loud, gritty, ringing |

Friction coefficients are kept physically plausible — the rock values
follow **Byerlee's law** (μ ≈ 0.6–0.85 for most rocks); μ is shown live
in the readout. Picking a material recomputes the surface friction
floor, crackle grain/decay/brightness, a resonant "material ring", the
hiss spectral tilt, low-end weight, and presses a fresh per-disc defect
fingerprint with that material's character.

### Dust & background playback

A **ホコリ / dust** fader controls a sparse population of random
mid-band pops — the irregular *パチ … パチ* of dust on the surface —
by scaling the stochastic pop density, level and big-pop probability
together (static buildup still rides on top of it over time).

For **background playback**, the engine routes its output through a
`MediaStreamAudioDestinationNode` into a hidden `<audio>` element rather
than straight to the context destination, so the browser treats it as
media playback and keeps it alive when the tab is hidden or the screen
locks. The **Media Session API** exposes play/pause on the OS lock
screen / notification, and a `visibilitychange` handler resumes the
context on return. (`.play()` is retried on the next tap if the first
call is blocked by autoplay policy.)

```
xdg-open  vinyl.html   # Linux
open      vinyl.html   # macOS
start     vinyl.html   # Windows
```

## License

MIT. ULT-SOUND, Toyo Gakki and DS-4M are trademarks of their
respective owners; this project is not affiliated with or endorsed
by either. It exists purely as a homage to a beautiful instrument.
