#!/usr/bin/env node
/* ===================================================================
   Unit tests for the POLYTEMPO recorder core (WAV / AIFF encoders).

   The encoders are sliced out of zure.html and fed synthetic
   channel data; the results are re-parsed here with independent
   readers (little-endian RIFF walker, big-endian IFF walker, 80-bit
   extended float decoder) so an encoder bug can't hide behind its own
   symmetric decoder.
   =================================================================== */
'use strict';

const fs = require('fs');
const path = require('path');

// optional argv override so the suite can be pointed at a mutated copy
const HTML = process.argv[2] || path.join(__dirname, '..', '..', 'zure.html');

function loadCore () {
  const src = fs.readFileSync(HTML, 'utf8');
  const a = src.indexOf('/*<<<RECORDER_CORE>>>*/');
  const b = src.indexOf('/*<<<END_RECORDER_CORE>>>*/');
  if (a < 0 || b < 0) throw new Error('RECORDER_CORE markers not found in zure.html');
  return new Function(src.slice(a, b) + '\nreturn RecorderCore;')();
}
const R = loadCore();

/* ---------------- assertion harness ---------------- */
let pass = 0, fail = 0;
function ok (cond, msg) {
  if (cond) pass++; else { fail++; console.log('  FAIL  ' + msg); }
}
function near (a, b, eps, msg) {
  if (Math.abs(a - b) <= eps) pass++;
  else { fail++; console.log('  FAIL  ' + msg + '  (' + a + ' vs ' + b + ')'); }
}
function suite (name, fn) {
  console.log('\n' + name);
  try { fn(); }
  catch (e) { fail++; console.log('  FAIL  suite threw: ' + e.message); }
}

/* ---------------- independent parsers ---------------- */
function str (buf, o, n) { return String.fromCharCode(...new Uint8Array(buf, o, n)); }

function parseWav (buf) {
  const dv = new DataView(buf);
  if (str(buf, 0, 4) !== 'RIFF' || str(buf, 8, 4) !== 'WAVE') throw new Error('not a WAV');
  const riffSize = dv.getUint32(4, true);
  let o = 12, fmt = null, fact = null, data = null;
  while (o + 8 <= buf.byteLength) {
    const id = str(buf, o, 4), sz = dv.getUint32(o + 4, true);
    if (id === 'fmt ') {
      fmt = { size: sz,
        format: dv.getUint16(o + 8, true), ch: dv.getUint16(o + 10, true),
        sr: dv.getUint32(o + 12, true), byteRate: dv.getUint32(o + 16, true),
        block: dv.getUint16(o + 20, true), bits: dv.getUint16(o + 22, true) };
    } else if (id === 'fact') fact = { frames: dv.getUint32(o + 8, true) };
    else if (id === 'data') data = { off: o + 8, len: sz };
    o += 8 + sz + (sz & 1);
  }
  const frames = data.len / fmt.block;
  const chans = Array.from({ length: fmt.ch }, () => new Float64Array(frames));
  let p = data.off;
  for (let i = 0; i < frames; i++) for (let c = 0; c < fmt.ch; c++) {
    if (fmt.format === 3) { chans[c][i] = dv.getFloat32(p, true); p += 4; }
    else if (fmt.bits === 24) {
      let v = dv.getUint8(p) | (dv.getUint8(p + 1) << 8) | (dv.getUint8(p + 2) << 16);
      if (v & 0x800000) v -= 0x1000000;
      chans[c][i] = v / 8388607; p += 3;
    } else { chans[c][i] = dv.getInt16(p, true) / 32767; p += 2; }
  }
  return { riffSize, fmt, fact, frames, chans, total: buf.byteLength };
}

function readFloat80 (dv, o) {
  const e = dv.getUint16(o) & 0x7fff;
  const hi = dv.getUint32(o + 2), lo = dv.getUint32(o + 6);
  return (hi * 4294967296 + lo) / Math.pow(2, 63) * Math.pow(2, e - 16383);
}

