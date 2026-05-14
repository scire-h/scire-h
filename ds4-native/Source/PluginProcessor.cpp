#include "PluginProcessor.h"
#include "PluginEditor.h"

DS4MProcessor::DS4MProcessor()
    : AudioProcessor (BusesProperties()
                       .withInput  ("Trigger In", juce::AudioChannelSet::stereo(), false)
                       .withOutput ("Output",     juce::AudioChannelSet::stereo(), true))
    , apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    for (int i = 0; i < P::kNumChannels; ++i)
        voices[(size_t)i].setChannelIndex (i);
}

DS4MProcessor::~DS4MProcessor() = default;

void DS4MProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {
    currentSampleRate = sampleRate;
    for (auto& v : voices) v.prepare (sampleRate, samplesPerBlock);
    for (auto& p : piezos) {
        p.prepare (sampleRate);
        p.setThresholds (0.20f, 0.05f);
        p.setAttackMs  (0.5f);
        p.setReleaseMs (12.0f);
        p.setHoldMs    (8.0f);
    }
}

void DS4MProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DS4MProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo()
     && out != juce::AudioChannelSet::mono())
        return false;
    /* The trigger input bus is optional -- mono or stereo. */
    const auto in  = layouts.getMainInputChannelSet();
    return in.isDisabled()
        || in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}
#endif

void DS4MProcessor::fireChannel (int ch, float vel, bool followCascade) {
    if (ch < 0 || ch >= P::kNumChannels) return;
    voices[(size_t)ch].trigger (vel);
    if (followCascade && voices[(size_t)ch].getMultiVCO()) {
        /* MULTI VCO PULL routes the trigger to the next channel. */
        const int next = (ch + 1) % P::kNumChannels;
        /* Pass followCascade=false so we never get an infinite loop
           even if multiple channels chain together: a hit cascades
           one hop, not endlessly. */
        voices[(size_t)next].trigger (vel * 0.8f);
    }
}

void DS4MProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    /* Capture the trigger input BEFORE we clear the output buffer
       (in some hosts the input and output buses share storage). */
    juce::AudioBuffer<float> trigBuf;
    if (auto inLayout = getBusesLayout().getMainInputChannelSet();
        ! inLayout.isDisabled())
    {
        auto inBus = getBus (true, 0);
        if (inBus != nullptr && inBus->isEnabled()) {
            auto inBlock = inBus->getBusBuffer (buffer);
            trigBuf.makeCopyOf (inBlock, true);
        }
    }

    buffer.clear();

    /* Pull parameter snapshots for the block. */
    const bool beatSense =
        apvts.getRawParameterValue (P::gid::beatSense)->load() > 0.5f;
    for (auto& v : voices) v.updateParameters (apvts, beatSense);

    /* Drain pending pad / external triggers from the GUI. */
    int s1, sz1, s2, sz2;
    triggerFifo.prepareToRead (triggerFifo.getNumReady(), s1, sz1, s2, sz2);
    for (int i = 0; i < sz1; ++i) {
        auto& t = triggerBuffer[(size_t)(s1 + i)];
        fireChannel (t.ch, t.vel, true);
    }
    for (int i = 0; i < sz2; ++i) {
        auto& t = triggerBuffer[(size_t)(s2 + i)];
        fireChannel (t.ch, t.vel, true);
    }
    triggerFifo.finishedRead (sz1 + sz2);

    /* MIDI: notes 36..39 (C1..D#1) trigger Ch1..Ch4 respectively;
       higher notes are forwarded to the same four channels via modulo
       so any MIDI source on any octave still drives the drums. */
    for (const auto md : midi) {
        const auto m = md.getMessage();
        if (m.isNoteOn()) {
            const int note = m.getNoteNumber();
            int ch = (note - 36) % P::kNumChannels;
            if (ch < 0) ch += P::kNumChannels;
            if (ch >= 0 && ch < P::kNumChannels)
                fireChannel (ch, m.getFloatVelocity(), true);
        }
    }

    /* Audio-rate trigger detection from the side-chain bus.
       Channel layout:
         - mono input  -> Ch1 only
         - stereo input -> L = Ch1, R = Ch2
       (Channels 3 and 4 stay MIDI/GUI only since a single side-chain
       in JUCE only exposes one bus.) */
    if (trigBuf.getNumChannels() > 0 && trigBuf.getNumSamples() > 0) {
        const int numInChans = trigBuf.getNumChannels();
        const int numSamples = trigBuf.getNumSamples();
        for (int c = 0; c < juce::jmin (numInChans, P::kNumChannels); ++c) {
            const float* in = trigBuf.getReadPointer (c);
            auto& detector = piezos[(size_t)c];
            for (int n = 0; n < numSamples; ++n) {
                const float v = detector.processSample (in[n]);
                if (v > 0.0f) fireChannel (c, v, true);
            }
        }
    }

    /* Render. */
    const int numSamples = buffer.getNumSamples();
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    for (auto& v : voices) v.processBlock (outL, outR, numSamples);

    /* Master volume. */
    const float master = apvts.getRawParameterValue (P::gid::masterVolume)->load();
    buffer.applyGain (master);
}

