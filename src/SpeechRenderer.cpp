#include "../include/SpeechRenderer.h"
#include "../include/AudioProcessor.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace { template<class T> T clamp11(T v, T lo, T hi) { return v < lo ? lo : v > hi ? hi : v; } }

namespace SharpVox {

// ============================================================
// SpeechRenderer.cs — constructor, RenderStreaming, Render,
//                     SetPhonContext, GP, PF, PC, OvX, LogToLin
// ============================================================

SpeechRenderer::SpeechRenderer(const VoiceData& voice)
    : _voice(voice)
{
    for (int32_t i = 0; i < kNumOfBlocks; i++) {
        _cb[i] = ControlBlock{};
        _cb[i].ptrToTargetList = -1;
    }
    std::memset(_controlData,  0, sizeof(_controlData));
    std::memset(_diphEntries,  0, sizeof(_diphEntries));
    std::memset(_curKlattsch,  0, sizeof(_curKlattsch));
    std::memset(_klattschStep, 0, sizeof(_klattschStep));

    _male = (voice.VoiceType == 0);
    _envelopeListTable        = _male ? Tables::MaleEnvelopeTable        : Tables::FemaleEnvelopeTable;
    _locusTable               = _male ? Tables::MaleLocusTable            : Tables::FemaleLocusTable;
    _voiceAmplitudeVoicingTable = _male ? Tables::MaleAmplitudeVoicingVolumeTable
                                        : Tables::FemaleAmplitudeVoicingVolumeTable;
    _voiceNoiseAmplitudeTable = _male ? Tables::MaleNoiseAmplitudeTable  : Tables::FemaleNoiseAmplitudeTable;
    _nasalTargFreq    = voice.NasalTarg;
    _nasalBaseFreq    = voice.NasalBase;
    _locusOffset      = voice.Locus;
    _voiceBWgain1     = (voice.BwGain1 << 16) / 100;
    _voiceBWgain2     = (voice.BwGain2 << 16) / 100;
    _voiceBWgain3     = (voice.BwGain3 << 16) / 100;
    _tractScale       = voice.TractScale > 0 ? voice.TractScale : 1.0f;
    _voiceTremDepth   = (uint8_t)clamp11<int32_t>(voice.TremoloDepth,  0, 100);
    _voiceTremRate    = (uint8_t)clamp11<int32_t>(voice.TremoloRate,   0, 200);
    _voiceOnsetHardness = clamp11<int32_t>(voice.OnsetHardness, 0, 100);
    // Seed tremolo state so it is active from the very first frame
    _curKlattsch[kTremDIdx] = _voiceTremDepth << 16;
    _curKlattsch[kTremRIdx] = _voiceTremRate  << 16;

    // Phoneme numbers aliased from AudioProcessor for unqualified access within this file
    _IY_ = AudioProcessor::_IY_; _ER_ = AudioProcessor::_ER_;
    _AY_ = AudioProcessor::_AY_; _OY_ = AudioProcessor::_OY_;
    _UW_ = AudioProcessor::_UW_; _YU_ = AudioProcessor::_YU_;
    _SIL_ = AudioProcessor::_SIL_; _LX_ = AudioProcessor::_LX_;
    _EL_ = AudioProcessor::_EL_; _EN_ = AudioProcessor::_EN_;
    _W_ = AudioProcessor::_W_;   _Y_ = AudioProcessor::_Y_;
    _R_ = AudioProcessor::_R_;   _L_ = AudioProcessor::_L_;
    _HH_ = AudioProcessor::_HH_; _M_ = AudioProcessor::_M_;
    _N_ = AudioProcessor::_N_;   _NG_ = AudioProcessor::_NG_;
    _F_ = AudioProcessor::_F_;   _V_ = AudioProcessor::_V_;
    _TH_ = AudioProcessor::_TH_; _DH_ = AudioProcessor::_DH_;
    _S_ = AudioProcessor::_S_;   _Z_ = AudioProcessor::_Z_;
    _SH_ = AudioProcessor::_SH_; _ZH_ = AudioProcessor::_ZH_;
    _P_ = AudioProcessor::_P_;   _B_ = AudioProcessor::_B_;
    _T_ = AudioProcessor::_T_;   _D_ = AudioProcessor::_D_;
    _K_ = AudioProcessor::_K_;   _G_ = AudioProcessor::_G_;
    _CH_ = AudioProcessor::_CH_; _JH_ = AudioProcessor::_JH_;
    _TX_ = AudioProcessor::_TX_; _DX_ = AudioProcessor::_DX_;
    _QX_ = AudioProcessor::_QX_; _DD_ = AudioProcessor::_DD_;
}

void SpeechRenderer::RenderStreaming(const SynthInputDump& dump, std::function<void(const Frame&)> callback) {
    _dump = &dump;
    _curPhonBufIndex = 0;
    _durDoneInPhon   = 0;
    _startingNewPhon = true;

    // Seed curP_END_Targ from the first phoneme so envelope ramps have a prior target
    if (_bigBang) {
        _bigBang = false;
        SetPhonContext(0);
        for (_curBlockIndex = 0; _curBlockIndex < kNumOfBlocks; _curBlockIndex++) {
            _cb[_curBlockIndex].curP_END_Targ = (int16_t)GetFirstTarget(0);
        }
    }

    PitchInterpolator pitchInterp(dump);
    int32_t totalFrames = 0;
    for (int32_t i = 0; i < dump.PhonBuf2InIndex; i++) {
        totalFrames += dump.DurBuf[i];
    }

    for (int32_t i = 0; i < totalFrames; i++) {
        if (_durDoneInPhon >= _dump->DurBuf[_curPhonBufIndex]) {
            _curPhonBufIndex++;
            _durDoneInPhon   = 0;
            _startingNewPhon = true;
        }
        if (_startingNewPhon) {
            InitCtrlsForNewPhon();
            pitchInterp.DoNote(_curPhonBufIndex);
            _startingNewPhon = false;
        }

        int16_t f0 = pitchInterp.Step();
        InterpolateControls();
        callback(SaveFrame(f0, (uint8_t)_dump->PhonCtrlBuf2[_curPhonBufIndex]));

        _durDoneInPhon++;
    }
}

std::vector<Frame> SpeechRenderer::Render(const SynthInputDump& dump) {
    std::vector<Frame> frames;
    RenderStreaming(dump, [&](const Frame& f) {
        frames.push_back(f);
    });
    return frames;
}

void SpeechRenderer::SetPhonContext(int32_t index) {
    _curPhon  = GP(index);     _curPhonFlags  = PF(_curPhon);  _curPhonCtrl  = PC(index);
    _nextPhon = GP(index + 1); _nextPhonFlags = PF(_nextPhon); _nextPhonCtrl = PC(index + 1);
    _prevPhon = GP(index - 1); _prevPhonFlags = PF(_prevPhon); _prevPhonCtrl = PC(index - 1);
    _prev2Phon = GP(index - 2); _prev2PhonFlags = PF(_prev2Phon); _prev2PhonCtrl = PC(index - 2);
    _curPhonDur = (index >= 0 && index < (int32_t)_dump->DurBuf.size()) ? _dump->DurBuf[index] : 0;
}

// Safe phoneme buffer access; returns SIL for out-of-range indices
int32_t SpeechRenderer::GP(int32_t i) const {
    if (i >= 0 && i < (int32_t)_dump->PhonBuf2.size()) {
        return _dump->PhonBuf2[i];
    }
    return _SIL_;
}

// Phoneme feature flags; returns 0 for out-of-range phoneme indices
uint32_t SpeechRenderer::PF(int32_t p) const {
    if (p >= 0 && p < Tables::PhonemeFeatureFlagsLength) {
        return Tables::GetFeatureFlags(p);
    }
    return 0;
}

// Phoneme control field; returns 0 for out-of-range indices
int32_t SpeechRenderer::PC(int32_t i) const {
    if (i >= 0 && i < (int32_t)_dump->PhonCtrlBuf2.size()) {
        return (int32_t)_dump->PhonCtrlBuf2[i];
    }
    return 0;
}

// One-over-x from reciprocal table for small x, or computed directly for large x
int32_t SpeechRenderer::OvX(int32_t x) const {
    if (x <= 0) {
        return 0;
    }
    if (x < ReciprocalTableSize) {
        return (int32_t)Tables::ReciprocalTable[x];
    }
    return (int32_t)(65536L / x);
}

// Converts 6-bit log-domain amplitude (Klatt 1980) to a linear multiplier via lookup.
/*static*/ int16_t SpeechRenderer::LogToLin(int16_t v) {
    if (v > 63) {
        v = 63;
    }
    if (v < 0) {
        return 0;
    }
    return Tables::LogarithmicToLinearTable[v >> 1];
}

// ============================================================
// SpeechRenderer.Frame.cs — InsertBurst, InterpolateControls,
//                            SaveFrame
// ============================================================

// Inserts burst, aspiration, and voicing-murmur events at plosive boundaries.
//
// During a plosive closure, amplitude control blocks (Ap2 through AB) are held at
// zero until the burst onset, modeling the silent closure interval visible in
// spectrograms of stop consonants. Liberman, Delattre, Cooper & Gerstman (1954)
// showed that the burst of noise following the closure interval is an acoustic cue
// for stop place (its frequency position separating bilabial, alveolar, and velar
// stops), and that the silent interval before the vowel onset is essential to the
// stop percept.
//
// After a voiceless stop release into a sonorant, aspiration noise (AF) and an AV
// delay model the aspirated release. Lisker & Abramson (1964) established voice-
// onset time (VOT) as the interval between stop release and the onset of periodic
// voicing; this is the primary acoustic dimension separating voiced from voiceless
// stops in English and cross-linguistically. English prevocalic voiceless stops
// have VOT values of approximately 40-80 ms, with the exact value depending on
// place of articulation and the following vowel. Klatt (1975) measured VOT in
// word-initial consonant clusters and found that:
//   (1) VOT is longer before sonorant consonants and high vowels than before mid
//       and low vowels.
//   (2) Aspiration is strongly reduced in /s/-stop clusters: the VOT for /sp-/,
//       /st-/, and /sk-/ is near zero because the /s/ provides the frication burst
//       and the stop is released into the sonorant without a separate aspiration
//       interval.
//   (3) The remaining aspiration duration after the frication burst is
//       approximately constant across place for comparable environments.
// These findings motivate the cluster-context check (_prev2Phon == _S_) that
// sharply reduces VOT when a voiceless stop follows /s/, and the front-vowel
// AF amplitude difference (front vowels receive a 6 dB lower aspirate level
// because the high F2 requires less noise excitation to be perceptually salient).
//
// Bandwidth widening of BW1 and BW2 during the aspirated interval reflects the
// spread-glottis configuration: with the arytenoids abducted, glottal damping
// increases and formant bandwidths rise. Klatt & Klatt (1990) describe aspiration
// noise as generated at the glottis and replacing higher harmonic energy; wider
// bandwidths are the passive correlate of this source change.
//
// A voiced stop before a devoiced context receives pre-devoicing murmur: AV is
// reduced and all three bandwidths are raised to their maximum values, reflecting
// the spread-glottis preconfiguration that precedes the onset of closure voicing
// before a voiceless environment.
void SpeechRenderer::InsertBurst() {
    if ((_curPhonFlags & kPlosiveF) != 0) {
        int32_t burstDur = Tables::BurstDurationTable[_curPhon] / kFrameTime;
        if ((_curPhonFlags & kStopF) != 0 && (_curPhonFlags & kVoicedF) == 0) {
            if ((_nextPhonFlags & (kStopF | kNasalF)) != 0) {
                burstDur = (_nextPhonCtrl & kPrimOrEmphStress) != 0 ? 0 : burstDur >> 1;
            }
        }
        int32_t closureDur = _curPhonDur - burstDur;
        if ((_curPhonFlags & kAffricateF) != 0 && closureDur > 80 / kFrameTime) {
            closureDur = 80 / kFrameTime;
        }
        for (int32_t i = kAp2; i <= kAB; i++) {
            _cb[i].onset_END_TIME = closureDur;
            _cb[i].onset_VAL      = 0;
        }
    }

    if ((_prevPhonFlags & kStopF) != 0 && (_prevPhonFlags & kVoicedF) == 0 && (_curPhonFlags & kSonorant1F) != 0) {
        // Aspirated release from a preceding voiceless stop into this sonorant.
        // Lisker & Abramson (1964) Table 6: American English, 4-speaker averages:
        //   /p/ 58ms  /t/ 70ms  /k/ 80ms
        // Front vowels use a lower AF amplitude because the high-frequency spectral
        // tilt of the front vocal tract already emphasizes the aspiration region.
        int32_t rel;
        if ((_prevPhonFlags & kLabialF) != 0) {
            rel = 58 / kFrameTime;
        } else if ((_prevPhonFlags & kVelar) != 0) {
            rel = 80 / kFrameTime;
        } else {
            rel = 70 / kFrameTime;
        }
        _cb[kAV].onset_VAL = 0;
        _cb[kAF].onset_VAL = (int16_t)(Tables::GetForwardRank(_nextPhon) == kFrontR ? 48 : 54);
        if ((_curPhonCtrl & kVowelF) == 0) {
            // Non-vowel sonorant context: Lisker & Abramson measured VOT before vowels;
            // Klatt (1975) found longer VOT before sonorant consonants, but the absolute
            // values are not tabulated by place, so the window is reset and extended below.
            rel = 25 / kFrameTime;
            _cb[kAF].onset_VAL -= 3;
        }
        if ((_curPhonCtrl & kLiqGlideF) != 0 || _curPhon == _ER_) {
            _cb[kAF].onset_VAL += 3;
        }
        // /s/-stop cluster: Klatt (1975) Table 1 cluster VOT by stop place of articulation.
        // The /s/ frication serves as the burst, reducing the post-release aspiration window.
        // /sp/ 12ms  /st/ 23ms  /sk/ 30ms
        // Function-word /s/ (syllable type 0) receives the full reduction;
        // content-word /s/ retains some aspiration after the stop.
        if (_prev2Phon == _S_) {
            if ((_prev2PhonCtrl & kSyllableTypeField) == 0) {
                if ((_prevPhonFlags & kLabialF) != 0) {
                    rel = 12 / kFrameTime;
                } else if ((_prevPhonFlags & kVelar) != 0) {
                    rel = 30 / kFrameTime;
                } else {
                    rel = 23 / kFrameTime;
                }
            }
        } else if ((_curPhonCtrl & kVowelF) == 0) {
            rel += 20 / kFrameTime;
        }
        if (rel >= _curPhonDur) {
            rel = _curPhonDur - 1;
        }
        if (rel > (_curPhonDur >> 1) && (_curPhonFlags & kVowelF) != 0 && (_curPhonCtrl & kPrimOrEmphStress) != 0) {
            rel = _curPhonDur >> 1;
        }
        if ((_curPhonCtrl & kPlosive_Release) != 0) {
            rel = _curPhonDur;
            _cb[kAF].onset_VAL = 0;
        }
        _cb[kAV].onset_END_TIME = _cb[kAF].onset_END_TIME = _cb[kBW1].onset_END_TIME = _cb[kBW2].onset_END_TIME = rel;
        // Bandwidth widening during aspiration: spread-glottis coupling raises
        // formant damping (Klatt & Klatt 1990). BW1 receives the larger increase
        // because F1 is most sensitive to subglottal coupling changes.
        _cb[kBW1].onset_VAL = (int16_t)(_cb[kBW1].curP_START_Targ + 250);
        _cb[kBW2].onset_VAL = (int16_t)(_cb[kBW2].curP_START_Targ + 70);
    }

    if ((_curPhonFlags & kStopF) != 0 && (_curPhonFlags & kVoicedF) != 0 &&
        (_prevPhonFlags & kVoicedF) != 0 && (_nextPhonFlags & kVoicedF) == 0 && _curPhon != _TX_) {
        // Voiced stop before a devoiced context: pre-devoicing murmur.
        // The arytenoids begin abducting before the stop closure completes, producing
        // weak breathy voicing with raised bandwidths throughout the closure.
        _cb[kAV].onset_END_TIME  = _curPhonDur - (10 / kFrameTime);
        _cb[kBW1].onset_END_TIME = _cb[kBW2].onset_END_TIME = _cb[kBW3].onset_END_TIME = _curPhonDur;
        _cb[kAV].onset_VAL  = 56;
        _cb[kBW1].onset_VAL = 1000; _cb[kBW2].onset_VAL = 1000; _cb[kBW3].onset_VAL = 1200;
    }
}

// Steps the 15 control blocks forward by one frame, applying HEAD and TAIL ramps
// and diphthong trajectory stepping.
//
// Frequency blocks (F1-FNZ) sum the diphthong step, HEAD ramp, and TAIL ramp
// offsets in the accumulator before a single final shift, preserving fixed-point
// accuracy. Amplitude blocks (AV-AB) apply HEAD and TAIL independently, then
// overlay the onset_VAL burst or aspiration window if still active.
void SpeechRenderer::InterpolateControls() {
    // Advance Klattsch parameters by one linear step
    for (int32_t i = 0; i < 7; i++) {
        _curKlattsch[i] += _klattschStep[i];
    }

    // Frequency and FNZ blocks: combined offset accumulator, shifted at end
    for (int32_t i = kF1; i <= kFNZ; i++) {
        ControlBlock& cb = _cb[i];
        if (cb.ptrToTargetList >= 0 && _durDoneInPhon > cb.curTarget_TIME) {
            int32_t p = cb.ptrToTargetList;
            cb.curTarget_TIME  = _diphEntries[p++];
            cb.curTarget_STEP  = _diphEntries[p++];
            cb.ptrToTargetList = p;
            cb.curP_START_Targ += (int16_t)(cb.curTarget_OFFS >> kStepSizeRes);
            cb.curTarget_OFFS   = 0;
        }
        cb.curTarget_OFFS += cb.curTarget_STEP;

        int32_t offset = cb.curTarget_OFFS + cb.HEAD_offs;
        if (cb.HEAD_offs != 0) {
            cb.HEAD_offs -= cb.HEAD_step;
        }
        if (_durDoneInPhon >= cb.TAIL_START_time) {
            offset += cb.TAIL_offs;
            cb.TAIL_offs += cb.TAIL_step;
        }

        _controlData[i] = (int16_t)(cb.curP_START_Targ + (offset >> kStepSizeRes));
    }

    // Amplitude blocks: HEAD and TAIL applied independently
    for (int32_t i = kAV; i <= kAB; i++) {
        ControlBlock& cb = _cb[i];
        int32_t val = cb.curP_START_Targ + (cb.HEAD_offs >> kStepSizeRes);
        if (cb.HEAD_offs != 0) {
            cb.HEAD_offs -= cb.HEAD_step;
        }
        if (_durDoneInPhon >= cb.TAIL_START_time) {
            val += cb.TAIL_offs >> kStepSizeRes;
            cb.TAIL_offs += cb.TAIL_step;
        }
        _controlData[i] = (int16_t)val;

        // Override with burst/aspiration value during the onset window
        if (cb.onset_END_TIME > 0) {
            if (_durDoneInPhon < cb.onset_END_TIME) {
                _controlData[i] = cb.onset_VAL;
            } else if (i >= kAp2 && _durDoneInPhon == cb.onset_END_TIME + 1 && _controlData[i] > 10) {
                _controlData[i] -= 10;
            }
        }
    }
}

// Assembles one output Frame from the current control block values and Klattsch state.
//
// Formant frequencies are scaled by TractScale to shift the resonator pattern for
// different vocal tract lengths. Klatt (1980) describes the cascade/parallel
// synthesizer architecture in which formant frequencies are independent control
// parameters; scaling all three formants by a constant factor approximates the
// effect of a shorter or longer vocal tract while preserving inter-formant
// relationships. Minimum inter-formant separations (F2-F1 >= 200 Hz,
// F3-F2 >= 600 Hz) prevent resonator crossing and the spectral artifacts that
// result from pole-zero near-cancellations.
//
// Bandwidths are gain-scaled per voice. Klatt & Klatt (1990) show that formant
// bandwidths vary with voice type: breathier voices have wider bandwidths due to
// increased subglottal coupling and open-quotient source characteristics.
//
// Amplitude parameters are converted from log to linear scale (LogToLin), matching
// the Klatt (1980) convention where amplitude parameters are specified in a
// compressed decibel-like range and the synthesizer expects linear multipliers.
Frame SpeechRenderer::SaveFrame(int16_t f0, uint8_t phonCtrl) {
    Frame f;
    f.F0 = f0;

    int16_t curF1 = (int16_t)(_controlData[kF1] * _tractScale);
    int16_t curF2 = (int16_t)(_controlData[kF2] * _tractScale);
    int16_t curF3 = (int16_t)(_controlData[kF3] * _tractScale);

    while (curF2 - curF1 < 200) {
        curF1 -= 10;
    }
    while (curF3 - curF2 < 600) {
        curF3 += 10;
    }

    f.F1  = KlattSynthesizer::HzToPitch(curF1);
    f.F2  = KlattSynthesizer::HzToPitch(curF2);
    f.F3  = KlattSynthesizer::HzToPitch(curF3);
    f.Bw1 = (int16_t)((_controlData[kBW1] * _voiceBWgain1) >> 16);
    f.Bw2 = (int16_t)((_controlData[kBW2] * _voiceBWgain2) >> 16);
    f.Bw3 = (int16_t)((_controlData[kBW3] * _voiceBWgain3) >> 16);
    f.FNZ = KlattSynthesizer::HzToPitch((int16_t)(_controlData[kFNZ] * _tractScale));
    f.Av  = (int16_t)(LogToLin(_controlData[kAV]) * _tractScale);
    f.Af  = LogToLin(_controlData[kAF]);
    f.A2  = LogToLin(_controlData[kAp2]);
    f.A3  = LogToLin(_controlData[kAp3]);
    f.A4  = LogToLin(_controlData[kAp4]);
    f.A5  = LogToLin(_controlData[kAp5]);
    f.A6  = LogToLin(_controlData[kAp6]);
    f.AB  = LogToLin(_controlData[kAB]);
    f.PhonEdge = (int16_t)(_durDoneInPhon == 0 ? 1 : 0);

    f.Aspiration = (uint8_t)(_curKlattsch[kAspIdx]  >> 16);
    f.Tilt       = (uint8_t)(_curKlattsch[kTiltIdx]  >> 16);
    f.Effort     = (uint8_t)(_curKlattsch[kEffIdx]   >> 16);
    f.VibDepth   = (uint8_t)(_curKlattsch[kVibDIdx]  >> 16);
    f.VibRate    = (uint8_t)(_curKlattsch[kVibRIdx]  >> 16);
    f.TremDepth  = (uint8_t)(_curKlattsch[kTremDIdx] >> 16);
    f.TremRate   = (uint8_t)(_curKlattsch[kTremRIdx] >> 16);

    return f;
}

// ============================================================
// SpeechRenderer.Locus.cs — GetLocus
// ============================================================

// Computes the formant transition target (_transLevel) and duration (_transTime)
// for consonant-vowel or vowel-consonant boundaries using acoustic locus theory.
//
// Locus theory (Liberman, Delattre, Cooper & Gerstman 1954) proposes that each
// consonant has a characteristic frequency, the locus, toward which adjacent vowel
// formants appear to transition. The transition reflects the articulatory movement
// from the consonant's fixed place of production to the vowel position, and its
// direction and extent are the primary perceptual cues for stop and nasal place.
//
// Delattre, Liberman & Cooper (1955) determined specific second-formant loci from
// synthetic speech experiments. The best b/p/m was heard with F2 near 720 Hz,
// the best d/t/n near 1800 Hz, and the best g/k near 3000 Hz for front vowels
// (with an abrupt shift to lower values before back vowels). The F1 locus is near
// or below 240 Hz for all stop places; the first formant locus is the same for
// b, d, and g, suggesting it is tied to manner rather than place. Crucially,
// Delattre et al. (1955) showed that the transition cannot begin at the locus and
// travel all the way to the vowel steady state; the voiced portion begins partway
// through, so the transition "points to" the locus without originating there. The
// best results in synthesis require a silent interval of approximately half the
// total locus-to-nucleus span before the voiced transition begins.
//
// Locus equations (Sussman, McCaffrey & Matthews 1991) formalize this relationship
// as a linear regression of F2 transition onset against F2 vowel nucleus frequency.
// For each place of articulation the regression has a distinct slope and y-intercept
// that are invariant across vowel contexts, yielding near-perfect discrimination of
// stop place from the locus equation parameters alone. The percentage interpolation
// used here (lFreq + lPcnt/100 * (v1Targ - lFreq)) is a linearized approximation
// of the same relationship: lPcnt = 0 places the onset at the bare locus frequency,
// lPcnt = 100 places it at the vowel nucleus, and intermediate values correspond to
// points along the regression line between the two.
//
// Only applies to frequency blocks (F1, F2, F3). If the consonant or vowel rank
// lookup fails the transition values are left unchanged.
//
// iCons: phoneme buffer index of the consonant
// iVowel: phoneme buffer index of the adjacent vowel
// bType: C_V_type (consonant precedes vowel) or V_C_type (vowel precedes consonant)
void SpeechRenderer::GetLocus(int32_t iCons, int32_t iVowel, int32_t bType) {
    if (_curBlockIndex < kF1 || _curBlockIndex > kF3) {
        return;
    }

    int32_t cons = GP(iCons); int32_t vow = GP(iVowel);
    int32_t vowRank, consRank;

    if (bType == C_V_type) {
        vowRank  = Tables::GetForwardRank(vow);
        consRank = Tables::GetBackwardRank(cons);
    } else {
        vowRank  = Tables::GetBackwardRank(vow);
        consRank = Tables::GetForwardRank(cons);
    }

    // Only apply locus adjustment when the adjacent segment is a consonant at a
    // known place of articulation and the current segment is a vowel-like sound
    if (consRank != kConsonantR || vowRank == kConsonantR) {
        return;
    }

    uint32_t vf = PF(vow); uint32_t cf = PF(cons);
    bool f2y = (vf & kYGlideStartF) != 0;

    int32_t v1Targ = (bType == C_V_type) ? GetFirstTarget(iVowel) : GetLastTarget(iVowel);

    // Select locus table entry based on vowel height/backness category.
    // Delattre et al. (1955) found that the g/k locus is well-defined only for
    // front vowels; for back vowels the locus relationship breaks down, so separate
    // front, middle, and back entries capture this place-vowel interaction.
    int32_t lociIdx;
    if (vowRank == kFrontR) {
        lociIdx = Tables::FrontLocusTable[cons];
    } else if (vowRank == kMiddleR) {
        lociIdx = Tables::MiddleLocusTable[cons];
    } else {
        lociIdx = Tables::BackLocusTable[cons];
    }

    if (lociIdx == kNoValue) {
        return;
    }

    // Each entry occupies two consecutive slots per formant: [F1, F2, F3] x 3 fields
    lociIdx = (lociIdx >> 1) + (_curBlockIndex - kF1) * 3;
    int32_t lFreq  = _locusTable[lociIdx++] + _locusOffset;
    int32_t lPcnt  = _locusTable[lociIdx++];
    _transTime     = _locusTable[lociIdx] / kFrameTime;

    if (_curBlockIndex == kF2) {
        // Sussman, McCaffrey & Matthews (1991) Table I group means, N=20 speakers:
        //   F2_onset = slope * F2_nucleus + intercept  (all in Hz)
        //   Labial:   slope=0.89  intercept=99 Hz   (slopeQ15=29163)
        //   Alveolar: slope=0.42  intercept=1211 Hz (slopeQ15=13763)
        //   Velar:    slope=0.71  intercept=792 Hz  (slopeQ15=23266)
        int32_t slopeQ15, intercept;
        if ((cf & kLabialF) != 0) {
            slopeQ15  = 29163;
            intercept = 99 + _locusOffset;
            // Lehiste & Peterson (1961) Table III: labials ~5.1 cs
            _transTime = 55 / kFrameTime;
        } else if ((cf & kVelar) != 0) {
            slopeQ15  = 23266;
            intercept = 792 + _locusOffset;
            // Lehiste & Peterson (1961) Table III: velars ~7.8-8.8 cs
            _transTime = 80 / kFrameTime;
        } else {
            slopeQ15  = 13763;
            intercept = 1211 + _locusOffset;
            // Lehiste & Peterson (1961) Table III: alveolars ~6.8-7.9 cs
            _transTime = 70 / kFrameTime;
        }
        _transLevel = ((slopeQ15 * v1Targ) >> 15) + intercept;
        // Nasals: velum opening is gradual; Lehiste & Peterson (1961) Table III
        // shows 4-5 cs for nasals vs. 3-4 cs for labial stops at the same place.
        if ((cf & kNasalF) != 0) {
            _transTime += _transTime >> 2;
        }
        return;
    }

    // F1 and F3: table-based locus interpolation (Delattre, Liberman & Cooper 1955).
    // Sussman et al. (1991) found F3 locus equations unreliable across vowel contexts,
    // so the table approach is retained for F1 and F3.
    if ((cf & kNasalF) == 0 && !f2y) {
        // Oral stops have faster onsets than nasals (Lehiste & Peterson 1961 Table III)
        _transTime -= _transTime >> 2;
    }

    // Rounded vowels in dental/palatal context: rounding shifts the front cavity
    // resonances and partially decouples the F3 trajectory from the constriction locus.
    if (vowRank == kRoundR && _curBlockIndex != kF1 && (cf & (kDentalF | kPalatalF)) != 0) {
        lPcnt = (lPcnt >> 1) + 50;
    }

    _transLevel = lFreq + (lPcnt * (v1Targ - lFreq)) / 100;
}

// ============================================================
// SpeechRenderer.Targets.cs — FillPhonTargets, GetTargetRaw,
//   GetFirstTarget, GetLastTarget, GetVoiceFormantValue,
//   GetDiphthongs, ScalePrcnt, AdjustColored
// ============================================================

// Initializes per-phoneme duration ratios used to scale transition times.
//
// The ratio of actual duration to canonical maximum duration is a key factor in
// vowel reduction: shorter (unstressed) phonemes undershoot their formant targets
// because the articulators do not have time to fully reach them.
void SpeechRenderer::FillPhonTargets() {
    for (int32_t i = 0; i < kNumOfBlocks; i++) {
        _cb[i].onset_END_TIME = 0;
    }
    _nextDiphEntryIdx = 0;

    if ((_curPhonFlags & kPlosFricF) != 0 || _curPhon == _SIL_) {
        return;
    }

    int32_t maxDur = Tables::GetMaximumDuration(_curPhon) / kFrameTime;
    _curPhonMaxDur  = maxDur > 0 ? maxDur : 1;

    // 16.16 fixed-point ratio of actual duration to canonical max duration.
    // Used to scale head/tail ramp times so stressed vowels get longer transitions
    // and unstressed (short) ones get proportionally shorter ones.
    _curPhonPctOfMaxDur  = ((int64_t)_curPhonDur << 16) / _curPhonMaxDur;
    _curPhonPctOfMaxDur1 = (_curPhonPctOfMaxDur >> 1) + kOneHalf;
    _curPhonPctOfMaxDur2 = _curPhonPctOfMaxDur1 - (10L * k1pct);
}

// Returns the raw table value for the current block at the given phoneme index.
// Values >= 0 are direct Hz targets; kNoValue (-1) means no target for this phoneme;
// values < kNoValue are diphthong envelope indices encoded as negative offsets.
int16_t SpeechRenderer::GetTargetRaw(int32_t index) {
    int32_t bt   = Tables::ControlBlockTypeTable[_curBlockIndex];
    int32_t cur  = GP(index);      uint32_t cf = PF(cur); int32_t ctrl = PC(index);
    int32_t next = GP(index + 1);  uint32_t nf = PF(next);
    int32_t prev = GP(index - 1);  uint32_t pf = PF(prev);
    int16_t tv = -1;

    if (bt == kFreqType || bt == kBWType) {
        tv = GetVoiceFormantValue(_curBlockIndex, cur);

        // Negative below kNoValue means diphthong envelope; return raw index
        if (tv < kNoValue) {
            return tv;
        }

        // No target for this phoneme: borrow from neighbors in priority order
        if (tv == kNoValue) {
            tv = GetVoiceFormantValue(_curBlockIndex, next);
            if (tv == kNoValue) {
                tv = GetVoiceFormantValue(_curBlockIndex, GP(index + 2));
                if (tv == kNoValue) {
                    tv = GetVoiceFormantValue(_curBlockIndex, prev);
                    if (tv < 0 && tv != kNoValue) {
                        tv = _envelopeListTable[(tv & 0x7FFF) + 2];
                    }
                    if (tv == kNoValue) {
                        tv = Tables::DefaultTargetFrequenciesTable[_curBlockIndex];
                    }
                }
            }
            if (tv < kNoValue) {
                tv = _envelopeListTable[tv & 0x7FFF];
            }
            if (_curBlockIndex == kF1 && (cf & kPlosFricF) != 0 && (cf & kObstF) == 0 && (pf & kVowelF) != 0) {
                tv += 40;
            }
        }

        // N/EN before non-front vowels: BW2 is wider due to nasal coupling
        if ((cur == _N_ || cur == _EN_) && _curBlockIndex == kBW2 && Tables::GetForwardRank(next) != kFrontR) {
            tv += 60;
        }
        // Before/after a Y-glide, NG raises BW3 to full nasal bandwidth
        if ((cur == _N_ || cur == _EN_ || cur == _NG_) && _curBlockIndex == kBW3 &&
            ((nf & kYGlideStartF) != 0 || (pf & kYGlideEndF) != 0)) {
            tv = (int16_t)kMaxBandWidth;
        }
    } else if (bt == kFNZType) {
        tv = (int16_t)(((cf & kNasalF) != 0) ? _nasalTargFreq : _nasalBaseFreq);
    } else if (bt == kSourceAmpType) {
        if (_curBlockIndex == kAV) {
            tv = Tables::GetAmplitudeVoicing(_male, cur);
            if ((ctrl & kPlosive_Release) != 0) {
                tv -= (int16_t)(((pf & kNasalF) != 0) ? 6 : 20);
            }
            if ((cf & kStopF) != 0 && (pf & kVoicedF) == 0) {
                tv = 0;
            }
            if (cur == _HH_ && (pf & kVoicedF) != 0 && (ctrl & kPrimOrEmphStress) == 0) {
                tv = 54;
            }
        } else if (cur == _HH_) {
            tv = (int16_t)(Tables::GetForwardRank(next) == kFrontR ? 58 : 62);
            if ((ctrl & kStressField) == 0) {
                tv -= 1;
            }
        } else {
            tv = 0;
        }
    } else if (bt == kResonAmpType) {
        tv = Tables::GetNoiseIndex(cur);
        if (tv == kNoValue) {
            tv = 0;
        } else {
            int32_t rank = (next == _SIL_) ? Tables::GetBackwardRank(prev) : Tables::GetForwardRank(next);
            if (rank == kRoundR) {
                rank = kBackR;
            }
            int32_t idx2 = tv + (_curBlockIndex - kAp2) + rank * 6;
            tv = _voiceNoiseAmplitudeTable[idx2];
            if ((PC(index + 1) & kPlosive_Release) != 0 && tv >= 4) {
                tv -= 4;
            }
        }
    }
    return tv;
}

// Returns the onset (first) target Hz value for the given phoneme index
int32_t SpeechRenderer::GetFirstTarget(int32_t index) {
    int16_t t = GetTargetRaw(index);
    if (t < kNoValue) {
        int32_t i = t & 0x7FFF;
        t = _envelopeListTable[i];
        if (Tables::ControlBlockTypeTable[_curBlockIndex] == kFreqType) {
            t += (int16_t)AdjustColored(index, 0);
        }
    }
    return t;
}

// Returns the final target Hz value for the given phoneme index
int32_t SpeechRenderer::GetLastTarget(int32_t index) {
    int16_t t = GetTargetRaw(index);
    if (t < kNoValue) {
        int32_t i = (t & 0x7FFF) + 2;
        t = _envelopeListTable[i];
        if (Tables::ControlBlockTypeTable[_curBlockIndex] == kFreqType) {
            t += (int16_t)AdjustColored(index, 1);
        }
    }
    return t;
}

int16_t SpeechRenderer::GetVoiceFormantValue(int32_t bi, int32_t phonemeId) const {
    bool male = (_voice.VoiceType == 0);
    switch (bi) {
        case kF1:  return male ? Tables::GetMaleFormant1(phonemeId)   : Tables::GetFemaleFormant1(phonemeId);
        case kF2:  return male ? Tables::GetMaleFormant2(phonemeId)   : Tables::GetFemaleFormant2(phonemeId);
        case kF3:  return male ? Tables::GetMaleFormant3(phonemeId)   : Tables::GetFemaleFormant3(phonemeId);
        case kBW1: return male ? Tables::GetMaleBandwidth1(phonemeId) : Tables::GetFemaleBandwidth1(phonemeId);
        case kBW2: return male ? Tables::GetMaleBandwidth2(phonemeId) : Tables::GetFemaleBandwidth2(phonemeId);
        case kBW3: return male ? Tables::GetMaleBandwidth3(phonemeId) : Tables::GetFemaleBandwidth3(phonemeId);
        default:   throw std::invalid_argument("GetVoiceFormantValue: invalid block index");
    }
}

// Sets up a 4-point interpolated formant trajectory for diphthong and glide phonemes.
//
// Diphthongs are characterized by continuous formant movement between two targets;
// the trajectory is stored as a pair (p1,t1) -> (p2,t2) in the _diphEntries buffer.
// [Lehiste & Peterson 1961]
//
// Coarticulatory adjustments are applied to both endpoints so that the glide into
// and out of adjacent phonemes is smooth.
void SpeechRenderer::GetDiphthongs(int32_t index) {
    ControlBlock& cb = _cb[_curBlockIndex];
    int32_t bt = Tables::ControlBlockTypeTable[_curBlockIndex];

    int16_t p1 = _envelopeListTable[index];
    int16_t t1 = _envelopeListTable[index + 1];
    int16_t p2 = _envelopeListTable[index + 2];
    int16_t t2 = _envelopeListTable[index + 3];

    t1 = (int16_t)ScalePrcnt(t1);
    t2 = (int16_t)ScalePrcnt(t2);

    if (bt == kFreqType) {
        int32_t artic = k1pct * 10;
        if (cb.prevP_END_Targ > 0) {
            p1 += (int16_t)(((cb.prevP_END_Targ - p1) * artic) >> 16);
        }
        p1 += (int16_t)AdjustColored(_curPhonBufIndex, 0);
        if (cb.nextP_START_Targ > 0) {
            p2 += (int16_t)(((cb.nextP_START_Targ - p2) * artic) >> 16);
        }
        p2 += (int16_t)AdjustColored(_curPhonBufIndex, 1);
    }

    int32_t rampTime = t2 - t1;
    int32_t diff     = (p2 - p1) << kStepSizeRes;
    int32_t step     = rampTime > 0
        ? (rampTime < ReciprocalTableSize
            ? (int32_t)(((int64_t)OvX(rampTime) * diff) >> 16)
            : diff / rampTime)
        : 0;

    cb.curP_START_Targ  = p1;
    cb.curTarget_TIME   = t1;
    cb.curTarget_STEP   = 0;
    cb.curP_END_Targ    = p2;

    cb.ptrToTargetList = _nextDiphEntryIdx;
    _diphEntries[_nextDiphEntryIdx++] = (int16_t)t2;
    _diphEntries[_nextDiphEntryIdx++] = (int16_t)step;
    _diphEntries[_nextDiphEntryIdx++] = (int16_t)_curPhonDur;
    _diphEntries[_nextDiphEntryIdx++] = 0;
}

// Scales a duration percentage by the ratio of actual-to-canonical phoneme duration.
// Produces a frame count proportional to how long this phoneme actually lasts.
int32_t SpeechRenderer::ScalePrcnt(int32_t pct) {
    int64_t t = (pct * _curPhonPctOfMaxDur) >> 8;
    t = (_curPhonMaxDur * t / 100) >> 8;
    return t <= 0 ? 1 : (int32_t)t;
}

// Applies vowel-context coloring adjustments to diphthong endpoint formants.
//
// F3 is lowered before or after a liquid consonant (/r/-colored context).
//
// F2 adjustments model several coarticulatory effects:
//   /l/ following a front vowel lowers F2 (dark-L effect). [Sproat & Fujimura 1993]
//   /uw/ after an alveolar raises F2 (fronting by place assimilation).
//   Stressed context halves the adjustment; unstressed context amplifies it.
//
// The entry parameter selects start (0) or end (1) of the trajectory.
int32_t SpeechRenderer::AdjustColored(int32_t index, int32_t entry) {
    int32_t cur  = GP(index);      int32_t next = GP(index + 1); int32_t prev = GP(index - 1);
    uint32_t cf  = PF(cur);        uint32_t nf  = PF(next);      uint32_t pf  = PF(prev);
    int32_t ctrl = PC(index);
    int32_t adj  = 0;

    if (_curBlockIndex == kF3) {
        if ((cf & kVowel1F) != 0 && cur != _ER_ && ((pf & kLiqGlide2F) != 0 || (nf & kLiqGlide2F) != 0)) {
            adj = -150;
        }
    } else if (_curBlockIndex == kF2) {
        if (next == _LX_) {
            if ((cf & kFrontF) != 0) {
                adj = -150;
            } else if ((cur == _AY_ || cur == _OY_) && entry > 0) {
                adj = -250;
            }
        }
        if ((prev == _LX_ || prev == _L_ || prev == _W_) && (cf & kFrontF) != 0) {
            adj = -150;
        }
        if (cur == _UW_ && (pf & kAlveolarF) != 0) {
            adj = 200;
        }
        if (entry > 0 && (cur == _UW_ || cur == _YU_) && (nf & kAlveolarF) != 0) {
            adj += 200;
        }
        if ((ctrl & kStressField) != 0) {
            adj >>= 1;
        } else {
            adj += adj >> 1;
            if (entry > 0 && cur == _YU_) {
                adj = 400;
            }
        }
        if (adj > 400) {
            adj = 400;
        } else if (adj < -400) {
            adj = -400;
        }
    }
    return adj;
}

// ============================================================
// SpeechRenderer.Transitions.cs — InitCtrlsForNewPhon,
//                                  HeadRules, TailRules
// ============================================================

// Sets up envelope ramps for all 15 control blocks at the start of a new phoneme.
//
// For each block:
//   1. Retrieve the phoneme's target (static Hz or diphthong trajectory).
//   2. Apply coarticulatory undershoot to static frequency targets.
//   3. Compute HEAD (onset) ramp parameters via HeadRules.
//   4. Compute TAIL (offset) ramp parameters via TailRules.
//   5. Apply onset-hardness scaling to the AV voicing block.
//
// Klattsch parameters (aspiration, tilt, effort, vibrato, tremolo) are linearly
// interpolated across the phoneme duration from their previous values.
void SpeechRenderer::InitCtrlsForNewPhon() {
    SetPhonContext(_curPhonBufIndex);
    FillPhonTargets();

    int32_t dur = _curPhonDur;
    if (dur < 1) {
        dur = 1;
    }

    auto SetKStep = [&](int32_t idx, uint8_t targetVal) {
        int32_t target = targetVal << 16;
        _klattschStep[idx] = (target - _curKlattsch[idx]) / dur;
    };

    if (_curPhonBufIndex < _dump->PhonBuf2InIndex) {
        SetKStep(kAspIdx,  _dump->AspirationBuf2[_curPhonBufIndex]);
        SetKStep(kTiltIdx, _dump->TiltBuf2[_curPhonBufIndex]);
        SetKStep(kEffIdx,  _dump->EffortBuf2[_curPhonBufIndex]);
        SetKStep(kVibDIdx, _dump->VibDepthBuf2[_curPhonBufIndex]);
        SetKStep(kVibRIdx, _dump->VibRateBuf2[_curPhonBufIndex]);
        // Tremolo falls back to voice-level baseline when no explicit value is set
        uint8_t tremD = _dump->TremDepthBuf2[_curPhonBufIndex] > 0 ? _dump->TremDepthBuf2[_curPhonBufIndex] : _voiceTremDepth;
        uint8_t tremR = (_voiceTremDepth > 0 && _dump->TremRateBuf2[_curPhonBufIndex] == 0) ? _voiceTremRate : _dump->TremRateBuf2[_curPhonBufIndex];
        SetKStep(kTremDIdx, tremD);
        SetKStep(kTremRIdx, tremR);
    } else {
        for (int32_t i = 0; i < 7; i++) {
            _klattschStep[i] = (0 - _curKlattsch[i]) / dur;
        }
    }

    for (_curBlockIndex = 0; _curBlockIndex < kNumOfBlocks; _curBlockIndex++) {
        ControlBlock& cb = _cb[_curBlockIndex];
        int32_t bt = Tables::ControlBlockTypeTable[_curBlockIndex];

        cb.prevP_END_Targ   = cb.curP_END_Targ;
        cb.nextP_START_Targ = (int16_t)GetFirstTarget(_curPhonBufIndex + 1);
        cb.curTarget_OFFS   = 0;
        cb.ptrToTargetList  = -1;

        int16_t rawTarg = GetTargetRaw(_curPhonBufIndex);
        if (rawTarg < kNoValue) {
            // Diphthong: store the multi-point trajectory rather than a scalar target
            GetDiphthongs(rawTarg & 0x7FFF);
        } else {
            cb.curP_START_Targ = rawTarg;
            cb.curTarget_STEP  = 0;
            cb.curTarget_TIME  = _curPhonDur;

            if (bt == kFreqType) {
                // Coarticulatory undershoot: blend the target toward the midpoint of
                // adjacent targets. Stressed phonemes undershoot less because they
                // receive more precise articulation.
                int32_t artic = k1pct * 10;
                if ((_curPhonCtrl & kStressField) != 0) {
                    artic = (_curBlockIndex == kF2) ? k1pct * 25 : k1pct * 15;
                }
                cb.curP_START_Targ += (int16_t)((((cb.prevP_END_Targ + cb.nextP_START_Targ) >> 1) - cb.curP_START_Targ) * artic >> 16);
            }
            cb.curP_END_Targ = cb.curP_START_Targ;
        }

        if (bt == kFreqType) {
            // Pull nextP_START_Targ 10% back toward the current end target so the
            // tail ramp meets the following phoneme at a blend rather than a hard step
            cb.nextP_START_Targ += (int16_t)((cb.curP_END_Targ - cb.nextP_START_Targ) * (k1pct * 10) >> 16);
        }

        // HEAD envelope: onset ramp from transLevel to curP_START_Targ over transTime
        _transLevel = (cb.prevP_END_Targ + cb.curP_START_Targ) >> 1;
        _transTime  = 32 / kFrameTime;
        HeadRules(cb, bt);

        // Onset hardness scales the AV ramp: 0 = soft (slow ramp from deep below),
        // 50 = neutral, 100 = hard (near-instant onset at target level)
        if (_curBlockIndex == kAV && _voiceOnsetHardness != 50) {
            float scale = (100 - _voiceOnsetHardness) / 50.0f;
            _transLevel = cb.curP_START_Targ + (int32_t)((_transLevel - cb.curP_START_Targ) * scale);
            _transTime  = (int32_t)(_transTime * scale);
            if (_transTime < 0) {
                _transTime = 0;
            }
            if (_transTime > _curPhonDur) {
                _transTime = _curPhonDur;
            }
        }

        cb.HEAD_offs = 0; cb.HEAD_step = 0;
        if (_transTime > 0) {
            cb.HEAD_offs = (_transLevel - cb.curP_START_Targ) << kStepSizeRes;
            if (cb.HEAD_offs != 0) {
                int32_t hs   = (int32_t)(((int64_t)OvX(_transTime) * cb.HEAD_offs) >> 16);
                cb.HEAD_step = hs;
                cb.HEAD_offs = hs * _transTime;
            }
        }

        // TAIL envelope: offset ramp from curP_END_Targ toward the following target
        _transLevel = (cb.curP_END_Targ + cb.nextP_START_Targ) >> 1;
        _transTime  = 25 / kFrameTime;
        TailRules(cb, bt);

        cb.TAIL_offs = 0; cb.TAIL_step = 0;
        if (_transTime > 0) {
            int32_t ts = (_transLevel - cb.curP_END_Targ) << kStepSizeRes;
            if (ts != 0) {
                cb.TAIL_step = (int32_t)(((int64_t)OvX(_transTime) * ts) >> 16);
            }
        }
    }

    InsertBurst();
}

// Computes the onset transition (_transLevel, _transTime) for the current block.
//
// HeadRules determines how quickly and from what level the formant or amplitude
// rises into the phoneme's steady-state target. Rules are organized by block type
// and adjacent phoneme features.
//
// Frequency blocks (kFreqType):
//   Sonorant onsets glide from the previous formant position. [Lehiste & Peterson 1961]
//   Silence holds the previous tract shape for the full duration.
//   Locus theory provides the C-V onset target. [Delattre et al. 1955]
//   Transition times are scaled by phoneme duration ratio (shorter = faster).
//
// FNZ block: nasal zero shifts when entering or leaving a nasal context.
//
// Bandwidth blocks (kBWType): widen at voicing onset and at pause boundaries.
//
// Amplitude blocks: ramp up from below target to model adduction time.
//   Plosive AV onset timing follows voice-onset-time data. [Lisker & Abramson 1964]
//   Frication AF onset after voiced segments is gradual. [Klatt 1975]
void SpeechRenderer::HeadRules(ControlBlock& cb, int32_t bt) {
    if (bt == kFreqType) {
        if ((_curPhonFlags & kSonorant1F) != 0) {
            if ((_curPhonFlags & kLiqGlideF) == 0) {
                // Lehiste & Peterson (1961) Table III: average initial transition durations
                // by consonant place of articulation (onset into vowel or nasal).
                // Labials ~5.1 cs, alveolars ~6.8-7.9 cs, velars ~7.8-8.8 cs.
                if ((_prevPhonFlags & kLabialF) != 0) {
                    _transTime = 55 / kFrameTime;
                } else if ((_prevPhonFlags & kVelar) != 0) {
                    _transTime = 80 / kFrameTime;
                } else if ((_prevPhonFlags & (kAlveolarF | kDentalF)) != 0) {
                    _transTime = 70 / kFrameTime;
                } else {
                    _transTime = 45 / kFrameTime;
                }
                if ((_prevPhonFlags & kLiqGlideF) != 0) {
                    // Liquid-to-sonorant: start halfway between liquid endpoint and midpoint
                    _transLevel = (cb.prevP_END_Targ + _transLevel) >> 1;
                    if (_prevPhon == _L_ && _curBlockIndex == kF1) {
                        // Dark-L raises F1 noticeably at the following vowel onset
                        _transLevel += 80;
                    } else if (_prevPhon == _R_ && _curBlockIndex != kF1) {
                        _transTime = 70 / kFrameTime;
                    }
                } else if (_curPhon == _HH_) {
                    // HH is voiceless and inherits the previous tract shape
                    _transLevel = (cb.prevP_END_Targ + _transLevel) >> 1;
                }
            } else {
                // Liquids and glides glide gently from the previous formant position
                _transLevel = (cb.prevP_END_Targ + _transLevel) >> 1;
                _transTime  = 32 / kFrameTime;
            }
        }

        if (_curPhon == _SIL_) {
            // Silence: no vocal tract so hold the previous formant for the full duration
            _transLevel = cb.prevP_END_Targ; _transTime = _curPhonDur;
        } else {
            // Apply C-V locus (consonant on left transitions into this phoneme)
            // and V-C locus (this phoneme as vowel transitioning into a consonant on left)
            GetLocus(_curPhonBufIndex - 1, _curPhonBufIndex, C_V_type);
            GetLocus(_curPhonBufIndex, _curPhonBufIndex - 1, V_C_type);
            if ((_prevPhonFlags & kStopF) != 0 && (_prevPhonFlags & kVoicedF) == 0 && _curBlockIndex == kF1) {
                // Aspirated release after a voiceless stop briefly raises F1
                // [Lisker & Abramson 1964]
                _transLevel += 100;
            }
            if ((_curPhonFlags & kPlosFricF) != 0) {
                _transTime = (_curBlockIndex == kF1) ? 20 / kFrameTime : 30 / kFrameTime;
                if ((_curPhonFlags & kStopF) != 0) {
                    // Stop closure holds the tract frozen for the full duration
                    _transTime = _curPhonDur;
                }
            }
            if ((_curPhonFlags & kNasalF) != 0) {
                // F1 couples to the nasal cavity instantly (velum opens abruptly);
                // F2/F3 require the full phoneme duration to reach nasal resonance position
                _transTime = (_curBlockIndex == kF1) ? 0 : _curPhonDur;
                if ((_curPhon == _N_ || _curPhon == _EN_) && Tables::GetBackwardRank(_prevPhon) == kFrontR) {
                    // Front-vowel context shifts N/EN onset F2 and F3 downward
                    if (_curBlockIndex == kF2) {
                        _transLevel -= (_prevPhonFlags & kYGlideEndF) != 0 ? 200 : 100;
                    } else if (_curBlockIndex == kF3) {
                        _transLevel -= 100;
                    }
                } else if (_curPhon == _M_ && _curBlockIndex == kF2 && (_prevPhonFlags & kYGlideEndF) != 0) {
                    _transLevel -= 150;
                }
            }
        }

        if ((_curPhonFlags & kPlosFricF) == 0 && Tables::GetBackwardRank(_prevPhon) != kConsonantR && _transTime > 0) {
            // Scale transition time by duration ratio: shorter vowels have faster
            // formant transitions because there is less time to reach the target.
            _transTime = 1 + (int32_t)((_curPhonPctOfMaxDur1 * _transTime) >> 16);
        }
    } else if (bt == kFNZType) {
        if ((_prevPhonFlags & kNasalF) != 0 && (_curPhonFlags & kNasalF) == 0) {
            // Coming from a nasal: FNZ starts halfway between nasal base and target
            _transLevel = _nasalBaseFreq + ((_nasalTargFreq - _nasalBaseFreq) >> 1);
            _transTime  = 80 / kFrameTime;
        }
        if ((_curPhonFlags & kNasalF) != 0) {
            _transLevel = _nasalTargFreq;
        }
    } else if (bt == kBWType) {
        // Bandwidths widen at voicing onset because the folds are not yet fully
        // adducted; BW1 widens more than BW2 since F1 is most sensitive to
        // glottal adduction state
        if ((_curPhonFlags & kVoicedF) != 0) {
            if ((_prevPhonFlags & kVoicedF) == 0 && _curBlockIndex == kBW1) {
                _transTime  = 50 / kFrameTime;
                _transLevel = (_cb[kF1].curP_START_Targ >> 3) + cb.curP_START_Targ;
            } else {
                _transTime = 40 / kFrameTime;
            }
        } else {
            _transTime = 20 / kFrameTime;
        }

        if (_prevPhon == _SIL_) {
            _transLevel = (kBW3 - bt) * 50 + cb.curP_START_Targ;
            _transTime  = 50 / kFrameTime;
        } else if (_curPhon == _SIL_) {
            _transLevel = (kBW3 - bt) * 50 + cb.prevP_END_Targ;
            if ((_prev2PhonFlags & kVoicedF) == 0 && (_prevPhonCtrl & kPlosive_Release) != 0 && _curBlockIndex == kBW1) {
                _transLevel = 250;
            }
            _transTime = 50 / kFrameTime;
        }
        if ((_prevPhonFlags & kNasalF) != 0) {
            // After a nasal, BW1 needs extra widening time because the velum is still
            // partially open at the start of the following oral phoneme
            _transLevel = cb.curP_START_Targ;
            if (_curBlockIndex == kBW2 && (_prevPhon == _N_ || _prevPhon == _EN_) && Tables::GetForwardRank(_curPhon) != kFrontR) {
                _transLevel += 60;
                _transTime   = 60 / kFrameTime;
            } else if (_curBlockIndex == kBW1) {
                _transLevel += 70;
                _transTime   = 100 / kFrameTime;
            }
        }
        if ((_curPhonFlags & kNasalF) != 0) {
            // Nasal bandwidth is set directly by the target; no onset ramp needed
            _transTime = 0;
        }
    } else {
        // Amplitude blocks: ramp from slightly below target to model finite adduction time
        int32_t ampT = cb.curP_START_Targ - 10;
        if (_transLevel < ampT || (_prevPhonFlags & kStopF) != 0 || _prevPhon == _JH_) {
            _transLevel = ampT;
            if ((_curPhonFlags & kPlosFricF) == 0) {
                _transTime = 20 / kFrameTime;
            }
            if (_curBlockIndex == kAV) {
                if (_prevPhon == _SIL_ && (_curPhonFlags & kVoicedF) != 0) {
                    // Voiced onset from silence: slow ramp models gradual fold adduction
                    _transLevel -= 8;
                    _transTime   = 45 / kFrameTime;
                }
                if ((_prevPhonFlags & kPlosFricF) != 0) {
                    _transLevel = ampT + 6;
                }
                if ((_prevPhonFlags & kStopF) != 0) {
                    _transLevel = cb.curP_START_Targ - 5;
                }
            }
        }
        if ((_curPhonFlags & kVoicedF) != 0 && (_prevPhonFlags & kNasalF) != 0) {
            // Vowel after a nasal: voicing was already running, no AV ramp needed
            _transTime = 0;
        }
        if ((_prevPhonFlags & kVoicedF) != 0 && (_curPhonFlags & kNasalF) != 0 && _curBlockIndex == kAV) {
            // Entering a nasal from a voiced segment: velum coupling is immediate
            _transTime = 0;
        }
        ampT = cb.prevP_END_Targ - 10;
        if (_transLevel < ampT) {
            _transLevel = ampT - 3;
            if (_curPhon == _SIL_) {
                _transTime = 70 / kFrameTime;
            }
        }
        if (_curBlockIndex == kAp3 && (_curPhonFlags & kAffricateF) != 0) {
            // Affricates hold Ap3 near zero during the stop closure, then ramp at release
            _transTime  = _curPhonDur - 2;
            _transLevel = cb.curP_START_Targ - 30;
        }
        if (_curBlockIndex == kAV && (_curPhonFlags & kPlosiveF) != 0) {
            // Plosive AV onset is brief to reach the closure voicing state quickly
            // [Lisker & Abramson 1964]
            _transTime = 10 / kFrameTime;
        }
        if (_curBlockIndex == kAF) {
            if (_curPhon == _SIL_ || _curPhon == _F_ || _curPhon == _TH_ || _curPhon == _S_ || _curPhon == _SH_) {
                if ((_prevPhonFlags & kVoicedF) != 0 && (_prevPhonFlags & kPlosFricF) == 0) {
                    // Frication onset after a voiced sonorant ramps gradually so that
                    // devoicing is not abrupt. [Klatt 1975]
                    if (_curPhon == _SIL_) {
                        _transTime  = 80 / kFrameTime;
                        _transLevel = 52;
                    } else {
                        _transTime  = 45 / kFrameTime;
                        _transLevel = 48;
                    }
                }
            }
        }
    }

    if (_transTime > _curPhonDur) {
        _transTime = _curPhonDur;
    }
    if (_transTime > 130 / kFrameTime) {
        _transTime = 130 / kFrameTime;
    }
    if (_transTime < 0) {
        _transTime = 0;
    }
}

// Computes the offset transition (_transLevel, _transTime) for the current block.
//
// TailRules mirrors HeadRules but looks ahead at _nextPhon rather than back at
// _prevPhon. The TAIL envelope governs how the formant or amplitude leaves its
// steady-state value toward the onset of the following phoneme.
//
// Frequency blocks: locus theory provides the V-C departure target.
//   [Delattre et al. 1955], [Liberman et al. 1954]
//   Transition times are scaled by a slightly tighter duration ratio so that
//   offset transitions are a little shorter than onset ones.
//
// Amplitude blocks: plosive and affricate AV tail timing follows VOT conventions.
//   [Lisker & Abramson 1964], [Klatt 1975]
void SpeechRenderer::TailRules(ControlBlock& cb, int32_t bt) {
    if (bt == kFreqType) {
        if ((_curPhonFlags & kSonorant1F) != 0) {
            // Lehiste & Peterson (1961) Table III: offset transition durations by
            // following consonant place of articulation.
            if ((_nextPhonFlags & kLabialF) != 0) {
                _transTime = 55 / kFrameTime;
            } else if ((_nextPhonFlags & kVelar) != 0) {
                _transTime = 80 / kFrameTime;
            } else if ((_nextPhonFlags & (kAlveolarF | kDentalF)) != 0) {
                _transTime = 70 / kFrameTime;
            } else {
                _transTime = 45 / kFrameTime;
            }
            if ((_curPhonFlags & kLiqGlideF) == 0) {
                if ((_nextPhonFlags & kLiqGlideF) != 0) {
                    if (_curBlockIndex == kF3) {
                        _transTime = 60 / kFrameTime;
                    }
                    if (_nextPhon == _L_ && _curBlockIndex == kF1) {
                        _transLevel += 80;
                    }
                } else if (_nextPhon == _HH_) {
                    _transLevel = (cb.curP_END_Targ + _transLevel) >> 1;
                }
            } else {
                if ((_nextPhonFlags & kLiqGlideF) == 0) {
                    _transLevel = (cb.curP_END_Targ + _transLevel) >> 1;
                    _transTime  = 20 / kFrameTime;
                } else {
                    _transLevel = (cb.curP_END_Targ + _transLevel) >> 1;
                    _transTime  = 40 / kFrameTime;
                }
            }
        }

        if (_nextPhon == _SIL_) {
            _transTime = 0;
        } else {
            // Apply V-C locus (next consonant) and C-V locus (this phoneme as consonant)
            GetLocus(_curPhonBufIndex + 1, _curPhonBufIndex, V_C_type);
            GetLocus(_curPhonBufIndex, _curPhonBufIndex + 1, C_V_type);
            if ((_curPhonFlags & kPlosFricF) != 0) {
                _transTime = (_curBlockIndex == kF1) ? 20 / kFrameTime : 30 / kFrameTime;
                if ((_curPhonFlags & kStopF) != 0) {
                    _transTime = _curPhonDur;
                    if ((_curPhonFlags & kVoicedF) == 0 && _curBlockIndex == kF1) {
                        _transLevel += 100;
                    }
                }
            }
            if ((_curPhonFlags & kNasalF) != 0) {
                _transTime = (_curBlockIndex == kF1) ? 0 : _curPhonDur;
                if ((_curPhon == _N_ || _curPhon == _EN_) && Tables::GetForwardRank(_nextPhon) == kFrontR) {
                    if (_curBlockIndex == kF2) {
                        _transLevel -= 100;
                        if ((_nextPhonFlags & kYGlideStartF) != 0) {
                            _transLevel -= 100;
                        }
                    } else if (_curBlockIndex == kF3) {
                        _transLevel -= 100;
                    }
                } else if (_curPhon == _M_ && _curBlockIndex == kF2 && (_nextPhonFlags & kYGlideStartF) != 0) {
                    _transLevel -= 150;
                }
            }
        }

        if ((_curPhonFlags & kPlosFricF) == 0 && Tables::GetForwardRank(_nextPhon) != kConsonantR && _transTime > 0) {
            _transTime = 1 + (int32_t)((_curPhonPctOfMaxDur2 * _transTime) >> 16);
        }
    } else if (bt == kFNZType) {
        if ((_nextPhonFlags & kNasalF) != 0 && (_curPhonFlags & kNasalF) == 0) {
            _transLevel = _nasalTargFreq;
            _transTime  = 80 / kFrameTime;
        }
    } else if (bt == kBWType) {
        if ((_curPhonFlags & kVoicedF) != 0) {
            _transTime = 40 / kFrameTime;
            if ((_nextPhonFlags & kVoicedF) == 0 && _curBlockIndex == kBW1) {
                _transTime  = 50 / kFrameTime;
                _transLevel = (_cb[kF1].curP_START_Targ >> 3) + cb.curP_END_Targ;
            }
        } else {
            _transTime = 20 / kFrameTime;
        }
        if (_nextPhon == _SIL_) {
            _transLevel = (kBW3 - bt) * 50 + cb.curP_END_Targ;
            _transTime  = 50 / kFrameTime;
        } else if (_curPhon == _SIL_) {
            _transLevel = (kBW3 - bt) * 50 + cb.nextP_START_Targ;
            _transTime  = 50 / kFrameTime;
        }
        if ((_nextPhonFlags & kNasalF) != 0) {
            _transLevel = cb.curP_END_Targ;
            if (_curBlockIndex == kBW2 && (_nextPhon == _N_ || _nextPhon == _EN_) && Tables::GetForwardRank(_curPhon) != kFrontR) {
                _transLevel += 60;
                _transTime   = 60 / kFrameTime;
            } else if (_curBlockIndex == kBW1) {
                _transLevel += 100;
                _transTime   = 100 / kFrameTime;
            }
        }
        if ((_curPhonFlags & kNasalF) != 0) {
            _transTime = 0;
        }
    } else {
        int32_t ampT = cb.nextP_START_Targ - 10;
        if (_transLevel < ampT) {
            _transLevel = ampT;
            if (_curPhon == _SIL_) {
                _transTime = 70 / kFrameTime;
            }
        }

        bool gotoEnd = false;
        if (_curBlockIndex == kAV && _transLevel < cb.nextP_START_Targ) {
            if (_curPhon != _V_ && _curPhon != _DH_ && _curPhon != _JH_ && _curPhon != _ZH_ && _curPhon != _Z_) {
                _transTime = 0;
                if ((_curPhonFlags & (kStopF | kAffricateF)) != 0) {
                    if ((_curPhonFlags & kVoicedF) != 0) {
                        _transLevel = cb.curP_END_Targ - 3;
                        _transTime  = 45 / kFrameTime;
                    } else {
                        _transTime = 0;
                    }
                    gotoEnd = true;
                }
            }
        }

        if (!gotoEnd) {
            if ((_curPhonFlags & kVoicedF) != 0 && (_nextPhonFlags & kNasalF) != 0) {
                _transTime = 0;
            }
            if ((_curPhonFlags & kNasalF) != 0) {
                bool nextVoicedNonStop = (_nextPhonFlags & kVoicedF) != 0
                    && (_curPhonFlags & kPlosFricF) == 0
                    && (_nextPhonCtrl & kPlosive_Release) == 0;
                _transTime = nextVoicedNonStop ? 0 : 40 / kFrameTime;
            }
            ampT = cb.curP_END_Targ - 10;
            if ((_curPhonFlags & kPlosiveF) != 0) {
                _transTime = 15 / kFrameTime;
                if ((_curPhonFlags & kStopF) != 0 || _curPhon == _DX_ || _curPhon == _QX_ || _curPhon == _DD_) {
                    ampT = cb.curP_END_Targ;
                }
            }
            if (_transLevel < ampT) {
                _transLevel = ampT - 3;
                _transTime  = 20 / kFrameTime;
            }
            if (_curBlockIndex == kAV) {
                if (_transLevel < ampT || (ampT > 0 && (_nextPhonCtrl & kPlosive_Release) != 0)) {
                    _transLevel = ampT + 3;
                    if (_nextPhon == _SIL_ || (_nextPhonCtrl & kPlosive_Release) != 0) {
                        _transTime = 75 / kFrameTime;
                    }
                }
            }
            if (_nextPhon >= _P_) {
                if ((_curPhonFlags & kNasalF) == 0 || _curBlockIndex != kAV) {
                    _transTime = 0;
                }
            }
            if (_curBlockIndex == kAF) {
                if (_curPhon == _F_ || _curPhon == _TH_ || _curPhon == _S_ || _curPhon == _SH_) {
                    if ((_nextPhonFlags & kVoicedF) != 0 && (_nextPhonFlags & kPlosFricF) == 0) {
                        // Frication offset into a voiced sonorant ramps down gradually
                        // [Klatt 1975]
                        _transTime  = 40 / kFrameTime;
                        _transLevel = 52;
                    }
                }
                if ((_curPhonFlags & kVowelF) != 0 && _nextPhon == _SIL_) {
                    _transTime  = 130 / kFrameTime;
                    _transLevel = 52;
                }
            }
        }
    }

    if (_transTime > _curPhonDur) {
        _transTime = _curPhonDur;
    }
    if (_transTime > 130 / kFrameTime) {
        _transTime = 130 / kFrameTime;
    }
    _cb[_curBlockIndex].TAIL_START_time = _curPhonDur - _transTime;
    if (_transTime < 0) {
        _transTime = 0;
    }
}

}  // namespace SharpVox
