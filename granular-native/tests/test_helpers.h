#pragma once
// Minimal assert helpers shared by the granular-native DSP tests.
// Same conventions as ds4-native/tests: [pass]/[FAIL] lines, non-zero exit
// on any failure, no framework dependencies.

#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;

#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if (cond) std::printf ("  [pass] %s\n", msg);                        \
        else      { std::printf ("  [FAIL] %s\n", msg); ++g_failures; }      \
    } while (0)

#define INFO(...)                                                            \
    do { std::printf ("  [info] " __VA_ARGS__); std::printf ("\n"); } while (0)

static inline int testSummary (const char* suite)
{
    if (g_failures == 0) { std::printf ("All %s tests passed.\n", suite); return 0; }
    std::printf ("%d %s test(s) FAILED.\n", g_failures, suite);
    return 1;
}
