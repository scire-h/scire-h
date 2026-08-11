// Evolver tests: invariants of every mutation + pass distribution.
#include "../Source/Evolver.h"
#include "test_helpers.h"

using namespace granular;

static int countOn (const StepSequencer& s)
{
    int n = 0;
    for (int i = 0; i < s.numSteps; ++i) if (s.active[(size_t) i]) ++n;
    return n;
}
static bool slicesInRange (const StepSequencer& s)
{
    for (int i = 0; i < s.numSteps; ++i)
        if (s.sliceMap[(size_t) i] < 0 || s.sliceMap[(size_t) i] >= s.numSlices)
            return false;
    return true;
}

int main()
{
    std::printf ("Evolver tests:\n");

    // 10k random mutations never corrupt the pattern arrays
    {
        StepSequencer s;
        s.numSteps = 16;
        s.mode = StepMode::Slice;
        for (int i = 0; i < 16; i += 4) s.active[(size_t) i] = true;
        Evolver e (7);
        bool ok = true;
        for (int k = 0; k < 10000; ++k) {
            e.applyMutation (s, e.pickWeighted (true));
            const int on = countOn (s);
            if (on < 0 || on > s.numSteps || ! slicesInRange (s)) { ok = false; break; }
        }
        CHECK (ok, "10k mutations keep pattern within bounds");
    }

    // DensityUp adds, DensityDown removes, ClearZone clears a run
    {
        StepSequencer s;
        s.numSteps = 16;
        Evolver e (11);
        e.applyMutation (s, Mutation::DensityUp);
        CHECK (countOn (s) >= 1, "density+ turns cells on from empty");

        for (int i = 0; i < 16; ++i) s.active[(size_t) i] = true;
        e.applyMutation (s, Mutation::DensityDown);
        CHECK (countOn (s) < 16, "density- removes cells from full");

        for (int i = 0; i < 16; ++i) s.active[(size_t) i] = true;
        e.applyMutation (s, Mutation::ClearZone);
        const int cleared = 16 - countOn (s);
        INFO ("clear-zone removed %d consecutive cells", cleared);
        CHECK (cleared >= 3 && cleared <= 5, "clear-zone blanks a 3-5 cell run");
    }

    // Shift preserves the on-count; Mirror twice with same side is identity-ish
    {
        StepSequencer s;
        s.numSteps = 16;
        for (int i : { 0, 3, 7, 12 }) s.active[(size_t) i] = true;
        Evolver e (13);
        const int before = countOn (s);
        e.applyMutation (s, Mutation::Shift);
        CHECK (countOn (s) == before, "shift preserves the number of on-cells");
        e.applyMutation (s, Mutation::Swap);
        CHECK (countOn (s) == before, "swap preserves the number of on-cells");
        e.applyMutation (s, Mutation::PolyShift);
        CHECK (countOn (s) == before, "polyshift preserves the number of on-cells");
    }

    // Reslice does nothing in Hit mode
    {
        StepSequencer s;
        s.numSteps = 16;
        s.mode = StepMode::Hit;
        for (int i = 0; i < 16; ++i) s.active[(size_t) i] = true;
        auto slicesBefore = s.sliceMap;
        Evolver e (17);
        e.applyMutation (s, Mutation::Reslice);
        CHECK (s.sliceMap == slicesBefore, "reslice is a no-op in hit mode");
    }

    // Pass distribution over many bars ≈ 15/63/18/4
    {
        StepSequencer s;
        s.numSteps = 16;
        for (int i = 0; i < 16; i += 2) s.active[(size_t) i] = true;
        Evolver e (23);
        int counts[4] = { 0, 0, 0, 0 };
        const int trials = 20000;
        for (int k = 0; k < trials; ++k) {
            const int p = e.evolveBar (s);
            if (p >= 0 && p <= 3) ++counts[p];
        }
        const double p0 = (double) counts[0] / trials;
        const double p1 = (double) counts[1] / trials;
        const double p2 = (double) counts[2] / trials;
        const double p3 = (double) counts[3] / trials;
        INFO ("pass distribution: %.3f / %.3f / %.3f / %.3f", p0, p1, p2, p3);
        CHECK (std::abs (p0 - 0.15) < 0.02 && std::abs (p1 - 0.63) < 0.02
                 && std::abs (p2 - 0.18) < 0.02 && std::abs (p3 - 0.04) < 0.01,
               "0/1/2/3-mutation bars follow the 15/63/18/4 distribution");
    }

    return testSummary ("Evolver");
}
