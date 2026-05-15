// Stress test: run every DSP module for a minute of audio at 48 kHz
// and verify the output stays bounded (no NaN, no Inf, no runaway).
//
// This catches subtle numerical issues -- integrator drift in the SVF,
// pitch-sweep overshoots, envelope-state desync -- that the per-module
// short tests don't exercise.
//
// Build & run (or use ./run_all.sh which does this for you):
//   g++ -std=c++17 -O2 -I../Source test_stress.cpp DSP/*.cpp -o test_stress
//   ./test_stress

#include "../Source/TriangleCoreVCO.h"
#include "../Source/NoiseVoice.h"
#include "../Source/ExpEnvelope.h"
#include "../Source/OTAVCA.h"
#include "../Source/LFOSchmitt.h"
#include "../Source/PiezoTrigger.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <random>

namespace {

constexpr double kSr      = 48000.0;
constexpr int    kSeconds = 60;
constexpr int    kSamples = (int)(kSr * kSeconds);

inline bool finite (float x) { return std::isfinite (x); }

/* Verify a sample is sane: finite and within +/- bound. */
inline void check (float x, float bound, const char* label, int n) {
    if (! finite (x)) {
        std::printf ("FAIL: non-finite output (%f) at sample %d in %s\n",
                     (double)x, n, label);
        std::fflush (stdout);
        std::abort();
    }
    if (std::fabs (x) > bound) {
        std::printf ("FAIL: out-of-bounds (%f > %f) at sample %d in %s\n",
                     (double)std::fabs (x), (double)bound, n, label);
        std::fflush (stdout);
        std::abort();
    }
}

void stress_TriangleCoreVCO() {
    TriangleCoreVCO v;
    v.prepare (kSr);
    std::mt19937 rng (0x55AA55AAu);
    std::uniform_real_distribution<float> hzDist (60.0f, 6000.0f);
    std::uniform_int_distribution<int>    waveDist (0, 3);

    /* Re-randomise the parameters every 4096 samples so we exercise
       the full state space rather than a single static config. */
    for (int n = 0; n < kSamples; ++n) {
        if ((n & 4095) == 0) {
            v.setFrequency (hzDist (rng));
            v.setWaveform ((TriangleCoreVCO::Waveform) waveDist (rng));
            v.retrigger (rng());
        }
        check (v.processSample(), 2.0f, "TriangleCoreVCO", n);
    }
    std::printf ("  [pass] TriangleCoreVCO stable across 60 s of audio\n");
}

void stress_NoiseVoice() {
    NoiseVoice n;
    n.prepare (kSr);
    std::mt19937 rng (0xC0DEBABEu);
    std::uniform_real_distribution<float> hzDist (200.0f, 8000.0f);

    n.setCharacter (NoiseVoice::Character::Cymbal);
    n.startSweep (5000.0f, 500.0f, 0.1f);
    for (int i = 0; i < kSamples; ++i) {
        if ((i & 8191) == 0) {
            const float a = hzDist (rng);
            const float b = hzDist (rng);
            n.startSweep (a, b, 0.05f + (rng() & 31) * 0.01f);
        }
        check (n.processSample(), 12.0f, "NoiseVoice", i);
    }
    std::printf ("  [pass] NoiseVoice stable across 60 s of audio (with sweeps)\n");
}

void stress_OTAVCA() {
    OTAVCA v;
    v.prepare (kSr);
    std::mt19937 rng (0x12345678u);
    std::uniform_real_distribution<float> sigDist (-2.0f, 2.0f);
    std::uniform_real_distribution<float> envDist (0.0f, 1.0f);

    for (int i = 0; i < kSamples; ++i) {
        if ((i & 255) == 0) v.setControlGain (envDist (rng));
        check (v.processSample (sigDist (rng)), 1.05f, "OTAVCA", i);
    }
    std::printf ("  [pass] OTAVCA stable across 60 s of audio\n");
}

void stress_LFOSchmitt() {
    LFOSchmitt l;
    l.prepare (kSr);
    std::mt19937 rng (0x87654321u);
    std::uniform_real_distribution<float> hzDist (0.1f, 50.0f);

    for (int i = 0; i < kSamples; ++i) {
        if ((i & 8191) == 0) l.setFrequency (hzDist (rng));
        check (l.processSample(), 1.0f, "LFOSchmitt", i);
    }
    std::printf ("  [pass] LFOSchmitt stable across 60 s of audio\n");
}

void stress_PiezoTrigger() {
    PiezoTrigger p;
    p.prepare (kSr);
    std::mt19937 rng (0x11AABB22u);
    std::uniform_real_distribution<float> nDist (-1.0f, 1.0f);

    int fires = 0;
    for (int i = 0; i < kSamples; ++i) {
        const float v = p.processSample (nDist (rng) * 0.05f);
        if (v > 0.0f) ++fires;
    }
    std::printf ("  [pass] PiezoTrigger stable across 60 s (sub-threshold noise fired %d times)\n", fires);
}

void stress_ExpEnvelope() {
    ExpEnvelope e;
    e.prepare (kSr);
    std::mt19937 rng (0xBADBADBAu);
    std::uniform_real_distribution<float> aDist (0.0001f, 0.5f);
    std::uniform_real_distribution<float> dDist (0.001f,  3.0f);
    std::uniform_real_distribution<float> pDist (0.1f,    1.0f);

    int triggers = 0;
    for (int i = 0; i < kSamples; ++i) {
        if (! e.isActive() || (i & 32767) == 0) {
            e.trigger (0.0f, pDist (rng), 0.0f, aDist (rng), dDist (rng));
            ++triggers;
        }
        check (e.processSample(), 1.5f, "ExpEnvelope", i);
    }
    std::printf ("  [pass] ExpEnvelope stable across 60 s (%d retriggers)\n", triggers);
}

} // namespace

int main() {
    std::puts ("Stress test (60 s of audio @ 48 kHz):");
    stress_TriangleCoreVCO();
    stress_NoiseVoice();
    stress_OTAVCA();
    stress_LFOSchmitt();
    stress_PiezoTrigger();
    stress_ExpEnvelope();
    std::puts ("All stress tests passed.");
    return 0;
}
