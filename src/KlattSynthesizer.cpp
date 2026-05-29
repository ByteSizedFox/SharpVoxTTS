#include "../include/KlattSynthesizer.h"
#include "../include/SynthData.h"
#include "../include/VoiceData.h"

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <map>

namespace SharpVox {

    // NoiseScale and OutputGain were tuned empirically: lower sample rates need louder
    // noise and higher output gain to compensate for the narrower spectral bandwidth.
    struct SampleRatePreset {
        float NoiseScale;
        float OutputGain;
    };

    static const std::map<int32_t, SampleRatePreset> _ratePresets = {
        { 8000,  { 1.15f, 1.50f  } },
        { 11025, { 0.29f, 2.35f  } },
        { 22050, { 1.00f, 5.00f  } },
        { 44100, { 1.20f, 9.20f  } },
        { 48000, { 1.20f, 10.00f } },
        { 96000, { 2.30f, 15.00f } },
    };

    std::vector<int32_t> KlattSynthesizer::SupportedSampleRates() {
        std::vector<int32_t> keys;
        keys.reserve(_ratePresets.size());
        for (auto& kv : _ratePresets) {
            keys.push_back(kv.first);
        }
        return keys;
    }

    KlattSynthesizer::KlattSynthesizer(int32_t sampleRate) {
        auto it = _ratePresets.find(sampleRate);
        if (it == _ratePresets.end()) {
            throw std::invalid_argument(
                "Unsupported sample rate " + std::to_string(sampleRate) + " Hz.");
        }

        _sampleRate = sampleRate;
        _internalRate = sampleRate;
        _noiseScale = it->second.NoiseScale;
        _outputGain = it->second.OutputGain;

        double rawLen = _sampleRate * (KDefaultSampFrameLen / (double)KDefaultSampleRate);
        int32_t len = (int32_t)std::floor(rawLen + 0.5);
        if (len < 2) {
            len = 2;
        }
        SampFrameLen = len;

        _maxRvbDelay = 4096;
        _delayBuffer = new int16_t[_maxRvbDelay]();
        _tapBuffer[0] = 404;
        _tapBuffer[1] = 1058;
        _tapBuffer[2] = 1362;
        _tapBuffer[3] = 2318;
        _tapBuffer[4] = 2909;
        _tapBuffer[5] = 3723;
        _tapBuffer[6] = 4030;
        _tapBuffer[7] = 4096;

        // Fixed subglottal resonator ~350 Hz, BW 80 -- chest cavity coupling
        Calc_Pole_Coefficients(_sgA, _sgB, _sgC, HzToPitch(350), 80);

        // Zero-initialize filter state and modulation state
        _f1A = _f1B = _f1C = 0.0f;
        _f2A = _f2B = _f2C = 0.0f;
        _f3A = _f3B = _f3C = 0.0f;
        _f4A = _f4B = _f4C = 0.0f;
        _f5cA = _f5cB = _f5cC = 0.0f;
        _f4pA = _f4pB = _f4pC = 0.0f;
        _f5pA = _f5pB = _f5pC = 0.0f;
        _f6pA = _f6pB = _f6pC = 0.0f;
        _nzA = _nzB = _nzC = 0.0f;
        _npA = _npB = _npC = 0.0f;

        _f1D1 = _f1D2 = 0.0f;
        _f2D1 = _f2D2 = 0.0f;
        _f3D1 = _f3D2 = 0.0f;
        _f4D1 = _f4D2 = 0.0f;
        _f5cD1 = _f5cD2 = 0.0f;
        _f2pD1 = _f2pD2 = 0.0f;
        _f3pD1 = _f3pD2 = 0.0f;
        _f4pD1 = _f4pD2 = 0.0f;
        _f5pD1 = _f5pD2 = 0.0f;
        _f6pD1 = _f6pD2 = 0.0f;
        _nzD1 = _nzD2 = 0.0f;
        _npD1 = _npD2 = 0.0f;

        _voiceAmp = _fricAmp = _abAmp = 0.0f;
        _pAmp2 = _pAmp3 = _pAmp4 = _pAmp5 = _pAmp6 = 0.0f;

        _glotPhase = _glotPhaseInc = 0;
        _chorusPhase = _chorusPhaseInc = 0;

        for (int32_t j = 0; j < 256; j++) {
            _voiceWave[j] = 0;
            _chorusWave[j] = 0;
        }

        _cycleCount = 0;
        _fryStallSamples = 0;

        _sgD1 = _sgD2 = 0.0f;

        _vibratoPhase = 0.0f;
        _tremoloPhase = 0.0f;
        _tiltPrev = 0.0f;
        _lastVibDepth = _lastVibRate = 0.0f;
        _lastTremDepth = _lastTremRate = 0.0f;
        _lastAsp = _lastTilt = 0.0f;

        _preemphPrev = 0.0f;

        _noiseAmp = 0.0f;
        _breathGain = 0.0f;
        _breathCycle = 0;
        _nasalPoleFreq = 0;
        _nasalPoleBW = 0;
        _f4cFreq = _f4cBW = 0;
        _f5cFreq = _f5cBW = 0;
        _f4pFreq = _f4pBW = 0;
        _f5pFreq = _f5pBW = 0;
        _f6pFreq = _f6pBW = 0;

        _pink0 = _pink1 = _pink2 = _pink3 = _pink4 = _pink5 = _pink6 = 0.0f;
    }

