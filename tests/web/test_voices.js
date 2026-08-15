#!/usr/bin/env node
/* ===================================================================
   Unit tests for the POLYTEMPO voice library.

   Runs the voice-construction code out of zure.html against a
   stub AudioContext that enforces the parts of the Web Audio contract
   browsers throw on: start-before-stop, one start per source, finite
   parameter values, no exponential ramp to zero, nothing scheduled in
   the past. A voice that would raise in Chrome raises here instead.

   Voices take an edit-params object (wave / pitch / decay / sus) and
   return a choke handle; this suite covers the stock 'auto' sound,
   every waveform override, decay scaling, sustain behaviour and the
   choke path.
   =================================================================== */
'use strict';

const fs = require('fs');
const path = require('path');

// optional argv override so the suite can be pointed at a mutated copy
const HTML = process.argv[2] || path.join(__dirname, '..', '..', 'zure.html');

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
    setTargetAtTime (v, t, tc) {
      chk(owner.kind + '.' + name, v); chk('time', t); chk('timeConstant', tc);
      if (tc <= 0) throw new Error(owner.kind + '.' + name + ': non-positive time constant');
      if (t < trigT - 1e-9) throw new Error(owner.kind + '.' + name + ' target scheduled in the past');
      log.push({ n: owner, name, op: 'target', v, t, tc });
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
    const st = n.start;
    n.start = (t, off) => {
      if (off !== undefined && (off < 0 || !isFinite(off))) throw new Error('bad buffer offset');
      st(t);
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
  if (a < 0 || b < 0) throw new Error('VOICES markers not found in zure.html');
  return new Function('ctx', 'noiseBuf', 'Math',
    src.slice(a, b) + '\nreturn { Voices, SUS_CAP };')(ctx, noiseBuf, Math);
}
const { Voices, SUS_CAP } = loadVoices();

/* default edit params — mirror defaultVp() in the app */
function P (over) {
  return Object.assign({ wave: 'auto', pitch: 50, cutoff: 2600, decay: 0.3, sus: 0 }, over);
}

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
  const handle = fn(out);
  return { out, handle, nodes: nodes.slice(), log: log.slice() };
}

/* =================================================================== */
const LIBS = ['808', '909'];
const T0 = 12.345;                                 // a non-zero start time

function cases (lib) {
  return {
    kick:  out => Voices.kick(T0, out, lib, 1, P({ pitch: 50, decay: 0.6 })),
    snare: out => Voices.snare(T0, out, lib, 1, P({ pitch: 180, decay: 0.2 })),
    hatCH: out => Voices.hat(T0, out, lib, 1, P({ pitch: 40, decay: 0.04 })),
    hatOH: out => Voices.hat(T0, out, lib, 1, P({ pitch: 40, decay: 0.36 })),
    chord: out => Voices.chord(T0, out, lib, [220, 277.18, 329.63], P({ decay: 1.2 }))
  };
}

suite('every voice builds without violating the Web Audio contract', () => {
  for (const lib of LIBS) {
    for (const [nm, fn] of Object.entries(cases(lib))) {
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
      ok(r.handle && typeof r.handle.release === 'function',
         lib + ' ' + nm + ' returns a choke handle');
    }
  }
});

suite('sources outlive their envelopes', () => {
  for (const lib of LIBS) {
    for (const [nm, fn] of Object.entries(cases(lib))) {
      const r = render(fn, T0);
      const lastEnv = Math.max(...r.log.filter(e => e.name === 'gain' && e.op !== 'set')
                                       .map(e => e.t));
      const lastStop = Math.max(...r.nodes.filter(n => n.stopped !== null).map(n => n.stopped));
      ok(lastStop >= lastEnv - 1e-9, lib + ' ' + nm + ' does not cut its own release short');
    }
  }
});

