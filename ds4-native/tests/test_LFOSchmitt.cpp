// Unit tests for LFOSchmitt.
#include "../Source/LFOSchmitt.h"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

constexpr double kSr = 48000.0;

int count_zero_crossings (LFOSchmitt& l, int numSamples) {
    int crossings = 0;
    float prev = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        const float s = l.processSample();
        if ((prev < 0.0f && s >= 0.0f) || (prev > 0.0f && s <= 0.0f))
            ++crossings;
        prev = s;
    }
    return crossings;
}

void test_frequency_matches_setting() {
    /* 2 Hz LFO over 1 second should produce ~4 zero crossings
       (one full cycle per 0.5 s, two crossings per cycle). */
    LFOSchmitt l;
    l.prepare (kSr);
    l.setFrequency (2.0f);
    l.setOutput (LFOSchmitt::Output::Triangle);
    l.setAmplitude (1.0f);
    const int n = count_zero_crossings (l, (int)kSr);
    std::printf ("  [info] 2 Hz triangle, 1 s -> %d zero crossings (expected ~4)\n", n);
    std::fflush (stdout);
    assert (n >= 3 && n <= 5);
    std::puts ("  [pass] frequency matches setting");
}

void test_amplitude_scaling() {
    LFOSchmitt l;
    l.prepare (kSr);
    l.setFrequency (5.0f);
    l.setOutput (LFOSchmitt::Output::Triangle);
    l.setAmplitude (0.4f);
    float maxAbs = 0.0f;
    for (int i = 0; i < (int)kSr; ++i)
        maxAbs = std::max (maxAbs, std::fabs (l.processSample()));
    std::printf ("  [info] amplitude 0.4 -> peak %.4f\n", maxAbs);
    std::fflush (stdout);
    assert (maxAbs > 0.35f && maxAbs <= 0.42f);
    std::puts ("  [pass] amplitude scales output");
}

void test_square_output_is_square() {
    LFOSchmitt l;
    l.prepare (kSr);
    l.setFrequency (3.0f);
    l.setOutput (LFOSchmitt::Output::Square);
    l.setAmplitude (1.0f);
    for (int i = 0; i < 1000; ++i) {
        const float s = l.processSample();
        assert (std::fabs (std::fabs (s) - 1.0f) < 1e-5f);
    }
    std::puts ("  [pass] square mode emits +/- 1 exactly");
}

} // namespace

int main() {
    std::puts ("LFOSchmitt tests:");
    test_frequency_matches_setting();
    test_amplitude_scaling();
    test_square_output_is_square();
    std::puts ("All LFOSchmitt tests passed.");
    return 0;
}
