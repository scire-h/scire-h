#pragma once
#include <cstdint>
#include <random>

/* Phase 2.1 -- ICL8038-style triangle-core VCO.

   See ds4-native/docs/CIRCUIT_RESEARCH.md §2 for the analog reference.

   The 8038 (and therefore the DS-4 VCO it sits inside of) generates
   sine + triangle + square SIMULTANEOUSLY from a single
   current-controlled triangle core:

       i_up        i_dn
         ▲          ▼
         └─── + Ct ─┘
              │
              ├── triangle = Vct
              │
              ├── square   = comparator(Vct, threshold)
              │
              └── sine     = Vct passed through a piecewise-linear
                              breakpoint shaper (~1-3 % THD,
                              odd harmonics only)

   So our DSP model carries one phase state (the "capacitor voltage")
   and a rising/falling boolean (the "current source selector"). The
   sine output is a 3rd-order Bhaskara polynomial of the triangle --
   which is mathematically equivalent to what the 8038's diode
   breakpoint network does, with the same characteristic odd-harmonic
   THD signature.

   Sawtooth isn't a native 8038 output. We synthesise it from a
   separate PolyBLEP-anti-aliased accumulator locked to the same
   control current. */

class TriangleCoreVCO {
public:
    enum class Waveform : int { Sine = 0, Triangle = 1, Square = 2, Sawtooth = 3 };

    void prepare(double sampleRate);
    void reset();
    void retrigger(uint32_t hitSeed);

    void setFrequency(float hz)              { freq = hz; }
    void setWaveform(Waveform w)             { wave = w; }
    void setDetuneCents(float c)             { manualDetuneCents = c; }
    void setWobbleAmount(float cents)        { wobbleAmount = cents; }
    void setExternalDetuneCents(float c)     { externalDetuneCents = c; }

    float processSample();

    /* The ICL8038's sine output is *not* sin(); it's a piecewise-linear
       approximation with characteristic odd-harmonic THD. The static
       method below is exposed so unit tests (and a future panel
       "vintage / clean" toggle) can call it directly. */
    static float sineShaper(float triangleSample);

private:
    double sr = 44100.0;

    /* The triangle-core state. `phase` simulates the timing capacitor
       voltage swinging between -1 and +1; `rising` simulates the flip-
       flop that decides which of the two matched current sources is
       active. */
    double phase  = 0.0;
    bool   rising = true;

    /* Sawtooth uses its own phase since the 8038 has no saw output. */
    double sawPhase = 0.0;

    float    freq = 220.0f;
    Waveform wave = Waveform::Sine;

    /* Analog drift -- identical model to the previous DriftedVCO, kept
       so the audible "no two hits identical" behaviour carries over. */
    float perHitDetuneCents   = 0.0f;
    float slowWalkCents       = 0.0f;
    float manualDetuneCents   = 0.0f;
    float externalDetuneCents = 0.0f;
    float wobblePhase         = 0.0f;
    float wobbleFreqHz        = 0.25f;
    float wobbleAmount        = 1.5f;

    std::mt19937 rng { 0xC0FFEEu };

    static float polyBLEP(double t, double dt);
};
