// StepSequencer tests: timing, intra-block offsets, modes, triplet rates.
#include "../Source/StepSequencer.h"
#include "test_helpers.h"

using namespace granular;

int main()
{
    std::printf ("StepSequencer tests:\n");
    const double sr = 48000.0;

    // 120 BPM, 1/16 → step every 60/120/4 s = 0.125 s = 6000 samples
    {
        StepSequencer s;
        s.numSteps = 16;
        s.setRate (16);
        s.prepare (sr, 120.0);
        for (int i = 0; i < StepSequencer::kMaxSteps; ++i) s.active[(size_t) i] = true;

        std::vector<StepSequencer::Fire> fires;
        s.advance ((int) sr, fires);   // one second
        INFO ("fires in 1 s at 120 BPM 1/16: %d", (int) fires.size());
        CHECK ((int) fires.size() == 8 || (int) fires.size() == 9,
               "1/16 at 120 BPM fires 8 steps per second");

        bool spacing = true;
        for (size_t i = 1; i < fires.size(); ++i) {
            // offsets are within one advance() call, so spacing check needs
            // cumulative reconstruction — here all fires are in one call:
            const int d = fires[i].sampleOffset - fires[i - 1].sampleOffset;
            if (std::abs (d - 6000) > 1) spacing = false;
        }
        CHECK (spacing, "steps are exactly 6000 samples apart");
    }

    // triplet rate 1/8T (rateN 12): 120 BPM → 6 steps/sec
    {
        StepSequencer s;
        s.numSteps = 12;
        s.setRate (12);
        s.prepare (sr, 120.0);
        for (int i = 0; i < StepSequencer::kMaxSteps; ++i) s.active[(size_t) i] = true;
        std::vector<StepSequencer::Fire> fires;
        s.advance ((int) sr, fires);
        INFO ("fires in 1 s at 120 BPM 1/8T: %d", (int) fires.size());
        CHECK ((int) fires.size() == 6 || (int) fires.size() == 7,
               "1/8T at 120 BPM fires 6 steps per second");
    }

    // only active steps fire; step indices wrap at numSteps
    {
        StepSequencer s;
        s.numSteps = 8;
        s.setRate (16);
        s.prepare (sr, 120.0);
        s.active[(size_t) 0] = true;
        s.active[(size_t) 4] = true;
        std::vector<StepSequencer::Fire> fires;
        s.advance ((int) (2.0 * sr), fires);   // 16 steps → 2 pattern loops
        bool onlyActive = true, wraps = false;
        for (auto& f : fires) {
            if (f.step != 0 && f.step != 4) onlyActive = false;
            if (f.step == 0) wraps = true;
        }
        INFO ("fires over 2 loops of 8 steps: %d", (int) fires.size());
        CHECK (onlyActive, "only active steps fire");
        CHECK (wraps && (int) fires.size() == 4, "pattern wraps and fires twice per loop");
    }

    // slice mode reports slice centre positions; hit mode reports -1
    {
        StepSequencer s;
        s.numSteps = 4;
        s.numSlices = 16;
        s.setRate (4);
        s.prepare (sr, 120.0);
        s.active[(size_t) 0] = true;
        s.sliceMap[(size_t) 0] = 8;
        s.mode = StepMode::Slice;
        std::vector<StepSequencer::Fire> fires;
        s.advance ((int) (2.0 * sr), fires);
        CHECK (! fires.empty() && fires[0].sliced
                 && std::abs (fires[0].slicePos - (8.5 / 16.0)) < 1e-9,
               "slice mode maps step to slice-centre position");

        s.mode = StepMode::Hit;
        s.reset();
        fires.clear();
        s.advance ((int) (2.0 * sr), fires);
        CHECK (! fires.empty() && ! fires[0].sliced, "hit mode marks fires unsliced");
    }

    // disabled sequencer stays silent
    {
        StepSequencer s;
        s.prepare (sr, 120.0);
        for (int i = 0; i < StepSequencer::kMaxSteps; ++i) s.active[(size_t) i] = true;
        s.enabled = false;
        std::vector<StepSequencer::Fire> fires;
        s.advance ((int) sr, fires);
        CHECK (fires.empty(), "disabled sequencer produces no fires");
    }

    return testSummary ("StepSequencer");
}
