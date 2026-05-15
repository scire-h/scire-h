#include "Voice.h"
#include <cmath>
#include <random>

namespace {
    constexpr float kSemitone = 1.0f / 12.0f;
    constexpr float kRefA3    = 220.0f;
}

Voice::Voice() = default;

void Voice::setChannelIndex(int idx) { channelIdx = idx; }

void Voice::prepare(double sr, int blockSize) {
    sampleRate = sr;
    vco.prepare(sr);
    noise.prepare(sr);
    noise.setCharacter(characterFor(channelIdx));

    /* MoogLadder doesn't need a JUCE ProcessSpec -- just sample rate. */
    (void)blockSize;
    ladder.prepare(sr);
    ladder.setCutoff(18000.0f);
    ladder.setResonance(0.3f);
    ladder.setDrive(1.0f);

    vca.prepare(sr);
    lfo.prepare(sr);
    lfo.setOutput(LFOSchmitt::Output::Triangle);

    ampEnv.prepare(sr);
    pitchEnv.prepare(sr);
    filterEnv.prepare(sr);
    reset();
}

void Voice::reset() {
    vco.reset();
    noise.reset();
    ladder.reset();
    vca.reset();
    lfo.reset();
    ampEnv.reset();
    pitchEnv.reset();
    filterEnv.reset();
}

NoiseVoice::Character Voice::characterFor(int chIdx) const {
    if (chIdx <= 1) return NoiseVoice::Character::Cymbal;
    if (chIdx == 2) return NoiseVoice::Character::Snare;
    return NoiseVoice::Character::Noise;
}

float Voice::computeBaseFreq(float tuning01, int octave, float beatTune01) {
    const float semis   = (tuning01 - 0.5f) * 24.0f;
    const float octShft = (float)(octave - 3) * 12.0f;
    const float cents   = beatTune01 * 200.0f;
    const float exp     = (semis + octShft + cents * 0.01f) * kSemitone;
    return kRefA3 * std::pow(2.0f, exp);
}

void Voice::updateParameters(const juce::AudioProcessorValueTreeState& apvts,
                             bool beatSense) {
    using namespace P;
    const auto p = chPrefix(channelIdx);

    pTuning   = apvts.getRawParameterValue(p + pid::vco)->load();
    pBeatTune = apvts.getRawParameterValue(p + pid::beatTune)->load();
    pOctave   = (int)apvts.getRawParameterValue(p + pid::octave)->load();
    pAttack   = apvts.getRawParameterValue(p + pid::attack)->load();
    pSustain  = apvts.getRawParameterValue(p + pid::sustain)->load();
    pWaveform = (int)apvts.getRawParameterValue(p + pid::waveform)->load();
    pNoiseOn  = apvts.getRawParameterValue(p + pid::noiseOn)->load() > 0.5f;
    pLfoRate  = apvts.getRawParameterValue(p + pid::lfoRate)->load();
    pLfoDepth = apvts.getRawParameterValue(p + pid::lfoDepth)->load();
    pLfoOn    = apvts.getRawParameterValue(p + pid::lfoOn)->load() > 0.5f;
    pSweep    = apvts.getRawParameterValue(p + pid::sweep)->load();
    pSweepDir = (int)apvts.getRawParameterValue(p + pid::sweepDir)->load();
    pOutput   = apvts.getRawParameterValue(p + pid::output)->load();
    pSense    = apvts.getRawParameterValue(p + pid::sense)->load();
    pMultiVCO = apvts.getRawParameterValue(p + pid::multiVCO)->load() > 0.5f;
    beatSenseGlobal = beatSense;

    /* LFO frequency: 0.8 .. 22 Hz, exponential taper. */
    const float lfoHz = 0.8f * std::pow(22.0f / 0.8f, pLfoRate);
    lfo.setFrequency(lfoHz);
}

