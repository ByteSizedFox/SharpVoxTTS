#ifndef SHARPVOX_KLATT_SYNTHESIZER_FP_H
#define SHARPVOX_KLATT_SYNTHESIZER_FP_H

// Fixed-point drop-in replacement for KlattSynthesizer.
// All per-sample IIR operations and modulation use integer arithmetic.
// Coefficient computation (Calc_Pole_Coefficients) still uses float at frame
// boundaries  that's negligible cost and keeps the math exact.
//
// Numeric formats:
//   Coefficients (A/B/C): int32_t Q15  (float * 32768)
//   Delay taps:           int32_t Q0   (raw integers; signal magnitudes are large
//                                       enough that sub-1.0 fractional precision
//                                       is below the -85 dB noise floor)
//   Amplitude state:      int32_t Q8   (float * 256, for smooth per-sample step)
//   Modulation params:    int32_t Q8   (VibDepth/Rate, TremRate) or Q15 (TremDepth/Asp/Tilt)
//   Pink noise state:     int32_t Q15  (small values  50 max  1.6M, fits int32_t)
//   Vibrato/tremolo phase: uint32_t [0, 1<<17) = [0, 2)
//   Sin LUT:              int16_t Q15, 512 entries
//
// Enable by building with -DSHARPVOX_FIXED_POINT_SYNTH. TtsEngine.h/cpp use
// #ifdef guards to swap KlattSynthesizerFP into the _synth member.

#include "KlattSynthesizer.h"  // for Frame, HzToPitch/PitchToHz shared static helpers

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <vector>

namespace SharpVox {

    class KlattSynthesizerFP {
    public:
        static constexpr int32_t KMaxBandWidth       = KlattSynthesizer::KMaxBandWidth;
        static constexpr int32_t KPrecision          = KlattSynthesizer::KPrecision;
        static constexpr int32_t KOnePtOh            = KlattSynthesizer::KOnePtOh;
        static constexpr int32_t KNoiseGain          = KlattSynthesizer::KNoiseGain;
        static constexpr int32_t KDefaultSampleRate  = KlattSynthesizer::KDefaultSampleRate;
        static constexpr int32_t KDefaultSampFrameLen= KlattSynthesizer::KDefaultSampFrameLen;

        int32_t SampleRate() const { return _sampleRate; }
        int32_t SampFrameLen;

        // Delegate static helpers to the float version  identical math, no duplication.
        static int16_t HzToPitch(int16_t hz)    { return KlattSynthesizer::HzToPitch(hz); }
        static int16_t PitchToHz(int16_t pitch) { return KlattSynthesizer::PitchToHz(pitch); }

        int16_t VoiceChorus    = 0;
        int16_t Jitter         = 0;
        int16_t Shimmer        = 0;
        int16_t Diplophonia    = 0;
        int16_t FryAmount      = 0;
        int16_t SubglottalAmt  = 0;
        int16_t BreathAmt      = 0;
        int16_t PitchOffsetHz  = 0;

        int16_t LarynxOffset   = 0;
        int16_t PharyngealAmt  = 0;
        int16_t LipRounding    = 0;

        int16_t OQStressLink   = 0;
        int16_t OQF0Link       = 0;
        float   BasePitchHz    = 0.0f;

        int16_t OpenQuotient_get() const { return _openQuotient; }
        void    OpenQuotient_set(int16_t value) {
            _openQuotient  = value;
            _voiceTiltBias = (50 - value) * 0.012f;
        }

        explicit KlattSynthesizerFP(int32_t sampleRate = KDefaultSampleRate);

        void SetVoice(int16_t nGain, bool bit16,
                      int16_t f4_Freq,  int16_t f4_BW,
                      int16_t f5_Freq,  int16_t f5_BW,
                      int16_t f4p_Freq, int16_t bw4p_BW,
                      int16_t f5p_Freq, int16_t bw5p_BW,
                      int16_t f6p_Freq, int16_t bw6p_BW,
                      int16_t nasal_Base, int16_t nasal_BW,
                      int16_t aGain = 0, int16_t aCycle = 192);

        void ComputeGlotWave(int16_t vGain);

#ifdef SHARPVOX_SAMPLED_GLOT
        // Load a glottal source sample. pcm is mono float [-1,1] at srcRate Hz.
        // naturalPitchHz is the F0 of the source; playback speed scales by targetF0/naturalPitchHz.
        // Resamples to the synth rate internally. Replaces the polynomial until ClearGlottalSample().
        void SetGlottalSample(const float* pcm, int32_t length, int32_t srcRate, float naturalPitchHz);
        void ClearGlottalSample();
        bool SgPitchShift = true;
#endif

        void SynthesizeFrame(Frame frame, int16_t* outputBuffer, int32_t offset);

