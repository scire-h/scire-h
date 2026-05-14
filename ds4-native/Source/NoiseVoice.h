#pragma once
#include <random>
#include "TPTSvf.h"

/* Per-channel noise voice -- band-passed white noise with a resonant
   peaking emphasis that gives CYMBAL its metallic shimmer, SNARE its
   buzz and NOISE its broadband hash.

   Phase 2.3: now backed by TPTSvf instead of juce::dsp::IIR, which
   removes the per-sweep heap allocation and the coefficient-update
   cost the IIR pair was paying. The peaking emphasis is implemented
   as a second TPT-SVF in BP mode summed back into the signal with a
   linear gain. */

class NoiseVoice {
public:
    enum class Character { Cymbal, Snare, Noise };

    void prepare(double sampleRate);
    void reset();
    void setCharacter(Character c) { character = c; updateQs(); }

    /* Sweep centre frequency from startHz to endHz over `seconds`. */
    void startSweep(float startHz, float endHz, float seconds);

    float processSample();

private:
    Character character = Character::Cymbal;
    double sr           = 44100.0;
    std::mt19937 rng { 0xA1A1A1A1u };

    TPTSvf bp;
    TPTSvf peakBp;

    /* Sweep state -- exponential interpolation. */
    float currentFreq  = 1000.0f;
    float endFreq      = 1000.0f;
    float perSampleMul = 1.0f;
    int   sweepRemain  = 0;

    /* Per-character voicing -- centre offset multiplier for the
       peaking BP and gains. */
    float peakOffsetMul = 1.4f;
    float peakBoost     = 0.7f;

    void updateQs();
};
