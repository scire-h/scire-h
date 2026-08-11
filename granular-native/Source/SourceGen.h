#pragma once
// SourceGen — synthesises the granular source buffers.
// Direct port of granular.html's generateBuffer(): sine / tri / saw / sqr /
// white / pink (Paul Kellet) / imp (click train) / chord (stacked sines with
// slow FM wobble). Pure C++17, no JUCE — testable with g++ alone.

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace granular {

enum class SourceType { Sine, Tri, Saw, Sqr, White, Pink, Imp, Chord };

struct StereoBuffer {
    std::vector<float> left, right;
    double sampleRate = 48000.0;
    int length() const { return (int) left.size(); }
    double durationSeconds() const {
        return sampleRate > 0 ? (double) left.size() / sampleRate : 0.0;
    }
};

class SourceGen {
public:
    // Generate `seconds` of the given source at `freq` Hz into a stereo
    // buffer. The right channel of pitched sources is phase-shifted by
    // ~2.3 ms for stereo width, matching the web version.
    static StereoBuffer generate (SourceType type, double freq,
                                  double seconds, double sampleRate,
                                  uint32_t seed = 0x9e3779b9u)
    {
        StereoBuffer buf;
        buf.sampleRate = sampleRate;
        const int len = std::max (1, (int) std::lround (seconds * sampleRate));
        buf.left.assign  ((size_t) len, 0.0f);
        buf.right.assign ((size_t) len, 0.0f);

        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> uni (-1.0f, 1.0f);

        renderChannel (buf.left, type, freq, sampleRate, rng, uni);

        const bool noise = (type == SourceType::White || type == SourceType::Pink);
        if (noise) {
            // independent noise per channel
            renderChannel (buf.right, type, freq, sampleRate, rng, uni);
        } else {
            // phase-shifted copy for width
            const int shift = (int) std::lround (sampleRate * 0.0023);
            for (int i = 0; i < len; ++i)
                buf.right[(size_t) i] = buf.left[(size_t) ((i + shift) % len)];
        }
        return buf;
    }

private:
    static void renderChannel (std::vector<float>& d, SourceType type,
                               double f, double sr, std::mt19937& rng,
                               std::uniform_real_distribution<float>& uni)
    {
        const int len = (int) d.size();
        const double omega = 2.0 * M_PI * f / sr;

        switch (type) {
            case SourceType::Sine:
                for (int i = 0; i < len; ++i)
                    d[(size_t) i] = (float) (std::sin (omega * i) * 0.9);
                break;

            case SourceType::Tri:
                for (int i = 0; i < len; ++i) {
                    double p = std::fmod (i * f / sr, 1.0);
                    d[(size_t) i] = (float) ((4.0 * std::abs (p - 0.5) - 1.0) * 0.9);
                }
                break;

            case SourceType::Saw:
                for (int i = 0; i < len; ++i) {
                    double p = std::fmod (i * f / sr, 1.0);
                    d[(size_t) i] = (float) ((2.0 * p - 1.0) * 0.85);
                }
                break;

            case SourceType::Sqr:
                for (int i = 0; i < len; ++i) {
                    double p = std::fmod (i * f / sr, 1.0);
                    d[(size_t) i] = (float) ((p < 0.5 ? 1.0 : -1.0) * 0.7);
                }
                break;

            case SourceType::White:
                for (int i = 0; i < len; ++i)
                    d[(size_t) i] = uni (rng) * 0.7f;
                break;

            case SourceType::Pink: {
                // Paul Kellet economy pink filter (as in the web version)
                float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
                for (int i = 0; i < len; ++i) {
                    const float wn = uni (rng);
                    b0 = 0.99886f * b0 + wn * 0.0555179f;
                    b1 = 0.99332f * b1 + wn * 0.0750759f;
                    b2 = 0.96900f * b2 + wn * 0.1538520f;
                    b3 = 0.86650f * b3 + wn * 0.3104856f;
                    b4 = 0.55000f * b4 + wn * 0.5329522f;
                    b5 = -0.7616f * b5 - wn * 0.0168980f;
                    const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + wn * 0.5362f;
                    b6 = wn * 0.115926f;
                    d[(size_t) i] = pink * 0.13f;
                }
                break;
            }

            case SourceType::Imp: {
                const int step = std::max (1, (int) std::lround (sr / std::max (1.0, f / 4.0)));
                float prev = 0.0f;
                std::uniform_real_distribution<float> coin (0.0f, 1.0f);
                for (int i = 0; i < len; ++i) {
                    if (i % step == 0)
                        prev = (i == 0 ? 0.95f
                                       : (coin (rng) < 0.5f ? 1.0f : -1.0f) * 0.95f);
                    else
                        prev *= 0.92f;
                    d[(size_t) i] = prev;
                }
                break;
            }

            case SourceType::Chord: {
                static const double ratios[] = { 1.0, 1.5, 2.0, 2.25 }; // 2.5*0.9
                const int nRatios = 4;
                for (int i = 0; i < len; ++i) {
                    double v = 0.0;
                    for (int k = 0; k < nRatios; ++k)
                        v += std::sin (2.0 * M_PI * f * ratios[k] * i / sr);
                    v += 0.2 * std::sin (2.0 * M_PI * 0.3 * i / sr)
                             * std::sin (2.0 * M_PI * f * 1.01 * i / sr);
                    d[(size_t) i] = (float) (v / (nRatios + 0.2) * 0.85);
                }
                break;
            }
        }
    }
};

} // namespace granular
