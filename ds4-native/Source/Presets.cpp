#include "Presets.h"

namespace Presets {

/* Per-channel helper: produce the 15 (id, value) pairs for one channel
   from a struct-of-arrays. Keeps each preset definition readable. */
struct ChanSpec {
    float vco, beatTune;
    int   octave;
    float attack, sustain;
    int   waveform;          // 0:sin 1:tri 2:sqr 3:saw
    bool  noiseOn;
    float lfoRate, lfoDepth;
    bool  lfoOn;
    float sweep;
    int   sweepDir;          // 0:up 1:off 2:down
    float output, sense;
    bool  multiVCO;
};

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
        /* 0 ---- INIT ---- everything at the APVTS default. */
        makePreset ("Init",
            { 0.5f, 0.0f, 3, 0.0f, 0.3f, 0, true,  0.3f, 0.5f, false, 0.5f, 2, 0.8f, 0.7f, false },
            { 0.5f, 0.0f, 3, 0.0f, 0.3f, 0, true,  0.3f, 0.5f, false, 0.5f, 2, 0.8f, 0.7f, false },
            { 0.5f, 0.0f, 3, 0.0f, 0.3f, 0, true,  0.3f, 0.5f, false, 0.5f, 2, 0.8f, 0.7f, false },
            { 0.5f, 0.0f, 3, 0.0f, 0.3f, 0, true,  0.3f, 0.5f, false, 0.5f, 2, 0.8f, 0.7f, false }),

        /* 1 ---- SYNDRUM CLASSIC ---- iconic disco-era 'pew' kit. */
        makePreset ("Syndrum Classic",
            /* Ch1 cymbal */ { 0.78f, 0.0f, 4, 0.0f, 0.18f, 0, true,  0.3f, 0.5f, false, 0.45f, 2, 0.75f, 0.7f, false },
            /* Ch2 hi-tom */ { 0.72f, 0.0f, 4, 0.0f, 0.28f, 0, false, 0.3f, 0.5f, false, 0.55f, 2, 0.80f, 0.7f, false },
            /* Ch3 lo-tom */ { 0.58f, 0.0f, 3, 0.0f, 0.32f, 0, false, 0.3f, 0.5f, false, 0.60f, 2, 0.85f, 0.7f, false },
            /* Ch4 kick   */ { 0.35f, 0.0f, 2, 0.0f, 0.30f, 0, false, 0.3f, 0.5f, false, 0.60f, 2, 0.95f, 0.7f, false }),

        /* 2 ---- BIG TOMS ---- triangle waveform, longer decay. */
        makePreset ("Big Toms",
            { 0.70f, 0.12f, 4, 0.05f, 0.45f, 1, false, 0.3f, 0.5f, false, 0.55f, 2, 0.80f, 0.8f, false },
            { 0.62f, 0.15f, 3, 0.05f, 0.50f, 1, false, 0.3f, 0.5f, false, 0.60f, 2, 0.82f, 0.8f, false },
            { 0.50f, 0.18f, 3, 0.04f, 0.55f, 1, false, 0.3f, 0.5f, false, 0.65f, 2, 0.85f, 0.8f, false },
            { 0.38f, 0.20f, 2, 0.04f, 0.60f, 1, false, 0.3f, 0.5f, false, 0.70f, 2, 0.90f, 0.8f, false }),

        /* 3 ---- ELECTRO KIT ---- bright cymbal, snappy snare, thumpy kick. */
        makePreset ("Electro Kit",
            /* Ch1 closed-hat */ { 0.85f, 0.0f, 5, 0.0f, 0.08f, 0, true,  0.3f, 0.5f, false, 0.25f, 2, 0.72f, 0.7f, false },
            /* Ch2 open-hat   */ { 0.78f, 0.0f, 4, 0.0f, 0.32f, 0, true,  0.3f, 0.5f, false, 0.45f, 2, 0.70f, 0.7f, false },
            /* Ch3 snare      */ { 0.50f, 0.0f, 3, 0.0f, 0.20f, 1, true,  0.3f, 0.5f, false, 0.30f, 2, 0.88f, 0.7f, false },
            /* Ch4 kick       */ { 0.30f, 0.0f, 2, 0.0f, 0.28f, 0, false, 0.3f, 0.5f, false, 0.65f, 2, 0.98f, 0.7f, false }),

        /* 4 ---- COSMIC UP-SWEEP ---- all four channels rise, LFO wobble. */
        makePreset ("Cosmic Up-Sweep",
            { 0.30f, 0.0f, 2, 0.0f, 0.55f, 0, false, 0.55f, 0.65f, true, 0.75f, 0, 0.80f, 0.7f, false },
            { 0.35f, 0.0f, 3, 0.0f, 0.60f, 1, false, 0.50f, 0.60f, true, 0.80f, 0, 0.80f, 0.7f, false },
            { 0.40f, 0.0f, 3, 0.0f, 0.65f, 0, false, 0.45f, 0.55f, true, 0.85f, 0, 0.80f, 0.7f, false },
            { 0.45f, 0.0f, 4, 0.0f, 0.70f, 0, false, 0.40f, 0.50f, true, 0.90f, 0, 0.80f, 0.7f, false }),

        /* 5 ---- DRONE PAD ---- no sweep, LFO modulates pitch, long tails. */
        makePreset ("Drone Pad",
            { 0.50f, 0.0f, 4, 0.40f, 0.95f, 1, false, 0.25f, 0.55f, true, 0.0f, 1, 0.55f, 0.7f, false },
            { 0.55f, 0.0f, 4, 0.40f, 0.95f, 0, false, 0.20f, 0.60f, true, 0.0f, 1, 0.55f, 0.7f, false },
            { 0.62f, 0.0f, 3, 0.40f, 0.95f, 1, false, 0.15f, 0.65f, true, 0.0f, 1, 0.55f, 0.7f, false },
            { 0.48f, 0.0f, 3, 0.40f, 0.95f, 0, false, 0.30f, 0.50f, true, 0.0f, 1, 0.55f, 0.7f, false },
            0.45f),

        /* 6 ---- MULTI-VCO CASCADE DEMO ---- pull all four PULL switches:
                                              each hit chains to the next. */
        makePreset ("Multi-VCO Cascade",
            { 0.65f, 0.0f, 4, 0.0f, 0.20f, 0, false, 0.3f, 0.5f, false, 0.45f, 2, 0.75f, 0.7f, true },
            { 0.55f, 0.0f, 4, 0.0f, 0.25f, 0, false, 0.3f, 0.5f, false, 0.50f, 2, 0.75f, 0.7f, true },
            { 0.45f, 0.0f, 3, 0.0f, 0.30f, 0, false, 0.3f, 0.5f, false, 0.55f, 2, 0.75f, 0.7f, true },
            { 0.35f, 0.0f, 2, 0.0f, 0.35f, 0, false, 0.3f, 0.5f, false, 0.60f, 2, 0.75f, 0.7f, true }),
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
