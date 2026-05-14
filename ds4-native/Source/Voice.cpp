#include "Voice.h"
#include <cmath>
#include <random>

namespace {
    constexpr float kSemitone   = 1.0f / 12.0f;
    constexpr float kRefA3      = 220.0f;
    /* Tunings in cents for the three detuned multi-vco siblings.
       7 cents is just under a syntonic comma -- enough to fatten
       without sounding overtly out of tune. */
    constexpr float kMultiVoiceCents[3] = { +7.0f, -7.0f, +0.0f };
}

Voice::Voice() = default;

void Voice::setChannelIndex(int idx) {
    channelIdx = idx;
}

void Voice::prepare(double sr, int blockSize) {
    sampleRate = sr;
    for (auto& v : vco) v.prepare(sr);
    noise.prepare(sr);
    noise.setCharacter(characterFor(channelIdx));

    juce::dsp::ProcessSpec spec { sr, (juce::uint32)blockSize, 1 };
    ladder.prepare(spec);
    ladder.setMode(juce::dsp::LadderFilterMode::LPF24);
    ladder.setCutoffFrequencyHz(18000.0f);
    ladder.setResonance(0.3f);
    ladder.setDrive(1.0f);

    ampEnv.prepare(sr);
    pitchEnv.prepare(sr);
    filterEnv.prepare(sr);
    reset();
}

void Voice::reset() {
    for (auto& v : vco) v.reset();
    noise.reset();
    ladder.reset();
    ampEnv.reset();
    pitchEnv.reset();
    filterEnv.reset();
    lfoPhase = 0.0f;
}

NoiseVoice::Character Voice::characterFor(int chIdx) const {
    /* Match the silk-screen pills: Ch1 + Ch2 = CYMBAL, Ch3 = SNARE,
       Ch4 = NOISE. */
    if (chIdx <= 1) return NoiseVoice::Character::Cymbal;
    if (chIdx == 2) return NoiseVoice::Character::Snare;
    return NoiseVoice::Character::Noise;
}

float Voice::computeBaseFreq(float tuning01, int octave, float beatTune01) {
    /* Knob covers two octaves around A3, beatTune is 0..200 cents up,
       OCTAVE selector is 1..5 with 3 = unity. */
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
}

void Voice::trigger(float velocity) {
    hitVelocity = juce::jlimit(0.0f, 1.0f, velocity);

    baseFreq = computeBaseFreq(pTuning, pOctave, pBeatTune);

    /* Pitch sweep parameters. */
    const float widthOct = 6.0f * pSweep;
    const float widthMul = std::pow(2.0f, widthOct);
    if (pSweepDir == 0) {            /* up */
        sweepFromHz = baseFreq;
        sweepToHz   = baseFreq * widthMul;
    } else if (pSweepDir == 2) {     /* down */
        sweepFromHz = baseFreq * widthMul;
        sweepToHz   = baseFreq;
    } else {                          /* off */
        sweepFromHz = sweepToHz = baseFreq;
    }
    sweepSeconds = 0.025f + std::pow(pSweep, 0.85f) * 0.25f;

    /* Trigger envelopes. */
    const float attackSec = 0.001f + std::pow(pAttack, 1.4f) * 0.35f;
    const float decaySec  = 0.03f  + std::pow(pSustain, 1.4f) * 4.5f;
    const float sense = beatSenseGlobal
                      ? juce::jmax(0.15f, hitVelocity)
                      : 1.0f;
    const float peak = (0.05f + pOutput * 0.95f)
                     * (0.25f + pSense * 0.75f)
                     * sense;
    ampEnv.trigger(0.0f, peak, 0.0f, attackSec, decaySec);

    /* Pitch envelope is a normalised RC sweep we'll scale to Hz inside
       the audio callback. We just need it to fall from 1 to 0 with the
       correct time constant. */
    pitchEnv.trigger(1.0f, 1.0f, 0.0f, 0.001f, sweepSeconds);

    /* Filter envelope: 1 = wide open, 0 = closed down to "resting" cutoff. */
    filterEnv.trigger(1.0f, 1.0f, 0.0f, 0.001f, decaySec * 0.9f);

    /* Retrigger VCOs (this regenerates the per-hit drift). */
    std::random_device rd;
    const uint32_t baseSeed = rd();
    int idx = 0;
    vco[idx].setWaveform((DriftedVCO::Waveform)pWaveform);
    vco[idx].setDetuneCents(0.0f);
    vco[idx].retrigger(baseSeed);
    ++idx;
    if (pMultiVCO) {
        for (int k = 0; k < 3; ++k, ++idx) {
            vco[idx].setWaveform((DriftedVCO::Waveform)pWaveform);
            vco[idx].setDetuneCents(kMultiVoiceCents[k]);
            vco[idx].retrigger(baseSeed ^ (uint32_t)(0xDEADBEEFu * (k + 1)));
        }
    }

    /* Tell the noise voice where to sweep the filter centre. */
    if (pNoiseOn) {
        const auto chr   = characterFor(channelIdx);
        noise.setCharacter(chr);
        /* Pick centre frequencies per character, scaled by VCO knob. */
        float centerLo = 0.0f, centerHi = 0.0f;
        switch (chr) {
            case NoiseVoice::Character::Cymbal: centerLo = 2500.0f; centerHi = 9000.0f; break;
            case NoiseVoice::Character::Snare:  centerLo =  600.0f; centerHi = 4200.0f; break;
            case NoiseVoice::Character::Noise:  centerLo =  140.0f; centerHi = 2200.0f; break;
        }
        const float c = centerLo + (centerHi - centerLo) * pTuning;
        const float widthMul2 = std::pow(2.0f, widthOct * 0.5f);
        float f0 = c, f1 = c;
        if (pSweepDir == 0)       { f0 = c; f1 = c * widthMul2; }
        else if (pSweepDir == 2)  { f0 = c * widthMul2; f1 = c; }
        noise.startSweep(f0, f1, sweepSeconds);
    }
}

