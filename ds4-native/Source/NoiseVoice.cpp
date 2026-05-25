#include "NoiseVoice.h"
#include <cmath>
#include <algorithm>

void NoiseVoice::prepare(double sampleRate) {
    sr = sampleRate;
    bp.prepare(sampleRate);
    peakBp.prepare(sampleRate);
    /* Default modes; updateQs() re-selects per character. CYMBAL needs
       LP because its measured spectrum is flat across 100..10000 Hz
       with a sharp roll-off above (broadband + LPF), not band-passed. */
    bp.setMode(TPTSvf::Output::BP);
    peakBp.setMode(TPTSvf::Output::BP);
    reset();
    updateQs();
}

void NoiseVoice::reset() {
    bp.reset();
    peakBp.reset();
    currentFreq  = 1000.0f;
    endFreq      = 1000.0f;
    perSampleMul = 1.0f;
    sweepRemain  = 0;
}

void NoiseVoice::updateQs() {
    /* Per-character voicing based on direct spectral measurements of
       the reference drum-synth software (LFO off, SWEEP off, hit at
       default settings):

       CYMBAL: averaged 1/3-octave spectrum 50-400 ms after the hit
               is FLAT within +/- 1 dB from 100 Hz up to ~10 kHz,
               then drops 22 dB at 10-14 kHz and 40 dB at 14-20 kHz.
               It's broadband white noise lowpassed near 10 kHz --
               NOT a narrow band-pass with peaking emphasis as we
               had it before. So Q is low and the peaking emphasis
               adds essentially nothing.

       SNARE and NOISE: not yet measured against the reference. Kept
       at moderate / wide settings; will refine as data comes in. */
    float bpQ = 0.7f, peakQ = 1.0f, peakMul = 1.0f, boost = 0.0f;
    switch (character) {
        case Character::Cymbal:
            /* Broadband white noise -> LP at ~10 kHz. Peaking BP off. */
            bp.setMode    (TPTSvf::Output::LP);
            peakBp.setMode(TPTSvf::Output::LP);
            bpQ = 0.7f; peakQ = 0.7f; peakMul = 1.0f; boost = 0.0f;
            break;
        case Character::Snare:
            /* Mid-band-passed noise with a body resonance. */
            bp.setMode    (TPTSvf::Output::BP);
            peakBp.setMode(TPTSvf::Output::BP);
            bpQ = 1.8f; peakQ = 2.4f; peakMul = 1.3f; boost = 0.7f;
            break;
        case Character::Noise:
            /* Broadband -- almost no shaping. */
            bp.setMode    (TPTSvf::Output::LP);
            peakBp.setMode(TPTSvf::Output::BP);
            bpQ = 0.7f; peakQ = 1.0f; peakMul = 1.0f; boost = 0.1f;
            break;
    }
    bp.setQ(bpQ);
    peakBp.setQ(peakQ);
    peakOffsetMul = peakMul;
    peakBoost     = boost;
}

void NoiseVoice::startSweep(float startHz, float endHz_, float seconds) {
    currentFreq = std::max(40.0f, startHz);
    endFreq     = std::max(40.0f, endHz_);
    const int n = std::max(1, (int)(seconds * sr));
    sweepRemain = n;
    /* exponential interpolation: constant frequency ratio per sample */
    perSampleMul = (float)std::pow((double)(endFreq / currentFreq), 1.0 / (double)n);
    bp.setCutoff(currentFreq);
    peakBp.setCutoff(currentFreq * peakOffsetMul);
}

float NoiseVoice::processSample() {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    const float n = dist(rng);

    /* Main band-pass + peaking emphasis added in parallel. */
    const float y = bp.processSample(n) + peakBoost * peakBp.processSample(n);

    if (sweepRemain > 0) {
        currentFreq *= perSampleMul;
        /* Update coefficients every 4 samples is plenty -- the filter
           cutoff smooths it naturally and tan() is the only real cost. */
        if ((sweepRemain & 3) == 0) {
            bp.setCutoff(currentFreq);
            peakBp.setCutoff(currentFreq * peakOffsetMul);
        }
        --sweepRemain;
    }
    return y;
}
