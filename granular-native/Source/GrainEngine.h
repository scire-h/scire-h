#pragma once
// GrainEngine — one track's granulator. Port of granular.html's per-track
// grain scheduling + playback:
//   - free-rate or BPM-synced grain stream with density jitter
//   - position + jitter, scan, freeze
//   - pitch (semitones) + jitter via playback rate, reverse probability
//   - envelope: Hann / Tri / Exp / Rect
//   - equal-ish pan spread per grain
// Rendering is sample-accurate against an internal fixed-size voice pool.
// Pure C++17, no JUCE.

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include "SourceGen.h"

namespace granular {

enum class EnvShape { Hann, Tri, Exp, Rect };

struct GrainParams {
    double sizeMs      = 80.0;   // 2..500
    double freeRate    = 20.0;   // grains/sec (sync off)
    double perBeat     = 4.0;    // grains/beat (sync on)
    double densityJit  = 0.15;   // 0..1
    double position    = 0.10;   // 0..1
    double posJit      = 0.05;   // 0..1
    double scanSpeed   = 0.0;    // -1..1
    bool   freeze      = false;
    double pitchSemis  = 0.0;    // -24..24
    double pitchJit    = 0.0;    // 0..24 semitones
    double reverseProb = 0.0;    // 0..1
    double panSpread   = 0.4;    // 0..1
    EnvShape env       = EnvShape::Hann;
    double gainLin     = 1.0;    // grain gain (linear)
    bool   bpmSync     = true;
};

class GrainEngine {
public:
    static constexpr int kMaxVoices = 64;

    GrainEngine() { setSeed (0x1234567u); buildHann(); }

    void setSeed (uint32_t s) { rng.seed (s); }
    void setSampleRate (double sr) { sampleRate = sr; }
    void setBpm (double b) { bpm = std::max (1.0, b); }

    GrainParams params;

    // The source material. Engine keeps a *reversed* copy for reverse grains
    // (mirrors the web version's cached reversed buffer).
    void setBuffer (const StereoBuffer& b)
    {
        buffer = b;
        reversed.left.assign  (buffer.left.rbegin(),  buffer.left.rend());
        reversed.right.assign (buffer.right.rbegin(), buffer.right.rend());
        reversed.sampleRate = buffer.sampleRate;
    }
    bool hasBuffer() const { return buffer.length() > 0; }

    // Continuous (non-sequenced) mode: schedule + render `n` frames,
    // ACCUMULATING into outL/outR.
    void renderFree (float* outL, float* outR, int n)
    {
        if (! hasBuffer()) return;
        for (int i = 0; i < n; ++i) {
            if (--countdown <= 0) {
                trigger();
                countdown = nextIntervalSamples();
                advanceScan();
            }
            renderOneFrame (outL[i], outR[i]);
        }
    }

    // Sequenced mode: the caller triggers grains explicitly (fireAt) and
    // just renders the running voices.
    void renderVoicesOnly (float* outL, float* outR, int n)
    {
        for (int i = 0; i < n; ++i)
            renderOneFrame (outL[i], outR[i]);
    }

    // Fire one grain now, optionally at an overridden position (slice mode).
    void trigger (double posOverride = -1.0)
    {
        if (! hasBuffer()) return;
        Voice* v = findFreeVoice();
        if (v == nullptr) return;

        const double durSec = std::clamp (params.sizeMs / 1000.0, 0.002, 2.0);
        const double semis  = params.pitchSemis + uniform (-params.pitchJit, params.pitchJit);
        const double rate   = std::pow (2.0, semis / 12.0);
        const bool   rev    = uniform01() < params.reverseProb;

        const double base = posOverride >= 0.0 ? posOverride : params.position;
        const double posN = std::clamp (base + uniform (-params.posJit, params.posJit), 0.0, 1.0);
        const double maxOffset = std::max (0.0, buffer.durationSeconds() - durSec * rate - 0.001);
        double offsetSec = posN * maxOffset;
        if (rev)   // mirrored start inside the reversed copy
            offsetSec = std::max (0.0, buffer.durationSeconds() - offsetSec - durSec * rate);

        const double pan = uniform (-params.panSpread, params.panSpread);

        v->active   = true;
        v->reverse  = rev;
        v->srcPos   = offsetSec * buffer.sampleRate;
        v->rate     = rate * (buffer.sampleRate / sampleRate);
        v->age      = 0;
        v->dur      = std::max (1, (int) std::lround (durSec * sampleRate));
        v->amp      = (float) params.gainLin;
        v->env      = params.env;
        // constant-power-ish pan
        const double a = (pan + 1.0) * 0.25 * M_PI;
        v->panL = (float) std::cos (a);
        v->panR = (float) std::sin (a);
        ++grainsFired;
    }

