#include "PluginProcessor.h"
#include "PluginEditor.h"

DS4MProcessor::DS4MProcessor()
    : AudioProcessor (BusesProperties()
                       .withInput  ("Trigger In", juce::AudioChannelSet::stereo(), false)
                       .withOutput ("Main Out",   juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Ch1 Out",    juce::AudioChannelSet::stereo(), false)
                       .withOutput ("Ch2 Out",    juce::AudioChannelSet::stereo(), false)
                       .withOutput ("Ch3 Out",    juce::AudioChannelSet::stereo(), false)
                       .withOutput ("Ch4 Out",    juce::AudioChannelSet::stereo(), false))
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
    /* Main output must be mono or stereo. */
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo()
     && out != juce::AudioChannelSet::mono())
        return false;

    /* Each of the four optional per-channel aux outputs must be
       either disabled or stereo. */
    for (int i = 1; i <= P::kNumChannels; ++i) {
        if (layouts.outputBuses.size() > i) {
            const auto a = layouts.outputBuses.getReference(i);
            if (! a.isDisabled() && a != juce::AudioChannelSet::stereo())
                return false;
        }
    }

    /* Optional trigger input. */
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled()
        || in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}
#endif

void DS4MProcessor::fireChannel (int ch, float vel, bool followCascade) {
    if (ch < 0 || ch >= P::kNumChannels) return;
    voices[(size_t)ch].trigger (vel);
    if (followCascade && voices[(size_t)ch].getMultiVCO()) {
        const int next = (ch + 1) % P::kNumChannels;
        voices[(size_t)next].trigger (vel * 0.8f);
    }
}

