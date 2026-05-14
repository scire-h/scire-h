#include "OTAVCA.h"
#include <cmath>

void OTAVCA::prepare(double /*sampleRate*/) { reset(); }
void OTAVCA::reset()                        { ctrlGain = 0.0f; }

float OTAVCA::processSample(float in) {
    /* canonical OTA VCA characteristic:
           V_out = G_env  ·  tanh( V_in · drive )
       with G_env the envelope-derived gain and drive a fixed scale
       that puts the saturation knee around |V_in| ≈ 0.6 for drive≈1.4. */
    return ctrlGain * std::tanh(in * drive);
}
