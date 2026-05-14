#pragma once
#include <cstdint>
#include <random>

/* Anti-aliased oscillator with per-hit + slow random analog drift.

   Uses PolyBLEP (Polynomial Band-Limited Step) on square and sawtooth
   discontinuities to suppress aliasing. Sine and triangle are derived
   without discontinuities so they're alias-free at any pitch.

   The "analog drift" model has two layers:
     - Per-hit detune (uniform random, +/-3 cents, regenerated at trigger)
     - Slow random walk between hits (-3..+3 cents), with a 0.15-0.4 Hz
       continuous wobble of +/-1.5 cents on top.

   This matches the natural pitch instability of analog VCOs whose
   reference voltage isn't perfectly stable. */

class DriftedVCO {
public:
    enum class Waveform : int { Sine = 0, Triangle = 1, Square = 2, Sawtooth = 3 };

    void prepare(double sr);
    void reset();
    void retrigger(uint32_t hitSeed);

    void setFrequency(float hz)           { freq      = hz; }
    void setWaveform (Waveform w)         { wave      = w;  }
    void setDetuneCents(float c)          { manualDetuneCents = c; }
    void setWobbleAmount(float cents)     { wobbleAmount = cents; }

    /* One sample of audio. The caller is responsible for any external
       modulation -- this just runs the oscillator core. */
    float processSample();

    /* Add additional cents to the pitch this sample only (e.g. LFO,
       pitch sweep). */
    void setExternalDetuneCents(float c)  { externalDetuneCents = c; }

private:
    double sr     = 44100.0;
    double phase  = 0.0;
    float  freq   = 220.0f;

    Waveform wave = Waveform::Sine;

    /* Drift state. */
    float perHitDetuneCents     = 0.0f;
    float slowWalkCents         = 0.0f;
    float manualDetuneCents     = 0.0f;
    float externalDetuneCents   = 0.0f;
    float wobblePhase           = 0.0f;
    float wobbleFreqHz          = 0.25f;
    float wobbleAmount          = 1.5f;

    /* Triangle leaky integrator state (for the integration of a square). */
    float triState = 0.0f;

    std::mt19937 rng { 0xC0FFEEu };

    static float polyBLEP(double t, double dt);
};
