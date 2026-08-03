# scire-h — analog drum-synth recreations

Digital recreations of 1970s analog drum synthesisers: the
**ULT-SOUND DS-4M** (Toyo Gakki, 1978) and the **Pollard Syndrum**
(1976–78), the instrument that started the family.

```
scire-h/
├── index.html        ← DS-4M single-file HTML / Web Audio clone
├── ds4-native/       ← DS-4M JUCE 8 C++ plug-in (Standalone + AU + VST3)
└── synth-drum-4/     ← Synth Drum 4: Pollard Syndrum 478 channel, exact recreation
```

## 0. Synth Drum 4 (`synth-drum-4/`)

A **digitally exact recreation of one Pollard Syndrum 478 channel**,
schematic-derived. One pad for now; the full channel control set is
reproduced — SENSITIVITY, TONE (sine/tri/square), TUNE, TONE SUSTAIN
(to 20 s), SWEEP switch (UP/OFF/DOWN) + RANGE + SWEEP TIME, VIBRATO
(SQR/TRI/RAMP, 0.5–250 Hz — real audio-rate FM), SNARE (OFF/1/2) +
SNARE SUSTAIN, VOLUME. Velocity drives level, sweep depth and sustain
length, like the hardware trigger conditioner.

It is a single self-contained `synth-drum-4/index.html` (Web Audio, no
dependencies — open it in any browser). Circuit research and
panel-to-DSP mapping live in
[`synth-drum-4/docs/RESEARCH.md`](synth-drum-4/docs/RESEARCH.md).

**▶ Try it live (no install):**
[**raw.githack.com → Synth Drum 4**](https://raw.githack.com/scire-h/scire-h/claude/70s-synth-drum-design-lwul61/synth-drum-4/index.html)
— served straight from the development branch via githack. Tap the pad
(centre = harder hit) or use keys `Z` / `X` / `Space`, then click a
preset. (githack serves this branch URL; once merged, swap the branch
segment for `main`.)

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