suite('waveform edit', () => {
  for (const lib of LIBS) {
    for (const w of ['sine', 'triangle', 'square', 'sawtooth']) {
      const rk = render(out => Voices.kick(T0, out, lib, 1, P({ wave: w, decay: 0.5 })), T0);
      const body = rk.nodes.find(n => n.kind === 'osc');
      ok(body.type === w, lib + ' kick body takes wave=' + w);

      const rh = render(out => Voices.hat(T0, out, lib, 1, P({ pitch: 40, decay: 0.05, wave: w })), T0);
      const partials = rh.nodes.filter(n => n.kind === 'osc');
      ok(partials.length === 6 && partials.every(o => o.type === w),
         lib + ' all 6 hat partials take wave=' + w);

      const rc = render(out => Voices.chord(T0, out, lib, [220], P({ wave: w })), T0);
      ok(rc.nodes.filter(n => n.kind === 'osc').every(o => o.type === w),
         lib + ' chord oscillators take wave=' + w);
    }
  }
  // AUTO keeps the stock character
  const a8 = render(out => Voices.chord(T0, out, '808', [220], P({ wave: 'auto' })), T0);
  const t8 = a8.nodes.filter(n => n.kind === 'osc').map(o => o.type).sort();
  ok(t8.join(',') === 'sine,triangle', "808 chord AUTO stays sine+triangle (" + t8 + ")");
  const a9 = render(out => Voices.chord(T0, out, '909', [220], P({ wave: 'auto' })), T0);
  ok(a9.nodes.filter(n => n.kind === 'osc').every(o => o.type === 'sawtooth'),
     '909 chord AUTO stays sawtooth');
  const k8 = render(out => Voices.kick(T0, out, '808', 1, P({ wave: 'auto' })), T0);
  ok(k8.nodes.find(n => n.kind === 'osc').type === 'sine', 'kick AUTO stays sine');
});

suite('decay edit', () => {
  for (const lib of LIBS) {
    const rShort = render(out => Voices.kick(T0, out, lib, 1, P({ decay: 0.1 })), T0);
    const rLong  = render(out => Voices.kick(T0, out, lib, 1, P({ decay: 2.0 })), T0);
    const end = r => Math.max(...r.log.filter(e => e.name === 'gain' && e.op === 'ramp').map(e => e.t));
    ok(end(rLong) - end(rShort) > 1.5,
       lib + ' kick decay knob stretches the envelope (' +
       (end(rShort) - T0).toFixed(2) + 's vs ' + (end(rLong) - T0).toFixed(2) + 's)');
    ok(end(rShort) - T0 < 0.2, lib + ' short decay ends quickly');
  }
  // CH vs OH is now purely a decay-param difference
  const ch = render(out => Voices.hat(T0, out, '808', 1, P({ pitch: 40, decay: 0.042 })), T0);
  const oh = render(out => Voices.hat(T0, out, '808', 1, P({ pitch: 40, decay: 0.36 })), T0);
  const lastGain = r => Math.max(...r.log.filter(e => e.name === 'gain').map(e => e.t));
  ok(lastGain(oh) > lastGain(ch) + 0.2, 'open-hat params ring longer than closed-hat params');
});

suite('sustain edit', () => {
  for (const lib of LIBS) {
    const r = render(out => Voices.kick(T0, out, lib, 1, P({ decay: 0.3, sus: 0.4 })), T0);
    const tgt = r.log.find(e => e.name === 'gain' && e.op === 'target' && e.v > 0.01);
    ok(!!tgt, lib + ' sus>0 decays toward a held level');
    ok(tgt && Math.abs(tgt.v / (lib === '909' ? 1.0 : 0.95) - 0.4) < 0.01,
       lib + ' held level is sus × peak');
    const body = r.nodes.find(n => n.kind === 'osc');
    ok(body.stopped >= T0 + SUS_CAP - 1e-9, lib + ' sustained voice lives to the safety cap');
    const fade = r.log.find(e => e.op === 'target' && e.v < 0.001);
    ok(!!fade && fade.t < body.stopped, lib + ' safety fade precedes the cap stop');

    const r0 = render(out => Voices.kick(T0, out, lib, 1, P({ decay: 0.3, sus: 0 })), T0);
    const b0 = r0.nodes.find(n => n.kind === 'osc');
    ok(b0.stopped < T0 + 1, lib + ' sus=0 stays a one-shot');
  }
  // sustained snare noise runs to the cap as well
  const sn = render(out => Voices.snare(T0, out, '808', 1, P({ pitch: 180, decay: 0.2, sus: 0.6 })), T0);
  const nz = sn.nodes.find(n => n.kind === 'bufsrc');
  ok(nz.stopped >= T0 + SUS_CAP - 1e-9, 'sustained snare noise lives to the cap');
});