void DS4MProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    /* Snapshot the trigger input before we clear the buffer. */
    juce::AudioBuffer<float> trigBuf;
    {
        auto inBus = getBus (true, 0);
        if (inBus != nullptr && inBus->isEnabled()) {
            auto inBlock = inBus->getBusBuffer (buffer);
            trigBuf.makeCopyOf (inBlock, true);
        }
    }

    /* Clear ALL output buses (main + every aux). */
    for (int b = 0; b < getBusCount (false); ++b) {
        auto out = getBusBuffer (buffer, false, b);
        out.clear();
    }

    /* Parameter snapshot per block. */
    const bool beatSense =
        apvts.getRawParameterValue (P::gid::beatSense)->load() > 0.5f;
    for (auto& v : voices) v.updateParameters (apvts, beatSense);

    /* Drain pending GUI triggers. */
    int s1, sz1, s2, sz2;
    triggerFifo.prepareToRead (triggerFifo.getNumReady(), s1, sz1, s2, sz2);
    for (int i = 0; i < sz1; ++i) fireChannel (triggerBuffer[(size_t)(s1 + i)].ch,
                                               triggerBuffer[(size_t)(s1 + i)].vel, true);
    for (int i = 0; i < sz2; ++i) fireChannel (triggerBuffer[(size_t)(s2 + i)].ch,
                                               triggerBuffer[(size_t)(s2 + i)].vel, true);
    triggerFifo.finishedRead (sz1 + sz2);

    /* MIDI triggers (any octave routes to Ch1..Ch4 via modulo). */
    for (const auto md : midi) {
        const auto m = md.getMessage();
        if (m.isNoteOn()) {
            int ch = (m.getNoteNumber() - 36) % P::kNumChannels;
            if (ch < 0) ch += P::kNumChannels;
            fireChannel (ch, m.getFloatVelocity(), true);
        }
    }

    /* Audio-rate piezo trigger detection on the side-chain bus. */
    if (trigBuf.getNumChannels() > 0 && trigBuf.getNumSamples() > 0) {
        const int numInChans = trigBuf.getNumChannels();
        for (int c = 0; c < juce::jmin (numInChans, P::kNumChannels); ++c) {
            const float* in = trigBuf.getReadPointer (c);
            auto& detector  = piezos[(size_t)c];
            for (int n = 0; n < numSamples; ++n) {
                const float v = detector.processSample (in[n]);
                if (v > 0.0f) fireChannel (c, v, true);
            }
        }
    }

    /* Render each voice into a small per-channel scratch buffer
       (stereo). We then route it both to its dedicated aux output
       bus (if the host enabled it) and to the main mix. */
    juce::AudioBuffer<float> scratch (2, numSamples);
    for (int ch = 0; ch < P::kNumChannels; ++ch) {
        scratch.clear();
        float* sL = scratch.getWritePointer (0);
        float* sR = scratch.getWritePointer (1);
        voices[(size_t)ch].processBlock (sL, sR, numSamples);

        /* Aux bus output (bus index = ch + 1, since bus 0 is main). */
        if (auto* auxBus = getBus (false, ch + 1);
            auxBus != nullptr && auxBus->isEnabled())
        {
            auto auxBlock = auxBus->getBusBuffer (buffer);
            for (int outC = 0; outC < juce::jmin (auxBlock.getNumChannels(), 2); ++outC)
                auxBlock.copyFrom (outC, 0, scratch, outC, 0, numSamples);
        }

        /* Sum into main mix bus. */
        auto mainBlock = getBus (false, 0)->getBusBuffer (buffer);
        const int mainChans = mainBlock.getNumChannels();
        if (mainChans >= 2) {
            mainBlock.addFrom (0, 0, scratch, 0, 0, numSamples);
            mainBlock.addFrom (1, 0, scratch, 1, 0, numSamples);
        } else if (mainChans == 1) {
            /* Mono main: sum L+R into channel 0 at -3 dB. */
            mainBlock.addFrom (0, 0, scratch, 0, 0, numSamples, 0.707f);
            mainBlock.addFrom (0, 0, scratch, 1, 0, numSamples, 0.707f);
        }
    }

    /* Master volume + soft-clip limiter applied to ALL output buses.

       The OTAVCA on each channel already does signal-level dependent
       saturation, but four channels summing into the main bus can
       still push above unity. We apply a final tanh-based soft
       clipper that's transparent below |x| < 0.7 and rolls off
       smoothly past that, so the output is bounded in (-1, +1) but
       never hits a hard digital clip. */
    const float master = apvts.getRawParameterValue (P::gid::masterVolume)->load();
    for (int b = 0; b < getBusCount (false); ++b) {
        auto* bus = getBus (false, b);
        if (bus == nullptr || ! bus->isEnabled()) continue;
        auto block = bus->getBusBuffer (buffer);
        const int chans = block.getNumChannels();
        const int n     = block.getNumSamples();
        for (int c = 0; c < chans; ++c) {
            float* out = block.getWritePointer (c);
            for (int i = 0; i < n; ++i) {
                const float v = out[i] * master;
                /* tanh soft-clip with a 1.2 drive: transparent up to
                   ~0.6, smoothly limits beyond, asymptote at 1. */
                out[i] = std::tanh (v * 1.2f) * 0.833f;
            }
        }
    }
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
    int s1, sz1, s2, sz2;
    triggerFifo.prepareToWrite (1, s1, sz1, s2, sz2);
    if      (sz1 > 0) triggerBuffer[(size_t)s1] = { chIndex, velocity };
    else if (sz2 > 0) triggerBuffer[(size_t)s2] = { chIndex, velocity };
    else              return;
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
            ParameterID { p + P::pid::waveform, 2 },     nm + "Source",
            /* Reference measurements from the SDS-2002 era drum-synth
               software (LFO + SWEEP off, settled tone):
                  SIN    -> pure sin(2pi.f.t), H2 floor at -118 dB
                  PULSE  -> bandlimited 50% square, H3 -11 dB, H5 -16 dB,
                            even harmonics in the noise floor
                  CYMBAL -> band-passed noise centred ~3 kHz with a
                            secondary resonance ~5.7 kHz, *no* VCO content.
               So the source is a 3-way mutually-exclusive radio rather
               than the 4-way sine/tri/sqr/saw + separate noise toggle
               we had before. */
            StringArray { "Cymbal", "Pulse", "Sin" }, 1));
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
