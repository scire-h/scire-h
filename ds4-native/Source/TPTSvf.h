#pragma once
#include <cmath>

/* Phase 2.3 -- zero-delay-feedback state-variable filter (Vadim
   Zavalishin, "The Art of VA Filter Design", §3-4).

   The biquad pair previously used in NoiseVoice rebuilt coefficients
   on the heap every sweep update, which is both expensive and an
   allocation in the audio thread. A TPT SVF uses two integrator
   states and one `tan(π·fc/fs)` per coefficient update, with no
   allocation, no IIR-style cutoff/Q coupling, and stable behaviour
   right up to Nyquist. */

class TPTSvf {
public:
    enum class Output { LP, BP, HP, Notch, Peak };

    void prepare(double sampleRate) {
        sr = sampleRate;
        reset();
        update();
    }
    void reset() { ic1 = 0.0f; ic2 = 0.0f; }

    void setCutoff(float fc) {
        if (fc != cutoff) { cutoff = fc; update(); }
    }
    void setQ(float q) {
        if (q != Q) { Q = q; update(); }
    }
    void setMode(Output o) { mode = o; }

    /* Optional peak-shelf gain in dB. Only meaningful for Output::Peak. */
    void setPeakGainDb(float dB) { peakLin = std::pow(10.0f, dB / 20.0f); }

    inline float processSample(float x) {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        const float lp = v2;
        const float bp = v1;
        const float hp = x - k * v1 - v2;
        switch (mode) {
            case Output::LP:    return lp;
            case Output::BP:    return bp;
            case Output::HP:    return hp;
            case Output::Notch: return hp + lp;
            case Output::Peak:  return x + (peakLin - 1.0f) * bp * k;
        }
        return 0.0f;
    }

private:
    double sr     = 44100.0;
    float  cutoff = 1000.0f;
    float  Q      = 1.0f;
    float  peakLin = 1.0f;
    Output mode   = Output::BP;

    /* TPT integrator state. */
    float  ic1 = 0.0f, ic2 = 0.0f;
    /* Precomputed coefficients. */
    float  g = 0.0f, k = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

    void update() {
        const float fc = std::min((float)(sr * 0.49f), std::max(2.0f, cutoff));
        g = std::tan(3.14159265f * fc / (float)sr);
        k = 1.0f / std::max(0.05f, Q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};