        void Calc_Pole_Coefficients(float& Acoeff, float& Bcoeff, float& Ccoeff,
                                    int16_t pitch, int16_t bandWidth,
                                    int32_t voiceMinBW = 50);

        void Calc_Zero_Coefficients(float& Acoeff, float& Bcoeff, float& Ccoeff,
                                    int16_t pitch, int16_t bandWidth);

        void Calc_Matched_Pole_Coefficients(float& b0, float& b1,
                                            float& Bcoeff, float& Ccoeff,
                                            int16_t pitch, int16_t bandWidth,
                                            int32_t voiceMinBW = 50);

        // Continuous accepted range; all rate compensation is analytic.
        // Capped at 48k: frame-boundary interpolation residuals grow with
        // SampFrameLen and become audible clicks above this.
        static const int32_t KMinSampleRate = 8000;
        static const int32_t KMaxSampleRate = 48000;

    private:
        int32_t _sampleRate;
        int32_t _internalRate;

        //  IIR coefficients: Q15 (float * 32768) 
        // Variable formants (F1-F3, NZ): interpolated per-sample inside SynthesizeFrame.
        // Fixed formants (F4, F5c, parallel bank, NP, SG): set at SetVoice time.
        // F1 cascade: tracked in (r, cos) float space  Q15 not stored.
        // F2/F3: Q15 kept for parallel bank; r/cos used for cascade interpolation.
        int32_t _f2A,  _f2B,  _f2C;
        int32_t _f3A,  _f3B,  _f3C;
        // Cascade F1-F3 physical interpolation state: pole radius and cos().
        // Interpolating in (r, cos) space guarantees B=2rcos and C=-r are always
        // consistent, so the pole z=re^(i) is always inside the unit circle for r<1.
        float _f1r, _f1cosw;
        float _f2r, _f2cosw;
        float _f3r, _f3cosw;
        // Cascade matched one-zero numerators (Vicanek 2016), interpolated per-sample
        // for F1-F3; poles stay the classic Klatt impulse-invariant pair.
        float _f1b0, _f1b1, _f2b0, _f2b1, _f3b0, _f3b1;
        // Cascade F4/F5: float matched coefficients, fixed at SetVoice.
        float _f4b0,  _f4b1,  _f4B,  _f4C;
        float _f5cb0, _f5cb1, _f5cB, _f5cC;
        float _f6cb0, _f6cb1, _f6cB, _f6cC;
        // Cascade F7/F8: fills the >F6 envelope gap the original 10kHz Klatt
        // design got free from near-Nyquist phantom poles (Rabiner 1968 values).
        float _f7cb0, _f7cb1, _f7cB, _f7cC;
        float _f8cb0, _f8cb1, _f8cB, _f8cC;
        int32_t _f4pA, _f4pB, _f4pC;
        int32_t _f5pA, _f5pB, _f5pC;
        int32_t _f6pA, _f6pB, _f6pC;
        int32_t _nzA,  _nzB,  _nzC;
        int32_t _npA,  _npB,  _npC;
        int32_t _sgA,  _sgB,  _sgC;

        //  Cascade IIR delay taps: float (exact; avoids limit-cycle noise at high Fs) 
        float   _f1D1,  _f1D2;
        float   _f2D1,  _f2D2;
        float   _f3D1,  _f3D2;
        float   _f4D1,  _f4D2;
        float   _f5cD1, _f5cD2;
        float   _f6cD1, _f6cD2;
        float   _f7cD1, _f7cD2;
        float   _f8cD1, _f8cD2;
        // Cascade input taps x[n-1] for the matched one-zero numerators.
        float   _f1X1, _f2X1, _f3X1, _f4X1, _f5cX1, _f6cX1, _f7cX1, _f8cX1;
        float   _nzD1,  _nzD2;
        float   _npD1,  _npD2;
        float   _sgD1,  _sgD2;
        //  Parallel-bank IIR delay taps: Q0 (integer) 
        int32_t _f2pD1, _f2pD2;
        int32_t _f3pD1, _f3pD2;
        int32_t _f4pD1, _f4pD2;
        int32_t _f5pD1, _f5pD2;
        int32_t _f6pD1, _f6pD2;
        int32_t _preemphPrev;   // pre-emphasis one-sample delay, Q0
        int32_t _preemphA_q15;  // pre-emphasis zero, rate-compensated at construction
        int32_t _preemphScale_q12;  // fs/22050 in Q12, flattens differentiator gain across rates
        float _tiltPrev;        // spectral tilt one-sample delay (float, matches float variant)

        //  Amplitude state: Q8 (float * 256) 
        int32_t _voiceAmp_q8;
        int32_t _fricAmp_q8;
        int32_t _abAmp_q8;
        int32_t _pAmp2_q8, _pAmp3_q8, _pAmp4_q8, _pAmp5_q8, _pAmp6_q8;