function parseAiff (buf) {
  const dv = new DataView(buf);
  if (str(buf, 0, 4) !== 'FORM' || str(buf, 8, 4) !== 'AIFF') throw new Error('not an AIFF');
  const formSize = dv.getUint32(4);
  let o = 12, comm = null, ssnd = null;
  while (o + 8 <= buf.byteLength) {
    const id = str(buf, o, 4), sz = dv.getUint32(o + 4);
    if (id === 'COMM') {
      comm = { size: sz, ch: dv.getInt16(o + 8), frames: dv.getUint32(o + 10),
               bits: dv.getInt16(o + 14), sr: readFloat80(dv, o + 16) };
    } else if (id === 'SSND') {
      ssnd = { off: o + 8 + 8 + dv.getUint32(o + 8), len: sz - 8 - dv.getUint32(o + 8) };
    }
    o += 8 + sz + (sz & 1);
  }
  const bytes = comm.bits / 8;
  const chans = Array.from({ length: comm.ch }, () => new Float64Array(comm.frames));
  let p = ssnd.off;
  for (let i = 0; i < comm.frames; i++) for (let c = 0; c < comm.ch; c++) {
    if (comm.bits === 24) {
      let v = (dv.getUint8(p) << 16) | (dv.getUint8(p + 1) << 8) | dv.getUint8(p + 2);
      if (v & 0x800000) v -= 0x1000000;
      chans[c][i] = v / 8388607; p += bytes;
    } else { chans[c][i] = dv.getInt16(p) / 32767; p += bytes; }
  }
  return { formSize, comm, frames: comm.frames, chans, total: buf.byteLength };
}

/* ---------------- fixtures ---------------- */
function ramp (n, lo, hi) {
  const a = new Float32Array(n);
  for (let i = 0; i < n; i++) a[i] = lo + (hi - lo) * i / (n - 1);
  return a;
}
function maxErr (a, b) {
  let m = 0;
  for (let i = 0; i < a.length; i++) m = Math.max(m, Math.abs(a[i] - b[i]));
  return m;
}
const N = 480;
const L = ramp(N, -0.9, 0.9);
const Rc = ramp(N, 0.9, -0.9);

/* =================================================================== */
suite('WAV 16-bit', () => {
  const w = parseWav(R.encodeWav([L, Rc], 48000, 16));
  ok(w.riffSize === w.total - 8, 'RIFF size covers the whole file');
  ok(w.fmt.format === 1 && w.fmt.bits === 16 && w.fmt.ch === 2, 'PCM 16-bit stereo header');
  ok(w.fmt.sr === 48000 && w.fmt.byteRate === 48000 * 4 && w.fmt.block === 4,
     'rate / byteRate / blockAlign agree');
  ok(w.frames === N, 'frame count preserved');
  ok(maxErr(w.chans[0], L) < 1.01 / 32767, 'left channel round-trips within 1 LSB');
  ok(maxErr(w.chans[1], Rc) < 1.01 / 32767, 'right channel round-trips within 1 LSB');
  ok(w.chans[0][0] < 0 && w.chans[1][0] > 0, 'channel order is not swapped');
});

suite('WAV 24-bit', () => {
  const w = parseWav(R.encodeWav([L, Rc], 96000, 24));
  ok(w.fmt.format === 1 && w.fmt.bits === 24 && w.fmt.block === 6, 'PCM 24-bit stereo header');
  ok(w.fmt.sr === 96000, 'hi-res sample rate stored');
  ok(maxErr(w.chans[0], L) < 1.01 / 8388607, 'left round-trips within 1 LSB of 24 bits');
  ok(maxErr(w.chans[1], Rc) < 1.01 / 8388607, 'right round-trips within 1 LSB of 24 bits');
  const neg = parseWav(R.encodeWav([new Float32Array([-1, -0.5, -1e-7])], 48000, 24));
  ok(neg.chans[0][0] <= -0.9999999, 'full-scale negative survives (two\'s complement)');
});

