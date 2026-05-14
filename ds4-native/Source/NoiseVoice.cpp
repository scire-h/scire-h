#include "NoiseVoice.h"
#include <cmath>
#include <algorithm>

void NoiseVoice::prepare(double sampleRate) {
    sr = sampleRate;
    bp.prepare(sampleRate);
    peakBp.prepare(sampleRate);
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
    /* Per-character voicing -- centre Q and the peaking emphasis. */
    float bpQ = 1.0f, peakQ = 1.0f, peakMul = 1.4f, boost = 0.7f;
    switch (character) {
        case Character::Cymbal: bpQ = 6.0f; peakQ = 6.0f; peakMul = 1.4f; boost = 1.4f; break;
        case Character::Snare:  bpQ = 2.4f; peakQ = 3.0f; peakMul = 1.3f; boost = 1.0f; break;
        case Character::Noise:  bpQ = 0.9f; peakQ = 1.5f; peakMul = 1.0f; boost = 0.4f; break;
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
