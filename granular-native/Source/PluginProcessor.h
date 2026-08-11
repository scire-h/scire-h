#pragma once
// GRANULAR — 5-track granular synth. v0.1 native shell:
// each track free-runs its granulator (web app's "drone" mode); the
// sequencer + evolver modules are compiled and unit-tested but pattern UI
// wiring is a later phase (see docs/PORT_PLAN.md).

#include <juce_audio_processors/juce_audio_processors.h>
#include "GrainEngine.h"
#include "StepSequencer.h"
#include "Evolver.h"
#include "Parameters.h"

class GranularAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kNumTracks = 5;

    GranularAudioProcessor();

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override               { return true; }

    const juce::String getName() const override   { return "GRANULAR"; }
    bool acceptsMidi() const override             { return true; }
    bool producesMidi() const override            { return false; }
    double getTailLengthSeconds() const override  { return 0.5; }

    int getNumPrograms() override                 { return 1; }
    int getCurrentProgram() override              { return 0; }
    void setCurrentProgram (int) override         {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void regenerateBufferIfNeeded (int trackIndex);
    void pullParamsInto (int trackIndex);

    granular::GrainEngine    engines   [kNumTracks];
    granular::StepSequencer  sequencers[kNumTracks];
    granular::Evolver        evolvers  [kNumTracks];

    // cache of (source, freq) used to build each engine's buffer, so we only
    // re-synthesise when the user actually changes them
    int    builtSource[kNumTracks] { -1, -1, -1, -1, -1 };
    double builtFreq  [kNumTracks] { 0, 0, 0, 0, 0 };

    juce::AudioBuffer<float> trackBus;
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranularAudioProcessor)
};
