#pragma once
// Evolver — generative pattern mutation, direct port of granular.html's
// maybeEvolve/mutatePattern. Once per bar, a track with evolve enabled gets
// 0–3 weighted mutations:
//   flip 30 · shift 14 · swap 12 · density+ 12 · density- 10 · roll 8 ·
//   mirror 4 · clear-zone 4 · polyshift 6 · reslice 12 (slice mode only)
// Pass distribution: 15% none, 63% one, 18% two, 4% three.
// RNG is injectable for deterministic tests. Pure C++17, no JUCE.

#include <random>
#include "StepSequencer.h"

namespace granular {

enum class Mutation { Flip, Shift, Swap, DensityUp, DensityDown,
                      Roll, Mirror, ClearZone, PolyShift, Reslice };

class Evolver {
public:
    explicit Evolver (uint32_t seed = 0xC0FFEEu) : rng (seed) {}
    void seed (uint32_t s) { rng.seed (s); }

    // Apply one bar's worth of evolution to the sequencer's pattern.
    // Returns the number of mutations applied.
    int evolveBar (StepSequencer& seq)
    {
        const double r = uniform01();
        int passes;
        if      (r < 0.15) passes = 0;
        else if (r < 0.78) passes = 1;
        else if (r < 0.96) passes = 2;
        else               passes = 3;

        for (int p = 0; p < passes; ++p)
            applyMutation (seq, pickWeighted (seq.mode == StepMode::Slice));
        return passes;
    }

    // Exposed for tests.
    void applyMutation (StepSequencer& seq, Mutation m)
    {
        const int N = seq.numSteps;
        auto& act = seq.active;
        auto& slc = seq.sliceMap;

        switch (m) {
            case Mutation::Flip: {
                const int count = uniform01() < 0.7 ? 1 : 2;
                for (int k = 0; k < count; ++k) {
                    const int i = randInt (N);
                    act[(size_t) i] = ! act[(size_t) i];
                    if (act[(size_t) i] && seq.mode == StepMode::Slice)
                        slc[(size_t) i] = randInt (seq.numSlices);
                }
                break;
            }
            case Mutation::Shift: {
                const int dir = uniform01() < 0.5 ? 1 : -1;
                std::array<bool, StepSequencer::kMaxSteps> a {};
                std::array<int,  StepSequencer::kMaxSteps> s {};
                for (int i = 0; i < N; ++i) { a[(size_t) i] = act[(size_t) i]; s[(size_t) i] = slc[(size_t) i]; }
                for (int i = 0; i < N; ++i) {
                    const int j = ((i - dir) % N + N) % N;
                    act[(size_t) i] = a[(size_t) j];
                    slc[(size_t) i] = s[(size_t) j];
                }
                break;
            }
            case Mutation::Swap: {
                const int i = randInt (N), j = randInt (N);
                std::swap (act[(size_t) i], act[(size_t) j]);
                std::swap (slc[(size_t) i], slc[(size_t) j]);
                break;
            }
            case Mutation::DensityUp: {
                const int count = 1 + randInt (2);
                for (int k = 0; k < count; ++k) {
                    const int i = randInt (N);
                    act[(size_t) i] = true;
                    if (seq.mode == StepMode::Slice)
                        slc[(size_t) i] = randInt (seq.numSlices);
                }
                break;
            }
            case Mutation::DensityDown: {
                std::vector<int> on;
                for (int i = 0; i < N; ++i) if (act[(size_t) i]) on.push_back (i);
                if (on.empty()) break;
                const int count = std::min ((int) on.size(), 1 + randInt (2));
                for (int k = 0; k < count; ++k) {
                    const int idx = randInt ((int) on.size());
                    act[(size_t) on[(size_t) idx]] = false;
                    on.erase (on.begin() + idx);
                }
                break;
            }
            case Mutation::Roll: {
                const int len = 2 + randInt (3);
                const int start = randInt (N);
                for (int k = 0; k < len; ++k) {
                    const int i = (start + k) % N;
                    act[(size_t) i] = true;
                    if (seq.mode == StepMode::Slice)
                        slc[(size_t) i] = randInt (seq.numSlices);
                }
                break;
            }
            case Mutation::Mirror: {
                const bool fromLeft = uniform01() < 0.5;
                std::array<bool, StepSequencer::kMaxSteps> a {};
                std::array<int,  StepSequencer::kMaxSteps> s {};
                for (int i = 0; i < N; ++i) { a[(size_t) i] = act[(size_t) i]; s[(size_t) i] = slc[(size_t) i]; }
                for (int i = 0; i < N; ++i) {
                    const int src = fromLeft ? i : (N - 1 - i);
                    const int dst = fromLeft ? (N - 1 - i) : i;
                    act[(size_t) dst] = a[(size_t) src];
                    slc[(size_t) dst] = s[(size_t) src];
                }
                break;
            }
            case Mutation::ClearZone: {
                const int len = 3 + randInt (3);
                const int start = randInt (N);
                for (int k = 0; k < len; ++k)
                    act[(size_t) ((start + k) % N)] = false;
                break;
            }
            case Mutation::PolyShift: {
                const int half = N >> 1;
                if (half < 2) break;
                const int dir   = uniform01() < 0.5 ? 1 : -1;
                const int which = uniform01() < 0.5 ? 0 : half;
                const int len   = (which == 0) ? half : (N - half);
                std::vector<bool> a ((size_t) len);
                std::vector<int>  s ((size_t) len);
                for (int i = 0; i < len; ++i) { a[(size_t) i] = act[(size_t) (which + i)]; s[(size_t) i] = slc[(size_t) (which + i)]; }
                for (int i = 0; i < len; ++i) {
                    const int j = ((i - dir) % len + len) % len;
                    act[(size_t) (which + i)] = a[(size_t) j];
                    slc[(size_t) (which + i)] = s[(size_t) j];
                }
                break;
            }
            case Mutation::Reslice: {
                if (seq.mode != StepMode::Slice) break;
                std::vector<int> on;
                for (int i = 0; i < N; ++i) if (act[(size_t) i]) on.push_back (i);
                if (on.empty()) break;
                const int count = uniform01() < 0.6 ? 1 : 2;
                for (int k = 0; k < std::min (count, (int) on.size()); ++k) {
                    const int idx = randInt ((int) on.size());
                    slc[(size_t) on[(size_t) idx]] = randInt (seq.numSlices);
                    on.erase (on.begin() + idx);
                }
                break;
            }
        }
    }

    Mutation pickWeighted (bool sliceMode)
    {
        struct W { Mutation m; int w; };
        static const W base[] = {
            { Mutation::Flip, 30 },      { Mutation::Shift, 14 },
            { Mutation::Swap, 12 },      { Mutation::DensityUp, 12 },
            { Mutation::DensityDown, 10 },{ Mutation::Roll, 8 },
            { Mutation::Mirror, 4 },     { Mutation::ClearZone, 4 },
            { Mutation::PolyShift, 6 },
        };
        int total = 0;
        for (const auto& b : base) total += b.w;
        const int sliceW = sliceMode ? 12 : 0;
        total += sliceW;

        int r = randInt (total);
        for (const auto& b : base) {
            r -= b.w;
            if (r < 0) return b.m;
        }
        return Mutation::Reslice;
    }

private:
    double uniform01()
    {
        std::uniform_real_distribution<double> d (0.0, 1.0);
        return d (rng);
    }
    int randInt (int n)   // 0..n-1
    {
        std::uniform_int_distribution<int> d (0, std::max (0, n - 1));
        return d (rng);
    }
    std::mt19937 rng;
};

} // namespace granular
