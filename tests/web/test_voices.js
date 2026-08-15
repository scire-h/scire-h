#!/usr/bin/env node
/* ===================================================================
   Unit tests for the POLYTEMPO voice library.

   Runs the voice-construction code out of polytempo.html against a
   stub AudioContext that enforces the parts of the Web Audio contract
   browsers throw on: start-before-stop, one start per source, finite
   parameter values, no exponential ramp to zero, nothing scheduled in
   the past. A voice that would raise in Chrome raises here instead.
   =================================================================== */
'use strict';

const fs = require('fs');
const path = require('path');

// optional argv override so the suite can be pointed at a mutated copy
const HTML = process.argv[2] || path.join(__dirname, '..', '..', 'polytempo.html');

/* ---------------- assertion harness ---------------- */
let pass = 0, fail = 0;
function ok (cond, msg) {
  if (cond) pass++; else { fail++; console.log('  FAIL  ' + msg); }
}
function suite (name, fn) {
  console.log('\n' + name);
  try { fn(); }
  catch (e) { fail++; console.log('  FAIL  suite threw: ' + e.message); }
}

/* ---------------- stub Web Audio ---------------- */
const log = [];                                   // every scheduled event
let trigT = 0;                                    // time the voice was asked for

function chk (what, v) {
  if (typeof v !== 'number' || !isFinite(v)) throw new Error(what + ' is not finite: ' + v);
}
function mkParam (owner, name) {
  const p = {
    _v: 0,
    get value () { return p._v; },
    set value (v) { chk(owner.kind + '.' + name + '.value', v); p._v = v; },
    setValueAtTime (v, t) {
      chk(owner.kind + '.' + name, v); chk('time', t);
      if (t < trigT - 1e-9) throw new Error(owner.kind + '.' + name + ' scheduled in the past');
      log.push({ n: owner, name, op: 'set', v, t });
      p._v = v;
      return p;
    },
    exponentialRampToValueAtTime (v, t) {
      chk(owner.kind + '.' + name, v); chk('time', t);
      if (v === 0) throw new Error(owner.kind + '.' + name + ': exponential ramp to zero');
      if (t < trigT - 1e-9) throw new Error(owner.kind + '.' + name + ' ramp scheduled in the past');
      log.push({ n: owner, name, op: 'ramp', v, t });
      p._v = v;
      return p;
    },
    cancelScheduledValues (t) { chk('time', t); return p; }
  };
  return p;
}
function mkNode (kind, params, isSource) {
  const n = { kind, outs: [], started: null, stopped: null };
  params.forEach(nm => { n[nm] = mkParam(n, nm); });
  n.connect = dst => { n.outs.push(dst); return dst; };
  n.disconnect = () => {};
  if (isSource) {
    n.start = t => {
      chk('start time', t);
      if (n.started !== null) throw new Error(kind + ': start called twice');
      n.started = t;
    };
    n.stop = t => {
      chk('stop time', t);
      if (n.started === null) {
        throw new Error(kind + ": cannot call stop without calling start first");
      }
      if (t < n.started - 1e-9) throw new Error(kind + ': stop before start');
      n.stopped = t;
    };
  }
  return n;
}

const nodes = [];
function track (n) { nodes.push(n); return n; }

const ctx = {
  sampleRate: 48000,
  currentTime: 0,
  createOscillator () { return track(mkNode('osc', ['frequency', 'detune'], true)); },
  createGain () { return track(mkNode('gain', ['gain'])); },
  createBiquadFilter () { return track(mkNode('biquad', ['frequency', 'Q', 'gain'])); },
  createBufferSource () {
    const n = track(mkNode('bufsrc', ['playbackRate', 'detune'], true));
    n.start = (t, off) => {
      if (n.started !== null) throw new Error('bufsrc: start called twice');
      if (off !== undefined && (off < 0 || !isFinite(off))) throw new Error('bad buffer offset');
      n.started = t;
    };
    n.stop = t => {
      if (n.started === null) throw new Error('bufsrc: stop without start');
      n.stopped = t;
    };
    return n;
  }
};
const noiseBuf = { duration: 2, length: 96000 };

/* ---------------- load the voices ---------------- */
function loadVoices () {
  const src = fs.readFileSync(HTML, 'utf8');
  const a = src.indexOf('/*<<<VOICES>>>*/');
  const b = src.indexOf('/*<<<END_VOICES>>>*/');
  if (a < 0 || b < 0) throw new Error('VOICES markers not found in polytempo.html');
  return new Function('ctx', 'noiseBuf', 'Math',
    src.slice(a, b) + '\nreturn Voices;')(ctx, noiseBuf, Math);
}
const Voices = loadVoices();

/* Does `node` reach `out` by following connections? */
function reaches (node, out, seen) {
  seen = seen || new Set();
  if (node === out) return true;
  if (seen.has(node)) return false;
  seen.add(node);
  return node.outs.some(o => reaches(o, out, seen));
}

function render (fn, t) {
  nodes.length = 0; log.length = 0;
  trigT = t;
  const out = mkNode('dest', []);
  fn(out);
  return { out, nodes: nodes.slice(), log: log.slice() };
}

/* =================================================================== */
const LIBS = ['808', '909'];
const T0 = 12.345;                                 // a non-zero start time

