// Unit tests for ExpEnvelope.
#include "../Source/ExpEnvelope.h"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

constexpr double kSr = 48000.0;

void test_idle_then_trigger() {
    ExpEnvelope e;
    e.prepare (kSr);
    assert (! e.isActive());
    assert (e.processSample() == 0.0f);

    e.trigger (0.0f, 1.0f, 0.0f, 0.005f, 0.1f);
    assert (e.isActive());
    /* the very first sample should already be past zero */
    const float s0 = e.processSample();
    assert (s0 > 0.0f);
    std::puts ("  [pass] trigger flips active and ramps from zero");
}

void test_decay_3tau_is_about_5_percent() {
    ExpEnvelope e;
    e.prepare (kSr);
    const float decay = 0.1f;
    e.trigger (0.0f, 1.0f, 0.0f, 0.001f, decay);
    /* skip past the linear attack */
    for (int i = 0; i < (int)(kSr * 0.002); ++i) e.processSample();
    const float vAtAttackEnd = e.currentValue();
    /* now run `decay` seconds and check we're ~5 % of the original */
    for (int i = 0; i < (int)(kSr * decay); ++i) e.processSample();
    const float vAfter = e.currentValue();
    const float ratio  = vAfter / vAtAttackEnd;
    std::printf ("  [info] after 3-tau decay, value = %.4f of peak\n", ratio);
    std::fflush (stdout);
    assert (ratio < 0.10f && ratio > 0.01f);    // 3-tau definition ~ 5 %
    std::puts ("  [pass] decay reaches ~5 % at the 3-tau mark");
}

void test_envelope_goes_inactive_eventually() {
    ExpEnvelope e;
    e.prepare (kSr);
    e.trigger (0.0f, 1.0f, 0.0f, 0.001f, 0.05f);
    /* run plenty long -- envelope should go idle */
    int n = 0;
    while (e.isActive() && n < (int)(kSr * 2.0)) { e.processSample(); ++n; }
    std::printf ("  [info] envelope went idle after %.3f s\n", n / kSr);
    std::fflush (stdout);
    assert (! e.isActive());
    std::puts ("  [pass] envelope becomes idle within tail time");
}

} // namespace

int main() {
    std::puts ("ExpEnvelope tests:");
    test_idle_then_trigger();
    test_decay_3tau_is_about_5_percent();
    test_envelope_goes_inactive_eventually();
    std::puts ("All ExpEnvelope tests passed.");
    return 0;
}
