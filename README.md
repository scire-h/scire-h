# scire-h — ULT-SOUND DS-4M clones

Two implementations of the **ULT-SOUND DS-4M** (Toyo Gakki, 1978),
a four-channel analog drum synthesiser in the Pollard Syndrum family,
plus a quiet companion instrument.

```
scire-h/
├── index.html          ← single-file HTML / Web Audio clone
├── mushishigure.html   ← 虫時雨 — autumn-night insect field, ambient sound gadget
└── ds4-native/         ← JUCE 8 C++ plug-in (Standalone + AU + VST3)
```

## 0. 虫時雨 mushi-shigure (`mushishigure.html`)

A single self-contained HTML page that simulates the insect chorus of
a quiet Japanese autumn night — as an instrument, not a sample player.

* Every voice is a **sine carrier with wing-stroke amplitude
  modulation** (real cricket stridulation is nearly pure-tone), so it
  rings like 鈴虫, not like static.
* Pitch is deliberately alive: per-individual Ornstein–Uhlenbeck
  drift, an onset glide inside every ring, and a slow sub-Hz waver.
* Individuals are persistent characters — own detune, own pace, own
  spot in the garden, occasional long rests, call-and-response — so
  the field never repeats itself.
* 気温 (temperature) follows Dolbear's law: cold nights sing slower
  and slightly flatter. 間 (ma) makes the whole field fall silent now
  and then; a strong gust of 風 can do the same.
* Per species, timing is either 野 (free, natural intervals) or 拍 —
  a TR-808-style 16-step grid with swing (うねり) and per-hit timing
  looseness (ゆらぎ).
* Touch the moon to begin the night. Settings persist in the browser.

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

## License

MIT. ULT-SOUND, Toyo Gakki and DS-4M are trademarks of their
respective owners; this project is not affiliated with or endorsed
by either. It exists purely as a homage to a beautiful instrument.