suite('every voice builds without violating the Web Audio contract', () => {
  for (const lib of LIBS) {
    const cases = {
      kick:    out => Voices.kick(T0, out, lib, 1),
      snare:   out => Voices.snare(T0, out, lib, 1),
      hatCH:   out => Voices.hat(T0, out, lib, 1, false),
      hatOH:   out => Voices.hat(T0, out, lib, 1, true),
      tone:    out => Voices.tone(T0, out, lib, 220, 1.2)
    };
    for (const [nm, fn] of Object.entries(cases)) {
      let r = null, err = null;
      try { r = render(fn, T0); } catch (e) { err = e; }
      ok(!err, lib + ' ' + nm + ' builds' + (err ? ': ' + err.message : ''));
      if (!r) continue;

      const srcs = r.nodes.filter(n => n.started !== null);
      ok(srcs.length > 0, lib + ' ' + nm + ' creates at least one source');
      ok(srcs.every(n => n.stopped !== null),
         lib + ' ' + nm + ' stops every source it starts (no leaked nodes)');
      ok(srcs.every(n => n.started >= T0 - 1e-9),
         lib + ' ' + nm + ' starts no earlier than the step time');
      ok(srcs.every(n => reaches(n, r.out)),
         lib + ' ' + nm + ' routes every source to the track output');
      ok(r.log.length > 0, lib + ' ' + nm + ' schedules envelope/pitch automation');
    }
  }
});

suite('sources are stopped after their envelopes finish', () => {
  for (const lib of LIBS) {
    for (const [nm, fn] of [['kick', out => Voices.kick(T0, out, lib, 1)],
                            ['snare', out => Voices.snare(T0, out, lib, 1)],
                            ['hatOH', out => Voices.hat(T0, out, lib, 1, true)],
                            ['tone', out => Voices.tone(T0, out, lib, 220, 1.2)]]) {
      const r = render(fn, T0);
      const lastEnv = Math.max(...r.log.filter(e => e.name === 'gain').map(e => e.t));
      const lastStop = Math.max(...r.nodes.filter(n => n.stopped !== null).map(n => n.stopped));
      ok(lastStop >= lastEnv - 1e-9,
         lib + ' ' + nm + ' does not cut its own release short');
    }
  }
});

suite('tuning', () => {
  // KEY/OCT/FINE must move drum pitch, and must do so multiplicatively
  for (const lib of LIBS) {
    const base = render(out => Voices.kick(T0, out, lib, 1), T0);
    const up   = render(out => Voices.kick(T0, out, lib, 2), T0);
    const f0 = base.log.find(e => e.name === 'frequency' && e.op === 'set').v;
    const f1 = up.log.find(e => e.name === 'frequency' && e.op === 'set').v;
    ok(Math.abs(f1 / f0 - 2) < 1e-9, lib + ' kick tuning is a clean octave (x' + (f1 / f0) + ')');

    const b2 = render(out => Voices.hat(T0, out, lib, 1, false), T0);
    const u2 = render(out => Voices.hat(T0, out, lib, 2, false), T0);
    const h0 = b2.log.filter(e => e.name === 'frequency').map(e => e.v);
    const h1 = u2.log.filter(e => e.name === 'frequency').map(e => e.v);
    ok(h0.length === h1.length && h0.every((v, i) => Math.abs(h1[i] / v - 2) < 1e-9),
       lib + ' hat tuning scales every partial by the same ratio');
  }

  // the kick sweeps downward, which is what makes it a kick
  for (const lib of LIBS) {
    const r = render(out => Voices.kick(T0, out, lib, 1), T0);
    const f = r.log.filter(e => e.name === 'frequency');
    ok(f.length >= 2 && f[1].v < f[0].v, lib + ' kick pitch sweeps down');
  }
});

suite('library voicing differences', () => {
  const hp = lib => {
    const r = render(out => Voices.hat(T0, out, lib, 1, false), T0);
    return r.nodes.filter(n => n.kind === 'biquad' && n.type === 'highpass')
                  .map(n => n.frequency.value)[0];
  };
  ok(hp('909') > hp('808'), '909 hats sit brighter than 808 hats');

  const dec = (lib, open) => {
    const r = render(out => Voices.hat(T0, out, lib, 1, open), T0);
    return Math.max(...r.log.filter(e => e.name === 'gain').map(e => e.t)) - T0;
  };
  for (const lib of LIBS) ok(dec(lib, true) > dec(lib, false), lib + ' open hat rings longer than closed');

  const sawCount = lib => {
    const r = render(out => Voices.tone(T0, out, lib, 220, 1), T0);
    return r.nodes.filter(n => n.kind === 'osc' && n.type === 'sawtooth').length;
  };
  ok(sawCount('909') > 0 && sawCount('808') === 0,
     '909 chords use sawtooths, 808 chords stay round');
});

suite('extreme tuning stays finite', () => {
  // +/- 2 octaves plus 100 cents, the widest the panel allows
  for (const lib of LIBS) {
    for (const tune of [0.25, 0.9438, 1, 1.0595, 4]) {
      let err = null;
      try {
        render(out => { Voices.kick(T0, out, lib, tune); Voices.snare(T0, out, lib, tune);
                        Voices.hat(T0, out, lib, tune, true); }, T0);
      } catch (e) { err = e; }
      ok(!err, lib + ' survives tune x' + tune + (err ? ': ' + err.message : ''));
    }
  }
  // chord voices across the whole usable range
  for (const lib of LIBS) {
    for (const f of [27.5, 110, 440, 1760, 4186]) {
      let err = null;
      try { render(out => Voices.tone(T0, out, lib, f, 0.2), T0); } catch (e) { err = e; }
      ok(!err, lib + ' tone at ' + f + ' Hz' + (err ? ': ' + err.message : ''));
    }
  }
});

/* =================================================================== */
console.log('\n' + (fail ? 'FAILED' : 'OK') + ' — ' + pass + ' assertions passed, ' + fail + ' failed');
process.exit(fail ? 1 : 0);
