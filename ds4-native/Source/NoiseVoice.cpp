#include "NoiseVoice.h"
#include <cmath>

void NoiseVoice::prepare(double sampleRate) {
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, 32, 1 };
    bandpass.prepare(spec);
    peaking.prepare(spec);
    reset();
}

void NoiseVoice::reset() {
    bandpass.reset();
    peaking.reset();
    currentFreq = 1000.0f;
    endFreq     = 1000.0f;
    perSampleMul = 1.0f;
    sweepRemain = 0;
}

void NoiseVoice::startSweep(float startHz, float endHz_, float seconds) {
    currentFreq = std::max(40.0f, startHz);
    endFreq     = std::max(40.0f, endHz_);
    const int   n = std::max(1, (int)(seconds * sr));
    sweepRemain  = n;
    /* Exponential interpolation -- a constant ratio per sample. */
    perSampleMul = (float)std::pow((double)(endFreq / currentFreq), 1.0 / (double)n);
    updateCoefficients();
}

void NoiseVoice::updateCoefficients() {
    float q       = 1.0f;
    float peakDb  = 0.0f;
    float peakQ   = 1.0f;
    float peakMul = 1.0f;
    switch (character) {
        case Character::Cymbal:
            q = 6.0f;  peakDb = 14.0f; peakQ = 6.0f; peakMul = 1.4f; break;
        case Character::Snare:
            q = 2.4f;  peakDb = 9.0f;  peakQ = 3.0f; peakMul = 1.3f; break;
        case Character::Noise:
            q = 0.9f;  peakDb = 4.0f;  peakQ = 1.5f; peakMul = 1.0f; break;
    }
    const double fb = (double)currentFreq;
    const double fp = (double)(currentFreq * peakMul);
    *bandpass.coefficients = *juce::dsp::IIR::Coefficients<float>
                              ::makeBandPass(sr, fb, q);
    *peaking.coefficients  = *juce::dsp::IIR::Coefficients<float>
                              ::makePeakFilter(sr, fp, peakQ,
                                               juce::Decibels::decibelsToGain(peakDb));
}

float NoiseVoice::processSample() {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    float x = dist(rng);
    x = bandpass.processSample(x);
    x = peaking.processSample(x);

    if (sweepRemain > 0) {
        currentFreq *= perSampleMul;
        --sweepRemain;
        /* Avoid the (somewhat) expensive coefficient rebuild every
           sample by only refreshing every 32 samples. */
        if ((sweepRemain & 31) == 0) updateCoefficients();
    }
    return x;
}
