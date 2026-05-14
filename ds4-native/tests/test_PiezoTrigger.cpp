// Unit tests for PiezoTrigger.
#include "../Source/PiezoTrigger.h"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

constexpr double kSr = 48000.0;

void test_no_trigger_below_threshold() {
    PiezoTrigger p;
    p.prepare (kSr);
    p.setThresholds (0.30f, 0.05f);
    /* feed quiet noise -- should never fire */
    int fires = 0;
    for (int i = 0; i < 5000; ++i) {
        const float x = 0.1f * std::sin (0.05f * i);
        if (p.processSample (x) > 0.0f) ++fires;
    }
    assert (fires == 0);
    std::puts ("  [pass] sub-threshold signal never triggers");
}

void test_hit_triggers_once() {
    PiezoTrigger p;
    p.prepare (kSr);
    p.setThresholds (0.30f, 0.05f);
    p.setAttackMs (0.5f);
    p.setReleaseMs (12.0f);
    p.setHoldMs (8.0f);

    int fires = 0;
    /* short loud pulse -- piezo-like */
    for (int i = 0; i < 100; ++i) {
        const float env = std::exp (-i * 0.05f);
        const float x   = env * std::sin (0.6f * i);
        if (p.processSample (x) > 0.0f) ++fires;
    }
    /* tail -- should remain silent */
    for (int i = 0; i < 5000; ++i)
        if (p.processSample (0.0f) > 0.0f) ++fires;

    std::printf ("  [info] hit produced %d trigger(s)\n", fires);
    std::fflush (stdout);
    assert (fires == 1);
    std::puts ("  [pass] hit produces exactly one trigger");
}

void test_hold_time_prevents_retrigger() {
    PiezoTrigger p;
    p.prepare (kSr);
    p.setThresholds (0.30f, 0.05f);
    p.setHoldMs (20.0f);

    int fires = 0;
    /* two hits 5 ms apart -- hold time should swallow the second */
    auto strike = [&] {
        for (int i = 0; i < 20; ++i) {
            const float env = std::exp (-i * 0.1f);
            const float x   = env * 0.9f;
            if (p.processSample (x) > 0.0f) ++fires;
        }
    };
    strike();
    /* 5 ms gap -- inside the 20 ms hold window */
    for (int i = 0; i < (int)(kSr * 0.005); ++i) p.processSample (0.0f);
    strike();
    std::printf ("  [info] two strikes within hold window produced %d trigger(s)\n", fires);
    std::fflush (stdout);
    assert (fires == 1);
    std::puts ("  [pass] hold time prevents retriggering");
}

void test_velocity_reported() {
    PiezoTrigger p;
    p.prepare (kSr);
    p.setThresholds (0.30f, 0.05f);
    p.setHoldMs (8.0f);

    float reported = 0.0f;
    for (int i = 0; i < 50; ++i) {
        const float env = std::exp (-i * 0.05f);
        const float v   = p.processSample (env * 0.7f);
        if (v > 0.0f) { reported = v; break; }
    }
    std::printf ("  [info] strike reported velocity = %.3f\n", reported);
    std::fflush (stdout);
    assert (reported > 0.30f);          // crossed the high threshold
    assert (reported <= 1.0f);          // clamped
    std::puts ("  [pass] velocity reflects strike intensity");
}

} // namespace

int main() {
    std::puts ("PiezoTrigger tests:");
    test_no_trigger_below_threshold();
    test_hit_triggers_once();
    test_hold_time_prevents_retrigger();
    test_velocity_reported();
    std::puts ("All PiezoTrigger tests passed.");
    return 0;
}
