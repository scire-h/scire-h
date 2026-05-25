#include "Presets.h"

namespace Presets {

/* Per-channel helper: produce the 15 (id, value) pairs for one channel
   from a struct-of-arrays. Keeps each preset definition readable. */
struct ChanSpec {
    float vco, beatTune;
    int   octave;
    float attack, sustain;
    int   waveform;          // 0:Cymbal 1:Pulse 2:Sin   (was 0:sin 1:tri 2:sqr 3:saw)
    bool  noiseOn;           // legacy -- ignored by new Voice (Cymbal mode does the noise)
    float lfoRate, lfoDepth;
    bool  lfoOn;
    float sweep;
    int   sweepDir;          // 0:up 1:off 2:down
    float output, sense;
    bool  multiVCO;
};

/* Source aliases for readability in the preset table below. */
constexpr int CYMBAL = 0;
constexpr int PULSE  = 1;
constexpr int SIN_   = 2;

static void pushChan (std::vector<Entry>& out, int chIdx, const ChanSpec& c) {
    const auto p = P::chPrefix (chIdx);
    out.push_back ({ p + P::pid::vco,       c.vco });
    out.push_back ({ p + P::pid::beatTune,  c.beatTune });
    out.push_back ({ p + P::pid::octave,    (float)c.octave });
    out.push_back ({ p + P::pid::attack,    c.attack });
    out.push_back ({ p + P::pid::sustain,   c.sustain });
    out.push_back ({ p + P::pid::waveform,  (float)c.waveform });
    out.push_back ({ p + P::pid::noiseOn,   c.noiseOn ? 1.0f : 0.0f });
    out.push_back ({ p + P::pid::lfoRate,   c.lfoRate });
    out.push_back ({ p + P::pid::lfoDepth,  c.lfoDepth });
    out.push_back ({ p + P::pid::lfoOn,     c.lfoOn ? 1.0f : 0.0f });
    out.push_back ({ p + P::pid::sweep,     c.sweep });
    out.push_back ({ p + P::pid::sweepDir,  (float)c.sweepDir });
    out.push_back ({ p + P::pid::output,    c.output });
    out.push_back ({ p + P::pid::sense,     c.sense });
    out.push_back ({ p + P::pid::multiVCO,  c.multiVCO ? 1.0f : 0.0f });
}

static Preset makePreset (const juce::String& name,
                          const ChanSpec& c1, const ChanSpec& c2,
                          const ChanSpec& c3, const ChanSpec& c4,
                          float masterVol = 0.55f,
                          bool  beatSense = false) {
    Preset pr; pr.name = name;
    pushChan (pr.entries, 0, c1);
    pushChan (pr.entries, 1, c2);
    pushChan (pr.entries, 2, c3);
    pushChan (pr.entries, 3, c4);
    pr.entries.push_back ({ P::gid::masterVolume, masterVol });
    pr.entries.push_back ({ P::gid::beatSense,    beatSense ? 1.0f : 0.0f });
    return pr;
}

const std::vector<Preset>& factoryPresets() {
    static const std::vector<Preset> presets = {
        /* 0 ---- INIT ---- SIN at middle-octave, no sweep. The most
           neutral starting point: a clean sine you can hear cleanly. */
        makePreset ("Init",
            { 0.5f, 0.0f, 3, 0.0f, 0.3f, SIN_, false, 0.3f, 0.5f, false, 0.0f, 1, 0.8f, 0.7f, false },
            { 0.5f, 0.0f, 3, 0.0f, 0.3f, SIN_, false, 0.3f, 0.5f, false, 0.0f, 1, 0.8f, 0.7f, false },
            { 0.5f, 0.0f, 3, 0.0f, 0.3f, SIN_, false, 0.3f, 0.5f, false, 0.0f, 1, 0.8f, 0.7f, false },
            { 0.5f, 0.0f, 3, 0.0f, 0.3f, SIN_, false, 0.3f, 0.5f, false, 0.0f, 1, 0.8f, 0.7f, false }),

        /* 1 ---- SYNDRUM CLASSIC ---- the iconic disco-era 'PEW!' kit
           that all four sources of the reference were designed for:
           SIN with a big downward sweep. */
        makePreset ("Syndrum Classic",
            /* Ch1 hi-tom */ { 0.85f, 0.0f, 4, 0.0f, 0.18f, SIN_, false, 0.3f, 0.5f, false, 0.55f, 2, 0.78f, 0.7f, false },
            /* Ch2 mid    */ { 0.65f, 0.0f, 4, 0.0f, 0.22f, SIN_, false, 0.3f, 0.5f, false, 0.55f, 2, 0.80f, 0.7f, false },
            /* Ch3 lo-tom */ { 0.45f, 0.0f, 3, 0.0f, 0.28f, SIN_, false, 0.3f, 0.5f, false, 0.55f, 2, 0.85f, 0.7f, false },
            /* Ch4 kick   */ { 0.25f, 0.0f, 2, 0.0f, 0.32f, SIN_, false, 0.3f, 0.5f, false, 0.55f, 2, 0.92f, 0.7f, false }),

        /* 2 ---- ELECTRO KIT ---- showcases all three sources at once:
           CYMBAL noise on the top channels, PULSE for snare-like
           clack, SIN for the kick. */
        makePreset ("Electro Kit",
            /* Ch1 closed-hat */ { 0.5f, 0.0f, 3, 0.0f, 0.08f, CYMBAL, true,  0.3f, 0.5f, false, 0.0f, 1, 0.70f, 0.7f, false },
            /* Ch2 open-hat   */ { 0.5f, 0.0f, 3, 0.0f, 0.28f, CYMBAL, true,  0.3f, 0.5f, false, 0.0f, 1, 0.72f, 0.7f, false },
            /* Ch3 snare      */ { 0.55f,0.0f, 3, 0.0f, 0.18f, PULSE,  true,  0.3f, 0.5f, false, 0.30f,2, 0.85f, 0.7f, false },
            /* Ch4 kick       */ { 0.20f,0.0f, 2, 0.0f, 0.30f, SIN_,   false, 0.3f, 0.5f, false, 0.55f,2, 0.95f, 0.7f, false }),

        /* 3 ---- BIG TOMS ---- SIN with longer SUSTAIN and slight
           BEAT TUNE on top channels for a chorus-y body. */
        makePreset ("Big Toms",
            { 0.70f, 0.10f, 4, 0.0f, 0.45f, SIN_, false, 0.3f, 0.5f, false, 0.30f, 2, 0.80f, 0.8f, false },
            { 0.60f, 0.12f, 3, 0.0f, 0.50f, SIN_, false, 0.3f, 0.5f, false, 0.35f, 2, 0.82f, 0.8f, false },
            { 0.50f, 0.14f, 3, 0.0f, 0.55f, SIN_, false, 0.3f, 0.5f, false, 0.40f, 2, 0.85f, 0.8f, false },
            { 0.40f, 0.16f, 2, 0.0f, 0.60f, SIN_, false, 0.3f, 0.5f, false, 0.45f, 2, 0.90f, 0.8f, false }),

        /* 4 ---- COSMIC UP-SWEEP ---- SIN, all four ramps RISING
           with LFO wobble on the way up. */
        makePreset ("Cosmic Up-Sweep",
            { 0.30f, 0.0f, 2, 0.0f, 0.55f, SIN_, false, 0.55f, 0.65f, true, 0.75f, 0, 0.80f, 0.7f, false },
            { 0.35f, 0.0f, 3, 0.0f, 0.60f, SIN_, false, 0.50f, 0.60f, true, 0.80f, 0, 0.80f, 0.7f, false },
            { 0.40f, 0.0f, 3, 0.0f, 0.65f, SIN_, false, 0.45f, 0.55f, true, 0.85f, 0, 0.80f, 0.7f, false },
            { 0.45f, 0.0f, 4, 0.0f, 0.70f, SIN_, false, 0.40f, 0.50f, true, 0.90f, 0, 0.80f, 0.7f, false }),

        /* 5 ---- DRONE PAD ---- SIN, no sweep, LFO modulates pitch,
           very long SUSTAIN and slow soft attack -- behaves more
           like a pad than a drum. */
        makePreset ("Drone Pad",
            { 0.50f, 0.0f, 4, 0.40f, 0.95f, SIN_, false, 0.25f, 0.55f, true, 0.0f, 1, 0.55f, 0.7f, false },
            { 0.55f, 0.0f, 4, 0.40f, 0.95f, SIN_, false, 0.20f, 0.60f, true, 0.0f, 1, 0.55f, 0.7f, false },
            { 0.62f, 0.0f, 3, 0.40f, 0.95f, SIN_, false, 0.15f, 0.65f, true, 0.0f, 1, 0.55f, 0.7f, false },
            { 0.48f, 0.0f, 3, 0.40f, 0.95f, SIN_, false, 0.30f, 0.50f, true, 0.0f, 1, 0.55f, 0.7f, false },
            0.45f),

        /* 6 ---- MULTI-VCO CASCADE ---- engage PULL on every channel
           so triggering Ch_n also fires Ch_(n+1). All SIN so the
           chord effect is clean. */
        makePreset ("Multi-VCO Cascade",
            { 0.65f, 0.0f, 4, 0.0f, 0.20f, SIN_, false, 0.3f, 0.5f, false, 0.45f, 2, 0.75f, 0.7f, true },
            { 0.55f, 0.0f, 4, 0.0f, 0.25f, SIN_, false, 0.3f, 0.5f, false, 0.50f, 2, 0.75f, 0.7f, true },
            { 0.45f, 0.0f, 3, 0.0f, 0.30f, SIN_, false, 0.3f, 0.5f, false, 0.55f, 2, 0.75f, 0.7f, true },
            { 0.35f, 0.0f, 2, 0.0f, 0.35f, SIN_, false, 0.3f, 0.5f, false, 0.60f, 2, 0.75f, 0.7f, true }),

        /* 7 ---- PURE CYMBAL ---- all four channels in CYMBAL mode at
           different sustains so you can hear the broadband-LP noise
           character alone. */
        makePreset ("Cymbal Wash",
            { 0.5f, 0.0f, 3, 0.0f, 0.15f, CYMBAL, true, 0.3f, 0.5f, false, 0.0f, 1, 0.65f, 0.7f, false },
            { 0.5f, 0.0f, 3, 0.0f, 0.30f, CYMBAL, true, 0.3f, 0.5f, false, 0.0f, 1, 0.65f, 0.7f, false },
            { 0.5f, 0.0f, 3, 0.0f, 0.50f, CYMBAL, true, 0.3f, 0.5f, false, 0.0f, 1, 0.65f, 0.7f, false },
            { 0.5f, 0.0f, 3, 0.0f, 0.85f, CYMBAL, true, 0.3f, 0.5f, false, 0.0f, 1, 0.65f, 0.7f, false }),
    };
    return presets;
}

void apply (juce::AudioProcessorValueTreeState& apvts, int index) {
    const auto& list = factoryPresets();
    if (index < 0 || (std::size_t)index >= list.size()) return;
    for (const auto& e : list[(std::size_t)index].entries) {
        if (auto* p = apvts.getParameter (e.id))
            p->setValueNotifyingHost (
                p->convertTo0to1 (e.value));
    }
}

}  // namespace Presets
