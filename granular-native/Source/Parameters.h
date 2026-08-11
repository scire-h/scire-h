#pragma once
// Parameter IDs for the GRANULAR plug-in. Five identical per-track blocks
// (t1..t5) plus master. Kept as plain helpers so both the processor and the
// editor agree on IDs.

#include <juce_audio_processors/juce_audio_processors.h>

namespace granular::ids {

inline juce::String track (int i, const char* p)   // i = 0..4
{
    return "t" + juce::String (i + 1) + "_" + p;
}

// per-track parameter suffixes
inline constexpr const char* kSource   = "source";    // choice
inline constexpr const char* kFreq     = "freq";      // 20..4000 Hz (log)
inline constexpr const char* kSize     = "size";      // 2..500 ms
inline constexpr const char* kPerBeat  = "perbeat";   // 1..32 grains/beat
inline constexpr const char* kPosition = "pos";       // 0..1
inline constexpr const char* kPosJit   = "posjit";    // 0..1
inline constexpr const char* kPitch    = "pitch";     // -24..24 st
inline constexpr const char* kPitchJit = "pitchjit";  // 0..24 st
inline constexpr const char* kRevProb  = "revprob";   // 0..1
inline constexpr const char* kSpread   = "spread";    // 0..1
inline constexpr const char* kEnv      = "env";       // choice
inline constexpr const char* kGain     = "gain";      // -40..+6 dB
inline constexpr const char* kMute     = "mute";      // bool

// master
inline constexpr const char* kMasterGain = "master_gain";  // -60..+6 dB
inline constexpr const char* kBpm        = "bpm";          // 30..240 (fallback when host gives none)

} // namespace granular::ids
