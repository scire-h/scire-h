# scire-h — drum machines in the browser and in C++

Two implementations of the **ULT-SOUND DS-4M** (Toyo Gakki, 1978),
a four-channel analog drum synthesiser in the Pollard Syndrum family,
plus **ZURE**, a four-track polytempo loop machine.

```
scire-h/
├── index.html        ← single-file HTML / Web Audio DS-4M clone
├── zure.html         ← ZURE, single-file polytempo loop machine
├── tests/web/        ← Node unit tests for the web apps
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

## 2. ZURE — polytempo loop machine (`zure.html`)

Four loops, four independent tempos, and an operation that makes one
loop *catch up* to another. The drift and the convergence are the
instrument: tracks run as a polytempo mass, then slide into agreement.

```
xdg-open zure.html           # or: make zure
```

**Tempo.** Each of the four tracks holds its own continuously-variable
BPM — no integer steps, values are real and displayed to two decimals.
Scheduling is lookahead against the Web Audio clock; the length of each
step is obtained by integrating the tempo curve (Simpson's rule, solved
by fixed-point iteration), so a step stays exact even while the tempo is
moving underneath it.

**Catching up.** Pick a MASTER track, hit CATCH on another, and choose:

| control | effect |
|---|---|
| **CATCH RATE** | how long the merge takes, 0.25 s – 60 min (readout switches to M:SS above a minute). At the far end the tempo creeps at hundredths of a BPM per second — a convergence you notice only after it has happened. Turning the knob *during* a catch re-plans the remaining travel from that instant, so a catch can be hurried or stretched while it is audibly in flight |
| **BPM SYNC** | tempo only. Bar heads stay apart and the two loops keep running out of phase |
| **PHASE SYNC** | tempo *and* bar heads. After the tempos converge, the BPM briefly swells past the target and settles back, walking the bar head into place, then locks |
| **LINEAR / EXP** | constant-rate merge, or fast-then-asymptotic |
| **AHEAD / NEAREST / BEHIND** | which way the phase closes: hurry forward, take the short way, or fall back |

The phase-lock swell is a raised cosine whose integral is exactly the
measured bar-head error, so it starts and ends on the target BPM having
moved the phase by precisely the right amount — the overshoot is
audible, and the landing is exact. Residual error is re-measured and
re-corrected until it is under 15 ms, then snapped.

**Every BPM change glides.** Turning a track's BPM knob, typing a value
(double-click the readout — a non-blocking inline editor, so the audio
never stalls), or sending MIDI CC does not jump the tempo: it glides
from the current playing tempo to the new value at the console's
CATCH RATE / CURVE. Re-aiming mid-glide re-plans from wherever the
tempo is now. While stopped, changes apply instantly.

**Recording.** The RECORDER panel (or the `R` key) captures the master
bus — post soft-clip, pre monitor volume — and, with PARA on, every
unmuted track in parallel. A STEM switch picks what the stems carry:
DRY taps the track post-gain before its FX, WET taps the point where
the track's dry signal, its reverb return and its ping-pong echo have
summed — the full produced track, isolated. Capture is raw Float32 via
an AudioWorklet; every file is cut to the identical frame range, so
all stems are sample-aligned. Output is WAV (16 / 24 /
32-bit float) or AIFF (16 / 24), at the context rate — selectable
44.1 / 48 / 88.2 / 96 kHz (the AudioContext is rebuilt on change, so
rate switching happens while stopped).

**Sound.** Per track: an 808 or 909 drum kit (kick / snare / closed and
open hat), a chord voice playing scale degrees I / IV / V / vi over
KEY + OCTAVE + FINE, or a loaded sample. Libraries can be swapped while
running — the pattern stays, only the voicing changes.

**Voice editing.** Every synth voice is editable in the track's VOICE
EDIT panel: waveform (AUTO = the library's stock shape), ATTACK, pitch
(cutoff for the chord voice), decay and sustain — plus, on drums,
TONE (attack hardness and brightness: ±12 dB shelf around a neutral
0.5), SNAPPY on the snare (wire/noise level; 0 turns it into a tom)
and a per-kit ACCENT that lifts the level while also driving the kick
into tanh saturation and hardening its attack, the way the analog
circuit would. SUSTAIN is gated by the painted run: consecutive ON
cells are one note — it opens at the first cell and releases right
after the last, so a note lasts exactly as long as you clicked in.
Rows are mono with choke (CH/OH mutual); releases go through a
dedicated gain so no envelope automation is ever cancelled. Edits
survive 808↔909 switches and are saved in the project file.

**WANDER.** Each track's header row holds an oscillating tempo drive:
engage it and the BPM bounces between its home value and DEST,
dwelling DWELL seconds at each end and travelling each leg over
TRAVEL seconds — anywhere from 0.25 s to a full hour per leg —
through the same step-exact glide machinery. Manual
BPM moves or a CATCH take the wheel back automatically.

**Pitch.** Synth tracks are clock-driven, so tempo changes do not move
their pitch; KEY / OCTAVE / FINE tune them independently. Sample tracks
are the tape case: `playbackRate = BPM / BASE BPM`, so a catch-up is
heard as a pitch swoop. Their TUNE knob writes the track's BPM
directly — tuning *is* tempo, which is the point of the machine.
Audio files carry no BPM metadata, so a loaded sample's BASE BPM
defaults to 120; FIT LOOP derives it from the loop length, or type it.

**Per-track FX.** Each track has a reverb send and its
own ping-pong L/R echo. The echo time is set as a note value (1/16 -
1/2) against that track's OWN tempo, so during a catch-up the echoes
sweep along with the pitch — or set FREE (turning the TIME knob claims
it automatically) for a manual 20 ms - 2 s time. The reverb is built
per track (four convolvers sharing one impulse response) rather than
as a shared bus, precisely so that a WET stem can carry its own tail
and nobody else's.

**FILL.** One button (or `F`): every unmuted drum track drops into a
one-bar randomly generated fill (six template families plus jitter) at
its own next bar head, then falls back to its pattern by itself — the
fills roll around the polytempo field rather than landing together.

**Patterns and playing surface.** Drum tracks hold four pattern
memories (P1-P4, saved in the project). Steps paint with press-and-
slide, not just clicks. The header carries A-D on/off toggles next to
PLAY, and a selected track (click it, or Numpad 1-4) takes BPM nudges
from the keyboard: `+`/`-` for ±1, Shift for ±0.1, `*`//` for ±10 —
all through the glide. A header stepper does the same with buttons,
digit by digit (±0.01 / 0.1 / 1 / 10, hold to auto-repeat), with a
live readout of the selected track's target BPM.

