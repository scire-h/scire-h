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
    /* The panel knob's silk-screen ring runs C, C#, D, D#, E, F, F#,
       G, G#, A, A#, B around its rotation -- one full octave end to
       end. So TUNING is _not_ a +/-12 semitone selector; it just
       picks the note within an octave. The OCTAVE 1..5 selector
       below it picks WHICH octave. */
    const float noteSemis = juce::jlimit (0.0f, 12.0f, tuning01 * 12.0f);

    /* OCTAVE 1..5 where 3 sits at middle C (MIDI 48 = 130.81 Hz). */
    const float midiC3 = 48.0f;
    const float octShift = (float) (octave - 3) * 12.0f;

    /* BEAT TUNE silk-screen says 0..5 with an OCT unit label. We
       respect the panel literally: 0 = no detune, 5 = +5 octaves.
       Five octaves is a lot but it's what the original device's
       fader markings claim, and the MULTI VCO PULL feature lets
       you blend the detuned neighbour back in as a chord rather
       than a beat. */
    const float beatSemis = beatTune01 * 60.0f;

    const float midi = midiC3 + noteSemis + octShift + beatSemis;
    return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f);
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
    /* Sweep time on a real analog drum synth is set by an RC time
       constant on the pitch-envelope generator (tau = R*C). It is
       FIXED -- the SWEEP WIDTH slider only changes the starting
       voltage / starting frequency, not the duration. Picking
       ~45 ms gives the classic 'PEW!' you hear on Pollard records;
       anything longer starts to sound like a synth glide. */
    sweepSeconds = 0.045f;

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
            /* CYMBAL noise is LP'd at ~10 kHz on the reference; TUNING
               only nudges the cutoff a bit. NOISE is also broadband.
               SNARE keeps a true band-pass for the snare body
               resonance. */
            case NoiseVoice::Character::Cymbal: centerLo = 8000.0f; centerHi = 12000.0f; break;
            case NoiseVoice::Character::Snare:  centerLo =  600.0f; centerHi =  4200.0f; break;
            case NoiseVoice::Character::Noise:  centerLo = 3000.0f; centerHi = 10000.0f; break;
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

    /* Source (was a 4-way sine/tri/sqr/saw selector + separate noise
       toggle; reference measurements show the original device is a
       3-way mutually-exclusive radio: 0 = Cymbal, 1 = Pulse, 2 = Sin).
       We map to the VCO waveform once per block and pick whether to
       run the noise path or the VCO path. */
    const bool useCymbal = (pWaveform == 0);
    const bool usePulse  = (pWaveform == 1);
    const bool useSine   = (pWaveform == 2) || (!useCymbal && !usePulse);
    if      (usePulse) vco.setWaveform(TriangleCoreVCO::Waveform::Square);
    else if (useSine)  vco.setWaveform(TriangleCoreVCO::Waveform::Sine);

    /* per-sample loop */
    for (int n = 0; n < numSamples; ++n) {
        const float aE = ampEnv.processSample();
        const float pE = pitchEnv.processSample();
        (void) filterEnv.processSample();

        /* Pitch -- exponential sweep from sweepFromHz to sweepToHz. */
        const float currHz = sweepToHz *
            std::pow(sweepFromHz / sweepToHz, pE);

        /* LFO -> detune cents. */
        const float lfoVal = lfoCents * lfo.processSample();

        float source = 0.0f;
        if (useCymbal) {
            /* Cymbal mode = noise voice only. The noise voice's
               band-pass (per CYMBAL / SNARE / NOISE channel role) is
               the only thing on the signal path -- the VCO is silent
               in this mode, matching the reference. */
            source = noise.processSample();
        } else {
            /* Pulse or Sin -- pure VCO, no noise mix. */
            vco.setFrequency(currHz);
            vco.setExternalDetuneCents(lfoVal);
            source = vco.processSample();
            /* Slightly attenuate the square so it doesn't tower over
               the sine when the user A/Bs between waveforms. */
            if (usePulse) source *= 0.55f;
        }

        /* OTAVCA: envelope sets I_abc, audio passes through tanh. */
        vca.setControlGain(aE);
        const float y = vca.processSample(source);

        outL[n] += y;
        if (outR) outR[n] += y;
    }
}
