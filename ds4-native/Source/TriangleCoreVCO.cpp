#include "TriangleCoreVCO.h"
#include <cmath>
#include <algorithm>

namespace {
    constexpr double kTwoPi = 6.28318530717958647692;
}

void TriangleCoreVCO::prepare(double sampleRate) {
    sr = sampleRate;
    reset();
}

void TriangleCoreVCO::reset() {
    phase  = 0.0;
    rising = true;
    sawPhase = 0.0;
    perHitDetuneCents   = 0.0f;
    slowWalkCents       = 0.0f;
    externalDetuneCents = 0.0f;
    wobblePhase         = 0.0f;
    wobbleFreqHz        = 0.25f;
}

void TriangleCoreVCO::retrigger(uint32_t hitSeed) {
    rng.seed(hitSeed);
    std::uniform_real_distribution<float> hit  (-3.0f, 3.0f);
    std::uniform_real_distribution<float> walk (-0.6f, 0.6f);
    std::uniform_real_distribution<float> wob  ( 0.15f, 0.40f);

    perHitDetuneCents = hit(rng);
    slowWalkCents = std::min(3.0f, std::max(-3.0f, slowWalkCents + walk(rng)));
    wobbleFreqHz = wob(rng);
}

float TriangleCoreVCO::polyBLEP(double t, double dt) {
    if (t < dt) {
        const double x = t / dt;
        return (float)(x + x - x * x - 1.0);
    }
    if (t > 1.0 - dt) {
        const double x = (t - 1.0) / dt;
        return (float)(x * x + x + x + 1.0);
    }
    return 0.0f;
}

/* Map triangle [-1, +1] -> true sine via y = sin(pi/2 * t).
   The previous 3rd-order Bhaskara polynomial added ~1.3% THD (H3
   around -28 dBc) on the assumption that the original DS-family
   instruments used an ICL8038-style diode breakpoint shaper. That
   assumption was wrong: actual reference measurements of the
   SDS-2002 era drum synth software show H2 sitting at -118 dB and
   all other harmonics at -95 dB or quieter, i.e. an essentially
   mathematically pure sine.  Returning to std::sin removes the
   spurious 'hollow flute' character. */
float TriangleCoreVCO::sineShaper(float t) {
    return std::sin (1.5707963267948966f * t);
}

float TriangleCoreVCO::processSample() {
    /* 1. Drift modulation -- continuous wobble updates every sample. */
    wobblePhase += (float)(wobbleFreqHz / sr);
    if (wobblePhase >= 1.0f) wobblePhase -= 1.0f;
    const float wobble = wobbleAmount * std::sin((float)kTwoPi * wobblePhase);

    const float cents = perHitDetuneCents
                      + slowWalkCents
                      + manualDetuneCents
                      + externalDetuneCents
                      + wobble;
    const double freqMul = std::pow(2.0, (double)cents / 1200.0);
    const double effFreq = (double)freq * freqMul;
    if (effFreq <= 0.0) return 0.0f;

    /* 2. Triangle core.  A full cycle (-1 → +1 → -1) covers 4 units of
       phase, so the per-sample step is 4·f/sr.  At each Schmitt
       threshold the flip-flop toggles which current source is active
       and the phase "bounces" with the residual overshoot conserved
       (same as a physical capacitor cannot overshoot the comparator
       trip point by more than the time-step would allow). */
    const double step = 4.0 * effFreq / sr;
    if (rising) {
        phase += step;
        if (phase >= 1.0) {
            phase  = 2.0 - phase;   // reflect off the upper threshold
            rising = false;
        }
    } else {
        phase -= step;
        if (phase <= -1.0) {
            phase  = -2.0 - phase;  // reflect off the lower threshold
            rising = true;
        }
    }

    /* 3. Output selection. */
    if (wave == Waveform::Sawtooth) {
        /* Separate PolyBLEP saw -- the 8038 has no native saw output. */
        const double dt = effFreq / sr;
        sawPhase += dt;
        if (sawPhase >= 1.0) sawPhase -= 1.0;
        float y = (float)(2.0 * sawPhase - 1.0);
        y -= polyBLEP(sawPhase, dt);
        return y;
    }

    switch (wave) {
        case Waveform::Triangle: return (float)phase;
        case Waveform::Sine:     return sineShaper((float)phase);
        case Waveform::Square:   return rising ? 1.0f : -1.0f;
        default:                 return 0.0f;
    }
}
