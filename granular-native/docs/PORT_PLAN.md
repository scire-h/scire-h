# GRANULAR native port — plan & status

Goal: move `granular.html` (Web Audio) to a native JUCE instrument so it
runs glitch-free on iPhone and loads **inside AUM as an AUv3** — AUM then
records it natively, removing the iOS screen-recording noise problem
entirely.

## Module map (web → native)

| granular.html                        | native module            | status |
|--------------------------------------|--------------------------|--------|
| `generateBuffer()` (8 source types)  | `Source/SourceGen.h`     | ✅ ported + tested |
| grain scheduler / `fireGrain`        | `Source/GrainEngine.h`   | ✅ ported + tested |
| envelopes (Hann/Tri/Exp/Rect)        | `GrainEngine::envelopeValue` | ✅ ported + tested |
| reverse grains (cached reversed buf) | `GrainEngine::setBuffer` | ✅ ported |
| freeze / scan                        | `GrainEngine`            | ✅ ported + tested |
| step sequencer (Hit/Slice, triplets) | `Source/StepSequencer.h` | ✅ ported + tested |
| Evolve (9 mutations, weighted)       | `Source/Evolver.h`       | ✅ ported + tested |
| 5-track mix + master soft-clip       | `PluginProcessor`        | ✅ v0.1 (free-run mode) |
| host tempo sync                      | `PluginProcessor` (playhead) | ✅ v0.1 |
| pattern grid UI                      | —                        | ⬜ phase 2 |
| per-track FX (LPF/HPF/Delay/PingPong/Reverb/BitCrush/Dist/Comp/Gate/Binaural/Flanger/Phaser) | — | ⬜ phase 2 (juce::dsp) |
| XY performance pad                   | —                        | ⬜ phase 3 |
| 1-bit Macintosh look                 | custom LookAndFeel       | ⬜ phase 3 |
| file import (drag audio)             | —                        | ⬜ phase 3 |
| memories / project save              | APVTS state (basic done) | 🔶 presets phase 3 |
| multitrack WAV export                | AUM records the AUv3     | ✅ by design |

## v0.1 behaviour

- 5 tracks, each free-running its granulator (the web app's "drone" mode).
  T1 unmuted by default; unmute the rest with their `mute` params.
- Sequencer + Evolver compile and pass tests but are not yet driven by the
  processor — pattern UI and step-driven triggering are phase 2.
- Generic parameter editor with a 1-bit header strip.

## Build

### macOS (Standalone + AU + VST3)

```bash
cd granular-native
cmake -B build -G Xcode
cmake --build build --config Release
```

### iOS (Standalone + AUv3 for AUM)

Requires Xcode with an iOS signing team.

```bash
cd granular-native
cmake -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
open build-ios/GRANULAR.xcodeproj
```

In Xcode: select your team on both the Standalone and AUv3 targets, plug in
the iPhone, build & run the **Standalone** target once (this installs the
AUv3 extension it embeds). Then in AUM: Sources → Audio Unit Extension →
GRANULAR.

Free Apple ID = 7-day install; Apple Developer Program = TestFlight / long-
lived installs.

### DSP tests (no JUCE, any platform)

```bash
cd granular-native/tests
./run_all.sh
```
