// GrainEngine tests: density, boundedness, envelope anti-click, freeze/scan,
// reverse grains, pitch rate.
#include "../Source/GrainEngine.h"
#include "test_helpers.h"
#include <vector>

using namespace granular;

static GrainEngine makeEngine (double sr = 48000.0)
{
    GrainEngine e;
    e.setSampleRate (sr);
    e.setBpm (120.0);
    e.setBuffer (SourceGen::generate (SourceType::Sine, 220.0, 2.0, sr));
    return e;
}

int main()
{
    std::printf ("GrainEngine tests:\n");
    const double sr = 48000.0;
    const int block = 512;

    // free-rate density: 20 g/s for 5 s ≈ 100 grains (density jitter ±15 %)
    {
        auto e = makeEngine (sr);
        e.params.bpmSync = false;
        e.params.freeRate = 20.0;
        std::vector<float> L ((size_t) block), R ((size_t) block);
        const int totalFrames = (int) (5.0 * sr);
        for (int done = 0; done < totalFrames; done += block) {
            std::fill (L.begin(), L.end(), 0.0f);
            std::fill (R.begin(), R.end(), 0.0f);
            e.renderFree (L.data(), R.data(), block);
        }
        INFO ("grains fired in 5 s at 20 g/s: %ld", e.totalGrainsFired());
        CHECK (e.totalGrainsFired() > 80 && e.totalGrainsFired() < 120,
               "free-rate density lands near requested grains/sec");
    }

    // bpm sync: 120 BPM × 4 g/beat = 8 g/s → ~40 in 5 s
    {
        auto e = makeEngine (sr);
        e.params.bpmSync = true;
        e.params.perBeat = 4.0;
        std::vector<float> L ((size_t) block), R ((size_t) block);
        for (int done = 0; done < (int) (5.0 * sr); done += block) {
            std::fill (L.begin(), L.end(), 0.0f);
            std::fill (R.begin(), R.end(), 0.0f);
            e.renderFree (L.data(), R.data(), block);
        }
        INFO ("grains fired in 5 s at 120 BPM x 4/beat: %ld", e.totalGrainsFired());
        CHECK (e.totalGrainsFired() > 32 && e.totalGrainsFired() < 48,
               "bpm-synced density follows tempo");
    }

    // output stays finite and bounded with dense settings
    {
        auto e = makeEngine (sr);
        e.params.bpmSync = false;
        e.params.freeRate = 200.0;
        e.params.sizeMs = 200.0;
        e.params.pitchJit = 12.0;
        e.params.reverseProb = 0.5;
        std::vector<float> L ((size_t) block), R ((size_t) block);
        float pk = 0; bool finite = true;
        for (int done = 0; done < (int) (3.0 * sr); done += block) {
            std::fill (L.begin(), L.end(), 0.0f);
            std::fill (R.begin(), R.end(), 0.0f);
            e.renderFree (L.data(), R.data(), block);
            for (int i = 0; i < block; ++i) {
                if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) finite = false;
                pk = std::max ({ pk, std::abs (L[(size_t) i]), std::abs (R[(size_t) i]) });
            }
        }
        INFO ("dense-cloud peak: %.2f", pk);
        CHECK (finite, "dense cloud output is finite");
        CHECK (pk < 64.0f, "dense cloud output is bounded (voice pool caps energy)");
    }

    // a single triggered hann grain starts and ends near zero (no clicks)
    {
        auto e = makeEngine (sr);
        e.params.sizeMs = 100.0;
        e.params.posJit = 0; e.params.pitchJit = 0; e.params.reverseProb = 0;
        e.trigger();
        const int dur = (int) (0.1 * sr);
        std::vector<float> L ((size_t) (dur + 256), 0.0f), R (L.size(), 0.0f);
        e.renderVoicesOnly (L.data(), R.data(), (int) L.size());
        float head = 0, tail = 0, mid = 0;
        for (int i = 0; i < 32; ++i)               head = std::max (head, std::abs (L[(size_t) i]));
        for (int i = dur - 32; i < dur; ++i)       tail = std::max (tail, std::abs (L[(size_t) i]));
        for (int i = dur / 2 - 64; i < dur / 2 + 64; ++i) mid = std::max (mid, std::abs (L[(size_t) i]));
        INFO ("hann grain head %.4f mid %.4f tail %.4f", head, mid, tail);
        CHECK (head < 0.1f && tail < 0.1f, "hann grain edges are near-silent (no click)");
        CHECK (mid > 0.2f, "hann grain speaks in the middle");
    }

    // freeze pins position, scan moves it
    {
        auto e = makeEngine (sr);
        e.params.bpmSync = false; e.params.freeRate = 50.0;
        e.params.scanSpeed = 0.5; e.params.freeze = true;
        const double p0 = e.params.position;
        std::vector<float> L ((size_t) block), R ((size_t) block);
        for (int done = 0; done < (int) sr; done += block) {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            e.renderFree (L.data(), R.data(), block);
        }
        CHECK (e.params.position == p0, "freeze holds position despite scan");

        e.params.freeze = false;
        for (int done = 0; done < (int) sr; done += block) {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            e.renderFree (L.data(), R.data(), block);
        }
        INFO ("position after 1 s of scan 0.5: %.3f (from %.3f)", e.params.position, p0);
        CHECK (e.params.position > p0, "scan advances position when not frozen");
    }

    // +12 st doubles playback rate: a grain consumes source twice as fast.
    {
        auto e = makeEngine (sr);
        e.params.sizeMs = 250.0;
        e.params.pitchSemis = 12.0;
        e.params.pitchJit = 0; e.params.posJit = 0; e.params.reverseProb = 0;
        e.params.position = 0.0;
        e.trigger();
        // render half the grain; the voice should still be alive (dur is in
        // OUTPUT samples, unchanged by pitch)
        const int dur = (int) (0.25 * sr);
        std::vector<float> L ((size_t) (dur / 2), 0.0f), R (L.size(), 0.0f);
        e.renderVoicesOnly (L.data(), R.data(), (int) L.size());
        CHECK (e.activeVoices() == 1, "pitched grain lasts its full output duration");
    }

    return testSummary ("GrainEngine");
}
