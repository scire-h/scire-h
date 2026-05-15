#pragma once
#include <cmath>
#include <algorithm>

/* Phase 2.9 -- zero-delay-feedback Moog ladder low-pass filter,
   24 dB/oct, with implicit feedback solve.

   Four cascaded TPT one-pole low-pass stages with global negative
   feedback gain `k`. Because the feedback closes the loop with no
   sample delay, naive iterative substitution would oscillate or
   add 1-sample lag; instead we solve the linear feedback equation
   analytically each sample, then propagate the integrator states
   with the corrected input.

   The closed-form derivation (see Zavalishin, "The Art of VA Filter
   Design" §5.2) gives:

       y3 = (G^4 · x + S4) / (1 + k · G^4)

   where:
       g  = tan(pi · fc / sr)
       G  = g / (1 + g)           // per-stage low-pass DC->Nyquist gain
       S4 = (1 - G) · (G^3 · z0 + G^2 · z1 + G · z2 + z3)
                                  // weighted sum of integrator states
       k  = 4 · resonance         // feedback amount, k=4 -> self-osc

   Drop-in replacement for juce::dsp::LadderFilter that:
     - is JUCE-independent (header-only, testable in isolation)
     - allows per-sample cutoff modulation with no smoother lag
     - exposes the cutoff/Q split in pure-math terms */

class MoogLadder {
public:
    void prepare (double sampleRate) {
        sr = sampleRate;
        reset();
        setCutoff (1000.0f);
        setResonance (0.0f);
    }

    void reset() {
        for (auto& v : z) v = 0.0f;
    }

    void setCutoff (float fc) {
        const float fcClamped = std::min ((float)(sr * 0.49),
                                          std::max (20.0f, fc));
        g  = std::tan (3.14159265f * fcClamped / (float)sr);
        G  = g / (1.0f + g);
        const float G2 = G  * G;
        G3 = G2 * G;
        G4 = G2 * G2;
        oneMinusG = 1.0f - G;
    }

    /* resonance 0..1, where 1 is self-oscillation. Values slightly
       above 1 (up to ~1.05) give a clean, sustained self-oscillator. */
    void setResonance (float r) {
        const float clamped = std::max (0.0f, std::min (1.05f, r));
        k = 4.0f * clamped;
    }

    /* Optional input drive (1.0 = unity). The OTAVCA already
       saturates downstream, so the ladder's drive is usually left at 1. */
    void setDrive (float d) { drive = d; }

    inline float processSample (float x) {
        const float xd = x * drive;

        /* Analytical ZDF feedback solve. */
        const float S4 = oneMinusG
                       * (G3 * z[0] + (G * G) * z[1] + G * z[2] + z[3]);
        const float y3 = (G4 * xd + S4) / (1.0f + k * G4);

        /* Propagate states with the corrected input u = x - k·y3. */
        const float u  = xd - k * y3;

        const float v0 = (u    - z[0]) * G;
        const float y0 = v0 + z[0];
        z[0] = y0 + v0;

        const float v1 = (y0   - z[1]) * G;
        const float y1 = v1 + z[1];
        z[1] = y1 + v1;

        const float v2 = (y1   - z[2]) * G;
        const float y2 = v2 + z[2];
        z[2] = y2 + v2;

        const float v3 = (y2   - z[3]) * G;
        const float y3a = v3 + z[3];
        z[3] = y3a + v3;

        return y3a;
    }

private:
    double sr = 44100.0;
    float g  = 0.0f;
    float G  = 0.0f;
    float G3 = 0.0f;
    float G4 = 0.0f;
    float oneMinusG = 0.0f;
    float k     = 0.0f;
    float drive = 1.0f;
    float z[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
};