    void KlattSynthesizer::SetVoice(int16_t nGain, bool bit16,
                                    int16_t f4_Freq, int16_t f4_BW,
                                    int16_t f5_Freq, int16_t f5_BW,
                                    int16_t f4p_Freq, int16_t bw4p_BW,
                                    int16_t f5p_Freq, int16_t bw5p_BW,
                                    int16_t f6p_Freq, int16_t bw6p_BW,
                                    int16_t nasal_Base, int16_t nasal_BW,
                                    int16_t aGain, int16_t aCycle) {
        _breathGain = (aGain * KNoiseGain) / 100.0f;
        _breathCycle = aCycle;

        _noiseAmp = nGain / 100.0f;
        if (bit16) {
            _noiseAmp *= (0xCCCC / 65536.0f);
        }

        _f4cFreq = HzToPitch(f4_Freq);
        _f4cBW = f4_BW;
        _f5cFreq = HzToPitch(f5_Freq);
        _f5cBW = f5_BW;

        _f4pFreq = HzToPitch(f4p_Freq);
        _f4pBW = bw4p_BW;
        _f5pFreq = HzToPitch(f5p_Freq);
        _f5pBW = bw5p_BW;
        _f6pFreq = HzToPitch(f6p_Freq);
        _f6pBW = bw6p_BW;

        _nasalPoleFreq = HzToPitch(nasal_Base);
        _nasalPoleBW = nasal_BW;

        InitFixedFormants();
    }

    void KlattSynthesizer::InitFixedFormants() {
        Calc_Pole_Coefficients(_f4A, _f4B, _f4C, _f4cFreq, _f4cBW);
        Calc_Pole_Coefficients(_f5cA, _f5cB, _f5cC, _f5cFreq, _f5cBW);

        Calc_Pole_Coefficients(_f4pA, _f4pB, _f4pC, _f4pFreq, _f4pBW);
        _f4pA *= (KNoiseGain / 8192.0f);

        Calc_Pole_Coefficients(_f5pA, _f5pB, _f5pC, _f5pFreq, _f5pBW);
        _f5pA *= (KNoiseGain / 8192.0f);

        Calc_Pole_Coefficients(_f6pA, _f6pB, _f6pC, _f6pFreq, _f6pBW);
        _f6pA *= (KNoiseGain / 8192.0f);

        Calc_Pole_Coefficients(_npA, _npB, _npC, _nasalPoleFreq, _nasalPoleBW);
    }

    void KlattSynthesizer::InvDFT(int16_t* vWave, int16_t* vWave1, int16_t vGain) {
        if (vWave == nullptr || vWave1 == nullptr) {
            for (int32_t j = 0; j < 256; j++) {
                _voiceWave[j] = 0;
                _chorusWave[j] = 0;
            }
            return;
        }

        float w0[256] = {};
        float w1[256] = {};
        float gain = vGain / 200.0f;

        for (int32_t i = 0; i < 48; i++) {
            float amp0 = vWave[i] * gain;
            float amp1 = vWave1[i] * gain;

            int32_t sIndex = 0;
            for (int32_t j = 0; j < 256; j++) {
                // Match prior fixed-point scaling:
                // sine15 ~= round(16383*sin(..)); sample += (amp*sine15)>>16
                float sine15 = 16383.0f * (float)std::sin(2.0 * M_PI * sIndex / 256.0);
                w0[j] += (amp0 * sine15) / 65536.0f;
                w1[j] += (amp1 * sine15) / 65536.0f;
                sIndex = (sIndex + i) & 0xFF;
            }
        }

        float peak0 = 0, peak1 = 0;
        for (int32_t j = 0; j < 256; j++) {
            float a0 = std::abs(w0[j]);
            if (a0 > peak0) {
                peak0 = a0;
            }
            float a1 = std::abs(w1[j]);
            if (a1 > peak1) {
                peak1 = a1;
            }
        }

        // Only scale the chorus waveform to match the primary waveform's peak.
        float chorusScale = (peak1 > 0) ? (peak0 / peak1) : 0;
        for (int32_t j = 0; j < 256; j++) {
            float v0 = std::round(w0[j]);
            v0 = std::max(v0, (float)INT16_MIN);
            v0 = std::min(v0, (float)INT16_MAX);
            _voiceWave[j] = (int16_t)v0;

            float v1 = std::round(w1[j] * chorusScale);
            v1 = std::max(v1, (float)INT16_MIN);
            v1 = std::min(v1, (float)INT16_MAX);
            _chorusWave[j] = (int16_t)v1;
        }
    }