void Voice::trigger(float velocity) {
    hitVelocity = juce::jlimit(0.0f, 1.0f, velocity);
    baseFreq = computeBaseFreq(pTuning, pOctave, pBeatTune);

    /* Pitch sweep extents (0 .. 6 oct WIDTH). */
    const float widthOct = 6.0f * pSweep;
    const float widthMul = std::pow(2.0f, widthOct);
    if (pSweepDir == 0)         { sweepFromHz = baseFreq;            sweepToHz = baseFreq * widthMul; }
    else if (pSweepDir == 2)    { sweepFromHz = baseFreq * widthMul; sweepToHz = baseFreq;            }
    else                        { sweepFromHz = sweepToHz = baseFreq; }
    sweepSeconds = 0.025f + std::pow(pSweep, 0.85f) * 0.25f;

    /* Envelope time constants. */
    const float attackSec = 0.001f + std::pow(pAttack, 1.4f) * 0.35f;
    const float decaySec  = 0.03f  + std::pow(pSustain, 1.4f) * 4.5f;

    /* Per-hit peak voltage: BEAT SENSE ON tracks velocity, OFF is fixed. */
    const float sense = beatSenseGlobal
                      ? juce::jmax(0.15f, hitVelocity)
                      : 1.0f;
    const float peak  = (0.05f + pOutput * 0.95f)
                      * (0.25f + pSense * 0.75f)
                      * sense;

    ampEnv.trigger   (0.0f, peak, 0.0f, attackSec, decaySec);
    pitchEnv.trigger (1.0f, 1.0f, 0.0f, 0.001f, sweepSeconds);
    filterEnv.trigger(1.0f, 1.0f, 0.0f, 0.001f, decaySec * 0.9f);

    /* Reseed the VCO's analog drift -- new per-hit detune
       offset + slow random walk increment. */
    std::random_device rd;
    vco.setWaveform((TriangleCoreVCO::Waveform)pWaveform);
    vco.setDetuneCents(0.0f);
    vco.retrigger(rd());

    if (pNoiseOn) {
        noise.setCharacter(characterFor(channelIdx));
        float centerLo = 0.0f, centerHi = 0.0f;
        switch (characterFor(channelIdx)) {
            case NoiseVoice::Character::Cymbal: centerLo = 2500.0f; centerHi = 9000.0f; break;
            case NoiseVoice::Character::Snare:  centerLo =  600.0f; centerHi = 4200.0f; break;
            case NoiseVoice::Character::Noise:  centerLo =  140.0f; centerHi = 2200.0f; break;
        }
        const float c  = centerLo + (centerHi - centerLo) * pTuning;
        const float wm = std::pow(2.0f, widthOct * 0.5f);
        float f0 = c, f1 = c;
        if (pSweepDir == 0)      { f0 = c;       f1 = c * wm;  }
        else if (pSweepDir == 2) { f0 = c * wm;  f1 = c;       }
        noise.startSweep(f0, f1, sweepSeconds);
    }
}

void Voice::processBlock(float* outL, float* outR, int numSamples) {
    if (!ampEnv.isActive()) return;

    /* per-sample LFO output (Hz already set in updateParameters). */
    const float lfoCents = pLfoOn ? 20.0f + pLfoDepth * 180.0f : 0.0f;

    const float fClose = 220.0f + 1400.0f * pSustain;
    const float fOpen  = 18000.0f;
    const float reso   = 0.05f + pSweep * 0.55f;
    ladder.setResonance(reso);

    /* per-sample loop */
    for (int n = 0; n < numSamples; ++n) {
        const float aE = ampEnv.processSample();
        const float pE = pitchEnv.processSample();
        const float fE = filterEnv.processSample();

        /* Pitch -- exponential sweep from sweepFromHz to sweepToHz. */
        const float currHz = sweepToHz *
            std::pow(sweepFromHz / sweepToHz, pE);

        /* LFO -> detune cents. */
        const float lfoVal = lfoCents * lfo.processSample();

        vco.setFrequency(currHz);
        vco.setExternalDetuneCents(lfoVal);
        float vcoSum = vco.processSample();

        /* Per-waveform attenuation to keep mix at sensible level. */
        const float oscMix = (pWaveform == 0) ? 1.0f
                           : (pWaveform == 1) ? 0.85f
                           : (pWaveform == 2) ? 0.45f
                                              : 0.55f;
        vcoSum *= oscMix;

        float noiseSig = 0.0f;
        if (pNoiseOn) noiseSig = noise.processSample() * 0.55f;

        /* VCF: lowpass cutoff modulated by filter env. */
        const float cutoff = fClose + (fOpen - fClose) * fE;
        ladder.setCutoff(cutoff);
        float mixed = vcoSum + noiseSig;
        mixed = ladder.processSample(mixed);

        /* OTAVCA: envelope sets I_abc, audio passes through tanh. */
        vca.setControlGain(aE);
        const float y = vca.processSample(mixed);

        outL[n] += y;
        if (outR) outR[n] += y;
    }
}
