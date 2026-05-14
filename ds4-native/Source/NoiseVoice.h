#pragma once
#include <random>
#include <juce_dsp/juce_dsp.h>

/* Per-channel noise voice -- band-passed white noise with a resonant
   peaking emphasis that gives CYMBAL its metallic shimmer, SNARE its
   buzz and NOISE its broadband hash. The voicing table is the
   same one used by the HTML clone and tuned by ear to the published
   sound clips of the real Toyo Gakki / ULT-SOUND DS-4M. */

class NoiseVoice {
public:
    enum class Character { Cymbal, Snare, Noise };

    void prepare(double sr);
    void reset();
    void setCharacter(Character c) { character = c; }

    /* Sweep the centre frequency from `startHz` to `endHz`
       across `seconds`. Subsequent processSample() calls walk the
       ramp automatically. */
    void startSweep(float startHz, float endHz, float seconds);

    /* One stereo-ish sample (mono in/out). */
    float processSample();

private:
    Character character = Character::Cymbal;
    double sr           = 44100.0;
    std::mt19937 rng { 0xA1A1A1A1u };

    juce::dsp::IIR::Filter<float> bandpass;
    juce::dsp::IIR::Filter<float> peaking;

    float currentFreq = 1000.0f;
    float endFreq     = 1000.0f;
    float perSampleMul = 1.0f;
    int   sweepRemain = 0;

    void updateCoefficients();
};