    // Applies larynx-height + pharyngeal constriction + lip rounding/spreading to a formant pitch value.
    // formant: 1=F1, 2=F2, 3=F3
    int16_t KlattSynthesizer::AdjFormant(int16_t pitch, int32_t formant) {
        if (LarynxOffset == 0 && PharyngealAmt == 0 && LipRounding == 0) {
            return pitch;
        }
        int32_t hz = PitchToHz(pitch) + LarynxOffset;
        hz += formant == 1 ? PharyngealAmt - (LipRounding / 2)
            : formant == 2 ? -PharyngealAmt * 2 - (LipRounding * 3)
            : formant == 3 ? -LipRounding
            : 0;
        hz = std::max(hz, (int32_t)50);
        hz = std::min(hz, (int32_t)8000);
        return HzToPitch((int16_t)hz);
    }

    int32_t KlattSynthesizer::NextNoise() {
        _noiseSeed = (_noiseSeed * 1103515245 + 12345) & 0x7FFFFFFF;
        return (_noiseSeed >> 16) & 0xFF;
    }

    float KlattSynthesizer::NextPinkNoise() {
        float white = (NextNoise() - 128) / 128.0f;
        _pink0 = 0.99886f * _pink0 + white * 0.0555179f;
        _pink1 = 0.99332f * _pink1 + white * 0.0750759f;
        _pink2 = 0.96900f * _pink2 + white * 0.1538520f;
        _pink3 = 0.86650f * _pink3 + white * 0.3104856f;
        _pink4 = 0.55000f * _pink4 + white * 0.5329522f;
        _pink5 = -0.7616f * _pink5 - white * 0.0168980f;
        float pink = _pink0 + _pink1 + _pink2 + _pink3 + _pink4 + _pink5 + _pink6 + white * 0.5362f;
        _pink6 = white * 0.115926f;
        return pink * 0.18f; // normalize to roughly -128..128 range (boosted from 0.11f for RMS balance)
    }

