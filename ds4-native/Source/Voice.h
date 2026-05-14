#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "TriangleCoreVCO.h"
#include "NoiseVoice.h"
#include "ExpEnvelope.h"
#include "OTAVCA.h"
#include "LFOSchmitt.h"
#include "Parameters.h"

/* One DS-4M channel.

   Signal flow (Phase 2):

       VCO1 ─┐                                              (per-channel)
       VCO2 ─┤  (multi-VCO PULL triggers neighbour
       VCO3 ─┤   channel via the cascade router instead)
       VCOn ─┼─► Mix ─► LadderLPF24 ─► OTAVCA ─► Stereo Out
       Noise ┘            ▲                ▲
                          │                │
                      Filter EG          Amp EG
                                         (RC discharge)

   The pitch sweep and amp envelope are dedicated RC-style
   ExpEnvelopes; the filter envelope is derived from the amp env
   (cutoff opens at hit, closes as the note dies). The VCA is now an
   OTA tanh model (Phase 2.2) and the LFO is an op-amp Schmitt
   relaxation oscillator (Phase 2.5). */

class Voice {
public:
    Voice();

    void setChannelIndex(int idx);
    void prepare(double sampleRate, int blockSize);
    void reset();

    /* Pull live parameter values from the APVTS once per block. */
    void updateParameters(const juce::AudioProcessorValueTreeState& apvts,
                          bool beatSenseGlobal);

    /* Trigger a hit. Velocity 0..1. */
    void trigger(float velocity);

    /* Render the voice into the stereo output buffer (additive). */
    void processBlock(float* outL, float* outR, int numSamples);

    bool isActive() const { return ampEnv.isActive(); }

    /* For the MULTI VCO cascade router (Phase 2.6): when an upstream
       channel's PULL is engaged, fire this channel as well. */
    bool getMultiVCO() const { return pMultiVCO; }

private:
    int     channelIdx  = 0;
    double  sampleRate  = 44100.0;

    /* The DS-4 has ONE VCO per channel. MULTI VCO is a cascade-
       trigger feature handled in PluginProcessor::triggerChannel,
       not a per-voice unison. */
    TriangleCoreVCO vco;
    NoiseVoice noise;

    juce::dsp::LadderFilter<float> ladder;
    OTAVCA   vca;
    LFOSchmitt lfo;

    /* Envelopes. */
    ExpEnvelope ampEnv;
    ExpEnvelope pitchEnv;
    ExpEnvelope filterEnv;

    /* Cached parameter snapshot (refreshed per block from APVTS). */
    float pTuning   = 0.5f;
    float pBeatTune = 0.0f;
    int   pOctave   = 3;
    float pAttack   = 0.0f;
    float pSustain  = 0.3f;
    int   pWaveform = 0;
    bool  pNoiseOn  = true;
    float pLfoRate  = 0.3f;
    float pLfoDepth = 0.5f;
    bool  pLfoOn    = false;
    float pSweep    = 0.5f;
    int   pSweepDir = 2;     // 0:up 1:off 2:down
    float pOutput   = 0.8f;
    float pSense    = 0.7f;
    bool  pMultiVCO = false;
    bool  beatSenseGlobal = false;

    /* Trigger-time state. */
    float hitVelocity   = 1.0f;
    float baseFreq      = 220.0f;
    float sweepFromHz   = 220.0f;
    float sweepToHz     = 220.0f;
    float sweepSeconds  = 0.1f;

    /* Map ch.label to NoiseVoice character. */
    NoiseVoice::Character characterFor(int chIdx) const;

    /* C..B note ring + octave selector + beat tune cents -> Hz. */
    static float computeBaseFreq(float tuning01, int octave, float beatTune01);
};
