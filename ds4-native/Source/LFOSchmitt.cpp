#include "LFOSchmitt.h"

void LFOSchmitt::prepare(double sampleRate) { sr = sampleRate; reset(); }
void LFOSchmitt::reset() {
    phase = 0.0f;
    rising = true;
}

float LFOSchmitt::processSample() {
    /* 4 units of phase per full cycle -- same as TriangleCoreVCO. */
    const float step = 4.0f * freq / (float)sr;
    if (rising) {
        phase += step;
        if (phase >= 1.0f) { phase = 2.0f - phase; rising = false; }
    } else {
        phase -= step;
        if (phase <= -1.0f) { phase = -2.0f - phase; rising = true; }
    }

    switch (mode) {
        case Output::Triangle: return amp * phase;
        case Output::Square:   return amp * (rising ? 1.0f : -1.0f);
        case Output::Sine:     return amp * phase * (1.5f - 0.5f * phase * phase);
    }
    return 0.0f;
}