    // Synthesizes one frame with linear parameter interpolation to ensure smooth transitions.
    void KlattSynthesizer::SynthesizeFrame(Frame frame, int16_t* outputBuffer, int32_t offset) {
        float targetVoiceAmp = frame.Av * _speechVolume;
        float targetFricAmp = frame.Af * _speechVolume * 4.0f;
        float targetAbAmp = frame.AB * _speechVolume;
        float targetPAmp2 = frame.A2 / 32.0f;
        float targetPAmp3 = frame.A3 / 32.0f;
        float targetPAmp4 = frame.A4 / 32.0f;
        float targetPAmp5 = frame.A5 / 32.0f;
        float targetPAmp6 = frame.A6 / 32.0f;

        // Reset filter state when voicing or frication starts from silence.
        // Starting from zero prevents a transient burst caused by residual filter energy.
        if ((_voiceAmp == 0) && (_fricAmp == 0) && (targetVoiceAmp > 0 || targetFricAmp > 0)) {
            _glotPhase = 0;
            _chorusPhase = 0;
            _shimmerScale = 1.0f;
            _diploScale = 1.0f;
            _cycleCount = 0;
            _fryStallSamples = 0;

            _sgD1 = _sgD2 = 0;
            _f1D1 = _f1D2 = _f2D1 = _f2D2 = _f3D1 = _f3D2 = _f4D1 = _f4D2 = _f5cD1 = _f5cD2 = 0;
            _npD1 = _npD2 = _nzD1 = _nzD2 = 0;

            // Initialize coefficients to targets to avoid slides from old values.
            Calc_Pole_Coefficients(_f1A, _f1B, _f1C, AdjFormant((int16_t)(frame.F1 + _f1FreqOffset), 1), frame.Bw1);
            Calc_Pole_Coefficients(_f2A, _f2B, _f2C, AdjFormant((int16_t)(frame.F2 + _f2FreqOffset), 2), frame.Bw2);
            Calc_Pole_Coefficients(_f3A, _f3B, _f3C, AdjFormant((int16_t)(frame.F3 + _f3FreqOffset), 3), frame.Bw3);

            if (frame.FNZ != _nasalPoleFreq) {
                Calc_Zero_Coefficients(_nzA, _nzB, _nzC, (int16_t)(frame.FNZ + _nasalFreqOffset), _nasalPoleBW);
                _nasalNorm = _nzA != 0 ? (_npA / _nzA) : 1.0f;
            } else {
                _nzA = _npA; _nzB = -_npB; _nzC = -_npC;
                _nasalNorm = 1.0f;
            }

            _voiceAmp = 0;
            _fricAmp = 0;
            _abAmp = 0;
            _pAmp2 = targetPAmp2; _pAmp3 = targetPAmp3; _pAmp4 = targetPAmp4; _pAmp5 = targetPAmp5; _pAmp6 = targetPAmp6;
        }

        // Compute target resonator coefficients for F1-F3 and the nasal zero (NZ).
        float f1TA, f1TB, f1TC;
        float f2TA, f2TB, f2TC;
        float f3TA, f3TB, f3TC;
        Calc_Pole_Coefficients(f1TA, f1TB, f1TC, AdjFormant((int16_t)(frame.F1 + _f1FreqOffset), 1), frame.Bw1);
        Calc_Pole_Coefficients(f2TA, f2TB, f2TC, AdjFormant((int16_t)(frame.F2 + _f2FreqOffset), 2), frame.Bw2);
        Calc_Pole_Coefficients(f3TA, f3TB, f3TC, AdjFormant((int16_t)(frame.F3 + _f3FreqOffset), 3), frame.Bw3);

        float nzTA, nzTB, nzTC, targetNasalNorm;
        if (frame.FNZ != _nasalPoleFreq) {
            Calc_Zero_Coefficients(nzTA, nzTB, nzTC, (int16_t)(frame.FNZ + _nasalFreqOffset), _nasalPoleBW);
            targetNasalNorm = nzTA != 0 ? (_npA / nzTA) : 1.0f;
        } else {
            nzTA = _npA; nzTB = -_npB; nzTC = -_npC;
            targetNasalNorm = 1.0f;
        }

        // Compute per-sample interpolation deltas so every parameter reaches its target
        // exactly at the end of the frame without a discontinuous jump.
        float dVoiceAmp = (targetVoiceAmp - _voiceAmp) / SampFrameLen;
        float dFricAmp = (targetFricAmp - _fricAmp) / SampFrameLen;
        float dAbAmp = (targetAbAmp - _abAmp) / SampFrameLen;
        float dPAmp2 = (targetPAmp2 - _pAmp2) / SampFrameLen;
        float dPAmp3 = (targetPAmp3 - _pAmp3) / SampFrameLen;
        float dPAmp4 = (targetPAmp4 - _pAmp4) / SampFrameLen;
        float dPAmp5 = (targetPAmp5 - _pAmp5) / SampFrameLen;
        float dPAmp6 = (targetPAmp6 - _pAmp6) / SampFrameLen;

        float df1A = (f1TA - _f1A) / SampFrameLen;
        float df1B = (f1TB - _f1B) / SampFrameLen;
        float df1C = (f1TC - _f1C) / SampFrameLen;
        float df2A = (f2TA - _f2A) / SampFrameLen;
        float df2B = (f2TB - _f2B) / SampFrameLen;
        float df2C = (f2TC - _f2C) / SampFrameLen;
        float df3A = (f3TA - _f3A) / SampFrameLen;
        float df3B = (f3TB - _f3B) / SampFrameLen;
        float df3C = (f3TC - _f3C) / SampFrameLen;
        float dNzA = (nzTA - _nzA) / SampFrameLen;
        float dNzB = (nzTB - _nzB) / SampFrameLen;
        float dNzC = (nzTC - _nzC) / SampFrameLen;
        float dNasalNorm = (targetNasalNorm - _nasalNorm) / SampFrameLen;

        float targetVibDepth = frame.VibDepth;
        float targetVibRate = frame.VibRate / 10.0f;
        float targetTremDepth = frame.TremDepth / 100.0f;
        float targetTremRate = frame.TremRate / 10.0f;
        float targetAsp = frame.Aspiration / 100.0f;
        float targetTilt = (frame.Tilt / 100.0f) * 1.9f - 0.95f;  // map 0..100 to -0.95..0.95

        float dVibDepth = (targetVibDepth - _lastVibDepth) / SampFrameLen;
        float dVibRate = (targetVibRate - _lastVibRate) / SampFrameLen;
        float dTremDepth = (targetTremDepth - _lastTremDepth) / SampFrameLen;
        float dTremRate = (targetTremRate - _lastTremRate) / SampFrameLen;
        float dAsp = (targetAsp - _lastAsp) / SampFrameLen;
        float dTilt = (targetTilt - _lastTilt) / SampFrameLen;

        float breathGainBase = _breathGain / 8192.0f;

        // Dynamic open-quotient tilt bias combining static voice settings with per-frame stress and F0.
        _frameTiltBias = _voiceTiltBias;
        if (OQStressLink != 0 && frame.Effort > 0) {
            _frameTiltBias -= (frame.Effort / 100.0f) * (OQStressLink / 100.0f) * 0.3f;
        }
        if (OQF0Link != 0 && frame.F0 > 0) {
            float f0BaseHz = PitchToHz(frame.F0) + PitchOffsetHz;
            float f0RefHz = BasePitchHz > 0 ? BasePitchHz : 100.0f;
            float f0Ratio = std::max(0.0f, std::min(2.0f, (f0BaseHz - f0RefHz) / f0RefHz)) * 0.5f;
            _frameTiltBias += f0Ratio * (OQF0Link / 100.0f) * 0.3f;
        }
        _frameTiltBias = std::max(-0.95f, std::min(0.95f, _frameTiltBias));

        for (int32_t sampCtr = SampFrameLen - 1; sampCtr >= 0; --sampCtr) {
            // Step all interpolated parameters toward their frame targets.
            _voiceAmp += dVoiceAmp;
            _fricAmp += dFricAmp;
            _abAmp += dAbAmp;
            _pAmp2 += dPAmp2; _pAmp3 += dPAmp3; _pAmp4 += dPAmp4; _pAmp5 += dPAmp5; _pAmp6 += dPAmp6;
            _f1A += df1A; _f1B += df1B; _f1C += df1C;
            _f2A += df2A; _f2B += df2B; _f2C += df2C;
            _f3A += df3A; _f3B += df3B; _f3C += df3C;
            _nzA += dNzA; _nzB += dNzB; _nzC += dNzC;
            _nasalNorm += dNasalNorm;

            _lastVibDepth += dVibDepth;
            _lastVibRate += dVibRate;
            _lastTremDepth += dTremDepth;
            _lastTremRate += dTremRate;
            _lastAsp += dAsp;
            _lastTilt += dTilt;

            // Vibrato: sine-wave F0 modulation. Phase accumulates at the vibrato rate;
            // the effective F0 is shifted by depth*sin(phase) Hz before the phase increment is computed.
            // glotPhaseInc is a 24-bit fixed-point increment into the 256-entry waveform table.
            _vibratoPhase += (float)(2 * M_PI * _lastVibRate / _sampleRate);
            if (_vibratoPhase > (float)(2 * M_PI)) {
                _vibratoPhase -= (float)(2 * M_PI);
            }
            float vibratoHz = _lastVibDepth * std::sin(_vibratoPhase);
            float effF0Hz = std::max(20.0f, PitchToHz(frame.F0) + (float)PitchOffsetHz + vibratoHz);
            _glotPhaseInc = (int32_t)std::round(effF0Hz * (double)(1 << 24) / _internalRate);
            if (VoiceChorus != 0) {
                float chorusF0Hz = std::max(20.0f, PitchToHz((int16_t)(frame.F0 + VoiceChorus)) + (float)PitchOffsetHz + vibratoHz);
                _chorusPhaseInc = (int32_t)std::round(chorusF0Hz * (double)(1 << 24) / _internalRate);
            }

            // Tremolo: amplitude modulation applied to the voiced source before the filters.
            // Uses a sine wave with a (0, 1) range so tremolo depth=0 gives full amplitude,
            // depth=1 modulates from 0 to full amplitude.
            _tremoloPhase += (float)(2 * M_PI * _lastTremRate / _sampleRate);
            if (_tremoloPhase > (float)(2 * M_PI)) {
                _tremoloPhase -= (float)(2 * M_PI);
            }
            float tremMod = 1.0f - _lastTremDepth * (0.5f + 0.5f * std::sin(_tremoloPhase));
            float voiceAmpTrem = _voiceAmp * tremMod;

            float cascadeIn = 0, cascadeOut = 0;
            float sampAB = 0, samp2 = 0, samp3 = 0, samp4 = 0, samp5 = 0, samp6 = 0;

            if (voiceAmpTrem > 0 || _fricAmp > 0 || _abAmp > 0 || _pAmp2 > 0 || _pAmp3 > 0 || _pAmp4 > 0 || _pAmp5 > 0 || _pAmp6 > 0 || _lastAsp > 0) {
                if (voiceAmpTrem > 0) {
                    float glotSample;
                    if (GlotType == KUseHarm) {
                        // KUseHarm: harmonic glottal waveform synthesized by InvDFT from spectral amplitudes.
                        // glotPhase is a 24-bit fixed-point phase; the top 8 bits index the 256-entry table.
                        // Fry stall: hold at glotPhase=0 (voiceWave[0]=0 = true closed phase)
                        if (_fryStallSamples > 0) {
                            _fryStallSamples--;
                        }
                        int32_t prevPhase = _glotPhase;
                        _glotPhase = (_fryStallSamples > 0 ? 0 : (_glotPhaseInc + _glotPhase)) & 0xFFFFFF;

                        if (_glotPhase < prevPhase) {  // glottal cycle boundary
                            if (Shimmer > 0) {
                                float shimDepth = Shimmer * 0.002f;
                                _shimmerScale = 1.0f + ((NextNoise() - 128) / 128.0f) * shimDepth;
                            }
                            if (Diplophonia > 0) {
                                _cycleCount++;
                                float weakRatio = std::max(0.0f, 1.0f - Diplophonia * 0.01f);
                                _diploScale = (_cycleCount & 1) == 0 ? 1.0f : weakRatio;
                            }
                            if (Jitter > 0) {
                                int32_t jitterRange = (int32_t)(Jitter * 0.0005f * (1 << 24));
                                _glotPhase = (_glotPhase + ((NextNoise() - 128) * jitterRange >> 7)) & 0xFFFFFF;
                            }
                            // Vocal fry: park at closed phase for a random fraction of the current period.
                            if (FryAmount > 0 && (NextNoise() & 0xFF) < FryAmount) {
                                int32_t period = _glotPhaseInc > 0 ? std::min((1 << 24) / _glotPhaseInc, (int32_t)1500) : (int32_t)200;
                                _fryStallSamples = (NextNoise() * period) >> 8;
                                if (_fryStallSamples > 0) {
                                    _glotPhase = 0;
                                }
                            }
                        }

                        glotSample = _voiceWave[_glotPhase >> 16];
                        if (VoiceChorus != 0) {
                            _chorusPhase = (_chorusPhaseInc + _chorusPhase) & 0xFFFFFF;
                            glotSample = (glotSample + _chorusWave[_chorusPhase >> 16]) * 0.5f;
                        }
                    } else {
                        // KUseSnd / KUseSyncSnd: play back an external sample at the current glottal rate.
                        // SampleIndex is a separate 24-bit phase so it can run at a different rate than glotPhase.
                        _glotPhase = (_glotPhaseInc + _glotPhase) & 0xFFFFFF;
                        if (SampleWave != nullptr) {
                            SampleIndex = (SampleInc + SampleIndex) & 0xFFFFFF;
                            glotSample = (SampleWave[SampleIndex >> 16] - 128) * _waveSampleGain;
                        } else {
                            glotSample = 0;
                        }
                    }

                    // Spectral tilt: one-pole filter y[n] = x[n] - a*x[n-1].
                    // tilt near +1 gives low-pass (breathy), near -1 gives high-pass (pressed).
                    float effectiveTilt = _frameTiltBias != 0.0f
                        ? std::max(-0.95f, std::min(0.95f, _lastTilt + _frameTiltBias))
                        : _lastTilt;
                    float tiltedSample = glotSample - effectiveTilt * _tiltPrev;
                    _tiltPrev = glotSample;
                    glotSample = tiltedSample;

                    cascadeIn = glotSample * voiceAmpTrem * _shimmerScale * _diploScale / 8192.0f;

                    // Subglottal resonance: ~350 Hz chest cavity coupling.
                    if (SubglottalAmt > 0) {
                        float sg = _sgA * cascadeIn + _sgB * _sgD1 + _sgC * _sgD2;
                        _sgD2 = _sgD1; _sgD1 = sg;
                        cascadeIn += sg * (SubglottalAmt * 0.005f);
                    }

                    // Cycle-synchronous breathiness: open-phase noise shaped by glottal waveform.
                    if (BreathAmt > 0) {
                        float openness = std::max(0.0f, (float)_voiceWave[_glotPhase >> 16]);
                        cascadeIn += (NextNoise() - 128) * openness * voiceAmpTrem * (BreathAmt * 0.00004f) * _noiseScale / 8192.0f;
                    }
                } else {
                    if (_breathGain > 0) {
                        _glotPhase = (_glotPhaseInc + _glotPhase) & 0xFFFFFF;
                    } else {
                        _glotPhase = 0;
                        _chorusPhase = 0;
                    }
                    cascadeIn = 0;
                }

                // Breath (aspiration) source: open-phase noise gated by glottal cycle position.
                float breathGainNow = breathGainBase * voiceAmpTrem;
                if (breathGainNow > 0 && (_glotPhase >> 16) > _breathCycle) {
                    cascadeIn += (NextNoise() - 128) * breathGainNow * _noiseScale / 2048.0f;
                }

                if (_lastAsp > 0) {
                    float aspGain = _lastAsp * voiceAmpTrem * 0.5f;
                    cascadeIn += (NextNoise() - 128) * aspGain * _noiseScale / 8192.0f;
                }

                if (voiceAmpTrem > 0 || _fricAmp > 0 || breathGainNow > 0 || _lastAsp > 0) {
                    // Frication noise mixed directly into the source before the vocal tract filters.
                    // _fricAmp is the frication amplitude (voiced fricatives have both voiceAmpTrem and _fricAmp nonzero).
                    cascadeIn += (NextNoise() - 128) * _fricAmp * _noiseScale / 8192.0f;

                    // Nasal antiresonator (NZ) and resonator (NP) with smooth interpolation for nasal phonemes.
                    cascadeOut = cascadeIn + (_nzB * _nzD1) + (_nzC * _nzD2);
                    _nzD2 = _nzD1; _nzD1 = cascadeIn;
                    cascadeOut *= _nasalNorm;
                    cascadeOut = cascadeOut + (_npB * _npD1) + (_npC * _npD2);
                    _npD2 = _npD1; _npD1 = cascadeOut;

                    // Cascade resonators F1 through F5 (F4+F5 are fixed voice-tract resonances).
                    // Each stage is a second-order IIR: output = A*input + B*y[n-1] + C*y[n-2].
                    cascadeOut = (_f1A * cascadeOut) + (_f1B * _f1D1) + (_f1C * _f1D2);
                    _f1D2 = _f1D1; _f1D1 = cascadeOut;
                    cascadeOut = (_f2A * cascadeOut) + (_f2B * _f2D1) + (_f2C * _f2D2);
                    _f2D2 = _f2D1; _f2D1 = cascadeOut;
                    cascadeOut = (_f3A * cascadeOut) + (_f3B * _f3D1) + (_f3C * _f3D2);
                    _f3D2 = _f3D1; _f3D1 = cascadeOut;
                    cascadeOut = (_f4A * cascadeOut) + (_f4B * _f4D1) + (_f4C * _f4D2);
                    _f4D2 = _f4D1; _f4D1 = cascadeOut;
                    cascadeOut = (_f5cA * cascadeOut) + (_f5cB * _f5cD1) + (_f5cC * _f5cD2);
                    _f5cD2 = _f5cD1; _f5cD1 = cascadeOut;
                }

                // Parallel formant bank (Klatt 1980 Table II) for independent aspiration/fricative noise modeling.
                //float parallelNoise = (NextNoise() - 128) * _noiseAmp * _noiseScale;
                float parallelNoise = NextPinkNoise() * 128.0f * _noiseAmp * _noiseScale;

                if (_abAmp > 0) {
                    sampAB = parallelNoise * _abAmp / 4096.0f;
                }
                if (_pAmp2 > 0) {
                    samp2 = (_f2A * _pAmp2 * parallelNoise) + (_f2B * _f2pD1) + (_f2C * _f2pD2);
                    _f2pD2 = _f2pD1; _f2pD1 = samp2;
                }
                if (_pAmp3 > 0) {
                    samp3 = (_f3A * _pAmp3 * parallelNoise) + (_f3B * _f3pD1) + (_f3C * _f3pD2);
                    _f3pD2 = _f3pD1; _f3pD1 = samp3;
                }
                if (_pAmp4 > 0) {
                    samp4 = (_f4pA * _pAmp4 * parallelNoise) + (_f4pB * _f4pD1) + (_f4pC * _f4pD2);
                    _f4pD2 = _f4pD1; _f4pD1 = samp4;
                }
                if (_pAmp5 > 0) {
                    samp5 = (_f5pA * _pAmp5 * parallelNoise) + (_f5pB * _f5pD1) + (_f5pC * _f5pD2);
                    _f5pD2 = _f5pD1; _f5pD1 = samp5;
                }
                if (_pAmp6 > 0) {
                    samp6 = (_f6pA * _pAmp6 * parallelNoise) + (_f6pB * _f6pD1) + (_f6pC * _f6pD2);
                    _f6pD2 = _f6pD1; _f6pD1 = samp6;
                }

                float sample = cascadeOut + (sampAB - samp3 + samp4 - samp5 + samp6 - samp2);
                if (_hfEmph) {
                    // First-difference pre-emphasis (Klatt 1980): y = x - 0.97*x[n-1] compensates
                    // for the 6 dB/octave roll-off of the radiation load at the lips.
                    float preemphOut = sample - 0.97f * _preemphPrev;
                    _preemphPrev = sample;
                    sample = preemphOut;
                }

                sample = std::max(-8191.0f, std::min(8191.0f, sample * _outputGain));
                float raw = std::round(sample * 4.0f);
                raw = std::max(raw, (float)INT16_MIN);
                raw = std::min(raw, (float)INT16_MAX);
                outputBuffer[offset++] = (int16_t)raw;
            } else {
                _glotPhase = 0;
                _chorusPhase = 0;
                outputBuffer[offset++] = 0;
            }
        }
    }

