#include "DriftedVCO.h"
#include <cmath>

namespace {
    constexpr double kTwoPi = 6.28318530717958647692;
}

void DriftedVCO::prepare(double sampleRate) {
    sr = sampleRate;
    reset();
}

void DriftedVCO::reset() {
    phase                = 0.0;
    perHitDetuneCents    = 0.0f;
    slowWalkCents        = 0.0f;
    externalDetuneCents  = 0.0f;
    wobblePhase          = 0.0f;
    wobbleFreqHz         = 0.25f;
    triState             = 0.0f;
}

void DriftedVCO::retrigger(uint32_t hitSeed) {
    rng.seed(hitSeed);
    std::uniform_real_distribution<float> hit  (-3.0f, 3.0f);
    std::uniform_real_distribution<float> walk (-0.6f, 0.6f);
    std::uniform_real_distribution<float> wob  ( 0.15f, 0.40f);

    perHitDetuneCents = hit(rng);

    /* Random walk -- bounded -3..+3 cents. */
    slowWalkCents = std::min(3.0f, std::max(-3.0f, slowWalkCents + walk(rng)));

    /* Pick a new wobble rate within the analog VCO instability range. */
    wobbleFreqHz = wob(rng);
}

float DriftedVCO::polyBLEP(double t, double dt) {
    /* Olli Niemitalo's polyBLEP correction. */
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

float DriftedVCO::processSample() {
    /* Update continuous wobble phase. */
    wobblePhase += (float)(wobbleFreqHz / sr);
    if (wobblePhase >= 1.0f) wobblePhase -= 1.0f;
    const float wobble = wobbleAmount *
                         std::sin((float)(kTwoPi) * wobblePhase);

    /* Sum all detune contributions, convert to a frequency multiplier
       via 2^(cents / 1200). */
    const float cents = perHitDetuneCents
                      + slowWalkCents
                      + manualDetuneCents
                      + externalDetuneCents
                      + wobble;
    const double freqMul = std::pow(2.0, (double)cents / 1200.0);
    const double effFreq = (double)freq * freqMul;

    const double dt = effFreq / sr;
    if (dt <= 0.0) {
        /* No-op safety. */
        return 0.0f;
    }

    /* Advance phase. */
    phase += dt;
    if (phase >= 1.0) phase -= 1.0;
    if (phase < 0.0)  phase += 1.0;

    float y = 0.0f;
    switch (wave) {
        case Waveform::Sine: {
            y = std::sin((float)(kTwoPi * phase));
            break;
        }
        case Waveform::Sawtooth: {
            y = (float)(2.0 * phase - 1.0);
            y -= polyBLEP(phase, dt);
            break;
        }
        case Waveform::Square: {
            y = (phase < 0.5) ? 1.0f : -1.0f;
            y += polyBLEP(phase, dt);
            const double phase2 = std::fmod(phase + 0.5, 1.0);
            y -= polyBLEP(phase2, dt);
            break;
        }
        case Waveform::Triangle: {
            /* Integrate a polyBLEP square -- gives a bandlimited triangle. */
            float sq = (phase < 0.5) ? 1.0f : -1.0f;
            sq += polyBLEP(phase, dt);
            const double phase2 = std::fmod(phase + 0.5, 1.0);
            sq -= polyBLEP(phase2, dt);
            const float intCoeff = (float)(4.0 * dt);
            triState = intCoeff * sq + (1.0f - intCoeff) * triState;
            y = triState;
            break;
        }
    }
    return y;
}
