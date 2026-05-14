#include "ExpEnvelope.h"
#include <cmath>
#include <algorithm>

void ExpEnvelope::prepare(double sampleRate) {
    sr = sampleRate;
    reset();
}

void ExpEnvelope::reset() {
    stage  = Stage::Idle;
    value  = 0.0f;
    active = false;
}

void ExpEnvelope::trigger(float startValue, float peakValue, float endValue,
                          float attackSec, float decaySec) {
    value  = startValue;
    peak   = peakValue;
    endVal = endValue;
    target = peakValue;

    /* Linear attack -- short, matches what an RC bootstrap circuit does
       when the trigger pulse closes a transistor switch hard. */
    const int attackSamples = std::max(1, (int)(attackSec * sr));
    attackInc = (peak - value) / (float)attackSamples;

    /* Decay follows an RC discharge. We compute the per-sample
       coefficient from tau so that after `decaySec` we hit ~5 % of
       the remaining range -- a standard 3-tau definition. */
    const double tau     = std::max(0.0005, (double)decaySec / 3.0);
    decayCoeff = (float)std::exp(-1.0 / (tau * sr));

    stage  = Stage::Attack;
    active = true;
}

float ExpEnvelope::processSample() {
    switch (stage) {
        case Stage::Idle: return 0.0f;
        case Stage::Attack: {
            value += attackInc;
            if (value >= peak) {
                value = peak;
                stage = Stage::Decay;
            }
            return value;
        }
        case Stage::Decay: {
            value = endVal + (value - endVal) * decayCoeff;
            if (std::abs(value - endVal) < 0.0005f) {
                value  = endVal;
                stage  = Stage::Idle;
                active = false;
            }
            return value;
        }
    }
    return 0.0f;
}
