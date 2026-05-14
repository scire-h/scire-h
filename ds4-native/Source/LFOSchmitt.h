#pragma once

/* Phase 2.5 -- relaxation-oscillator LFO.

   The classic op-amp LFO of the era is a Schmitt trigger + RC
   integrator pair. The integrator output is a triangle, the Schmitt
   output is a square, and they share the same fundamental. The
   triangle output is what the DS-4 routes to the VCO CV through the
   LEVEL pot.

   Topology:

                          R                C
                       ┌──/\/\──┬───────╫├──── triangle ──►
                       │        │
                       │        ●─────────►   to comparator
                       │                          │
                       └──────────────────────────┘
                                  (Schmitt: triangle hits
                                   threshold -> square flips ->
                                   integrator slope reverses)

   In DSP we do exactly the same: track triangle phase and a
   rising/falling boolean. Identical architecture to TriangleCoreVCO
   but at much lower frequencies, exposed for the user-controlled LFO
   panel section. */

class LFOSchmitt {
public:
    enum class Output { Triangle, Square, Sine };

    void prepare(double sampleRate);
    void reset();

    void  setFrequency(float hz)     { freq = hz; }
    void  setOutput(Output o)        { mode = o; }
    void  setAmplitude(float a)      { amp = a; }

    float processSample();

private:
    double sr   = 44100.0;
    float  freq = 1.0f;
    float  amp  = 1.0f;
    Output mode = Output::Triangle;

    /* Same state model as TriangleCoreVCO: capacitor voltage in
       [-1, +1] and the flip-flop polarity boolean. */
    float phase  = 0.0f;
    bool  rising = true;
};
