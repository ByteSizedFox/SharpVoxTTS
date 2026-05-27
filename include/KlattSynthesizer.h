#ifndef SHARPVOX_KLATT_SYNTHESIZER_H
#define SHARPVOX_KLATT_SYNTHESIZER_H

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <vector>

namespace SharpVox {

    struct Frame {
        int16_t Av;
        int16_t Af;
        int16_t F0;
        int16_t F1;
        int16_t F2;
        int16_t F3;
        int16_t A2;
        int16_t A3;
        int16_t A4;
        int16_t A5;
        int16_t A6;
        int16_t FNZ;
        int16_t AB;
        int16_t Bw1;
        int16_t Bw2;
        int16_t Bw3;
        int16_t PhonEdge;
        int64_t Marker;

        // Klattsch parameters
        uint8_t Aspiration;
        uint8_t Tilt;
        uint8_t Effort;
        uint8_t VibDepth;
        uint8_t VibRate;
        uint8_t TremDepth;
        uint8_t TremRate;
    };

    // Modified Klatt (1980) formant synthesizer with extended source models and multi-rate support.
    class KlattSynthesizer {
    public:
        static constexpr int32_t KMaxBandWidth = 1225;
        static constexpr int32_t KPrecision = 13;
        static constexpr int32_t KOnePtOh = 0x2000;
        static constexpr int32_t KNoiseGain = 2500;
        static constexpr int32_t KDefaultSampleRate = 22050;
        static constexpr int32_t KDefaultSampFrameLen = 112;

        static constexpr int32_t KUseHarm = 0;
        static constexpr int32_t KUseSnd = 1;
        static constexpr int32_t KUseSyncSnd = 2;

        int32_t SampleRate() const { return _sampleRate; }
        int32_t SampFrameLen;

        // Converts Hz to log-domain pitch code to allow integer formant arithmetic.
        static int16_t HzToPitch(int16_t hz);
        static int16_t PitchToHz(int16_t pitch);

        // Voice source perturbation parameters.
        int16_t VoiceChorus = 0;
        int32_t GlotType = KUseHarm;
        int16_t Jitter = 0;
        int16_t Shimmer = 0;
        int16_t Diplophonia = 0;
        int16_t FryAmount = 0;
        int16_t SubglottalAmt = 0;
        int16_t BreathAmt = 0;
        int16_t PitchOffsetHz = 0;

        // Vocal tract shaping parameters.
        int16_t LarynxOffset = 0;
        int16_t PharyngealAmt = 0;
        int16_t LipRounding = 0;  // -100=spread, 0=neutral, +100=rounded

        // Open-quotient coupling links.
        int16_t OQStressLink = 0;  // 0-100: effort -> pressed (high stress = lower effective OQ)
        int16_t OQF0Link = 0;  // 0-100: F0 -> breathy (high pitch = higher effective OQ)
        float BasePitchHz = 0.0f;  // voice baseline F0 in Hz (set from VoiceData.PitchHz)

        int16_t OpenQuotient_get() const { return _openQuotient; }
        void OpenQuotient_set(int16_t value) {
            _openQuotient = value;
            _voiceTiltBias = (50 - value) * 0.012f;
        }

        // Sample playback source (KUseSnd / KUseSyncSnd mode).
        uint8_t* SampleWave = nullptr;
        int32_t SampleInc = 0;
        int32_t SampleIndex = 0;

        explicit KlattSynthesizer(int32_t sampleRate = KDefaultSampleRate);

        void SetVoice(int16_t nGain, bool bit16, int16_t f4_Freq, int16_t f4_BW,
                      int16_t f5_Freq, int16_t f5_BW, int16_t f4p_Freq, int16_t bw4p_BW,
                      int16_t f5p_Freq, int16_t bw5p_BW, int16_t f6p_Freq, int16_t bw6p_BW,
                      int16_t nasal_Base, int16_t nasal_BW,
                      int16_t aGain = 0, int16_t aCycle = 192);

        void InvDFT(int16_t* vWave, int16_t* vWave1, int16_t vGain);

        // Synthesizes one frame with linear parameter interpolation to ensure smooth transitions.
        void SynthesizeFrame(Frame frame, int16_t* outputBuffer, int32_t offset);

        // Computes second-order IIR resonator coefficients for a pole at (hz, bandWidth).
        // Klatt (1980) eq. 1-3: r = exp(-pi*BW/Fs), C = -(r^2), B = 2r*cos(2pi*F/Fs), A = 1-B-C.
        // Transfer function: H(z) = A / (1 - B*z^-1 - C*z^-2); poles at z = r*exp(+/-j*2pi*F/Fs).
        void Calc_Pole_Coefficients(float& Acoeff, float& Bcoeff, float& Ccoeff,
                                    int16_t pitch, int16_t bandWidth, int32_t voiceMinBW = 50);

        // Antiresonator (Klatt 1980): sign-inverted pole coefficients produce a spectral zero.
        void Calc_Zero_Coefficients(float& Acoeff, float& Bcoeff, float& Ccoeff,
                                    int16_t pitch, int16_t bandWidth);

        static std::vector<int32_t> SupportedSampleRates();

    private:
        int32_t _sampleRate;
        int32_t _internalRate;

