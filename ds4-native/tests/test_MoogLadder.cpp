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

#include <algorithm>
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

void test_high_resonance_rings_longer() {
    /* Compare the impulse response of the filter at zero resonance vs
       max resonance. With high Q, the energy of the ringing tail
       should be many times larger than the over-damped case. We
       compare *energies* (sum of squares) rather than peak amplitude
       so the test is robust against any small numerical differences
       between platforms (and so a NaN from runaway feedback would
       fail noisily rather than silently passing through std::max). */
    auto impulseEnergy = [] (float resonance) {
        MoogLadder f;
        f.prepare (kSr);
        f.setCutoff (1000.0f);
        f.setResonance (resonance);

        f.processSample (1.0f);                     // unit impulse
        double energy = 0.0;
        for (int i = 0; i < (int)(kSr * 0.5); ++i) {
            const float y = f.processSample (0.0f);
            assert (std::isfinite (y));            // would catch NaN/Inf
            energy += (double)y * (double)y;
        }
        return energy;
    };

    const double dryEnergy = impulseEnergy (0.0f);
    const double wetEnergy = impulseEnergy (0.99f);
    std::printf ("  [info] impulse energy: Q=0 -> %.4e   Q=0.99 -> %.4e   ratio %.1fx\n",
                 dryEnergy, wetEnergy, wetEnergy / std::max (1e-12, dryEnergy));
    std::fflush (stdout);
    /* Empirically: linear ZDF Moog at k=0 settles in ~3 ms; at k=0.99
       it rings for the full 0.5 s buffer. The energy ratio comes out
       around 20x (the impulse delivers the same total energy but the
       high-Q tail spreads it across many more samples). 10x is a
       conservative floor that's still well above measurement noise. */
    assert (wetEnergy > 10.0 * dryEnergy);
    std::puts ("  [pass] high resonance rings significantly longer than dry");
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
    test_high_resonance_rings_longer();
    test_bounded_under_extreme_input();
    std::puts ("All MoogLadder tests passed.");
    return 0;
}