suite('choke handle', () => {
  const r = render(out => Voices.kick(T0, out, '808', 1, P({ decay: 0.5, sus: 0.8 })), T0);
  const tRel = T0 + 1.5;
  r.handle.release(tRel);
  const chokeEvents = log.filter(e => e.n === r.handle.in && e.name === 'gain');
  ok(chokeEvents.some(e => e.op === 'ramp' && e.v < 0.001 && e.t > tRel && e.t < tRel + 0.05),
     'release() ramps the choke gain to silence within ~15 ms');
  ok(reaches(r.nodes.find(n => n.kind === 'osc'), r.handle.in),
     'voice audio passes through the choke gain');
  let err = null;
  try { r.handle.release(tRel + 0.5); } catch (e) { err = e; }
  ok(!err, 'double release is harmless' + (err ? ': ' + err.message : ''));
});

suite('pitch edit and tuning compose', () => {
  for (const lib of LIBS) {
    const base = render(out => Voices.kick(T0, out, lib, 1, P({ pitch: 50 })), T0);
    const up   = render(out => Voices.kick(T0, out, lib, 2, P({ pitch: 50 })), T0);
    const pp   = render(out => Voices.kick(T0, out, lib, 1, P({ pitch: 100 })), T0);
    const f0 = r => r.log.find(e => e.name === 'frequency' && e.op === 'set').v;
    ok(Math.abs(f0(up) / f0(base) - 2) < 1e-9, lib + ' KEY/OCT tuning is a clean octave');
    ok(Math.abs(f0(pp) / f0(base) - 2) < 1e-9, lib + ' PITCH knob doubles the frequency');

    const h1 = render(out => Voices.hat(T0, out, lib, 1, P({ pitch: 40, decay: 0.05 })), T0);
    const h2 = render(out => Voices.hat(T0, out, lib, 1, P({ pitch: 80, decay: 0.05 })), T0);
    const fs1 = h1.log.filter(e => e.name === 'frequency').map(e => e.v);
    const fs2 = h2.log.filter(e => e.name === 'frequency').map(e => e.v);
    ok(fs1.length === fs2.length && fs1.every((v, i) => Math.abs(fs2[i] / v - 2) < 1e-9),
       lib + ' hat PITCH scales every partial');
  }
  const r = render(out => Voices.kick(T0, out, '808', 1, P({ pitch: 50 })), T0);
  const f = r.log.filter(e => e.name === 'frequency');
  ok(f.length >= 2 && f[1].v < f[0].v, 'kick still sweeps downward');
});

suite('library voicing differences survive the edit layer', () => {
  const hp = lib => {
    const r = render(out => Voices.hat(T0, out, lib, 1, P({ pitch: 40, decay: 0.04 })), T0);
    return r.nodes.filter(n => n.kind === 'biquad' && n.type === 'highpass')
                  .map(n => n.frequency.value)[0];
  };
  ok(hp('909') > hp('808'), '909 hats still sit brighter than 808 hats');
  const k9 = render(out => Voices.kick(T0, out, '909', 1, P({})), T0);
  ok(k9.nodes.some(n => n.kind === 'bufsrc'), '909 kick keeps its attack click');
});

suite('extreme edits stay finite', () => {
  for (const lib of LIBS) {
    for (const tune of [0.25, 1, 4]) {
      for (const over of [{ pitch: 15, decay: 0.008 }, { pitch: 150, decay: 3, sus: 1 },
                          { pitch: 500, decay: 2, wave: 'sawtooth' }]) {
        let err = null;
        try {
          render(out => { Voices.kick(T0, out, lib, tune, P(over));
                          Voices.snare(T0, out, lib, tune, P(over));
                          Voices.hat(T0, out, lib, tune, P(over)); }, T0);
        } catch (e) { err = e; }
        ok(!err, lib + ' tune x' + tune + ' ' + JSON.stringify(over) + ' builds' +
           (err ? ': ' + err.message : ''));
      }
    }
    for (const cutoff of [200, 2600, 9000]) {
      for (const f of [27.5, 440, 4186]) {
        let err = null;
        try { render(out => Voices.chord(T0, out, lib, [f], P({ cutoff, decay: 0.2 })), T0); }
        catch (e) { err = e; }
        ok(!err, lib + ' chord ' + f + ' Hz @ cutoff ' + cutoff + (err ? ': ' + err.message : ''));
      }
    }
  }
});

/* =================================================================== */
console.log('\n' + (fail ? 'FAILED' : 'OK') + ' — ' + pass + ' assertions passed, ' + fail + ' failed');
process.exit(fail ? 1 : 0);
