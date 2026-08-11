#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace granular;

//==============================================================================
GranularAudioProcessor::GranularAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout GranularAudioProcessor::createLayout()
{
    using P = juce::AudioProcessorValueTreeState;
    juce::ignoreUnused ((P*) nullptr);
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    const juce::StringArray sources { "Sine", "Tri", "Saw", "Sqr",
                                      "White", "Pink", "Imp", "Chord" };
    const juce::StringArray envs { "Hann", "Tri", "Exp", "Rect" };

    // matches the web TRACK_PRESETS
    static const int    defSource[kNumTracks] = { 0, 1, 4, 2, 7 };
    static const double defFreq  [kNumTracks] = { 220, 330, 165, 110, 220 };

    for (int t = 0; t < kNumTracks; ++t) {
        auto id = [t] (const char* p) { return ids::track (t, p); };

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id (ids::kSource), 1 }, id (ids::kSource), sources, defSource[t]));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (ids::kFreq), 1 }, id (ids::kFreq),
            juce::NormalisableRange<float> (20.0f, 4000.0f, 0.0f, 0.35f), (float) defFreq[t]));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (ids::kSize), 1 }, id (ids::kSize),
            juce::NormalisableRange<float> (2.0f, 500.0f, 0.0f, 0.4f), 80.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (ids::kPerBeat), 1 }, id (ids::kPerBeat),
            juce::NormalisableRange<float> (1.0f, 32.0f, 1.0f), 4.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (ids::kPosition), 1 }, id (ids::kPosition), 0.0f, 1.0f, 0.1f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (ids::kPosJit), 1 }, id (ids::kPosJit), 0.0f, 1.0f, 0.05f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (ids::kPitch), 1 }, id (ids::kPitch), -24.0f, 24.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (ids::kPitchJit), 1 }, id (ids::kPitchJit), 0.0f, 24.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (ids::kRevProb), 1 }, id (ids::kRevProb), 0.0f, 1.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (ids::kSpread), 1 }, id (ids::kSpread), 0.0f, 1.0f, 0.4f));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id (ids::kEnv), 1 }, id (ids::kEnv), envs, 0));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id (ids::kGain), 1 }, id (ids::kGain), -40.0f, 6.0f, -8.0f));
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { id (ids::kMute), 1 }, id (ids::kMute), t != 0));  // only T1 audible at first
    }

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::kMasterGain, 1 }, ids::kMasterGain, -60.0f, 6.0f, -6.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::kBpm, 1 }, ids::kBpm, 30.0f, 240.0f, 120.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
void GranularAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    trackBus.setSize (2, samplesPerBlock);
    for (int t = 0; t < kNumTracks; ++t) {
        engines[t].setSampleRate (sampleRate);
        engines[t].setSeed (0x1234u + (uint32_t) t * 7919u);
        sequencers[t].prepare (sampleRate, 120.0);
        builtSource[t] = -1;               // force buffer rebuild
        regenerateBufferIfNeeded (t);
    }
}

void GranularAudioProcessor::regenerateBufferIfNeeded (int t)
{
    const int    src  = (int) apvts.getRawParameterValue (ids::track (t, ids::kSource))->load();
    const double freq = (double) apvts.getRawParameterValue (ids::track (t, ids::kFreq))->load();
    if (src == builtSource[t] && std::abs (freq - builtFreq[t]) < 0.5)
        return;
    builtSource[t] = src;
    builtFreq[t]   = freq;
    engines[t].setBuffer (SourceGen::generate ((SourceType) src, freq, 2.0,
                                               currentSampleRate,
                                               0x9e3779b9u + (uint32_t) t));
}

void GranularAudioProcessor::pullParamsInto (int t)
{
    auto get = [this, t] (const char* p) {
        return (double) apvts.getRawParameterValue (ids::track (t, p))->load();
    };
    auto& e = engines[t];
    e.params.sizeMs      = get (ids::kSize);
    e.params.perBeat     = get (ids::kPerBeat);
    e.params.position    = get (ids::kPosition);
    e.params.posJit      = get (ids::kPosJit);
    e.params.pitchSemis  = get (ids::kPitch);
    e.params.pitchJit    = get (ids::kPitchJit);
    e.params.reverseProb = get (ids::kRevProb);
    e.params.panSpread   = get (ids::kSpread);
    e.params.env         = (EnvShape) (int) get (ids::kEnv);
    e.params.gainLin     = juce::Decibels::decibelsToGain (get (ids::kGain));
    e.params.bpmSync     = true;
}

void GranularAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    // tempo: host playhead if available, else the bpm parameter
    double bpm = (double) apvts.getRawParameterValue (ids::kBpm)->load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getBpm().hasValue())
                bpm = *pos->getBpm();

    if (trackBus.getNumSamples() < n)
        trackBus.setSize (2, n, false, false, true);

    for (int t = 0; t < kNumTracks; ++t) {
        if (apvts.getRawParameterValue (ids::track (t, ids::kMute))->load() > 0.5f)
            continue;

        regenerateBufferIfNeeded (t);
        pullParamsInto (t);
        engines[t].setBpm (bpm);

        trackBus.clear();
        engines[t].renderFree (trackBus.getWritePointer (0),
                               trackBus.getWritePointer (1), n);
        buffer.addFrom (0, 0, trackBus, 0, 0, n);
        buffer.addFrom (1, 0, trackBus, 1, 0, n);
    }

    // master gain + tanh soft-clip safety (mirrors the web version's
    // WaveShaper limiter: transparent below ~-0.7 dB, saturating above)
    const float master = juce::Decibels::decibelsToGain (
        apvts.getRawParameterValue (ids::kMasterGain)->load());
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i) {
            float x = d[i] * master;
            const float a = std::abs (x);
            if (a > 0.92f)
                x = std::copysign (0.92f + 0.08f * std::tanh ((a - 0.92f) / 0.08f), x);
            d[i] = x;
        }
    }
}

//==============================================================================
void GranularAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void GranularAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* GranularAudioProcessor::createEditor()
{
    return new GranularAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GranularAudioProcessor();
}