juce::AudioProcessorEditor* DS4MProcessor::createEditor() {
    return new DS4MEditor (*this);
}

void DS4MProcessor::getStateInformation (juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DS4MProcessor::setStateInformation (const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

void DS4MProcessor::triggerChannel (int chIndex, float velocity) {
    /* Editor thread -> audio thread via lock-free FIFO. */
    int s1, sz1, s2, sz2;
    triggerFifo.prepareToWrite (1, s1, sz1, s2, sz2);
    if      (sz1 > 0) triggerBuffer[(size_t)s1] = { chIndex, velocity };
    else if (sz2 > 0) triggerBuffer[(size_t)s2] = { chIndex, velocity };
    else              return;          /* FIFO full -- drop the hit */
    triggerFifo.finishedWrite (1);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DS4MProcessor::createParameterLayout() {
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    for (int i = 0; i < P::kNumChannels; ++i) {
        const auto p  = P::chPrefix (i);
        const auto nm = "Ch" + String (i + 1) + " ";

        params.push_back (std::make_unique<AudioParameterFloat>(
            ParameterID { p + P::pid::vco, 1 },          nm + "VCO",
            NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        params.push_back (std::make_unique<AudioParameterFloat>(
            ParameterID { p + P::pid::beatTune, 1 },     nm + "Beat Tune",
            NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        params.push_back (std::make_unique<AudioParameterInt>(
            ParameterID { p + P::pid::octave, 1 },       nm + "Octave",
            1, 5, 3));
        params.push_back (std::make_unique<AudioParameterFloat>(
            ParameterID { p + P::pid::attack, 1 },       nm + "Attack",
            NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        params.push_back (std::make_unique<AudioParameterFloat>(
            ParameterID { p + P::pid::sustain, 1 },      nm + "Sustain",
            NormalisableRange<float>(0.0f, 1.0f), 0.3f));
        params.push_back (std::make_unique<AudioParameterChoice>(
            ParameterID { p + P::pid::waveform, 1 },     nm + "Waveform",
            StringArray { "Sine", "Triangle", "Square", "Sawtooth" }, 0));
        params.push_back (std::make_unique<AudioParameterBool>(
            ParameterID { p + P::pid::noiseOn, 1 },      nm + "Noise", true));
        params.push_back (std::make_unique<AudioParameterFloat>(
            ParameterID { p + P::pid::lfoRate, 1 },      nm + "LFO Rate",
            NormalisableRange<float>(0.0f, 1.0f), 0.3f));
        params.push_back (std::make_unique<AudioParameterFloat>(
            ParameterID { p + P::pid::lfoDepth, 1 },     nm + "LFO Depth",
            NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        params.push_back (std::make_unique<AudioParameterBool>(
            ParameterID { p + P::pid::lfoOn, 1 },        nm + "LFO On", false));
        params.push_back (std::make_unique<AudioParameterFloat>(
            ParameterID { p + P::pid::sweep, 1 },        nm + "Sweep Width",
            NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        params.push_back (std::make_unique<AudioParameterChoice>(
            ParameterID { p + P::pid::sweepDir, 1 },     nm + "Sweep Direction",
            StringArray { "Up", "Off", "Down" }, 2));
        params.push_back (std::make_unique<AudioParameterFloat>(
            ParameterID { p + P::pid::output, 1 },       nm + "Output",
            NormalisableRange<float>(0.0f, 1.0f), 0.8f));
        params.push_back (std::make_unique<AudioParameterFloat>(
            ParameterID { p + P::pid::sense, 1 },        nm + "Sense Level",
            NormalisableRange<float>(0.0f, 1.0f), 0.7f));
        params.push_back (std::make_unique<AudioParameterBool>(
            ParameterID { p + P::pid::multiVCO, 1 },     nm + "Multi VCO", false));
    }

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { P::gid::masterVolume, 1 }, "Master Volume",
        NormalisableRange<float>(0.0f, 1.0f), 0.55f));
    params.push_back (std::make_unique<AudioParameterBool>(
        ParameterID { P::gid::beatSense,    1 }, "Beat Sense", false));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new DS4MProcessor();
}
