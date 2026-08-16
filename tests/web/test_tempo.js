#!/usr/bin/env node
/* ===================================================================
   Unit tests for the POLYTEMPO tempo core.

   The core lives inside zure.html (the app is a single
   self-contained file by design), delimited by TEMPO_CORE markers.
   This runner slices it out and evaluates it, so the tests always
   exercise the exact code the browser runs — no second copy to drift.
   =================================================================== */
'use strict';

const fs = require('fs');
const path = require('path');

// optional argv override so the suite can be pointed at a mutated copy
const HTML = process.argv[2] || path.join(__dirname, '..', '..', 'zure.html');

function loadCore () {
  const src = fs.readFileSync(HTML, 'utf8');
  const a = src.indexOf('/*<<<TEMPO_CORE>>>*/');
  const b = src.indexOf('/*<<<END_TEMPO_CORE>>>*/');
  if (a < 0 || b < 0) throw new Error('TEMPO_CORE markers not found in zure.html');
  return new Function(src.slice(a, b) + '\nreturn TempoCore;')();
}

const T = loadCore();

/* ---------------- tiny assertion harness ---------------- */
let pass = 0, fail = 0;
function ok (cond, msg) {
  if (cond) { pass++; }
  else { fail++; console.log('  FAIL  ' + msg); }
}
function near (a, b, eps, msg) {
  const d = Math.abs(a - b);
  if (d <= eps) { pass++; }
  else { fail++; console.log('  FAIL  ' + msg + '  (' + a + ' vs ' + b + ', |d|=' + d + ')'); }
}
function suite (name, fn) {
  console.log('\n' + name);
  try { fn(); }
  catch (e) { fail++; console.log('  FAIL  suite threw: ' + e.message); }
}

/* ---------------- simulation harness ---------------- */
const TICK = 0.025, LOOKAHEAD = 0.12;

function mkTrack (bpm, loopBars) {
  return { bpm, loopSteps: (loopBars || 1) * T.STEPS_PER_BAR,
           stepIndex: 0, nextStepTime: 0, catchState: null };
}

const OPT = catchTime => ({
  lockDur: Math.min(30, Math.max(0.4, catchTime * 0.6)),
  snapSec: 0.015, maxDev: 0.6, maxTries: 6
});

/* Runs the same scheduler loop the app runs, recording every emitted
   bar-head time so the assertions can look at real output, not just
   at the core's internal prediction. */
function run (slave, master, seconds, catchTime, hook) {
  const barsS = [], barsM = [];
  const bpmFn = tr => (tr === master || !tr.catchState)
    ? (t => tr.bpm)
    : (t => T.bpmAt(tr, tt => T.bpmAt(master, null, tt), t));

  const events = [];
  for (let now = 0; now < seconds; now += TICK) {
    if (hook) hook(now, slave, master);
    const ev = T.updateCatch(slave, master, now, OPT(catchTime));
    if (ev) events.push({ t: now, ev });
    for (const [tr, sink] of [[master, barsM], [slave, barsS]]) {
      if (tr.catchState) tr.bpm = bpmFn(tr)(now);
      T.advance(tr, now + LOOKAHEAD, bpmFn(tr), (step, time, abs) => {
        if (abs % T.STEPS_PER_BAR === 0) sink.push(time);
      });
    }
  }
  return { barsS, barsM, events };
}

/* Smallest distance from the slave's last bar head to the master's
   bar-head grid, in seconds. This is what "phase locked" means. */
function residual (barsS, barsM, barDur) {
  const s = barsS[barsS.length - 1];
  let best = Infinity;
  for (const m of barsM) {
    let d = s - m;
    d -= Math.round(d / barDur) * barDur;
    if (Math.abs(d) < Math.abs(best)) best = d;
  }
  return best;
}

