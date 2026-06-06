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

## 3. Vinyl crackle generator (`vinyl.html`)

A single self-contained `vinyl.html` (no external dependencies, no
sample files) that procedurally generates the surface noise of an
analog record — the *プチプチ / パチパチ* crackle — and loops it
forever for an ambient listening experience. Drop it onto any browser.

The noise is modelled as three frequency layers, the way the sound
actually decomposes:

* **低音部 / rumble** — brown noise → 95 Hz low-pass (turntable /
  bearing/floor vibration).
* **中音域 / surface + hum + pop** — band-passed pink noise, a
  50/60 Hz mains hum (+2nd harmonic), and mid-band *パチッ* pops.
* **高音 / hiss + crackle** — high-passed white noise plus dense fine
  *チリチリ* crackle.

The crackle and pops are produced by an `AudioWorklet` that fires
random impulses every sample in the audio thread (with a
`ScriptProcessor` fallback) — sample-accurate, jitter-free and
endless, without any recorded audio. A slow LFO breathes the crackle
density so it never feels static; wow/flutter, warmth and a generated
convolution reverb add the ambient space. Four presets
(クリーン / 埃っぽい / 年代物 / 深いアンビエント) and a spinning-disc
visual round it out.

```
xdg-open  vinyl.html   # Linux
open      vinyl.html   # macOS
start     vinyl.html   # Windows
```

## License

MIT. ULT-SOUND, Toyo Gakki and DS-4M are trademarks of their
respective owners; this project is not affiliated with or endorsed
by either. It exists purely as a homage to a beautiful instrument.
