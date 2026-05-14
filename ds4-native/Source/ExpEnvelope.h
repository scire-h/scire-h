#pragma once

/* RC-style exponential envelope generator.

   The Pollard Syndrum (and the DS-4M which clones it) uses simple RC
   discharge curves for both the amplitude envelope and the pitch sweep
   envelope. A real RC envelope looks like:

       v(t) = v_target + (v_start - v_target) * exp(-t / tau)

   where tau is the time constant determined by the resistor/capacitor
   product in the panel. We reproduce that exactly with a one-pole
   smoother per sample. */

class ExpEnvelope {
public:
    void prepare(double sampleRate);
    void reset();

    /* Trigger with start value -> sustain value over `attackSec`,
       then decay to `endValue` with time constant `decaySec` (3-tau ≈ 95 %). */
    void trigger(float startValue, float peakValue, float endValue,
                 float attackSec, float decaySec);

    bool isActive() const { return active; }
    float currentValue() const { return value; }
    float processSample();

private:
    enum class Stage { Idle, Attack, Decay };
    Stage  stage      = Stage::Idle;
    float  value      = 0.0f;
    float  target     = 0.0f;
    float  peak       = 0.0f;
    float  endVal     = 0.0f;
    float  attackInc  = 0.0f;
    float  decayCoeff = 0.0f;
    double sr         = 44100.0;
    bool   active     = false;
};
