#include "../include/KlattSynthesizerFP.h"
#include "../include/SynthData.h"
#include "../include/VoiceData.h"

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <map>


namespace SharpVox {

// Sin LUT: 512 entries, Q1
// Phase [0, 1<<17) maps to [0, 2); index = phase >> 8.

static int16_t s_sin_lut[512];
static bool    s_sin_lut_ready = false;

void KlattSynthesizerFP::InitSinLut() {
    if (s_sin_lut_ready) return;
    for (int i = 0; i < 512; i++) {
        s_sin_lut[i] = (int16_t)(32767.0f * sinf(2.0f * (float)M_PI * i / 512.0f));
    }
    s_sin_lut_ready = true;
}

static inline int32_t fp_sin(uint32_t phase) {
    return s_sin_lut[(phase >> 8) & 511];
}

//  Q15 coefficient helper 

void KlattSynthesizerFP::ToQ15(float A, float B, float C,
                                int32_t& Aq, int32_t& Bq, int32_t& Cq) {
    Aq = (int32_t)(A * 32768.0f);
    Bq = (int32_t)(B * 32768.0f);
    Cq = (int32_t)(C * 32768.0f);
}

//  IIR step: Q15 coefficients x Q0 delay taps 
// Equivalent to float: out = A*x + B*d1 + C*d2.
// Products Q15*Q0 = Q15; shift right 15 gives Q0 result.

static inline int32_t fp_iir(int32_t A, int32_t B, int32_t C,
                              int32_t x, int32_t& d1, int32_t& d2) {
    int64_t acc = (int64_t)A * x + (int64_t)B * d1 + (int64_t)C * d2;
    int32_t y = (int32_t)(acc >> 15);
    d2 = d1;
    d1 = y;
    return y;
}

// Cascade IIR with float delay taps  avoids integer quantization artifacts
// at high sample rates where pole radii approach 1.  Q15 coefficients are
// converted to float inline; the cost is negligible vs. the synthesis loop.
static inline float fp_iir_f(int32_t A, int32_t B, int32_t C,
                               float x, float& d1, float& d2) {
    static const float k = 1.0f / 32768.0f;
    float y = A * k * x + B * k * d1 + C * k * d2;
    d2 = d1;
    d1 = y;
    return y;
}

// All-float cascade IIR  used for F1/F2/F3 where coefficients are derived
// directly from (r, cos) without Q15 quantization.
static inline float fp_iir_ff(float A, float B, float C,
                                float x, float& d1, float& d2) {
    float y = A * x + B * d1 + C * d2;
    d2 = d1;
    d1 = y;
    return y;
}

// Matched one-zero resonator step (Vicanek 2016): numerator b0 + b1*z^-1
// over the classic Klatt pole pair. x1 holds the previous input sample.
static inline float fp_iir_zff(float b0, float b1, float B, float C,
                                 float x, float& x1, float& d1, float& d2) {
    float y = b0 * x + b1 * x1 + B * d1 + C * d2;
    x1 = x;
    d2 = d1;
    d1 = y;
    return y;
}

// LF glottal model (Fant, Liljencrants & Lin 1985, STL-QPSR 4/1985)
// Differentiated flow, Ee = 1, with wg = pi/Tp and an exponential return
// phase, shape constants from FL85 eq.12 and zero net flow. T0-normalized.

// Fant 1995 (STL-QPSR 36(2-3):119-156) Rd to T0-normalized (te,tp,ta),
// as implemented in covarep Rd2tetpta.m. 0.3=pressed, 1.0=modal, 2.7=breathy.
static inline void LfRdToTeTpTa(float rd, float& te, float& tp, float& ta) {
    float Rap = (-1.0f + 4.8f * rd) * 0.01f;                       // [1](2)
    float Rkp = (22.4f + 11.8f * rd) * 0.01f;                      // [1](3)
    float Rgp = 1.0f / (4.0f * ((0.11f * rd / (0.5f + 1.2f * Rkp)) - Rap) / Rkp);  // [1](4)
    tp = 1.0f / (2.0f * Rgp);                                      // [1] p.121
    te = tp * (Rkp + 1.0f);                                        // [1] p.121
    ta = Rap;                                                      // [1] p.121
}

// Return-phase constant e from 1 - e^(-e*(1-te)) = e*ta [FL85 eq.12]
// Bisection on (0, hi), the trivial root at e = 0 excluded
static inline float LfSolveEps(float te, float ta) {
    auto fb = [&](float e) { return 1.0f - std::exp(-e * (1.0f - te)) - e * ta; };
    float lo = 1e-9f;
    float hi = 1.0f / (ta > 1e-9f ? ta : 1e-9f) + 10.0f;
    while (fb(hi) > 0.0f) hi *= 2.0f;
    for (int i = 0; i < 64; i++) {
        float mid = 0.5f * (lo + hi);
        if (fb(mid) > 0.0f) lo = mid; else hi = mid;
    }
    return 0.5f * (lo + hi);
}

// Glottal tilt, scales the return-phase quotient Ra = ta/T0 before the
// shape solves, longer = darker. 1.0 = neutral, 0.3-3.0 spans ~8 dB H1-H2
static constexpr float kGlotTaScale = 0.6f;

// Open-phase decay a from zero net flow over the cycle (covarep
// gfm_spec_lf.m). Scan a log grid for the sign change, then bisect
static inline float LfSolveAlpha(float te, float tp, float ta, float eps) {
    float wg = (float)M_PI / tp;
    float A = (1.0f - std::exp(-eps * (1.0f - te))) / (eps * eps * ta)
            - (1.0f - te) * std::exp(-eps * (1.0f - te)) / (eps * ta);
    auto fa = [&](float a) {
        return (a * a + wg * wg) * std::sin(wg * te) * A
             + wg * std::exp(-a * te)
             + a * std::sin(wg * te) - wg * std::cos(wg * te);
    };
    static const float xs[10] = {0.0f, 1e1f, 1e2f, 1e3f, 1e4f, 1e5f, 1e6f, 1e7f, 1e8f, 1e9f};
    float lo = xs[0], hi = xs[9];
    for (int i = 0; i < 9; i++)
        if (fa(xs[i]) * fa(xs[i + 1]) < 0.0f) { lo = xs[i]; hi = xs[i + 1]; break; }
    for (int i = 0; i < 64; i++) {
        float mid = 0.5f * (lo + hi);
        if (fa(lo) * fa(mid) <= 0.0f) hi = mid; else lo = mid;
    }
    return 0.5f * (lo + hi);
}

//  Constructor

KlattSynthesizerFP::KlattSynthesizerFP(int32_t sampleRate) {
    if (sampleRate < KMinSampleRate || sampleRate > KMaxSampleRate) {
        throw std::invalid_argument(
            "Sample rate " + std::to_string(sampleRate) + " Hz outside supported range "
            + std::to_string(KMinSampleRate) + "-" + std::to_string(KMaxSampleRate) + " Hz.");
    }

    InitSinLut();

    _sampleRate     = sampleRate;
    _internalRate   = sampleRate;
    // White noise PSD in-band falls as 1/fs, so amplitude must rise as
    // sqrt(fs) to keep noise loudness rate-invariant through fixed-Hz filters.
    _noiseScale     = sqrtf((float)sampleRate / 22050.0f);
    // Rate-invariant since the preemph stage scales by fs/22050; the constant
    // is anchored so 48 kHz keeps the level the F0 headroom was tuned at.
    _outputGain     = 4.10f;
    _speechVolume   = 150.0f;
    _hfEmph         = true;

    _outputGain_q15   = (int32_t)(_outputGain * 32768.0f);
    _noiseGain_q0     = 0;  // set in SetVoice

    // Pre-emphasis zero, rate-compensated so its corner stays at ~107 Hz.
    // A fixed 0.97 puts the zero at a fixed fraction of Nyquist, which costs
    // ~1.5 dB of 2-4 kHz energy per doubling of rate. 22050 is the tuning anchor.
    _preemphA_q15 = (int32_t)(std::pow(0.97, 22050.0 / (double)sampleRate) * 32768.0 + 0.5);

    // The differentiator's passband gain falls as 1/fs with the corner fixed
    // in Hz; scaling by fs/22050 makes the output level rate-invariant.
    _preemphScale_q12 = (int32_t)(4096.0 * sampleRate / 22050.0 + 0.5);

    // (1<<28)/sampleRate in Q4: glotPhaseInc = effF0Hz * _phaseIncPerHz_q4 >> 4
    _phaseIncPerHz_q4 = (int32_t)((1LL << 28) / sampleRate);

    double rawLen = sampleRate * (KDefaultSampFrameLen / (double)KDefaultSampleRate);
    int32_t len = (int32_t)std::floor(rawLen + 0.5);
    if (len < 2) len = 2;
    SampFrameLen = len;

    // Fixed subglottal resonator ~350 Hz / BW 80.
    {
        float A, B, C;
        Calc_Pole_Coefficients(A, B, C, HzToPitch(350), 80);
        ToQ15(A, B, C, _sgA, _sgB, _sgC);
    }

    // Zero-initialise IIR coefficients.
    _f2A=_f2B=_f2C=0; _f3A=_f3B=_f3C=0;
    _f4b0=_f4b1=_f4B=_f4C=0.0f; _f5cb0=_f5cb1=_f5cB=_f5cC=0.0f; _f6cb0=_f6cb1=_f6cB=_f6cC=0.0f;
    _f7cb0=_f7cb1=_f7cB=_f7cC=0.0f; _f8cb0=_f8cb1=_f8cB=_f8cC=0.0f;
    _f4pA=_f4pB=_f4pC=0; _f5pA=_f5pB=_f5pC=0; _f6pA=_f6pB=_f6pC=0;
    _nzA=_nzB=_nzC=0; _npA=_npB=_npC=0;

    // Cascade F1-F3 physical interpolation state: neutral (pole at origin, unity numerator).
    _f1r=0.0f; _f1cosw=1.0f; _f2r=0.0f; _f2cosw=1.0f; _f3r=0.0f; _f3cosw=1.0f;
    _f1b0=1.0f; _f1b1=0.0f; _f2b0=1.0f; _f2b1=0.0f; _f3b0=1.0f; _f3b1=0.0f;

    // Zero-initialise delay taps and filter state.
    _f1D1=_f1D2=0; _f2D1=_f2D2=0; _f3D1=_f3D2=0;
    _f4D1=_f4D2=0; _f5cD1=_f5cD2=0; _f6cD1=_f6cD2=0;
    _f7cD1=_f7cD2=0; _f8cD1=_f8cD2=0;
    _f1X1=_f2X1=_f3X1=_f4X1=_f5cX1=_f6cX1=_f7cX1=_f8cX1=0.0f;
    _f2pD1=_f2pD2=0; _f3pD1=_f3pD2=0;
    _f4pD1=_f4pD2=0; _f5pD1=_f5pD2=0; _f6pD1=_f6pD2=0;
    _nzD1=_nzD2=0; _npD1=_npD2=0;
    _sgD1=_sgD2=0; _preemphPrev=0; _tiltPrev=0;

    _voiceAmp_q8=_fricAmp_q8=_abAmp_q8=0;
    _pAmp2_q8=_pAmp3_q8=_pAmp4_q8=_pAmp5_q8=_pAmp6_q8=0;
    _nasalNorm_q15=32768;

    _glotPhase=_glotPhaseInc=0;
    _chorusPhase=_chorusPhaseInc=0;
    _Ne_fp = _chorusNe_fp = 655360;
    _voiceGain_f = 0.0f;
#ifdef SHARPVOX_SAMPLED_GLOT
    _useSampledGlot = false;
    _sgNatPitchHz = 1.0f;
    _sgPhase = 0.0f;
    _sgBufSize = 0;
#endif

    _shimmerScale=1.0f; _diploScale=1.0f;
    _cycleCount=0; _fryStallSamples=0; _diploPhase=0;

    _vibratoPhase_fp=0; _tremoloPhase_fp=0;
    _vibDepth_q8=0; _vibRate_q8=0;
    _tremDepth_q15=0; _tremRate_q8=0;
    _asp_q15=0; _tilt_q15=0;

    _pink0=_pink1=_pink2=_pink3=_pink4=_pink5=_pink6=0;

    {
        float rat = 22050.0f / (float)sampleRate;
        const float po[] = {0.99886f,0.99332f,0.96900f,0.86650f,0.55000f,0.76160f};
        const float go[] = {0.0555179f,0.0750759f,0.1538520f,0.3104856f,0.5329522f,0.0168980f};
        int32_t* pq[] = {&_pnP0q15,&_pnP1q15,&_pnP2q15,&_pnP3q15,&_pnP4q15,&_pnP5mq15};
        int32_t* gq[] = {&_pnG0q15,&_pnG1q15,&_pnG2q15,&_pnG3q15,&_pnG4q15,&_pnG5q15};
        for (int i = 0; i < 6; i++) {
            float p = powf(po[i], rat);
            float g = go[i] * sqrtf((1.0f - p*p) / (1.0f - po[i]*po[i]));
            *pq[i] = (int32_t)(p * 32768.0f);
            *gq[i] = (int32_t)(g * 32768.0f);
        }
        // The two white passthrough terms need the same PSD compensation the
        // poles get above: gain sqrt(fs/22050) keeps the flat tail level fixed.
        float ws = sqrtf((float)sampleRate / 22050.0f);
        _pnW0q15 = (int32_t)(17574.0f * ws);
        _pnW1q15 = (int32_t)(3799.0f * ws);
    }

    _noiseSeed=0x12345;

    _noiseAmp=0.0f; _breathGain=0.0f; _breathCycle=0;
    _nasalPoleFreq=0; _nasalPoleBW=0;
    _f4cFreq=_f4cBW=0; _f5cFreq=_f5cBW=0;
    _f4pFreq=_f4pBW=0; _f5pFreq=_f5pBW=0; _f6pFreq=_f6pBW=0;

    _voiceTiltBias=0.0f; _openQuotient=50;
    UpdateLfGlottal();
}

//  SetVoice 

void KlattSynthesizerFP::SetVoice(int16_t nGain, bool bit16,
                                   int16_t f4_Freq,  int16_t f4_BW,
                                   int16_t f5_Freq,  int16_t f5_BW,
                                   int16_t f4p_Freq, int16_t bw4p_BW,
                                   int16_t f5p_Freq, int16_t bw5p_BW,
                                   int16_t f6p_Freq, int16_t bw6p_BW,
                                   int16_t nasal_Base, int16_t nasal_BW,
                                   int16_t aGain, int16_t aCycle) {
    _breathGain  = (aGain * KNoiseGain) / 100.0f;
    _breathCycle = aCycle;

    _noiseAmp = nGain / 100.0f;
    if (bit16) _noiseAmp *= (0xCCCC / 65536.0f);

    _f4cFreq = HzToPitch(f4_Freq);  _f4cBW = f4_BW;
    _f5cFreq = HzToPitch(f5_Freq);  _f5cBW = f5_BW;
    // Fixed cascade F6: Klatt 1980 higher-pole correction (4900Hz, BW 1000, NFC=6).
    _f6cFreq = HzToPitch(4900);  _f6cBW = 1000;
    // Fixed cascade F7/F8: Rabiner 1968 high poles the classic 10kHz Klatt got
    // free from phantom near-Nyquist poles, absent at 48kHz. Refills the >F6
    // voiced envelope so vowels do not cliff above 5kHz. Nyquist-clamped below.
    _f7cFreq = HzToPitch(6500);  _f7cBW = 720;
    _f8cFreq = HzToPitch(7500);  _f8cBW = 1250;
    _f4pFreq = HzToPitch(f4p_Freq); _f4pBW = bw4p_BW;
    _f5pFreq = HzToPitch(f5p_Freq); _f5pBW = bw5p_BW;
    _f6pFreq = HzToPitch(f6p_Freq); _f6pBW = bw6p_BW;
    _nasalPoleFreq = HzToPitch(nasal_Base);
    _nasalPoleBW   = nasal_BW;

    // No _noiseScale here: the pink generator's pole and tail gains already
    // hold its PSD rate-invariant, so scaling again overshot at high rates.
    _noiseGain_q0 = (int32_t)(128.0f * _noiseAmp);

    InitFixedFormants();
}

// White-noise RMS gain of the DC-normalized two-pole resonator, applying
// the same near-Nyquist clamp as Calc_Pole_Coefficients. Ratio against the
// 22050 reference holds a parallel branch's noise loudness rate-invariant.
static float ResonatorNoiseGain(float hz, float bw, float fs) {
    float nyq = fs * 0.5f;
    if (hz >= nyq * 0.85f) {
        hz = nyq * 0.80f;
        bw = std::max(bw, 2000.0f);
    }
    float r  = expf(-(float)M_PI * bw / fs);
    float w0 = 2.0f * (float)M_PI * hz / fs;
    float B = 2.0f * r * cosf(w0), C = -r * r, A = 1.0f - B - C;
    // Closed form for sum h[n]^2 of 1/(1 - B/z - C/z^2), times fs to turn
    // the normalized-frequency integral into Hz (input PSD is flat in Hz).
    return A * sqrtf(fs * (1.0f - C) / ((1.0f + C) * ((1.0f - C) * (1.0f - C) - B * B)));
}

void KlattSynthesizerFP::InitFixedFormants() {
    float A, B, C;

    Calc_Matched_Pole_Coefficients(_f4b0, _f4b1, _f4B, _f4C, _f4cFreq, _f4cBW);
    Calc_Matched_Pole_Coefficients(_f5cb0, _f5cb1, _f5cB, _f5cC, _f5cFreq, _f5cBW);
    Calc_Matched_Pole_Coefficients(_f6cb0, _f6cb1, _f6cB, _f6cC, _f6cFreq, _f6cBW);
    Calc_Matched_Pole_Coefficients(_f7cb0, _f7cb1, _f7cB, _f7cC, _f7cFreq, _f7cBW);
    Calc_Matched_Pole_Coefficients(_f8cb0, _f8cb1, _f8cB, _f8cC, _f8cFreq, _f8cBW);

    // No amplitude fade near Nyquist: Calc_Pole_Coefficients clamps such
    // poles to 0.8*nyq with widened BW (S was fully silent at 8 kHz), and
    // peak normalization keeps each branch's loudness rate-invariant.
    auto noiseComp = [&](int16_t pitchCode, int16_t bwv) -> float {
        float hz = (float)PitchToHz(pitchCode);
        return ResonatorNoiseGain(hz, (float)bwv, 22050.0f)
             / ResonatorNoiseGain(hz, (float)bwv, (float)_internalRate);
    };

    Calc_Pole_Coefficients(A, B, C, _f4pFreq, _f4pBW);
    A *= (KNoiseGain / 8192.0f) * noiseComp(_f4pFreq, _f4pBW);
    ToQ15(A, B, C, _f4pA, _f4pB, _f4pC);

    Calc_Pole_Coefficients(A, B, C, _f5pFreq, _f5pBW);
    A *= (KNoiseGain / 8192.0f) * noiseComp(_f5pFreq, _f5pBW);
    ToQ15(A, B, C, _f5pA, _f5pB, _f5pC);

    Calc_Pole_Coefficients(A, B, C, _f6pFreq, _f6pBW);
    A *= (KNoiseGain / 8192.0f) * noiseComp(_f6pFreq, _f6pBW);
    ToQ15(A, B, C, _f6pA, _f6pB, _f6pC);

    Calc_Pole_Coefficients(A, B, C, _nasalPoleFreq, _nasalPoleBW);
    ToQ15(A, B, C, _npA, _npB, _npC);
}

void KlattSynthesizerFP::UpdateLfGlottal() {
    // OpenQuotient slider drives Rd directly, 0->0.3 (pressed), 50->1.0
    // (modal), 100->2.2 (breathy)
    float oq = (float)std::max(0, std::min(100, (int)_openQuotient));
    float rd = (oq <= 50.0f) ? 0.3f + 0.7f * (oq / 50.0f)
                             : 1.0f + 1.2f * ((oq - 50.0f) / 50.0f);

    float te, tp, ta;
    LfRdToTeTpTa(rd, te, tp, ta);
    ta *= kGlotTaScale;
    // Return phase must fit inside the cycle or the solves break
    if (te + ta > 0.98f) ta = 0.98f - te;
    float eps   = LfSolveEps(te, ta);
    float alpha = LfSolveAlpha(te, tp, ta, eps);
    float wg    = (float)M_PI / tp;
    float e0    = -1.0f / (std::exp(alpha * te) * std::sin(wg * te));  // FL85 eq.5, Ee=1

    // Loudness is ~flat across OQ and tilt, fit from rendered RMS over
    // a (Rd, ta) grid, worst ~2 dB at the bright end
    float lfBase = 0.0508f + 0.0461f * rd;
    float lfCorr = 1.0549f - 0.3142f * rd + 6.4298f * ta + 1.3477f * rd * ta
                 - 6.7109f * ta * ta - 9.5047f * rd * ta * ta;
    _lfGain = lfBase * lfCorr;
    _lfTe = te; _lfWg = wg; _lfEps = eps; _lfAlpha = alpha; _lfE0 = e0; _lfTa = ta;
    _lfOpenScale = e0 / (alpha * alpha + wg * wg);
    _lfFlowGte = _lfOpenScale * (std::exp(alpha * te) *
                  (alpha * std::sin(wg * te) - wg * std::cos(wg * te)) + wg);
    _lfRetExp = std::exp(-eps * (1.0f - te));

    // Open/return boundary in 24-bit phase units, fry-scaled via effNe
    _Ne_fp = std::max(655360, std::min(16056320, (int32_t)std::round(te * 16777216.0f)));
    _chorusNe_fp = _Ne_fp;
}

void KlattSynthesizerFP::ComputeGlotWave(int16_t vGain) {
    UpdateLfGlottal();
    _voiceGain_f = (vGain > 0) ? (vGain * 1140.0f) : 0.0f;
}

// Differentiated LF flow at phase phi (24-bit cycle units), Ee = 1
// *flowOut receives the flow integral (breathiness gate) when non-null
// ne is the open/return boundary in phase units, fry-scaled
float KlattSynthesizerFP::LfPulse(int32_t phi, int32_t ne, float* flowOut) const {
    const float kInvP24 = 1.0f / 16777216.0f;
    float tau = (float)phi * kInvP24;
    float te  = (float)ne * kInvP24;
    float s = 0.0f, flow = 0.0f;
    if (tau < te) {
        float ea = std::exp(_lfAlpha * tau);
        float sw = std::sin(_lfWg * tau);
        float cw = std::cos(_lfWg * tau);
        s = _lfE0 * ea * sw;
        if (flowOut) flow = _lfOpenScale * (ea * (_lfAlpha * sw - _lfWg * cw) + _lfWg);
    } else if (tau < 1.0f) {
        float ed = std::exp(-_lfEps * (tau - te));
        s = -(1.0f / (_lfEps * _lfTa)) * (ed - _lfRetExp);
        if (flowOut) flow = _lfFlowGte - (1.0f / (_lfEps * _lfTa))
                            * ((1.0f - ed) / _lfEps - (tau - te) * _lfRetExp);
    }
    if (flowOut) *flowOut = flow;
    return s;
}

#ifdef SHARPVOX_SAMPLED_GLOT

void KlattSynthesizerFP::SetGlottalSample(const float* pcm, int32_t length,
                                           int32_t srcRate, float naturalPitchHz) {
    float ratio = (float)_sampleRate / (float)srcRate;
    int32_t outLen = std::max((int32_t)2, (int32_t)roundf((float)length * ratio));
    _sgBuf.resize(outLen);
    float invRatio = 1.0f / ratio;
    for (int32_t i = 0; i < outLen; i++) {
        float srcPos = (float)i * invRatio;
        int32_t s0 = (int32_t)srcPos;
        int32_t s1 = std::min(s0 + 1, length - 1);
        float frac = srcPos - (float)s0;
        _sgBuf[i] = pcm[s0] + frac * (pcm[s1] - pcm[s0]);
    }
    float maxAbs = 0.0f;
    for (float v : _sgBuf) { float a = v < 0 ? -v : v; if (a > maxAbs) maxAbs = a; }
    if (maxAbs > 0.0f) { float inv = 0.5f / maxAbs; for (float& v : _sgBuf) v *= inv; }
    _sgNatPitchHz = (naturalPitchHz > 0.0f) ? naturalPitchHz : 1.0f;
    _sgBufSize    = outLen;
    _sgPhase      = 0.0f;
    _useSampledGlot = true;
}

void KlattSynthesizerFP::ClearGlottalSample() {
    _useSampledGlot = false;
    _sgBuf.clear();
    _sgBuf.shrink_to_fit();
    _sgBufSize = 0;
    _sgPhase   = 0.0f;
}

#endif // SHARPVOX_SAMPLED_GLOT

//  AdjFormant / NextNoise

int16_t KlattSynthesizerFP::AdjFormant(int16_t pitch, int32_t formant) {
    if (LarynxOffset==0 && PharyngealAmt==0 && LipRounding==0) return pitch;
    int32_t hz = PitchToHz(pitch) + LarynxOffset;
    hz += formant==1 ? PharyngealAmt - (LipRounding/2)
        : formant==2 ? -PharyngealAmt*2 - (LipRounding*3)
        : formant==3 ? -LipRounding : 0;
    hz = std::max(hz, (int32_t)50);
    hz = std::min(hz, (int32_t)8000);
    return HzToPitch((int16_t)hz);
}

int32_t KlattSynthesizerFP::NextNoise() {
    _noiseSeed = (_noiseSeed * 1103515245 + 12345) & 0x7FFFFFFF;
    return (_noiseSeed >> 16) & 0xFF;
}

//  Pink noise (Q15) 
// State variables are Q15.  White input (NextNoise()-128)<<8  32768 (Q15).
// Poles and gains are rate-adapted at construction; _pnP*q15/_pnG*q15 are Q15.

int32_t KlattSynthesizerFP::NextPinkNoise_q15() {
    int32_t w = (NextNoise() - 128) << 8;  // Q15 white noise

    _pink0 = (int32_t)(((int64_t)_pnP0q15  * _pink0 + (int64_t)_pnG0q15 * w) >> 15);
    _pink1 = (int32_t)(((int64_t)_pnP1q15  * _pink1 + (int64_t)_pnG1q15 * w) >> 15);
    _pink2 = (int32_t)(((int64_t)_pnP2q15  * _pink2 + (int64_t)_pnG2q15 * w) >> 15);
    _pink3 = (int32_t)(((int64_t)_pnP3q15  * _pink3 + (int64_t)_pnG3q15 * w) >> 15);
    _pink4 = (int32_t)(((int64_t)_pnP4q15  * _pink4 + (int64_t)_pnG4q15 * w) >> 15);
    _pink5 = (int32_t)((-(int64_t)_pnP5mq15 * _pink5 - (int64_t)_pnG5q15 * w) >> 15);
    int32_t sum = _pink0 + _pink1 + _pink2 + _pink3 + _pink4 + _pink5
                + _pink6 + (int32_t)(((int64_t)_pnW0q15 * w) >> 15);
    _pink6 = (int32_t)(((int64_t)_pnW1q15 * w) >> 15);

    return (int32_t)(((int64_t)5898 * sum) >> 15);
}

//  Calc_Pole / Calc_Zero Coefficients 
// Identical to float version  output is float A/B/C, converted to Q15 by callers.

void KlattSynthesizerFP::Calc_Pole_Coefficients(float& Acoeff, float& Bcoeff, float& Ccoeff,
                                                  int16_t pitch, int16_t bandWidth,
                                                  int32_t voiceMinBW) {
    if (bandWidth > KMaxBandWidth) bandWidth = (int16_t)KMaxBandWidth;
    if (bandWidth < voiceMinBW)    bandWidth = (int16_t)voiceMinBW;
    if (pitch < 256)               pitch = 256;

    float hz = (float)PitchToHz(pitch);
    float nyquist = _internalRate * 0.5f;
    if (hz >= nyquist * 0.85f) {
        hz = nyquist * 0.80f;
        bandWidth = std::max(bandWidth, (int16_t)2000);
    }
    float r = expf(-(float)M_PI * bandWidth / _internalRate);
    float w = 2.0f * (float)M_PI * hz / _internalRate;
    Ccoeff = -(r * r);
    Bcoeff = 2.0f * r * cosf(w);
    Acoeff = 1.0f - Bcoeff - Ccoeff;
}

// Matched one-zero resonator (Vicanek 2016, "Matched Second Order Digital
// Filters", sec 4.1): same impulse-invariant poles as Calc_Pole_Coefficients,
// but numerator b0+b1*z^-1 fitted so |H| matches the analog resonance
// prototype H(s)=w0^2/(w0^2+s*w0/Q+s^2) with f0=sqrt(F^2+(BW/2)^2), Q=f0/BW.
void KlattSynthesizerFP::Calc_Matched_Pole_Coefficients(float& b0, float& b1,
                                                          float& Bcoeff, float& Ccoeff,
                                                          int16_t pitch, int16_t bandWidth,
                                                          int32_t voiceMinBW) {
    if (bandWidth > KMaxBandWidth) bandWidth = (int16_t)KMaxBandWidth;
    if (bandWidth < voiceMinBW)    bandWidth = (int16_t)voiceMinBW;
    if (pitch < 256)               pitch = 256;

    double hz = (double)PitchToHz(pitch);
    double nyquist = _internalRate * 0.5;
    if (hz >= nyquist * 0.85) {
        hz = nyquist * 0.80;
        bandWidth = std::max(bandWidth, (int16_t)2000);
    }
    double bw = bandWidth, fs = _internalRate;
    // Poles: identical values to Calc_Pole_Coefficients (Klatt sign convention).
    double a1 = -2.0 * std::exp(-M_PI * bw / fs) * std::cos(2.0 * M_PI * hz / fs);
    double a2 = std::exp(-2.0 * M_PI * bw / fs);
    Bcoeff = (float)(-a1);
    Ccoeff = (float)(-a2);

    double f0 = std::sqrt(hz * hz + 0.25 * bw * bw);
    double Q  = f0 / bw;
    double w0 = 2.0 * M_PI * f0 / fs;
    // Vicanek eq. 26-27, 31-34. Double precision required for the B1 cancellation.
    double p1 = std::sin(0.5 * w0); p1 *= p1;
    double p0 = 1.0 - p1;
    double A0 = 1.0 + a1 + a2; A0 *= A0;
    double A1 = 1.0 - a1 + a2; A1 *= A1;
    double A2 = -4.0 * a2;
    double R1 = (A0 * p0 + A1 * p1 + A2 * 4.0 * p0 * p1) * Q * Q;
    double B1 = (R1 - A0 * p0) / p1;
    double sB0 = 1.0 + a1 + a2;
    if (B1 <= 0.0) {
        // Degenerate fit: fall back to the all-pole unity-DC numerator.
        b0 = (float)sB0;
        b1 = 0.0f;
        return;
    }
    b0 = (float)(0.5 * (sB0 + std::sqrt(B1)));
    b1 = (float)(sB0 - b0);
}

void KlattSynthesizerFP::Calc_Zero_Coefficients(float& Acoeff, float& Bcoeff, float& Ccoeff,
                                                  int16_t pitch, int16_t bandWidth) {
    if (bandWidth > KMaxBandWidth) bandWidth = (int16_t)KMaxBandWidth;
    if (pitch < 256)               pitch = 256;

    float hz = (float)PitchToHz(pitch);
    float r = expf(-(float)M_PI * bandWidth / _internalRate);
    float w = 2.0f * (float)M_PI * hz / _internalRate;
    Ccoeff =  r * r;
    Bcoeff = -2.0f * r * cosf(w);
    Acoeff =  1.0f + Bcoeff + Ccoeff;
}

//  SynthesizeFrame 

void KlattSynthesizerFP::SynthesizeFrame(Frame frame, int16_t* outputBuffer, int32_t offset) {

    //  Frame-boundary target amplitudes (Q8 = float * 256) 
    int32_t tVoiceAmp = (int32_t)(frame.Av * _speechVolume * 256.0f);
    int32_t tFricAmp  = (int32_t)(frame.Af * _speechVolume * 4.0f * 256.0f);
    int32_t tAbAmp    = (int32_t)(frame.AB * _speechVolume * 256.0f);
    int32_t tPAmp2    = (int32_t)(frame.A2 / 32.0f * 256.0f);
    int32_t tPAmp3    = (int32_t)(frame.A3 / 32.0f * 256.0f);
    int32_t tPAmp4    = (int32_t)(frame.A4 / 32.0f * 256.0f);
    int32_t tPAmp5    = (int32_t)(frame.A5 / 32.0f * 256.0f);
    int32_t tPAmp6    = (int32_t)(frame.A6 / 32.0f * 256.0f);

    //  Onset reset (voiced/fricated start from silence) 
    if (_voiceAmp_q8==0 && _fricAmp_q8==0 && (tVoiceAmp>0 || tFricAmp>0)) {
        _glotPhase=0; _chorusPhase=0;
        _shimmerScale=1.0f; _diploScale=1.0f;
        _cycleCount=0; _fryStallSamples=0; _diploPhase=0;
#ifdef SHARPVOX_SAMPLED_GLOT
        _sgPhase=0.0f;
#endif
        _sgD1=_sgD2=0;
        _f1D1=_f1D2=_f2D1=_f2D2=_f3D1=_f3D2=_f4D1=_f4D2=_f5cD1=_f5cD2=_f6cD1=_f6cD2=0;
        _f7cD1=_f7cD2=_f8cD1=_f8cD2=0;
        _f1X1=_f2X1=_f3X1=_f4X1=_f5cX1=_f6cX1=_f7cX1=_f8cX1=0.0f;
        _npD1=_npD2=_nzD1=_nzD2=0;
        _preemphPrev=0; _tiltPrev=0;

        float A,B,C;
        Calc_Matched_Pole_Coefficients(_f1b0,_f1b1,B,C, AdjFormant((int16_t)(frame.F1+_f1FreqOffset),1), frame.Bw1);
        _f1r = sqrtf(-C); _f1cosw = (_f1r > 0.0f) ? B / (2.0f * _f1r) : 1.0f;
        Calc_Matched_Pole_Coefficients(_f2b0,_f2b1,B,C, AdjFormant((int16_t)(frame.F2+_f2FreqOffset),2), frame.Bw2);
        _f2r = sqrtf(-C); _f2cosw = (_f2r > 0.0f) ? B / (2.0f * _f2r) : 1.0f;
        _f2B = (int32_t)(2.0f*_f2r*_f2cosw*32768.0f); _f2C = -(int32_t)(_f2r*_f2r*32768.0f); _f2A = 32768-_f2B-_f2C;
        Calc_Matched_Pole_Coefficients(_f3b0,_f3b1,B,C, AdjFormant((int16_t)(frame.F3+_f3FreqOffset),3), frame.Bw3);
        _f3r = sqrtf(-C); _f3cosw = (_f3r > 0.0f) ? B / (2.0f * _f3r) : 1.0f;
        _f3B = (int32_t)(2.0f*_f3r*_f3cosw*32768.0f); _f3C = -(int32_t)(_f3r*_f3r*32768.0f); _f3A = 32768-_f3B-_f3C;

        if (frame.FNZ != _nasalPoleFreq) {
            Calc_Zero_Coefficients(A,B,C, (int16_t)(frame.FNZ+_nasalFreqOffset), _nasalPoleBW);
            _nasalNorm_q15 = (A != 0.0f)
                ? (int32_t)((_npA / 32768.0f) / A * 32768.0f) : 32768;
            ToQ15(A,B,C, _nzA,_nzB,_nzC);
        } else {
            _nzA=_npA; _nzB=-_npB; _nzC=-_npC;
            _nasalNorm_q15=32768;
        }

        _voiceAmp_q8=0; _fricAmp_q8=0; _abAmp_q8=0;
        _pAmp2_q8=tPAmp2; _pAmp3_q8=tPAmp3; _pAmp4_q8=tPAmp4;
        _pAmp5_q8=tPAmp5; _pAmp6_q8=tPAmp6;
    }

    //  Target (r, cos) and matched numerator for variable cascade formants
    float f1T_b0,f1T_b1,f1TB,f1TC, f2T_b0,f2T_b1,f2TB,f2TC, f3T_b0,f3T_b1,f3TB,f3TC;
    Calc_Matched_Pole_Coefficients(f1T_b0,f1T_b1,f1TB,f1TC, AdjFormant((int16_t)(frame.F1+_f1FreqOffset),1), frame.Bw1);
    Calc_Matched_Pole_Coefficients(f2T_b0,f2T_b1,f2TB,f2TC, AdjFormant((int16_t)(frame.F2+_f2FreqOffset),2), frame.Bw2);
    Calc_Matched_Pole_Coefficients(f3T_b0,f3T_b1,f3TB,f3TC, AdjFormant((int16_t)(frame.F3+_f3FreqOffset),3), frame.Bw3);

    float f1T_r = sqrtf(-f1TC), f1T_cw = f1T_r > 0.0f ? f1TB/(2.0f*f1T_r) : 1.0f;
    float f2T_r = sqrtf(-f2TC), f2T_cw = f2T_r > 0.0f ? f2TB/(2.0f*f2T_r) : 1.0f;
    float f3T_r = sqrtf(-f3TC), f3T_cw = f3T_r > 0.0f ? f3TB/(2.0f*f3T_r) : 1.0f;

    int32_t nzTA_q,nzTB_q,nzTC_q, tNasalNorm_q15=32768;
    if (frame.FNZ != _nasalPoleFreq) {
        float A,B,C;
        Calc_Zero_Coefficients(A,B,C, (int16_t)(frame.FNZ+_nasalFreqOffset), _nasalPoleBW);
        tNasalNorm_q15 = (A != 0.0f)
            ? (int32_t)((_npA / 32768.0f) / A * 32768.0f) : 32768;
        ToQ15(A,B,C, nzTA_q,nzTB_q,nzTC_q);
    } else {
        nzTA_q=_npA; nzTB_q=-_npB; nzTC_q=-_npC;
    }

    //  Per-sample interpolation deltas 
    int32_t dVoiceAmp=(tVoiceAmp-_voiceAmp_q8)/SampFrameLen;
    int32_t dFricAmp =(tFricAmp -_fricAmp_q8) /SampFrameLen;
    int32_t dAbAmp   =(tAbAmp   -_abAmp_q8)   /SampFrameLen;
    int32_t dPAmp2   =(tPAmp2   -_pAmp2_q8)   /SampFrameLen;
    int32_t dPAmp3   =(tPAmp3   -_pAmp3_q8)   /SampFrameLen;
    int32_t dPAmp4   =(tPAmp4   -_pAmp4_q8)   /SampFrameLen;
    int32_t dPAmp5   =(tPAmp5   -_pAmp5_q8)   /SampFrameLen;
    int32_t dPAmp6   =(tPAmp6   -_pAmp6_q8)   /SampFrameLen;

    float df1r =(f1T_r -_f1r) /SampFrameLen, df1cw=(f1T_cw-_f1cosw)/SampFrameLen;
    float df2r =(f2T_r -_f2r) /SampFrameLen, df2cw=(f2T_cw-_f2cosw)/SampFrameLen;
    float df3r =(f3T_r -_f3r) /SampFrameLen, df3cw=(f3T_cw-_f3cosw)/SampFrameLen;
    float df1b0=(f1T_b0-_f1b0)/SampFrameLen, df1b1=(f1T_b1-_f1b1)/SampFrameLen;
    float df2b0=(f2T_b0-_f2b0)/SampFrameLen, df2b1=(f2T_b1-_f2b1)/SampFrameLen;
    float df3b0=(f3T_b0-_f3b0)/SampFrameLen, df3b1=(f3T_b1-_f3b1)/SampFrameLen;
    int32_t dNzA=(nzTA_q-_nzA)/SampFrameLen, dNzB=(nzTB_q-_nzB)/SampFrameLen, dNzC=(nzTC_q-_nzC)/SampFrameLen;
    int32_t dNasalNorm=(tNasalNorm_q15-_nasalNorm_q15)/SampFrameLen;

    // Modulation targets (Q8 for depth/rate; Q15 for tremDepth/asp/tilt).
    int32_t tVibDepth  = (int32_t)frame.VibDepth * 256;
    int32_t tVibRate   = (int32_t)(frame.VibRate  / 10.0f * 256.0f);
    int32_t tTremDepth = (int32_t)(frame.TremDepth / 100.0f * 32768.0f);
    int32_t tTremRate  = (int32_t)(frame.TremRate  / 10.0f * 256.0f);
    int32_t tAsp       = (int32_t)(frame.Aspiration / 100.0f * 32768.0f);
    int32_t tTilt      = (int32_t)(((frame.Tilt / 100.0f) * 1.9f - 0.95f) * 32768.0f);

    int32_t dVibDepth =(tVibDepth -_vibDepth_q8)  /SampFrameLen;
    int32_t dVibRate  =(tVibRate  -_vibRate_q8)   /SampFrameLen;
    int32_t dTremDepth=(tTremDepth-_tremDepth_q15)/SampFrameLen;
    int32_t dTremRate =(tTremRate -_tremRate_q8)  /SampFrameLen;
    int32_t dAsp      =(tAsp      -_asp_q15)      /SampFrameLen;
    int32_t dTilt     =(tTilt     -_tilt_q15)     /SampFrameLen;

    // OQ/tilt bias  computed once in float before the hot loop.
    float frameTiltBias = _voiceTiltBias;
    if (OQStressLink != 0 && frame.Effort > 0)
        frameTiltBias -= (frame.Effort/100.0f) * (OQStressLink/100.0f) * 0.3f;
    if (OQF0Link != 0 && frame.F0 > 0) {
        float f0Hz  = (float)PitchToHz(frame.F0) + PitchOffsetHz;
        float f0Ref = BasePitchHz > 0 ? BasePitchHz : 100.0f;
        float ratio = std::max(0.0f, std::min(2.0f, (f0Hz-f0Ref)/f0Ref)) * 0.5f;
        frameTiltBias += ratio * (OQF0Link/100.0f) * 0.3f;
    }
    frameTiltBias = std::max(-0.95f, std::min(0.95f, frameTiltBias));
    int32_t frameTiltBias_q15 = (int32_t)(frameTiltBias * 32768.0f);

    // F0-dependent voicing gain compensation: cascade output climbs with F0,
    // so attenuate voiced excitation above a reference pitch to avoid clipping.
    float voiceF0Comp = 1.0f;
    {
        float f0Hz = (float)PitchToHz(frame.F0) + PitchOffsetHz;
        const float kF0CompRef   = 220.0f;  // pitch at/below which gain is unchanged
        const float kF0CompExp   = 0.85f;   // attenuation slope in log-pitch
        const float kF0CompFloor = 0.40f;   // strongest attenuation allowed
        if (f0Hz > kF0CompRef) {
            voiceF0Comp = std::pow(kF0CompRef / f0Hz, kF0CompExp);
            if (voiceF0Comp < kF0CompFloor) voiceF0Comp = kF0CompFloor;
        }
    }

    float breathGainBase = _breathGain / 8192.0f;

    //  Per-sample loop 
    for (int32_t sampCtr = SampFrameLen - 1; sampCtr >= 0; --sampCtr) {

        // Step all interpolated state.
        _voiceAmp_q8+=dVoiceAmp; _fricAmp_q8+=dFricAmp; _abAmp_q8+=dAbAmp;
        _pAmp2_q8+=dPAmp2; _pAmp3_q8+=dPAmp3; _pAmp4_q8+=dPAmp4; _pAmp5_q8+=dPAmp5; _pAmp6_q8+=dPAmp6;
        // Step F1-F3 in (r, cos) space  poles are always stable by construction.
        _f1r += df1r; _f1cosw += df1cw;
        _f2r += df2r; _f2cosw += df2cw;
        _f3r += df3r; _f3cosw += df3cw;
        // Step matched numerators; b0>|b1| region is convex so interpolants stay minimum-phase.
        _f1b0 += df1b0; _f1b1 += df1b1;
        _f2b0 += df2b0; _f2b1 += df2b1;
        _f3b0 += df3b0; _f3b1 += df3b1;
        // Derive float B/C for cascade; derive Q15 for F2/F3 parallel bank.
        float f1Bf = 2.0f*_f1r*_f1cosw, f1Cf = -_f1r*_f1r;
        float f2Bf = 2.0f*_f2r*_f2cosw, f2Cf = -_f2r*_f2r;
        float f3Bf = 2.0f*_f3r*_f3cosw, f3Cf = -_f3r*_f3r;
        _f2B=(int32_t)(f2Bf*32768.0f); _f2C=(int32_t)(f2Cf*32768.0f); _f2A=32768-_f2B-_f2C;
        _f3B=(int32_t)(f3Bf*32768.0f); _f3C=(int32_t)(f3Cf*32768.0f); _f3A=32768-_f3B-_f3C;
        _nzA+=dNzA; _nzB+=dNzB; _nzC+=dNzC;
        _nasalNorm_q15+=dNasalNorm;
        _vibDepth_q8+=dVibDepth; _vibRate_q8+=dVibRate;
        _tremDepth_q15+=dTremDepth; _tremRate_q8+=dTremRate;
        _asp_q15+=dAsp; _tilt_q15+=dTilt;

        // Vibrato: accumulate phase; look up sin; compute integer effF0Hz.
        // Phase increment = vibRate_q8 * 512 / sampleRate
        //   (vibRate_q8 = Hz*256; 512 = (1<<17)/256; gives increment in [0, 1<<17))
        _vibratoPhase_fp += (uint32_t)(_vibRate_q8 * 512u / (uint32_t)_sampleRate);
        int32_t sinVib = fp_sin(_vibratoPhase_fp);  // Q15

        // vibratoHz = (vibDepth_q8/256) * (sinVib/32768) = vibDepth_q8*sinVib >> 23
        int32_t vibratoHz = (int32_t)((int64_t)_vibDepth_q8 * sinVib >> 23);

        int32_t effF0Hz = (int32_t)PitchToHz(frame.F0) + PitchOffsetHz + vibratoHz;
        if (effF0Hz < 20) effF0Hz = 20;
        // glotPhaseInc = effF0Hz * (1<<24) / sampleRate  effF0Hz * _phaseIncPerHz_q4 >> 4
        _glotPhaseInc = (int32_t)((int64_t)effF0Hz * _phaseIncPerHz_q4 >> 4);

        if (VoiceChorus != 0) {
            int32_t cF0Hz = (int32_t)PitchToHz((int16_t)(frame.F0+VoiceChorus)) + PitchOffsetHz + vibratoHz;
            if (cF0Hz < 20) cF0Hz = 20;
            _chorusPhaseInc = (int32_t)((int64_t)cF0Hz * _phaseIncPerHz_q4 >> 4);
        }

        // Tremolo: accumulate phase; compute tremMod Q15 = 1 - depth*(0.5+0.5*sin).
        _tremoloPhase_fp += (uint32_t)(_tremRate_q8 * 512u / (uint32_t)_sampleRate);
        int32_t sinTrem = fp_sin(_tremoloPhase_fp);  // Q15
        // 0.5 + 0.5*sin in Q15 = 16384 + sinTrem/2
        int32_t halfSin = 16384 + (sinTrem >> 1);
        int32_t tremMod_q15 = 32768 - (int32_t)(((int64_t)_tremDepth_q15 * halfSin) >> 15);

        // voiceAmpTrem Q8 = voiceAmp_q8 * tremMod_q15 >> 15
        int32_t voiceAmpTrem_q8 = (int32_t)(((int64_t)_voiceAmp_q8 * tremMod_q15) >> 15);

        float   cascadeInF=0.0f, cascadeOutF=0.0f;
        int32_t cascadeOut=0;
        int32_t sampAB=0, samp2=0, samp3=0, samp4=0, samp5=0, samp6=0;

        bool active = voiceAmpTrem_q8>0 || _fricAmp_q8>0 || _abAmp_q8>0
                   || _pAmp2_q8>0 || _pAmp3_q8>0 || _pAmp4_q8>0
                   || _pAmp5_q8>0 || _pAmp6_q8>0;
                   // _asp_q15 excluded: aspGain is already gated by voiceAmpTrem_q8

        if (active) {
            if (voiceAmpTrem_q8 > 0) {
                // glotSample stays float: truncating it to int here quantized the
                // glottal pulse to ~voiceGain levels, so low VoicingGain produced a
                // coarse staircase = audible formant-colored noise. Matches float variant.
                float glotSample;
                float glotFlow = 0.0f;   // LF flow (breathiness gate)

                if (_fryStallSamples > 0) _fryStallSamples--;
                int32_t prevPhase = _glotPhase;
                _glotPhase = (_fryStallSamples>0 ? 0 : (_glotPhaseInc+_glotPhase)) & 0xFFFFFF;

                if (_glotPhase < prevPhase) {  // glottal cycle boundary
                    if (Shimmer > 0) {
                        float sd = Shimmer * 0.002f;
                        _shimmerScale = 1.0f + ((NextNoise()-128)/128.0f) * sd;
                    }
                    if (Jitter > 0) {
                        int32_t jr = (int32_t)(Jitter * 0.0005f * (1<<24));
                        _glotPhase = (_glotPhase + ((NextNoise()-128)*jr>>7)) & 0xFFFFFF;
                    }
                    if (FryAmount>0 && (NextNoise()&0xFF)<FryAmount) {
                        int32_t period = _glotPhaseInc>0
                            ? std::min((1<<24)/_glotPhaseInc, (int32_t)1500) : (int32_t)200;
                        _fryStallSamples = (NextNoise()*period)>>8;
                        if (_fryStallSamples>0) _glotPhase=0;
                    }
                }

                float fryFactor = (FryAmount > 0) ? std::max(0.05f, 1.0f - FryAmount * 0.003f) : 1.0f;
                int32_t effNe = (int32_t)(_Ne_fp * fryFactor);

#ifdef SHARPVOX_SAMPLED_GLOT
                if (_useSampledGlot && _sgBufSize > 0) {
                    _sgPhase += SgPitchShift ? (float)effF0Hz / _sgNatPitchHz : 1.0f;
                    if (_sgPhase >= (float)_sgBufSize) _sgPhase -= (float)_sgBufSize;
                    int32_t idx  = (int32_t)_sgPhase;
                    float   frac = _sgPhase - (float)idx;
                    int32_t idx1 = idx + 1 < _sgBufSize ? idx + 1 : 0;
                    float s = _sgBuf[idx] + frac * (_sgBuf[idx1] - _sgBuf[idx]);
                    glotSample = s * _voiceGain_f;
                } else
#endif
                {
                    int32_t phi = (int32_t)_glotPhase;
                    glotSample = LfPulse(phi, effNe, BreathAmt > 0 ? &glotFlow : nullptr)
                                 * _voiceGain_f * _lfGain;
                }
#ifdef SHARPVOX_SAMPLED_GLOT
                if (VoiceChorus != 0 && !_useSampledGlot) {
#else
                if (VoiceChorus != 0) {
#endif
                    _chorusPhase = (_chorusPhaseInc+_chorusPhase) & 0xFFFFFF;
                    int32_t phi2 = (int32_t)_chorusPhase;
                    float cFlow = 0.0f;
                    float chorus = LfPulse(phi2, _chorusNe_fp, BreathAmt > 0 ? &cFlow : nullptr)
                                   * _voiceGain_f * _lfGain;
                    glotSample = (glotSample + chorus) * 0.5f;
                    if (BreathAmt > 0) glotFlow = (glotFlow + cFlow) * 0.5f;
                }

#ifdef SHARPVOX_SAMPLED_GLOT
                if (Diplophonia > 0 && !_useSampledGlot) {
#else
                if (Diplophonia > 0) {
#endif
                    _diploPhase = (_diploPhase + (_glotPhaseInc >> 1)) & 0xFFFFFF;
                    int32_t phiD = _diploPhase;
                    float dFlow = 0.0f;
                    glotSample += LfPulse(phiD, effNe, BreathAmt > 0 ? &dFlow : nullptr)
                                  * _voiceGain_f * (Diplophonia * 0.007f) * _lfGain;
                    if (BreathAmt > 0) glotFlow += dFlow;
                }

                // 1-pole lowpass with corner held at the 22050 anchor
                // (d = 1 - (1-dBase)*22050/fs), so timbre is rate-invariant.
                int32_t eTilt = _tilt_q15 + frameTiltBias_q15;
                if (eTilt >  31130) eTilt =  31130;
                if (eTilt <      0) eTilt =      0;
                float dBase = (float)eTilt * (1.0f / 32768.0f);
                float d = 1.0f - (1.0f - dBase) * ((float)KDefaultSampleRate / (float)_sampleRate);
                if (d < 0.0f)   d = 0.0f;
                if (d > 0.995f) d = 0.995f;
                float tiltedSample = (1.0f - d) * glotSample + d * _tiltPrev;
                _tiltPrev = tiltedSample;

                cascadeInF = tiltedSample * (float)voiceAmpTrem_q8
                             * (_shimmerScale / (256.0f * 8192.0f)) * voiceF0Comp;

                // Subglottal resonance (~350 Hz chest-cavity coupling).
                if (SubglottalAmt > 0) {
                    float sg = fp_iir_f(_sgA, _sgB, _sgC, cascadeInF, _sgD1, _sgD2);
                    cascadeInF += sg * (SubglottalAmt * 0.005f);
                }

                // Cycle-synchronous breathiness gated on the raw LF flow
                if (BreathAmt > 0) {
                    float openness = std::max(0.0f, glotFlow);
                    cascadeInF += (float)(NextNoise()-128) * openness
                                  * (voiceAmpTrem_q8/256.0f) * (BreathAmt * 0.00004f)
                                  * _noiseScale / 8192.0f;
                }
            } else {
                if (_breathGain > 0)
                    _glotPhase = (_glotPhaseInc+_glotPhase) & 0xFFFFFF;
                else { _glotPhase=0; _chorusPhase=0; }
                cascadeInF = 0.0f;
            }

            // Aspiration / breath gating.
            float breathGainNow = breathGainBase * (voiceAmpTrem_q8 / 256.0f);
            if (breathGainNow > 0.0f && (_glotPhase>>16) > _breathCycle)
                cascadeInF += (float)(NextNoise()-128) * breathGainNow * _noiseScale / 2048.0f;

            if (_asp_q15 > 0) {
                float aspGain = (_asp_q15/32768.0f) * (voiceAmpTrem_q8/256.0f) * 0.5f;
                cascadeInF += (float)(NextNoise()-128) * aspGain * _noiseScale / 8192.0f;
            }

            bool hasSound = voiceAmpTrem_q8>0 || _fricAmp_q8>0
                         || breathGainNow>0.0f || _asp_q15>0;
            if (hasSound) {
                // Frication noise into cascade.
                cascadeInF += (float)(NextNoise()-128)
                              * (_fricAmp_q8/256.0f) * _noiseScale / 8192.0f;

                // Nasal antiresonator (NZ): A=1 (same as float version; _nzA not applied here).
                cascadeOutF = cascadeInF
                            + (_nzB / 32768.0f) * _nzD1
                            + (_nzC / 32768.0f) * _nzD2;
                _nzD2=_nzD1; _nzD1=cascadeInF;
                cascadeOutF *= (_nasalNorm_q15 / 32768.0f);

                // Nasal resonator (NP).
                cascadeOutF = fp_iir_f(32768, _npB, _npC, cascadeOutF, _npD1, _npD2);

                // Cascade F1F5: matched one-zero resonators (Vicanek 2016), all-float.
                cascadeOutF = fp_iir_zff(_f1b0, _f1b1, f1Bf, f1Cf, cascadeOutF, _f1X1, _f1D1, _f1D2);
                cascadeOutF = fp_iir_zff(_f2b0, _f2b1, f2Bf, f2Cf, cascadeOutF, _f2X1, _f2D1, _f2D2);
                cascadeOutF = fp_iir_zff(_f3b0, _f3b1, f3Bf, f3Cf, cascadeOutF, _f3X1, _f3D1, _f3D2);
                cascadeOutF = fp_iir_zff(_f4b0, _f4b1, _f4B, _f4C, cascadeOutF, _f4X1, _f4D1, _f4D2);
                cascadeOutF = fp_iir_zff(_f5cb0, _f5cb1, _f5cB, _f5cC, cascadeOutF, _f5cX1, _f5cD1, _f5cD2);
                // Fixed cascade F6 higher-pole correction (Klatt 1980, 4900Hz/BW1000).
                cascadeOutF = fp_iir_zff(_f6cb0, _f6cb1, _f6cB, _f6cC, cascadeOutF, _f6cX1, _f6cD1, _f6cD2);
                // F7/F8 refill the >5kHz voiced envelope (Rabiner 1968 high poles).
                cascadeOutF = fp_iir_zff(_f7cb0, _f7cb1, _f7cB, _f7cC, cascadeOutF, _f7cX1, _f7cD1, _f7cD2);
                cascadeOutF = fp_iir_zff(_f8cb0, _f8cb1, _f8cB, _f8cC, cascadeOutF, _f8cX1, _f8cD1, _f8cD2);
            }
            // Q6 conversion: truncating to Q0 here left a ~36 LSB output step
            // (OutputGain*4) heard as broadband hiss behind voiced speech.
            cascadeOut = (int32_t)(cascadeOutF * 64.0f);

            // Parallel bank noise. pink_fp / pink_float  32768 = 2^15 (from Q15 filter states),
            // so >>15 yields Q0 (float-scale), matching cascadeOut above.
            int32_t pink = NextPinkNoise_q15();
            int32_t parallelNoise = (int32_t)(((int64_t)pink * _noiseGain_q0) >> 15);

            // Parallel bank amplitudes.
            // Target: A_float * pAmp_float * noise  (Q0 result)
            // A_q15 * pAmp_q8 * noise >> 23  =  (A_float*32768) * (pAmp_float*256) * noise / (32768*256)
            //                                 =  A_float * pAmp_float * noise  
            // Using >> 23 (not >>8 then /4096) preserves precision for small amplitudes.
            if (_abAmp_q8 > 0)
                sampAB = (int32_t)(((int64_t)parallelNoise * _abAmp_q8) >> 20);
                // _abAmp is frame.AB * speechVolume / 4096: >>20 = >>8(Q8) + >>12(4096)

            if (_pAmp2_q8 > 0) {
                int32_t in2 = (int32_t)(((int64_t)_f2A * _pAmp2_q8 * parallelNoise) >> 23);
                samp2 = fp_iir(32768, _f2B, _f2C, in2, _f2pD1, _f2pD2);
            }
            if (_pAmp3_q8 > 0) {
                int32_t in3 = (int32_t)(((int64_t)_f3A * _pAmp3_q8 * parallelNoise) >> 23);
                samp3 = fp_iir(32768, _f3B, _f3C, in3, _f3pD1, _f3pD2);
            }
            if (_pAmp4_q8 > 0) {
                int32_t in4 = (int32_t)(((int64_t)_f4pA * _pAmp4_q8 * parallelNoise) >> 23);
                samp4 = fp_iir(32768, _f4pB, _f4pC, in4, _f4pD1, _f4pD2);
            }
            if (_pAmp5_q8 > 0) {
                int32_t in5 = (int32_t)(((int64_t)_f5pA * _pAmp5_q8 * parallelNoise) >> 23);
                samp5 = fp_iir(32768, _f5pB, _f5pC, in5, _f5pD1, _f5pD2);
            }
            if (_pAmp6_q8 > 0) {
                int32_t in6 = (int32_t)(((int64_t)_f6pA * _pAmp6_q8 * parallelNoise) >> 23);
                samp6 = fp_iir(32768, _f6pB, _f6pC, in6, _f6pD1, _f6pD2);
            }

            int32_t sample = cascadeOut + ((sampAB - samp3 + samp4 - samp5 + samp6 - samp2) << 6);

            // Pre-emphasis: y = x - a*x[n-1], a in Q15 (rate-compensated, see ctor)
            if (_hfEmph) {
                int32_t pe = sample - (int32_t)(((int64_t)_preemphA_q15 * _preemphPrev) >> 15);
                _preemphPrev = sample;
                sample = (int32_t)(((int64_t)pe * _preemphScale_q12) >> 12);
            }

            // Output gain on Q6 sample: product is Q21, rounded >>19 lands at
            // the old x4 output scale but with 1 LSB steps instead of 4.
            int32_t out = (int32_t)(((int64_t)sample * _outputGain_q15 + (1 << 18)) >> 19);
            if (out >  INT16_MAX) out =  INT16_MAX;
            if (out < -INT16_MAX) out = -INT16_MAX;
            outputBuffer[offset++] = (int16_t)out;

        } else {
            _glotPhase=0; _chorusPhase=0;
            outputBuffer[offset++] = 0;
        }
    }

    // Snap interpolated state to frame targets.
    // Integer delta truncation leaves small residuals (e.g. voiceAmp_q8=10 when
    // target=0), which keep 'active' true and generate faint noise during silence.
    _voiceAmp_q8   = tVoiceAmp;
    _fricAmp_q8    = tFricAmp;
    _abAmp_q8      = tAbAmp;
    _pAmp2_q8      = tPAmp2;
    _pAmp3_q8      = tPAmp3;
    _pAmp4_q8      = tPAmp4;
    _pAmp5_q8      = tPAmp5;
    _pAmp6_q8      = tPAmp6;
    _vibDepth_q8   = tVibDepth;
    _vibRate_q8    = tVibRate;
    _tremDepth_q15 = tTremDepth;
    _tremRate_q8   = tTremRate;
    _asp_q15       = tAsp;
    _tilt_q15      = tTilt;
    _f1r = f1T_r; _f1cosw = f1T_cw;
    _f2r = f2T_r; _f2cosw = f2T_cw;
    _f3r = f3T_r; _f3cosw = f3T_cw;
    _f1b0 = f1T_b0; _f1b1 = f1T_b1;
    _f2b0 = f2T_b0; _f2b1 = f2T_b1;
    _f3b0 = f3T_b0; _f3b1 = f3T_b1;
    // Keep F2/F3 Q15 in sync with snapped r/cos.
    _f2B=(int32_t)(2.0f*_f2r*_f2cosw*32768.0f); _f2C=-(int32_t)(_f2r*_f2r*32768.0f); _f2A=32768-_f2B-_f2C;
    _f3B=(int32_t)(2.0f*_f3r*_f3cosw*32768.0f); _f3C=-(int32_t)(_f3r*_f3r*32768.0f); _f3A=32768-_f3B-_f3C;
}

} // namespace SharpVox
