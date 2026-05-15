// Unit tests for MoogLadder.
//
// Verifies:
//   1. DC gain ≈ 1 with zero resonance.
//   2. Cutoff sweep produces monotonically more attenuation of a
//      Nyquist-adjacent tone as cutoff drops.
//   3. High resonance produces sustained self-oscillation when fed a
//      brief impulse.
//   4. Output stays finite and bounded for pathological inputs.

#include "../Source/MoogLadder.h"

#include <cassert>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

namespace {

constexpr double kSr = 48000.0;
constexpr double kPi = 3.141592653589793;

double bin_mag (const std::vector<float>& x, double freqHz, double sr) {
    const std::size_t N = x.size();
    std::complex<double> sum (0.0, 0.0);
    const double w = -2.0 * kPi * freqHz / sr;
    for (std::size_t n = 0; n < N; ++n)
        sum += std::complex<double> (x[n]) * std::polar (1.0, w * (double)n);
    return std::abs (sum) / (double)N;
}

void test_dc_gain_unity() {
    MoogLadder f;
    f.prepare (kSr);
    f.setCutoff (5000.0f);
    f.setResonance (0.0f);

    /* DC step; let the filter settle. */
    float y = 0.0f;
    for (int i = 0; i < 4000; ++i) y = f.processSample (1.0f);
    std::printf ("  [info] DC step settled to %.4f\n", y);
    std::fflush (stdout);
    assert (std::fabs (y - 1.0f) < 0.02f);
    std::puts ("  [pass] DC gain ≈ 1 with zero resonance");
}

void test_cutoff_attenuates_high_freq() {
    /* Feed a 10 kHz sine and verify that lowering the cutoff makes
       the magnitude drop monotonically. */
    auto measure = [] (float cutoffHz) {
        MoogLadder f;
        f.prepare (kSr);
        f.setCutoff (cutoffHz);
        f.setResonance (0.0f);
        std::vector<float> out (4096);
        for (std::size_t n = 0; n < out.size(); ++n) {
            const float x = std::sin (2.0f * (float)kPi * 10000.0f
                                      * (float)n / (float)kSr);
            out[n] = f.processSample (x);
        }
        return bin_mag (out, 10000.0, kSr);
    };

    const double m12k = measure (12000.0f);
    const double m6k  = measure (6000.0f);
    const double m3k  = measure (3000.0f);
    std::printf ("  [info] 10kHz mag @ fc=12k: %.4f  fc=6k: %.4f  fc=3k: %.4f\n",
                 m12k, m6k, m3k);
    std::fflush (stdout);
    assert (m12k > m6k);
    assert (m6k  > m3k);
    std::puts ("  [pass] cutoff sweep attenuates high freq monotonically");
}

void test_self_oscillation_with_high_q() {
    MoogLadder f;
    f.prepare (kSr);
    f.setCutoff (1000.0f);
    f.setResonance (1.02f);     // just past self-oscillation

    /* Kick the filter with a unit impulse so the resonance has
       something to feed back on. */
    float y = f.processSample (1.0f);
    (void) y;

    /* Run for 0.5 s and verify that the output is still significantly
       non-zero (the resonance is sustaining oscillation). */
    float maxAbs = 0.0f;
    for (int i = 0; i < (int)(kSr * 0.5); ++i)
        maxAbs = std::max (maxAbs, std::fabs (f.processSample (0.0f)));
    std::printf ("  [info] self-osc envelope max after 0.5 s = %.4f\n", maxAbs);
    std::fflush (stdout);
    assert (maxAbs > 0.05f);
    std::puts ("  [pass] high resonance produces sustained self-oscillation");
}

void test_bounded_under_extreme_input() {
    MoogLadder f;
    f.prepare (kSr);
    f.setCutoff (2000.0f);
    f.setResonance (1.0f);

    /* Feed a chaotic signal mixed with DC -- should stay bounded. */
    for (int i = 0; i < 100000; ++i) {
        const float x = 4.0f * std::sin (0.13f * i) + 2.0f;
        const float y = f.processSample (x);
        assert (std::isfinite (y));
        assert (std::fabs (y) < 8.0f);
    }
    std::puts ("  [pass] output stays finite and bounded under chaotic input");
}

} // namespace

int main() {
    std::puts ("MoogLadder tests:");
    test_dc_gain_unity();
    test_cutoff_attenuates_high_freq();
    test_self_oscillation_with_high_q();
    test_bounded_under_extreme_input();
    std::puts ("All MoogLadder tests passed.");
    return 0;
}