    int activeVoices() const
    {
        int n = 0;
        for (const auto& v : voices) if (v.active) ++n;
        return n;
    }
    long totalGrainsFired() const { return grainsFired; }

private:
    struct Voice {
        bool   active = false, reverse = false;
        double srcPos = 0.0, rate = 1.0;
        int    age = 0, dur = 1;
        float  amp = 1.0f, panL = 0.7f, panR = 0.7f;
        EnvShape env = EnvShape::Hann;
    };

    void renderOneFrame (float& L, float& R)
    {
        for (auto& v : voices) {
            if (! v.active) continue;
            const StereoBuffer& src = v.reverse ? reversed : buffer;
            const int idx = (int) v.srcPos;
            if (idx < 0 || idx + 1 >= src.length()) { v.active = false; continue; }

            const float frac = (float) (v.srcPos - idx);
            const float sl = src.left[(size_t) idx]  + frac * (src.left[(size_t) idx + 1]  - src.left[(size_t) idx]);
            const float sr = src.right[(size_t) idx] + frac * (src.right[(size_t) idx + 1] - src.right[(size_t) idx]);

            const float w = envelopeValue (v.env, (double) v.age / v.dur) * v.amp;
            L += sl * w * v.panL;
            R += sr * w * v.panR;

            v.srcPos += v.rate;
            if (++v.age >= v.dur) v.active = false;
        }
    }

    float envelopeValue (EnvShape e, double t) const
    {
        t = std::clamp (t, 0.0, 1.0);
        switch (e) {
            case EnvShape::Hann: {
                const double x = t * 63.0;
                const int i = (int) x;
                const double f = x - i;
                const double a = hann[(size_t) std::min (i, 63)];
                const double b = hann[(size_t) std::min (i + 1, 63)];
                return (float) (a + f * (b - a));
            }
            case EnvShape::Tri:  return (float) (t < 0.5 ? t * 2.0 : (1.0 - t) * 2.0);
            case EnvShape::Exp:  return (float) (t < 0.08 ? t / 0.08
                                                          : std::exp (-6.9 * (t - 0.08) / 0.92));
            case EnvShape::Rect: {
                const double edge = 0.01;  // 1 % anti-click ramps
                if (t < edge)       return (float) (t / edge);
                if (t > 1.0 - edge) return (float) ((1.0 - t) / edge);
                return 1.0f;
            }
        }
        return 0.0f;
    }

    int nextIntervalSamples()
    {
        const double jit = 1.0 + uniform (-params.densityJit, params.densityJit);
        double interval;
        if (params.bpmSync) interval = 1.0 / std::max (1e-4, (bpm / 60.0) * params.perBeat);
        else                interval = 1.0 / std::max (1e-4, params.freeRate);
        return std::max (1, (int) std::lround (interval * jit * sampleRate));
    }

    void advanceScan()
    {
        if (params.freeze || params.scanSpeed == 0.0) return;
        const double interval = params.bpmSync
            ? 1.0 / std::max (1e-4, (bpm / 60.0) * params.perBeat)
            : 1.0 / std::max (1e-4, params.freeRate);
        params.position = std::clamp (params.position + params.scanSpeed * interval * 0.1, 0.0, 1.0);
    }

    Voice* findFreeVoice()
    {
        for (auto& v : voices) if (! v.active) return &v;
        return nullptr;   // pool exhausted: drop the grain (web behaviour ~ browser limit)
    }

    void buildHann()
    {
        for (int i = 0; i < 64; ++i)
            hann[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / 63.0);
    }

    double uniform (double a, double b)
    {
        std::uniform_real_distribution<double> d (a, b);
        return d (rng);
    }
    double uniform01() { return uniform (0.0, 1.0); }

    StereoBuffer buffer, reversed;
    std::array<Voice, kMaxVoices> voices {};
    std::array<double, 64> hann {};
    double sampleRate = 48000.0, bpm = 120.0;
    int countdown = 1;
    long grainsFired = 0;
    std::mt19937 rng;
};

} // namespace granular