        // Cascade resonator coefficients: A = gain, B = y[n-1] feedback, C = y[n-2] feedback.
        // F1-F3 are interpolated each frame; F4 and F5c are fixed per voice setting.
        float _f1A, _f1B, _f1C;
        float _f2A, _f2B, _f2C;
        float _f3A, _f3B, _f3C;
        float _f4A, _f4B, _f4C;
        float _f5cA, _f5cB, _f5cC;

        // Parallel bank resonator coefficients (F4p-F6p have their own fixed coefficients;
        // F2p and F3p reuse the cascade F2/F3 coefficients with separate delay taps).
        float _f4pA, _f4pB, _f4pC;
        float _f5pA, _f5pB, _f5pC;
        float _f6pA, _f6pB, _f6pC;

        // Nasal filter coefficients: NZ = antiresonator (zero), NP = resonator (pole).
        float _nzA, _nzB, _nzC;
        float _npA, _npB, _npC;

        // Cascade resonator delay taps: D1 = y[n-1], D2 = y[n-2].
        float _f1D1, _f1D2;
        float _f2D1, _f2D2;
        float _f3D1, _f3D2;
        float _f4D1, _f4D2;
        float _f5cD1, _f5cD2;

        // Parallel bank delay taps (F2p/F3p share cascade coefficients but have own delay taps).
        float _f2pD1, _f2pD2;
        float _f3pD1, _f3pD2;
        float _f4pD1, _f4pD2;
        float _f5pD1, _f5pD2;
        float _f6pD1, _f6pD2;

        // Nasal filter delay taps. NZ stores input delays; NP stores output delays.
        float _nzD1, _nzD2;
        float _npD1, _npD2;

        // Per-frame amplitude state (linearly interpolated across each frame).
        float _voiceAmp;   // voiced source amplitude
        float _fricAmp;    // frication noise amplitude
        float _abAmp;      // AB broadband parallel amplitude
        float _pAmp2, _pAmp3, _pAmp4, _pAmp5, _pAmp6;

        // Glottal source state: 24-bit fixed-point phase counters into the 256-entry waveform table.
        int32_t _glotPhase;
        int32_t _glotPhaseInc;
        int32_t _chorusPhase;
        int32_t _chorusPhaseInc;
        int16_t _voiceWave[256];
        int16_t _chorusWave[256];

        // Source perturbation state (persist across frames).
        float _shimmerScale = 1.0f;
        float _diploScale = 1.0f;
        int32_t _cycleCount;
        int32_t _fryStallSamples;

        // Subglottal resonator fixed at ~350 Hz (chest-cavity coupling).
        float _sgA, _sgB, _sgC;
        float _sgD1, _sgD2;

        // DSP modulation state.
        float _vibratoPhase;
        float _tremoloPhase;
        float _tiltPrev;
        float _lastVibDepth, _lastVibRate;
        float _lastTremDepth, _lastTremRate;
        float _lastAsp, _lastTilt;

        // Pre-emphasis filter delay (first-difference).
        float _preemphPrev;

        // NZ/NP gain normalization factor, interpolated each frame.
        float _nasalNorm = 1.0f;

        // Voice configuration set by SetVoice().
        float _noiseAmp;           // parallel bank noise amplitude
        float _breathGain;
        int16_t _breathCycle;
        int16_t _nasalPoleFreq;
        int16_t _nasalPoleBW;
        int16_t _f4cFreq, _f4cBW;   // fixed cascade F4
        int16_t _f5cFreq, _f5cBW;   // fixed cascade F5
        int16_t _f4pFreq, _f4pBW;   // parallel F4p
        int16_t _f5pFreq, _f5pBW;   // parallel F5p
        int16_t _f6pFreq, _f6pBW;   // parallel F6p

        // Voice-level formant offsets in pitch units (typically 0; reserved for future use).
        int16_t _f1FreqOffset = 0, _f2FreqOffset = 0, _f3FreqOffset = 0;
        int16_t _nasalFreqOffset = 0;

        // Output gain and volume.
        float _noiseScale;
        float _outputGain;
        float _speechVolume = 150.0f;
        float _waveSampleGain = 0.0f;
        bool _hfEmph = true;

        // Reverb state.
        static constexpr int32_t KNumOfTaps = 8;
        int16_t _tapBuffer[KNumOfTaps];
        int16_t* _delayBuffer;
        int32_t _maxRvbDelay;

        // Noise PRNG state.
        int32_t _noiseSeed = 0x12345;

        // OQ/tilt coupling (static voice bias + per-frame dynamic adjustment).
        float _voiceTiltBias = 0.0f;
        float _frameTiltBias = 0.0f;

        int16_t _openQuotient = 50;

        // Pink noise state - add to class fields
        float _pink0, _pink1, _pink2, _pink3, _pink4, _pink5, _pink6;

        // Applies larynx-height + pharyngeal constriction + lip rounding/spreading to a formant pitch value.
        // formant: 1=F1, 2=F2, 3=F3
        int16_t AdjFormant(int16_t pitch, int32_t formant);

        int32_t NextNoise();
        float NextPinkNoise();

        void InitFixedFormants();
    };

} // namespace SharpVox

#endif // SHARPVOX_KLATT_SYNTHESIZER_H
