#pragma once
// StepSequencer — 64-step pattern per track, Hit / Slice modes, step rates
// including triplets (1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/64).
// advance(nFrames) walks time in samples and reports which steps fire inside
// the block (with intra-block sample offsets) so the caller can trigger
// grains sample-accurately. Pure C++17, no JUCE.

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace granular {

enum class StepMode { Hit, Slice };

// steps-per-quarter-note factors for the supported step rates.
// rate value N means "1/N note"; triplet rates use the web convention
// (12 = 1/8T, 24 = 1/16T).
inline double stepsPerBeatFor (int rateN) { return rateN / 4.0; }

class StepSequencer {
public:
    static constexpr int kMaxSteps = 64;

    struct Fire { int sampleOffset; int step; double slicePos; bool sliced; };

    bool  enabled  = true;
    StepMode mode  = StepMode::Hit;
    int   numSteps = 16;     // 8/16/32/64
    int   rateN    = 16;     // 4, 8, 12(=1/8T), 16, 24(=1/16T), 32, 64
    int   numSlices = 16;

    std::array<bool, kMaxSteps> active {};
    std::array<int,  kMaxSteps> sliceMap {};

    StepSequencer()
    {
        for (int i = 0; i < kMaxSteps; ++i) sliceMap[(size_t) i] = i % 16;
    }

    void prepare (double sr, double bpmIn)
    {
        sampleRate = sr;
        setBpm (bpmIn);
        reset();
    }
    void setBpm (double b)
    {
        bpm = std::max (1.0, b);
        samplesPerStep = sampleRate * 60.0 / (bpm * stepsPerBeatFor (rateN));
    }
    void setRate (int n) { rateN = n; setBpm (bpm); }
    void reset()
    {
        currentStep = -1;
        samplesUntilStep = 0.0;
    }

    int  step() const { return currentStep; }

    // Walk nFrames of time; append any step-fires (with intra-block offsets).
    void advance (int nFrames, std::vector<Fire>& fires)
    {
        if (! enabled) return;
        double remaining = (double) nFrames;
        double pos = 0.0;
        while (samplesUntilStep < remaining) {
            pos += samplesUntilStep;
            remaining -= samplesUntilStep;
            currentStep = (currentStep + 1) % numSteps;
            if (active[(size_t) currentStep]) {
                Fire f;
                f.sampleOffset = (int) pos;
                f.step   = currentStep;
                f.sliced = (mode == StepMode::Slice);
                f.slicePos = f.sliced
                    ? (sliceMap[(size_t) currentStep] + 0.5) / (double) numSlices
                    : -1.0;
                fires.push_back (f);
            }
            samplesUntilStep = samplesPerStep;
        }
        samplesUntilStep -= remaining;
    }

private:
    double sampleRate = 48000.0, bpm = 120.0;
    double samplesPerStep = 12000.0, samplesUntilStep = 0.0;
    int currentStep = -1;
};

} // namespace granular