/* =================================================================== */
suite('catchShape', () => {
  for (const c of ['linear', 'exp']) {
    near(T.catchShape(0, c), 1, 1e-12, c + ' shape(0)=1');
    near(T.catchShape(1, c), 0, 1e-12, c + ' shape(1)=0');
    ok(T.catchShape(-1, c) === 1 && T.catchShape(2, c) === 0, c + ' clamps outside [0,1]');
    let mono = true, prev = 1;
    for (let u = 0; u <= 1.0001; u += 0.01) {
      const v = T.catchShape(u, c);
      if (v > prev + 1e-12) mono = false;
      prev = v;
    }
    ok(mono, c + ' shape is monotonically decreasing');
  }
  // exp must approach faster than linear early on
  ok(T.catchShape(0.25, 'exp') < T.catchShape(0.25, 'linear'), 'exp closes distance faster early');
});

suite('rampBpm', () => {
  const ramp = { t0: 10, dur: 4, startBpm: 90, curve: 'linear' };
  near(T.rampBpm(ramp, 120, 10), 90, 1e-9, 'ramp starts at startBpm');
  near(T.rampBpm(ramp, 120, 14), 120, 1e-9, 'ramp ends on target');
  near(T.rampBpm(ramp, 120, 12), 105, 1e-9, 'linear ramp is linear at the midpoint');
  // a moving target is tracked without re-planning
  near(T.rampBpm(ramp, 150, 14), 150, 1e-9, 'ramp lands on a target that moved');
});

suite('lockDev / lockAmp', () => {
  const dur = 3, err = 0.2;                       // advance by 0.2 bar
  const lock = { t0: 5, dur, amp: T.lockAmp(err, dur) };
  near(T.lockDev(lock, 5), 0, 1e-12, 'swell starts at zero deviation');
  near(T.lockDev(lock, 8), 0, 1e-12, 'swell ends at zero deviation');
  ok(T.lockDev(lock, 6.5) > 0, 'positive error swells the BPM upward (overshoot)');

  // numerically integrate the swell: it must buy exactly `err` bars
  const N = 200000;
  let bars = 0;
  for (let i = 0; i < N; i++) {
    const t = 5 + (i + 0.5) * dur / N;
    bars += T.lockDev(lock, t) / 60 / T.BEATS_PER_BAR * (dur / N);
  }
  near(bars, err, 1e-6, 'swell integral equals the phase error it must absorb');

  const neg = { t0: 0, dur, amp: T.lockAmp(-0.3, dur) };
  ok(T.lockDev(neg, 1.5) < 0, 'negative error dips the BPM (falls back)');
});

suite('wrapPhase', () => {
  near(T.wrapPhase(0.3, 'ahead'), 0.3, 1e-12, 'ahead keeps a forward offset');
  near(T.wrapPhase(0.8, 'ahead'), 0.8, 1e-12, 'ahead never goes negative');
  near(T.wrapPhase(0.3, 'behind'), -0.7, 1e-12, 'behind always goes negative');
  near(T.wrapPhase(0.8, 'nearest'), -0.2, 1e-12, 'nearest picks the short way round');
  near(T.wrapPhase(0.2, 'nearest'), 0.2, 1e-12, 'nearest keeps a short forward hop');
  near(T.wrapPhase(3.25, 'nearest'), 0.25, 1e-12, 'whole bars are folded away');
  near(T.wrapPhase(-0.25, 'nearest'), -0.25, 1e-12, 'negative input folds correctly');
});

suite('stepDur', () => {
  near(T.stepDur(() => 120, 0), 0.125, 1e-12, 'constant 120 BPM 16th = 125 ms');
  near(T.stepDur(() => 60, 0), 0.25, 1e-12, 'constant 60 BPM 16th = 250 ms');

  // linear tempo sweep has a closed-form step length to compare against
  const b0 = 100, r = 40;                          // BPM/s
  const dt = T.stepDur(t => b0 + r * t, 0);
  const exact = (-b0 + Math.sqrt(b0 * b0 + 30 * r)) / r;
  near(dt, exact, 1e-12, 'integration matches the analytic linear-sweep solution');

  // integrating a full bar of a sweep must accumulate exactly 4 beats
  let t = 0, beats = 0;
  const bpm = tt => 90 + 30 * Math.sin(tt);
  for (let i = 0; i < T.STEPS_PER_BAR; i++) {
    const d = T.stepDur(bpm, t);
    const N = 4000;                                 // fine reference integral
    for (let j = 0; j < N; j++) beats += bpm(t + (j + 0.5) * d / N) / 60 * (d / N);
    t += d;
  }
  // 1e-6 beat at 90 BPM is well under a microsecond of drift per bar
  near(beats, T.BEATS_PER_BAR, 1e-6, 'a bar of steps is 4 beats under a moving tempo (<1 us/bar)');
});

