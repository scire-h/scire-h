# cyann · web — Apple-Silicon-native rebuild of the cyan/n rhythm engine

A single self-contained `index.html` (no build, no dependencies) that
reproduces the **ETR drum machine** at the heart of **cyan/n** — Katsuhiro
Chiba's Max/MSP live-performance groovebox (2003–2007) — and runs **natively
on Apple Silicon** in any modern browser.

## Why this exists

The original `cyann.app` is a **Max/MSP 4.6 standalone** whose executable is a
`ppc` + `i386` (PowerPC + 32-bit Intel) universal binary. **It cannot launch on
an M1/M2/M3 Mac**: PowerPC support ended at macOS Lion, 32-bit Intel at macOS
Catalina, and Rosetta 2 translates only `x86_64`. There is no 64-bit or
`arm64` slice to run.

So "make it work on M1" means rebuilding the instrument. Following the pattern
this repo already uses for the DS-4M, the rebuild is a self-contained Web Audio
app: the browser's audio engine is fully native on Apple Silicon, so it runs
with zero translation and zero install.

The synthesis and effects are transcribed directly from the Max patch graphs
decoded out of `cyann.mxf`. See [`EXTRACTION.md`](EXTRACTION.md) for the
reverse-engineering trail.

## Run it

```bash
open      cyann-web/index.html    # macOS (Apple Silicon or Intel)
xdg-open  cyann-web/index.html    # Linux
start     cyann-web/index.html    # Windows
```

Or just double-click the file.

## What it does

* **8-voice ETR sequencer** — BD, SD, LT, HT, CH, OH, PC and an AC (accent)
  row, 16 steps, exactly the grid `compo_etr` builds.
* **Analog drum voices** rebuilt from `p_etr_kick/tom/hat/perc`: `cycle~`
  carriers with `curve~` pitch/amp envelopes, `pink~`/`noise~` transients,
  `biquad~` colouring.
* **Transport** — play/stop, tempo 60–200 BPM, shuffle/swing, master level.
* **Per-voice** TUNE / DECAY / LEVEL, plus mute & solo.
* **Effects rack** — Drive (`kat.distortion~` soft-clip), tempo-synced Echo
  (`tapin~`/`tapout~`), Schroeder Reverb (8×`comb~` + `allpass~`) and a
  peak Limiter (`kat.peakhold~`), each bypassable.
* **Output** — oscilloscope + `abs~`→`slide~`-style level meter.
* Four starter patterns (four-on-floor, breaks, techno, latin).

### Controls

| action | control |
|--------|---------|
| play / stop | `▶` button or <kbd>space</kbd> |
| place a hit | click a step cell |
| accent a hit | right-click a step cell |
| audition a voice | number keys <kbd>1</kbd>–<kbd>8</kbd> |
| clear pattern | `CLEAR` |

## Scope

This rebuilds the standalone-meaningful core. It deliberately omits the parts
of cyan/n that only make sense against outboard gear or a host: ReWire
slaving, MIDI-clock sync, the file-based loop players, and the serial-key
check. Those are documented in `EXTRACTION.md` but not reimplemented.

## License

MIT (see repository root). *cyan/n* is the work of Katsuhiro Chiba; this is an
independent, unaffiliated homage and compatibility rebuild.