**Look.** A 1992 personal computer, not a game console: paper white
and ink black, checkerboard dither where a gradient would have been,
a bitmap-font stack (MS Gothic / Osaka-Mono / monospace), zero border
radius, hard offset shadows, buttons that press into their own
shadow. The one glowing thing in the room is the phase scope: a green
phosphor CRT in the Fairlight spirit — 92x92 raster upscaled with
`image-rendering: pixelated`, with persistence trails from decaying
the previous frame instead of clearing it. Credit line: SCIRE,
hayato YAMADA.

**Level safety.** Every track starts with 12 dB of headroom; the master
bus is 20 Hz high-pass → limiter → tanh soft clip, and boots quiet. A
peak meter with hold and a clip LED sit in the header.

**Also.** Phase scope showing all four loop positions at once, Web MIDI
Learn (receive), JSON project save/load, `Space` to run, `1`–`4` to
catch, `0` to realign.

### Run the web unit tests (no browser, no npm install)

```bash
cd tests/web && ./run_all.sh     # or: make test-web
```

333 assertions over the timing math, the catch-up/glide state machine,
the voice library (including the waveform/decay/sustain edit layer and
the choke handles) and the file writers. All three suites read the code
straight out of `zure.html`, so there is no duplicated copy to
fall out of date: `test_tempo.js` simulates the scheduler against a
fake clock and checks that phase lock converges to under 3 ms across
28 tempo/offset combinations, `test_voices.js` builds every voice
against a stub AudioContext that enforces the Web Audio rules browsers
throw on, and `test_recorder.js` re-parses the generated WAV/AIFF
files with independent little-endian/big-endian readers (including the
AIFF 80-bit extended sample-rate field).

## 3. JUCE plug-in (`ds4-native/`)

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