        //  Nasal normalisation: Q15 
        int32_t _nasalNorm_q15;

        //  Glottal source (same 24-bit phase scheme as float version)
        int32_t _glotPhase;
        int32_t _glotPhaseInc;
        int32_t _chorusPhase;
        int32_t _chorusPhaseInc;
        // Inline polynomial parameters (replace lookup tables — eliminates staircase quantization).
        int32_t _Ne_fp;          // open-phase end in 24-bit phase units
        int32_t _chorusNe_fp;
        float   _glotInvNe_f;   // 1.0f / _Ne_fp
        float   _chorusInvNe_f;
        float   _voiceGain_f;   // vGain * 288.0f (matches float synth _lfGain)

#ifdef SHARPVOX_SAMPLED_GLOT
        std::vector<float> _sgBuf;     // resampled glottal source, normalised to max|x|=1
        float   _sgNatPitchHz;         // natural F0 of the source
        float   _sgPhase;              // current read position in [0, _sgBuf.size())
        int32_t _sgBufSize;            // cached (int)_sgBuf.size()
        bool    _useSampledGlot;
#endif

        // Precomputed (1<<24)/sampleRate in Q4 for integer glotPhaseInc calculation.
        int32_t _phaseIncPerHz_q4;

        //  Source perturbation (updated at glottal cycle boundary)
        float   _shimmerScale;
        float   _diploScale;
        int32_t _cycleCount;
        int32_t _fryStallSamples;
        int32_t _diploPhase;

        //  Modulation oscillator state 
        // Phases in [0, 1<<17) representing [0, 2).
        uint32_t _vibratoPhase_fp;
        uint32_t _tremoloPhase_fp;

        // Interpolated modulation parameters.
        // VibDepth (Hz) and rates: Q8.  TremDepth, Asp, Tilt: Q15.
        int32_t _vibDepth_q8,   _vibRate_q8;
        int32_t _tremDepth_q15, _tremRate_q8;
        int32_t _asp_q15;
        int32_t _tilt_q15;

        //  Pink noise state: Q15 
        int32_t _pink0, _pink1, _pink2, _pink3, _pink4, _pink5, _pink6;
        // Rate-adapted poles (Q15) and gains (Q15); _pnP5mq15 is positive magnitude.
        int32_t _pnP0q15,_pnP1q15,_pnP2q15,_pnP3q15,_pnP4q15,_pnP5mq15;
        int32_t _pnG0q15,_pnG1q15,_pnG2q15,_pnG3q15,_pnG4q15,_pnG5q15;
        // White-tail gains (Q15), scaled sqrt(fs/22050) to keep tail PSD rate-invariant.
        int32_t _pnW0q15,_pnW1q15;

        //  Noise PRNG 
        int32_t _noiseSeed;

        //  OQ/tilt coupling 
        float   _voiceTiltBias;
        int16_t _openQuotient;

        //  Voice configuration (set by SetVoice) 
        float   _noiseAmp;
        float   _breathGain;
        int16_t _breathCycle;
        int16_t _nasalPoleFreq;
        int16_t _nasalPoleBW;
        int16_t _f4cFreq, _f4cBW;
        int16_t _f5cFreq, _f5cBW;
        int16_t _f6cFreq, _f6cBW;   // fixed cascade F6 (Klatt 1980 higher-pole correction)
        int16_t _f7cFreq, _f7cBW;   // fixed cascade F7 (Rabiner 1968, 6500Hz/BW720)
        int16_t _f8cFreq, _f8cBW;   // fixed cascade F8 (Rabiner 1968, 7500Hz/BW1250)
        int16_t _f4pFreq, _f4pBW;
        int16_t _f5pFreq, _f5pBW;
        int16_t _f6pFreq, _f6pBW;
        int16_t _f1FreqOffset = 0, _f2FreqOffset = 0, _f3FreqOffset = 0;
        int16_t _nasalFreqOffset = 0;

        //  Output / noise scaling 
        float   _noiseScale;
        float   _outputGain;
        float   _speechVolume;
        bool    _hfEmph;
        // Precomputed integer versions used in the hot loop.
        int32_t _outputGain_q15;    // _outputGain * 32768
        int32_t _noiseGain_q0;      // (int32_t)(128.0f * _noiseAmp * _noiseScale)

        //  Helpers 
        int16_t AdjFormant(int16_t pitch, int32_t formant);
        int32_t NextNoise();
        int32_t NextPinkNoise_q15();

        // Convert float A/B/C to Q15 int32_t in-place.
        static void ToQ15(float A, float B, float C,
                          int32_t& Aq, int32_t& Bq, int32_t& Cq);

        void InitFixedFormants();

        static void InitSinLut();
    };

} // namespace SharpVox

#endif // SHARPVOX_KLATT_SYNTHESIZER_FP_H
