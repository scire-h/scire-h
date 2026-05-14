// Unit tests for OTAVCA.
#include "../Source/OTAVCA.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

namespace {

void test_zero_gain_silences() {
    OTAVCA v;
    v.prepare (48000.0);
    v.setControlGain (0.0f);
    v.setDrive (1.4f);
    for (int i = 0; i < 1000; ++i)
        assert (v.processSample (0.5f) == 0.0f);
    std::puts ("  [pass] gain=0 silences the output");
}

void test_linearish_at_low_levels() {
    OTAVCA v;
    v.prepare (48000.0);
    v.setControlGain (1.0f);
    v.setDrive (1.4f);
    /* For small |x| the response should be near linear:
       tanh(x*drive) ≈ x*drive for x << 1/drive. */
    for (float x : {0.01f, 0.05f, 0.1f}) {
        const float y = v.processSample (x);
        const float ideal = x * 1.4f;
        const float err   = std::fabs (y - ideal) / ideal;
        assert (err < 0.05f);
    }
    std::puts ("  [pass] near-linear at low input levels");
}

void test_saturation_compresses_loud() {
    OTAVCA v;
    v.prepare (48000.0);
    v.setControlGain (1.0f);
    v.setDrive (1.4f);
    /* A unit-amplitude input shouldn't blow past +1 -- the tanh
       compresses it. */
    const float y1 = v.processSample (1.0f);
    const float y2 = v.processSample (4.0f);   // even way over the rails
    std::printf ("  [info] y(1)=%.3f  y(4)=%.3f\n", y1, y2);
    std::fflush (stdout);
    assert (y1 < 1.0f);     // soft-knee compression
    assert (y2 < 1.05f);    // tanh bounded by 1
    std::puts ("  [pass] tanh saturation keeps output bounded");
}

void test_envelope_scales_output() {
    OTAVCA v;
    v.prepare (48000.0);
    v.setDrive (1.0f);
    /* envelope ramps -- output should scale with it. */
    v.setControlGain (0.5f);
    const float yLow = v.processSample (0.2f);
    v.setControlGain (1.0f);
    const float yHi  = v.processSample (0.2f);
    std::printf ("  [info] yLow=%.4f yHi=%.4f ratio=%.3f\n",
                 yLow, yHi, yHi / yLow);
    std::fflush (stdout);
    /* should be very close to 2x since input is well below saturation */
    const float r = yHi / yLow;
    assert (r > 1.9f && r < 2.1f);
    std::puts ("  [pass] control gain scales output linearly");
}

} // namespace

int main() {
    std::puts ("OTAVCA tests:");
    test_zero_gain_silences();
    test_linearish_at_low_levels();
    test_saturation_compresses_loud();
    test_envelope_scales_output();
    std::puts ("All OTAVCA tests passed.");
    return 0;
}
