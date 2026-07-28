# cyann.app — reverse-engineering notes

`cyann.app` (a.k.a. **cyan/n**, by Katsuhiro Chiba, 2003–2007) is a loop-based
live-performance groovebox built as a **Max/MSP 4.6 standalone**. The uploaded
bundle carries this identity in `Contents/Info.plist`:

```
CFBundleGetInfoString  = "2.1, copyright 2003-2007 katsuhiro chiba"
CFBundleIdentifier     = com.cycling74.com.Max.cyann.appRuntime46
```

## Why it will not run on Apple Silicon

`Contents/MacOS/cyann` is a **Mach-O universal binary containing only `ppc`
and `i386` slices** — PowerPC and 32-bit Intel:

```
$ file cyann.app/Contents/MacOS/cyann
Mach-O universal binary with 2 architectures:
  [ppc:  Mach-O ppc executable]
  [i386: Mach-O i386 executable]
```

Apple Silicon (M1/M2/M3…) can run neither:

* **PowerPC** support ended with Rosetta 1 in macOS 10.7 Lion (2011).
* **32-bit Intel (`i386`)** support ended in macOS 10.15 Catalina (2019).
* **Rosetta 2** on Apple Silicon translates **`x86_64` only** — not `i386`,
  not `ppc`.

There is no `x86_64` slice and no `arm64` slice, so the binary cannot launch on
any M-series Mac under any translation layer. The app is also tied to the
discontinued Max 4.6 runtime and a set of custom externals (below). "Making it
run on M1" therefore means **rebuilding the instrument**, not repackaging the
binary — which is exactly the approach the rest of this repository already
takes for the DS-4M.

## What is inside the collective

The real program lives in `Contents/cyann.mxf`, a **Max collective** (`mx@c`
magic). It embeds 587 entries: the compiled externals (`*.mxo`), the GUI
media (`*.gif` / `*.pct` cyan-themed controls), and ~90 **binary patchers**
in Max's `pmax` v2 format.

`decode_pmax.py` (in the scratchpad during extraction) walks the collective's
`dlst` directory, pulls each embedded file out by `of32`/`sz32`, and decodes
the `pmax` token stream back into readable Max text patches (`max v2; #N
vpatcher …`). All 86 patchers decode cleanly.

### Signal architecture (from the decoded patches)

The global `send`/`receive` names map the feature set:

| subsystem | evidence |
|-----------|----------|
| 4 loop players + 1 recycler | `g_lop1..4_length/_pos`, `g_rcy1_*`, `compo_lop1..4`, `compo_rcy1` |
| **ETR step-sequenced drum machine** | `compo_etr`, `p_etr_kick/tom/hat/perc/full`, `etr_bd/sd/tt/hh/pc_p*` |
| effects rack | `fx_echo*`, `fx_reverb`, `fx_chorus`, `fx_flange`, `fx_limiter`, `fx_gravitor`, `fx_overtone`, `fx_reflector`, `fx_cycring` |
| mixer / master | `compo_main`, `p_mtxin/out`, `compo_xfd` (crossfade), `compo_wid` (width), `compo_lmt` |
| transport / sync | `g_master_tempo`, `g_master_shuffle`, `g_clock`, `g_run`, ReWire (`g_rewire*`), MIDI (`g_cc_in*`, `g_sync_out*`) |
| AIFF recorder | `LiveREC.pat`, `g_rec_status`, `sfrecord~` |
| licensing | `pscd` (serial table), `p_terpan` → `kat.findfolder` reads a key file |

Custom externals that would each need re-authoring for a modern build:
`kat.distortion~`, `kat.peakhold~`, `kat.volumefree`, `kat.led`,
`kat.findfolder`.

### ETR rhythm engine — the part reproduced here

`compo_etr` builds an **8-row × 16-step** matrix
(`matrixctrl 18 27 194 98 … 12 12 16 8`) clocked by `counter 0 0 15`.
The eight rows are labelled **BD SD LT HT CH OH PC AC** (bass drum, snare,
low/high tom, closed/open hat, perc, accent).

The voices (`p_etr_kick`, `p_etr_tom`, `p_etr_hat`, `p_etr_perc`) are analog
drum-synth graphs:

* **BD / LT / HT** — a `cycle~` sine carrier whose pitch is swept downward by a
  `curve~` exponential envelope (`0 , 1 1 -0.8 0 $1 -0.8`), amplitude shaped by
  a second `curve~`; the kick adds a short `pink~` click burst (`*~ 0.005`).
* **CH / OH** — `noise~` through a `biquad~` high-pass plus metallic `cycle~`
  partials, short vs long `curve~` decay.
* **PC** — `noise~` + `cycle~` into a resonant `biquad~` band-pass.

`index.html` transcribes these graphs to Web Audio: `OscillatorNode`
(`cycle~`), `exponentialRampToValueAtTime` (`curve~`), filtered noise buffers
(`noise~`/`pink~`), `BiquadFilterNode` (`biquad~`).

### Master effects reproduced

* **Drive** — `kat.distortion~` soft-clip → `WaveShaperNode` saturator + tone LPF.
* **Echo** — `fx_echo` `tapin~ 2000` / `tapout~` → tempo-synced `DelayNode`
  with damped feedback.
* **Reverb** — `fx_reverb` **8× `comb~` (fb 0.9) + `allpass~`** Schroeder network,
  rebuilt as 8 feedback-comb delays + 2 allpass sections.
* **Limiter** — `fx_limiter` / `kat.peakhold~ 45` peak limiter →
  `DynamicsCompressorNode` (ratio 20, hard knee).
* **Meter** — `p_meter` `abs~ → slide~ 0 4096` envelope follower → fast-attack /
  slow-release output meter.

Everything that is meaningful in a standalone browser context (voices,
sequencer, transport, tempo/shuffle, effects, metering) is reproduced. The
parts that only make sense against external hardware/hosts — ReWire slaving,
MIDI-clock sync, file-based loop players, the serial-key check — are
intentionally omitted.