    // Computes second-order IIR resonator coefficients for a pole at (hz, bandWidth).
    // Klatt (1980) eq. 1-3: r = exp(-pi*BW/Fs), C = -(r^2), B = 2r*cos(2pi*F/Fs), A = 1-B-C.
    // Transfer function: H(z) = A / (1 - B*z^-1 - C*z^-2); poles at z = r*exp(+/-j*2pi*F/Fs).
    void KlattSynthesizer::Calc_Pole_Coefficients(float& Acoeff, float& Bcoeff, float& Ccoeff,
                                                   int16_t pitch, int16_t bandWidth, int32_t voiceMinBW) {
        if (bandWidth > KMaxBandWidth) {
            bandWidth = (int16_t)KMaxBandWidth;
        }
        if (bandWidth < voiceMinBW) {
            bandWidth = (int16_t)voiceMinBW;
        }
        if (pitch < 256) {
            pitch = 256;
        }

        float hz = PitchToHz(pitch);
        float nyquist = _internalRate * 0.5f;
        if (hz >= nyquist * 0.85f) {
            // Near or above Nyquist: flatten into a wide shelf to avoid aliasing and near-Nyquist gain spikes.
            hz = nyquist * 0.80f;
            bandWidth = std::max(bandWidth, (int16_t)2000);
        }
        float r = (float)std::exp(-M_PI * bandWidth / _internalRate);
        float w = (float)(2.0 * M_PI * hz / _internalRate);

        Ccoeff = -(r * r);
        Bcoeff = 2.0f * r * (float)std::cos(w);
        Acoeff = 1.0f - Bcoeff - Ccoeff;
    }