suite('nextBarHead', () => {
  const tr = { stepIndex: 0, nextStepTime: 2.0 };
  near(T.nextBarHead(tr, 0, 0.125), 2.0, 1e-12, 'on a bar line, the head is the next step');
  tr.stepIndex = 4;
  near(T.nextBarHead(tr, 0, 0.125), 2.0 + 12 * 0.125, 1e-12, '12 steps left from step 4');
  tr.stepIndex = 0;
  near(T.nextBarHead(tr, 3.0, 0.125), 4.0, 1e-12, 'skips forward whole bars to reach t');
});

suite('BPM SYNC', () => {
  const m = mkTrack(120), s = mkTrack(96);
  T.beginCatch(s, 0, { mode: 'bpm', curve: 'exp', dir: 'nearest', dur: 4 });
  const { events } = run(s, m, 8, 4);
  near(s.bpm, 120, 0.01, 'slave reaches the master BPM');
  ok(s.catchState === null, 'catch state is cleared when done');
  ok(events.some(e => e.ev === 'bpm-locked'), 'emits bpm-locked');
  ok(events.every(e => e.ev !== 'lock-start'), 'BPM SYNC never enters the phase stage');
});

suite('BPM SYNC leaves the phase alone', () => {
  const m = mkTrack(120), s = mkTrack(96);
  s.nextStepTime = 0.037;                            // deliberately offset
  T.beginCatch(s, 0, { mode: 'bpm', curve: 'linear', dir: 'nearest', dur: 3 });
  const { barsS, barsM } = run(s, m, 8, 3);
  const barDur = 60 / 120 * T.BEATS_PER_BAR;
  ok(Math.abs(residual(barsS, barsM, barDur)) > 0.02,
     'bar heads stay apart — polyrhythmic drift is preserved');
});

suite('PHASE SYNC', () => {
  for (const curve of ['linear', 'exp']) {
    for (const dir of ['ahead', 'nearest', 'behind']) {
      const m = mkTrack(120), s = mkTrack(96);
      s.nextStepTime = 0.083;
      T.beginCatch(s, 0, { mode: 'phase', curve, dir, dur: 4 });
      const { barsS, barsM, events } = run(s, m, 40, 4);
      const barDur = 60 / 120 * T.BEATS_PER_BAR;
      near(s.bpm, 120, 0.01, curve + '/' + dir + ': BPM lands on target');
      ok(s.catchState === null, curve + '/' + dir + ': locked and released');
      ok(events.some(e => e.ev === 'phase-locked'), curve + '/' + dir + ': emits phase-locked');
      near(residual(barsS, barsM, barDur), 0, 0.002,
           curve + '/' + dir + ': bar heads coincide after lock');
    }
  }
});

suite('PHASE SYNC direction', () => {
  // A slave whose bar head trails the master's by a small amount:
  // AHEAD should hurry it forward, BEHIND should let it fall back a
  // whole bar. Both end aligned, but by opposite routes.
  const mk = dir => {
    const m = mkTrack(120), s = mkTrack(120);
    s.nextStepTime = 0.10;
    T.beginCatch(s, 0, { mode: 'phase', curve: 'exp', dir, dur: 2 });
    let maxBpm = 0, minBpm = 1e9;
    const r = run(s, m, 30, 2, (now, sl, ma) => {
      if (!sl.catchState) return;
      const b = T.bpmAt(sl, t => T.bpmAt(ma, null, t), now);
      maxBpm = Math.max(maxBpm, b); minBpm = Math.min(minBpm, b);
    });
    return { r, maxBpm, minBpm, s, m };
  };
  const A = mk('ahead'), B = mk('behind');
  const barDur = 2;
  ok(A.maxBpm > 120.5, 'AHEAD overshoots above the target BPM on the way in');
  ok(B.minBpm < 119.5, 'BEHIND dips below the target BPM on the way in');
  near(residual(A.r.barsS, A.r.barsM, barDur), 0, 0.002, 'AHEAD ends aligned');
  near(residual(B.r.barsS, B.r.barsM, barDur), 0, 0.002, 'BEHIND ends aligned');
});

