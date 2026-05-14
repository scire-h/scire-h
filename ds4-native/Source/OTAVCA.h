#pragma once

/* Phase 2.2 -- OTA-pair VCA (CA3080 / LM13700 model).

   The DS-4 uses an OTA-style VCA on each channel. An OTA in its
   normal current-control mode behaves like:

       i_out  =  I_abc  ·  tanh( V_in / (2 · V_T) )

   where:
       V_T   ≈  26 mV   (thermal voltage)
       I_abc =  control current (driven by the amp envelope)
       V_in  =  audio input (after the VCF)

   Two consequences matter audibly:

     1. The shape is tanh, not linear -- so as the input signal
        approaches the rails, the VCA softly compresses rather than
        clipping. The DS-4's characteristic "bloom" on heavy hits is
        exactly this saturation curve.
     2. The control current scales the output, but the shape is
        applied BEFORE that scale -- so the distortion is signal-
        level dependent, not envelope-level dependent. Quiet input =
        clean amplification; loud input = added 3rd harmonic.

   We model both: `setDrive` sets the input-side drive (how hard the
   tanh saturates for a given input level), and `setControlGain`
   takes the per-sample envelope value 0..1 from the amp envelope. */

class OTAVCA {
public:
    void  prepare(double sampleRate);
    void  reset();
    void  setControlGain(float g) { ctrlGain = g; }
    void  setDrive(float d)       { drive    = d; }
    float processSample(float input);

private:
    float ctrlGain = 0.0f;
    float drive    = 1.4f;       // ~tame: gentle saturation only above 0.6
};
