#pragma once

/* Phase 2.4 -- piezo trigger conditioner.

   On the real DS-4, each channel has a transistor envelope follower
   (peak detector with fast attack / slow release) feeding a CD4093
   Schmitt-trigger inverter. The Schmitt's hysteresis (V_hi vs V_lo)
   is what prevents the pad from re-firing during the ringing tail of
   a hard strike.

   Output:
     - returns the detected peak (velocity 0..1) on the *single sample*
       where the upper threshold is crossed -- exactly like a piezo
       triggering a transistor switch
     - returns 0 on every other sample
   plus a tracking peak the caller can query (used to drive the
   trigger LED). */

class PiezoTrigger {
public:
    void prepare(double sampleRate);
    void reset();

    void setThresholds(float hi, float lo) { vHi = hi; vLo = lo; }
    void setAttackMs (float ms);   /* envelope-follower attack  */
    void setReleaseMs(float ms);   /* envelope-follower release */
    void setHoldMs   (float ms);   /* min retrigger interval    */

    /* Returns the peak value of the triggering edge (0..1) at the
       sample the Schmitt arms, else 0.f. */
    float processSample(float x);

    float currentPeak() const { return peak; }

private:
    double sr  = 44100.0;
    float peak = 0.0f;

    float attackCoeff  = 0.0f;
    float releaseCoeff = 0.0f;

    float vHi = 0.25f, vLo = 0.05f;
    bool  armed       = true;
    int   holdRemain  = 0;
    int   holdSamples = 0;
};