suite('CATCH RATE knob is live', () => {
  const m = mkTrack(120), s = mkTrack(80);
  T.beginCatch(s, 0, { mode: 'bpm', curve: 'exp', dir: 'nearest', dur: 30 });
  let retimed = false;
  const { events } = run(s, m, 12, 30, (now, sl) => {
    if (!retimed && now >= 2) {                       // "user turns the knob"
      retimed = T.retimeCatch(sl, now, 1.5, sl.bpm);
    }
  });
  ok(retimed, 'retimeCatch re-plans a running converge stage');
  const done = events.find(e => e.ev === 'bpm-locked');
  ok(done && done.t < 4.0, 'shortening the knob mid-flight finishes early (t=' +
     (done ? done.t.toFixed(2) : 'never') + 's, was scheduled for 30s)');
  near(s.bpm, 120, 0.01, 'still lands exactly on the target');

  // and the other way: a longer time must not finish early
  const m2 = mkTrack(120), s2 = mkTrack(80);
  T.beginCatch(s2, 0, { mode: 'bpm', curve: 'exp', dir: 'nearest', dur: 2 });
  const r2 = run(s2, m2, 6, 2, (now, sl) => {
    if (now >= 0.5 && now < 0.53) T.retimeCatch(sl, now, 4, sl.bpm);
  });
  const d2 = r2.events.find(e => e.ev === 'bpm-locked');
  ok(d2 && d2.t > 4.0, 'lengthening the knob mid-flight defers the landing (t=' +
     (d2 ? d2.t.toFixed(2) : 'never') + 's)');
});

suite('PHASE SYNC is robust across tempos and offsets', () => {
  let worst = 0, worstCase = '';
  let allLocked = true;
  const cases = [
    [120, 60], [120, 200], [90, 91], [137.5, 92], [100, 99.5], [174, 87], [60, 180]
  ];
  for (const [mb, sb] of cases) {
    for (const off of [0, 0.017, 0.11, 0.31]) {
      const m = mkTrack(mb), s = mkTrack(sb, 2);
      s.nextStepTime = off;
      T.beginCatch(s, 0, { mode: 'phase', curve: 'exp', dir: 'nearest', dur: 3 });
      const { barsS, barsM } = run(s, m, 60, 3);
      if (s.catchState !== null) { allLocked = false; }
      const barDur = 60 / mb * T.BEATS_PER_BAR;
      const res = Math.abs(residual(barsS, barsM, barDur));
      if (res > worst) { worst = res; worstCase = mb + '<-' + sb + ' off=' + off; }
    }
  }
  ok(allLocked, 'every case reached lock');
  ok(worst < 0.003, 'worst residual across 28 cases is under 3 ms (' +
     (worst * 1000).toFixed(2) + ' ms, ' + worstCase + ')');
});

