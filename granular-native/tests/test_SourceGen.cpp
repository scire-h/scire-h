// SourceGen tests: length, boundedness, periodicity, stereo behaviour.
#include "../Source/SourceGen.h"
#include "test_helpers.h"

using namespace granular;

static bool allFinite (const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite (x)) return false;
    return true;
}
static float peak (const std::vector<float>& v)
{
    float p = 0;
    for (float x : v) p = std::max (p, std::abs (x));
    return p;
}

int main()
{
    std::printf ("SourceGen tests:\n");
    const double sr = 48000.0;

    // every type: correct length, finite, bounded
    const SourceType types[] = { SourceType::Sine, SourceType::Tri, SourceType::Saw,
                                 SourceType::Sqr, SourceType::White, SourceType::Pink,
                                 SourceType::Imp, SourceType::Chord };
    bool lenOk = true, finOk = true, ampOk = true;
    for (auto t : types) {
        auto b = SourceGen::generate (t, 220.0, 2.0, sr);
        lenOk &= (b.length() == (int) std::lround (2.0 * sr));
        finOk &= allFinite (b.left) && allFinite (b.right);
        ampOk &= peak (b.left) <= 1.001f && peak (b.right) <= 1.001f;
    }
    CHECK (lenOk, "all 8 types produce exactly 2.0 s of samples");
    CHECK (finOk, "all outputs are finite");
    CHECK (ampOk, "all outputs stay within +/-1");

    // sine periodicity: 220 Hz at 48 k → period ≈ 218.18 samples.
    // The sample nearest one full period should be close to sample 0.
    {
        auto b = SourceGen::generate (SourceType::Sine, 220.0, 1.0, sr);
        const double period = sr / 220.0;
        const int p = (int) std::lround (period * 11);   // 11 periods: integer-ish
        double maxDiff = 0.0;
        for (int i = 0; i < 100; ++i)
            maxDiff = std::max (maxDiff, (double) std::abs (b.left[(size_t) i] - b.left[(size_t) (i + p)]));
        INFO ("sine self-similarity across 11 periods: max diff %.4f", maxDiff);
        CHECK (maxDiff < 0.05, "sine repeats with its expected period");
    }

    // noise channels are decorrelated; pitched channels are a shifted copy
    {
        auto n = SourceGen::generate (SourceType::White, 220.0, 1.0, sr);
        double corr = 0;
        for (int i = 0; i < n.length(); ++i) corr += n.left[(size_t) i] * n.right[(size_t) i];
        corr /= n.length();
        INFO ("white L/R correlation: %.5f", corr);
        CHECK (std::abs (corr) < 0.01, "white noise channels are independent");

        auto s = SourceGen::generate (SourceType::Sine, 220.0, 1.0, sr);
        const int shift = (int) std::lround (sr * 0.0023);
        bool shifted = true;
        for (int i = 0; i < 1000; ++i)
            if (std::abs (s.right[(size_t) i] - s.left[(size_t) ((i + shift) % s.length())]) > 1e-6f)
                { shifted = false; break; }
        CHECK (shifted, "pitched right channel is the phase-shifted left");
    }

    // determinism: same seed → identical buffers
    {
        auto a = SourceGen::generate (SourceType::Pink, 220.0, 0.5, sr, 42);
        auto b = SourceGen::generate (SourceType::Pink, 220.0, 0.5, sr, 42);
        CHECK (a.left == b.left, "same seed reproduces identical pink noise");
    }

    return testSummary ("SourceGen");
}
