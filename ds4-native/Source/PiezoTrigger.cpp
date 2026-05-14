#include "PiezoTrigger.h"
#include <cmath>
#include <algorithm>

void PiezoTrigger::prepare(double sampleRate) {
    sr = sampleRate;
    setAttackMs(0.5f);
    setReleaseMs(15.0f);
    setHoldMs(8.0f);
    reset();
}

void PiezoTrigger::reset() {
    peak       = 0.0f;
    armed      = true;
    holdRemain = 0;
}

void PiezoTrigger::setAttackMs(float ms) {
    const float tau = std::max(0.05f, ms) * 0.001f;
    attackCoeff = std::exp(-1.0f / (tau * (float)sr));
}
void PiezoTrigger::setReleaseMs(float ms) {
    const float tau = std::max(0.5f, ms) * 0.001f;
    releaseCoeff = std::exp(-1.0f / (tau * (float)sr));
}
void PiezoTrigger::setHoldMs(float ms) {
    holdSamples = std::max(1, (int)(ms * 0.001f * sr));
}

float PiezoTrigger::processSample(float x) {
    const float ax = std::fabs(x);

    /* one-pole peak follower: fast attack, slow release. */
    if (ax > peak) peak = attackCoeff * peak + (1.0f - attackCoeff) * ax;
    else           peak = releaseCoeff * peak + (1.0f - releaseCoeff) * ax;
    if (ax > peak) peak = ax;  // ensure we never lag behind a true peak

    if (holdRemain > 0) --holdRemain;

    if (armed && peak > vHi) {
        armed      = false;
        holdRemain = holdSamples;
        return std::min(1.0f, peak);
    }
    if (!armed && peak < vLo && holdRemain == 0) {
        armed = true;
    }
    return 0.0f;
}