suite('WAV 32-bit float', () => {
  const hot = new Float32Array([0.25, -0.75, 1.5, -1.5, 1e-20]);   // incl. over-unity
  const w = parseWav(R.encodeWav([hot], 88200, 32));
  ok(w.fmt.format === 3 && w.fmt.bits === 32, 'IEEE-float format tag');
  ok(w.fmt.size === 18, 'float fmt chunk carries cbSize');
  ok(w.fact && w.fact.frames === hot.length, 'fact chunk holds the frame count');
  ok(w.riffSize === w.total - 8, 'RIFF size still consistent with fact chunk present');
  let exact = true;
  for (let i = 0; i < hot.length; i++) if (w.chans[0][i] !== hot[i]) exact = false;
  ok(exact, 'floats are bit-exact, over-unity values NOT clamped');
});

suite('integer clipping', () => {
  const hot = new Float32Array([1.5, -1.5]);
  const w16 = parseWav(R.encodeWav([hot], 48000, 16));
  near(w16.chans[0][0], 1, 1e-9, '+1.5 clamps to full scale in 16-bit');
  near(w16.chans[0][1], -1, 1e-9, '-1.5 clamps to full scale in 16-bit');
  ok(R.quantize(2, 32767) === 32767 && R.quantize(-2, 32767) === -32767,
     'quantize is symmetric at the rails');
});

suite('AIFF 16 / 24-bit', () => {
  for (const bits of [16, 24]) {
    const a = parseAiff(R.encodeAiff([L, Rc], 44100, bits));
    ok(a.formSize === a.total - 8, bits + ': FORM size covers the file');
    ok(a.comm.size === 18 && a.comm.ch === 2 && a.comm.bits === bits, bits + ': COMM header');
    ok(a.comm.frames === N, bits + ': frame count');
    near(a.comm.sr, 44100, 1e-6, bits + ': 80-bit extended sample rate decodes to 44100');
    const tol = 1.01 / (bits === 16 ? 32767 : 8388607);
    ok(maxErr(a.chans[0], L) < tol && maxErr(a.chans[1], Rc) < tol,
       bits + ': both channels round-trip within 1 LSB');
  }
});

suite('AIFF word alignment', () => {
  // mono 24-bit with odd frame count -> odd SSND payload -> pad byte
  const odd = R.encodeAiff([ramp(333, -0.5, 0.5)], 48000, 24);
  ok(odd.byteLength % 2 === 0, 'file is word-aligned even with an odd data chunk');
  const a = parseAiff(odd);
  ok(a.formSize === a.total - 8 && a.frames === 333, 'sizes account for the pad byte');
});

suite('80-bit extended sample rate', () => {
  const enc = v => {
    const dv = new DataView(new ArrayBuffer(10));
    R.writeFloat80(dv, 0, v);
    return [...new Uint8Array(dv.buffer)].map(x => x.toString(16).padStart(2, '0')).join('');
  };
  ok(enc(44100) === '400eac44000000000000', '44100 matches the canonical byte pattern');
  ok(enc(48000) === '400ebb80000000000000', '48000 matches the canonical byte pattern');
  ok(enc(96000) === '400fbb80000000000000', '96000 matches the canonical byte pattern');
  for (const sr of [8000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000]) {
    const dv = new DataView(new ArrayBuffer(10));
    R.writeFloat80(dv, 0, sr);
    near(readFloat80(dv, 0), sr, 1e-9, sr + ' Hz round-trips through extended precision');
  }
});

suite('channel-count generality', () => {
  const mono = parseWav(R.encodeWav([L], 48000, 16));
  ok(mono.fmt.ch === 1 && mono.fmt.block === 2 && mono.frames === N, 'mono WAV');
  const quad = parseWav(R.encodeWav([L, Rc, L, Rc], 48000, 24));
  ok(quad.fmt.ch === 4 && quad.fmt.block === 12 && quad.frames === N, '4-channel WAV');
  ok(maxErr(quad.chans[2], L) < 1.01 / 8388607, 'inner channels interleave correctly');
});

/* =================================================================== */
console.log('\n' + (fail ? 'FAILED' : 'OK') + ' — ' + pass + ' assertions passed, ' + fail + ' failed');
process.exit(fail ? 1 : 0);