suite('BPM glide (fixed-target)', () => {
  // a knob move glides to the value instead of jumping
  const m = mkTrack(120), s = mkTrack(100);
  T.beginGlide(s, 0, 140, { curve: 'exp', dur: 3 });
  ok(s.catchState.isGlide && s.catchState.fixedBpm === 140, 'glide state carries its target');
  const { events } = run(s, m, 8, 3);
  near(s.bpm, 140, 0.01, 'glide lands on the typed/knob value');
  ok(s.catchState === null, 'glide releases when done');
  ok(events.some(e => e.ev === 'bpm-locked'), 'glide completes through the same machinery');

  // the master's own BPM is irrelevant to a fixed-target glide
  const m2 = mkTrack(60), s2 = mkTrack(100);
  T.beginGlide(s2, 0, 90, { curve: 'linear', dur: 2 });
  run(s2, m2, 6, 2);
  near(s2.bpm, 90, 0.01, 'fixed target wins over whatever the master is doing');

  // mid-glide the tempo actually moves through intermediate values
  const m3 = mkTrack(120), s3 = mkTrack(100);
  T.beginGlide(s3, 0, 140, { curve: 'linear', dur: 4 });
  const mid = T.bpmAt(s3, () => 140, 2);
  ok(mid > 105 && mid < 135, 'halfway through, the BPM is between start and target (' +
     mid.toFixed(1) + ')');

  // retargeting mid-flight re-plans from the current live BPM
  const m4 = mkTrack(120), s4 = mkTrack(100);
  T.beginGlide(s4, 0, 140, { curve: 'exp', dur: 10 });
  run(s4, m4, 2, 10);
  const liveAt2 = s4.bpm;
  T.beginGlide(s4, 2, 80, { curve: 'exp', dur: 3 });
  near(s4.catchState.ramp.startBpm, liveAt2, 0.5, 'retarget starts from the live tempo, not the old target');
  run(s4, m4, 8, 3);
  near(s4.bpm, 80, 0.01, 'second target wins');

  // the CATCH RATE knob re-times glides too
  const m5 = mkTrack(120), s5 = mkTrack(100);
  T.beginGlide(s5, 0, 130, { curve: 'exp', dur: 60 });
  ok(T.retimeCatch(s5, 1, 0.5, 101), 'retimeCatch applies to a glide');
  run(s5, m5, 4, 0.5);
  near(s5.bpm, 130, 0.01, 'shortened glide still lands on target');
});

suite('a gliding reference reports its own trajectory', () => {
  // regression: a wandering/gliding MASTER used to report its stale bpm
  // to whoever was catching it (bpmAt with null targetAt ignored the
  // glide destination), so catch + wander together drifted apart
  const m = mkTrack(120);
  T.beginGlide(m, 0, 150, { curve: 'linear', dur: 2 });
  near(T.bpmAt(m, null, 1), 135, 1e-9, 'mid-flight the reference reads its true midpoint');
  near(T.bpmAt(m, null, 2), 150, 1e-9, 'at the end it reads the glide destination');
  near(T.bpmAt(m, null, 5), 150, 1e-9, 'and stays there after');

  // a slave catching that gliding master must land on the destination
  const m2 = mkTrack(120), s2 = mkTrack(90);
  T.beginGlide(m2, 0, 150, { curve: 'linear', dur: 2 });
  T.beginCatch(s2, 0, { mode: 'bpm', curve: 'exp', dir: 'nearest', dur: 3 });
  for (let now = 0; now < 8; now += 0.025) {
    T.updateCatch(m2, m2, now, OPT(2));
    T.updateCatch(s2, m2, now, OPT(3));
    if (m2.catchState) m2.bpm = T.bpmAt(m2, () => m2.catchState.fixedBpm, now);
    if (s2.catchState) s2.bpm = T.bpmAt(s2, t => T.bpmAt(m2, null, t), now);
  }
  near(s2.bpm, 150, 0.01, 'catching a gliding master lands on where the master went');
});

suite('advance', () => {
  const tr = mkTrack(120);
  const times = [];
  T.advance(tr, 1.0, () => 120, (step, time) => times.push([step, time]));
  ok(times.length === 8, 'emits every step before the horizon (got ' + times.length + ')');
  near(times[0][1], 0, 1e-12, 'first step at t=0');
  near(times[7][1], 0.875, 1e-12, 'eighth step at 875 ms');
  ok(times[0][0] === 0 && times[4][0] === 4, 'step index wraps within the loop');

  const short = mkTrack(120);
  short.loopSteps = 4;
  const steps = [];
  T.advance(short, 1.0, () => 120, s => steps.push(s));
  ok(steps.join('') === '01230123', 'a 4-step loop repeats (' + steps.join('') + ')');
});

suite('stepPosAt', () => {
  const tr = mkTrack(120);
  tr.stepIndex = 10; tr.nextStepTime = 5.0;
  near(T.stepPosAt(tr, 5.0, 0.125), 10, 1e-12, 'exactly on the next step');
  near(T.stepPosAt(tr, 4.9375, 0.125), 9.5, 1e-12, 'halfway through the previous step');
});

/* =================================================================== */
console.log('\n' + (fail ? 'FAILED' : 'OK') + ' — ' + pass + ' assertions passed, ' + fail + ' failed');
process.exit(fail ? 1 : 0);