void Voice::processBlock(float* outL, float* outR, int numSamples) {
    if (!ampEnv.isActive()) return;

    const float oscMix = (pWaveform == 0) ? 1.0f        /* sine */
                       : (pWaveform == 1) ? 0.85f       /* triangle */
                       : (pWaveform == 2) ? 0.45f       /* square */
                                          : 0.55f;       /* sawtooth */
    const int activeVCOs = pMultiVCO ? 4 : 1;
    const float vcoNorm  = 1.0f / std::sqrt((float)activeVCOs);

    const float lfoRateHz = 0.8f + pLfoRate * 22.0f;
    const float lfoCents  = pLfoOn ? 20.0f + pLfoDepth * 180.0f : 0.0f;

    /* Pre-compute filter envelope target floor (the "closed" cutoff). */
    const float fClose = 220.0f + 1400.0f * pSustain;
    const float fOpen  = 18000.0f;
    const float reso   = 0.05f + pSweep * 0.55f;
    ladder.setResonance(reso);

    for (int n = 0; n < numSamples; ++n) {
        /* 1) Envelopes. */
        const float aE = ampEnv.processSample();
        const float pE = pitchEnv.processSample();
        const float fE = filterEnv.processSample();

        /* 2) Compute current pitch (Hz) via exponential sweep. */
        const float currHz = sweepToHz *
            std::pow(sweepFromHz / sweepToHz, pE);

        /* 3) LFO -> detune (cents). */
        lfoPhase += (float)(lfoRateHz / sampleRate);
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        const float lfoVal = lfoCents *
                             std::sin(6.28318530718f * lfoPhase);

        /* 4) Sum oscillators. */
        float vcoSum = 0.0f;
        for (int i = 0; i < activeVCOs; ++i) {
            vco[i].setFrequency(currHz);
            vco[i].setExternalDetuneCents(lfoVal);
            vcoSum += vco[i].processSample();
        }
        vcoSum *= vcoNorm * oscMix;

        /* 5) Noise. */
        float noiseSig = 0.0f;
        if (pNoiseOn) noiseSig = noise.processSample() * 0.6f;

        /* 6) Filter (cutoff modulated by filter env). */
        const float cutoff = fClose + (fOpen - fClose) * fE;
        ladder.setCutoffFrequencyHz(cutoff);
        float mixed = vcoSum + noiseSig;
        /* JUCE LadderFilter wants a block API, but we can call
           processSample for a one-shot sample. */
        mixed = ladder.processSample(mixed, 0);

        /* 7) VCA + soft saturation. */
        float y = softSat(mixed * aE * 1.4f);

        /* Sum into stereo bus (mono routing). */
        outL[n] += y;
        if (outR) outR[n] += y;
    }
}