    // Antiresonator (Klatt 1980): sign-inverted pole coefficients produce a spectral zero.
    void KlattSynthesizer::Calc_Zero_Coefficients(float& Acoeff, float& Bcoeff, float& Ccoeff,
                                                   int16_t pitch, int16_t bandWidth) {
        if (bandWidth > KMaxBandWidth) {
            bandWidth = (int16_t)KMaxBandWidth;
        }
        if (pitch < 256) {
            pitch = 256;
        }

        float hz = PitchToHz(pitch);
        float r = (float)std::exp(-M_PI * bandWidth / _internalRate);
        float w = (float)(2.0 * M_PI * hz / _internalRate);

        Ccoeff = r * r;
        Bcoeff = -2.0f * r * (float)std::cos(w);
        Acoeff = 1.0f + Bcoeff + Ccoeff;
    }

    // Converts Hz to log-domain pitch code to allow integer formant arithmetic.
    int16_t KlattSynthesizer::HzToPitch(int16_t hz) {
        const int32_t ratioK = 2621;
        int32_t fk, freq;
        if (hz <= 0) {
            return 0;
        }
        if (hz < 50) {
            freq = hz << 4;
            fk = 0x0;
        } else if (hz < 100) {
            freq = hz << 3;
            fk = 0x100;
        } else if (hz < 200) {
            freq = hz << 2;
            fk = 0x200;
        } else if (hz < 400) {
            freq = hz << 1;
            fk = 0x300;
        } else if (hz < 800) {
            freq = hz;
            fk = 0x400;
        } else if (hz < 1600) {
            freq = hz >> 1;
            fk = 0x500;
        } else if (hz < 3200) {
            freq = hz >> 2;
            fk = 0x600;
        } else {
            freq = hz >> 3;
            fk = 0x700;
        }

        int32_t ratio = ((freq - 400) * ratioK) >> 11;
        if (ratio < 0) {
            ratio = 0;
        }
        if (ratio > 511) {
            ratio = 511;
        }
        // Runtime LogarithmBase2Table replacement: floor(256*log2(1 + ratio/512))
        int32_t log = (int32_t)(256.0 * std::log(1.0 + (ratio / 512.0)) / std::log(2.0));
        return (int16_t)(log + fk);
    }

    int16_t KlattSynthesizer::PitchToHz(int16_t pitch) {
        // Runtime OctaveFrequencyTable + ExponentialOf2Table replacement:
        // OctaveFrequencyTable[oct] = 25<<oct, ExponentialOf2Table[i] = round(32768*2^(i/256))
        int32_t oct = (pitch & 0xF00) >> 8;
        int32_t frac = pitch & 0xFF;
        int32_t baseFreq = 25 << oct;
        int32_t exp = (int32_t)std::round(32768.0 * std::pow(2.0, frac / 256.0));
        return (int16_t)((baseFreq * exp) >> 15);
    }

} // namespace SharpVox
