#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "Voice.h"
#include "PiezoTrigger.h"
#include "Parameters.h"

class DS4MProcessor : public juce::AudioProcessor {
public:
    DS4MProcessor();
    ~DS4MProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool   acceptsMidi()        const override { return true;  }
    bool   producesMidi()       const override { return false; }
    bool   isMidiEffect()       const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    /* Called from the editor when the user clicks a pad. */
    void triggerChannel (int chIndex, float velocity = 1.0f);

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    std::array<Voice, P::kNumChannels> voices;
    std::array<PiezoTrigger, P::kNumChannels> piezos;
    double currentSampleRate = 44100.0;

    /* Trigger requests come in from the editor on the message thread
       and need to land on the next audio block. We buffer them. */
    struct PendingTrigger { int ch; float vel; };
    juce::AbstractFifo triggerFifo { 32 };
    std::array<PendingTrigger, 32> triggerBuffer {};

    /* Phase 2.6 -- when a channel's MULTI VCO PULL is engaged its
       trigger also fires the *next* channel. We use this internal
       helper to avoid re-entrant FIFO writes from the audio thread. */
    void fireChannel (int chIndex, float velocity, bool followCascade);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DS4MProcessor)
};
