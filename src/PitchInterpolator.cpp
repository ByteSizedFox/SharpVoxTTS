#include "../include/PitchInterpolator.h"
#include "../include/SynthData.h"
#include "../include/VoiceData.h"
#include "../include/Tables.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace SharpVox {

    PitchInterpolator::PitchInterpolator(const ClausePlan& plan)
        : _plan(plan)
    {
        const PitchState& s = plan.Pitch;

        _nextPitchBufTime = s.NextPitchBufTime;
        _pitchBufOutIndex = s.PitchBufOutIndex;
        _curPitchBufTime = s.CurPitchBufTime;

        _phonIndexTarg = s.PhonIndexTarg;
        _timeIntoPhonTarg = s.TimeIntoPhonTarg;
        _curPhonDurCc = s.CurPhonDurCc;
        _phonDurDelay = s.PhonDurDelay;

        _phonIndexCp = s.PhonIndexCp;
        _timeIntoPhonCp = s.TimeIntoPhonCp;
        _curPhonDurCp = s.CurPhonDurCp;

        _uvPhonPitchTarg = s.UvPhonPitchTarg;
        _phonPitchOffset1 = s.PhonPitchOffset1;

        _baselineStartOffset = s.BaselineStartOffset;
        _baselineEndOffset = s.BaselineEndOffset;
        _downRampOffset = s.DownRampOffset;
        _downRampStep = s.DownRampStep;
        _rampSteps = std::vector<int64_t>(s.RampSteps.begin(), s.RampSteps.end());
        _curRamp = s.CurRamp;

        _vpIntonation = s.VpIntonation;
        _vpPitchRange = s.VpPitchRange;
        _vpBaselinePitch = s.VpBaselinePitch;

        _vibratoDepth1 = s.VibratoDepth1;
        _vibratoDepth2 = s.VibratoDepth2;
        _vibratoFreq = s.VibratoFreq;
        _vibratoPhase1 = s.VibratoPhase1;

        _singing = s.Singing != 0;
        _hzGlide = s.HzGlide != 0;
        _musicalNoteActive = s.MusicalNoteActive != 0;
        _portamentoAccum = s.PortamentoAccum;
        _portamentoStep = s.PortamentoStep;
        _newPortaTarget = s.NewPortaTarget != 0;
        _newSentence = s.NewSentence != 0;
        _speechRate = s.SpeechRate;

        _pitchBoundry = s.PitchBoundry;
        _lowGainCp = s.LowGainCp != 0;

        _voiceNaturalPitch = s.VpBaselinePitch;
        _pbHold = kNeverHappens;
        _pbLowGain = false;

        // Fujisaki model knobs (Q16 neper amplitudes / percent scales)
        _vpFujiPhraseAmp             = s.FujisakiPhraseAmpQ16;
        _vpFujiPrimaryAccentAmp      = s.FujisakiPrimaryAccentAmpQ16;
        _vpFujiHeadAccentAmp         = s.FujisakiHeadAccentAmpQ16;
        _vpFujiSecondaryAccentAmp    = s.FujisakiSecondaryAccentAmpQ16;
        _vpFujiEmphaticAccentAmp     = s.FujisakiEmphaticAccentAmpQ16;
        _vpFujiPitchCurveExp         = (double)s.FujisakiPitchCurveExpPct / 100.0;
        if (_vpFujiPitchCurveExp <= 0.0) {
            _vpFujiPitchCurveExp = 1.0;
        }
        _vpFujiMonoAccentDurScale    = (double)s.FujisakiMonoAccentDurScalePct / 100.0;
        _vpFujiCompoundStep          = (double)s.FujisakiCompoundStepQ16 / k100percent;
        {
            // Statement final-vowel drop: last vowel ends ~0.85x baseline
            double dropFrac = (double)s.FujisakiFinalDropPct / 100.0;
            if (dropFrac >= 1.0) dropFrac = 0.99;
            _vpFujiFinalDropPitch = (int32_t)(std::log(1.0 - dropFrac) * kPitchUnitsPerNeper);
        }

        _fuji.Reset();
        _punctSmooth = 0;
        _f0Smooth = 0;
        _f0SmoothPrime = true;
        _clauseFrame = 0;
        _phraseFireFrame = 0;

        _controlF0 = 0;
        _curPhonCtrlSinging = 0;
        _flutterPhaseA = 0;
        _flutterPhaseB = 0;
        _dbgFujiExcursion = 0;
        _dbgPhraseResp = 0;
        _dbgAccentResp = 0;
        _dbgBaselineOffset = 0;
        _dbgTotalOffset = 0;
        _punctOffset = 0;
        _fujiMeanOffset = 0;

        BuildFujiProsody();

        // Fire the clause-start phrase command; kPhraseReset re-fires it mid-clause
        _fuji.Phrase((double)_vpFujiPhraseAmp / k100percent,
                     FujisakiPitchModel::kDefaultPhraseLenFrames);
    }

    int16_t PitchInterpolator::Step() {
        Interpolate_Pitch();
        return (int16_t)_controlF0;
    }

    void PitchInterpolator::DoNote(int32_t phonIndex) {
        _hzGlide = false;
        _curPhonCtrlSinging = GetPhonCtrl(phonIndex);

        int64_t ctrl = (phonIndex >= 0 && phonIndex < (int32_t)_plan.PhonCtrlBuf.size())
                       ? _plan.PhonCtrlBuf[phonIndex] : 0;

        if ((ctrl & kSingingPhon) == 0) {
            _musicalNoteActive = false;
        }

        int16_t note = (phonIndex >= 0 && phonIndex < (int32_t)_plan.UserNoteBuf.size())
                       ? _plan.UserNoteBuf[phonIndex] : (int16_t)0;

        if (note != 0 && (ctrl & kSilenceDuration) == 0) {
            if ((ctrl & kSingingPhon) != 0) {
                if (note < 0) {
                    int32_t targetPitch = HzToPitch(-note);
                    int32_t curPitch = (int32_t)(_portamentoAccum >> 16);
                    int32_t frames = (phonIndex < (int32_t)_plan.DurBuf.size()) ? _plan.DurBuf[phonIndex] : 1;
                    if (frames < 1) {
                        frames = 1;
                    }
                    _vpBaselinePitch = targetPitch;
                    _portamentoStep = ((int64_t)(targetPitch - curPitch) << 16) / frames;
                    _newPortaTarget = true;
                    _hzGlide = true;
                } else {
                    int32_t targetPitch = HzToPitch(note);
                    _vpBaselinePitch = targetPitch;
                    _portamentoStep = 0;
                    _newPortaTarget = true;
                    _musicalNoteActive = true;
                }
            } else {
                int32_t n = (note & 0xFF) << 8;
                if (n != 0x7F00) {
                    _vpBaselinePitch = _voiceNaturalPitch + ((n * 0x1555) >> 16);
                    if (_vpBaselinePitch < 0) {
                        _vpBaselinePitch = 0;
                    }
                }
            }
        } else if ((ctrl & kSilenceDuration) == 0
                   && (ctrl & kSingingPhon) != 0
                   && (ctrl & kSingingDuration) != 0
                   && GetPhon(phonIndex) != _SIL_) {
            // Sung note with no explicit pitch: hold the voice's natural speaking
            // pitch (the seeded baseline, same scale normal speech renders at).
            // Guarded against SIL so the terminal release SIL (also carrying
            // kSingingPhon|kSingingDuration with note 0) doesn't reset portamento.
            _vpBaselinePitch = _voiceNaturalPitch;
            _portamentoStep = 0;
            _newPortaTarget = true;
            _musicalNoteActive = true;
        }
    }

    int32_t PitchInterpolator::HzToPitch(int32_t hz) {
        if (hz <= 0) {
            return 0;
        }
        int32_t freq, fk;
        if (hz < 50) {
            freq = hz << 4;
            fk = 0x000;
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

        int32_t ratio = ((freq - 400) * 2621) >> 11;
        if (ratio < 0) {
            ratio = 0;
        }
        if (ratio >= Tables::LogarithmBase2TableLength) {
            ratio = Tables::LogarithmBase2TableLength - 1;
        }
        return Tables::LogarithmBase2Table[ratio] + fk;
    }

    int16_t PitchInterpolator::GetPhon(int32_t index) const {
        if (index >= 0 && index < _plan.PhonBufInIndex) {
            return _plan.PhonBuf[index];
        }
        return _SIL_;
    }

    int64_t PitchInterpolator::GetPhonCtrl(int32_t index) const {
        if (index >= 0 && index < _plan.PhonBufInIndex) {
            return _plan.PhonCtrlBuf[index];
        }
        return 0;
    }

    void PitchInterpolator::Phon_Boundry_Pitch() {
        if (_timeIntoPhonCp >= _curPhonDurCp) {
            _timeIntoPhonCp -= _curPhonDurCp;
            _phonIndexCp++;
            _curPhonDurCp = (_phonIndexCp < (int32_t)_plan.DurBuf.size()) ? _plan.DurBuf[_phonIndexCp] : 0;

            int32_t curPhon = GetPhon(_phonIndexCp);
            uint32_t curFlags = Tables::GetFeatureFlags(curPhon);
            int64_t curCtrl = GetPhonCtrl(_phonIndexCp + 1);

            int32_t nextPhon = GetPhon(_phonIndexCp + 1);
            uint32_t nextFlags = Tables::GetFeatureFlags(nextPhon);
            int64_t nextCtrl = GetPhonCtrl(_phonIndexCp + 1);

            if (_pitchBoundry == 0) {
                _pitchBoundry = kNeverHappens;
            }
            if (_pitchBoundry > 0) {
                _pitchBoundry = 0;
            }

            _pbHold = kNeverHappens;
            _pbLowGain = false;

            if ((curFlags & kVowel1F) != 0
                && (nextCtrl & kMid_Syllable_In_Word) == 0
                && ((curCtrl & kSyllableTypeField) >= kWord_End)
                && nextPhon != _YU_) {
                if ((curFlags & kVowelF) != 0) {
                    if (curPhon == nextPhon && (nextCtrl & kPrimOrEmphStress) != 0) {
                        _pbHold = _curPhonDurCp;
                    } else if ((curCtrl & kSyllableTypeField) >= kPrep_End) {
                        _pbHold = _curPhonDurCp;
                        _pbLowGain = true;
                    }
                } else {
                    if ((curFlags & kStopF) == 0
                        && curPhon != 53 // _DX_
                        && (nextCtrl & kPrimOrEmphStress) != 0) {
                        _pbHold = _curPhonDurCp;
                    }
                }
            }

            if ((nextFlags & kGStopF) != 0) {
                _pbHold = _curPhonDurCp;
            }
            if ((curFlags & kGStopF) != 0) {
                _pbHold = _curPhonDurCp;
                return;
            }
        }

        int32_t timeAt50 = 50 / kFrameTime; // 10
        int32_t lastFrame = _curPhonDurCp - 1;
        if (_timeIntoPhonCp == timeAt50 || _timeIntoPhonCp == lastFrame) {
            _pitchBoundry = _pbHold;
            _lowGainCp = _pbLowGain;
        }
    }

    // Fires all pitch-buffer events due this frame (speech and singing alike)
    void PitchInterpolator::FirePitchEvents() {
        bool collect = true;
        do {
            if (_curPitchBufTime >= _nextPitchBufTime
                && _pitchBufOutIndex < (int32_t)_plan.PitchBufInIndex) {
                int32_t evAmp = _plan.PitchBufFreq[_pitchBufOutIndex];
                int32_t evFlags = _plan.PitchBufFlags[_pitchBufOutIndex];
                (void)_plan.PitchBufTiltX64[_pitchBufOutIndex];  // tilt field unused by the Fujisaki model
                int32_t evDuration = _plan.PitchBufDuration[_pitchBufOutIndex];

                _curPitchBufTime -= _nextPitchBufTime;
                _pitchBufOutIndex++;
                _nextPitchBufTime = _plan.PitchBufTime[_pitchBufOutIndex];

                if ((evFlags & kResetDecline) != 0) {
                    _downRampOffset = 0;
                } else if ((evFlags & kPhraseReset) != 0) {
                    _downRampOffset = (int64_t)(_baselineStartOffset - _baselineEndOffset) << 14;
                    if (_curRamp < (int32_t)_rampSteps.size() - 1) {
                        _curRamp++;
                    }
                    _downRampStep = _rampSteps[_curRamp];
                    // Fresh phrase command; filters start from rest
                    _fuji.Reset();
                    _fuji.Phrase((double)_vpFujiPhraseAmp / k100percent,
                                 FujisakiPitchModel::kDefaultPhraseLenFrames);
                    _phraseFireFrame = _clauseFrame;
                    _punctSmooth = 0;
                    _f0Smooth = 0;
                    _f0SmoothPrime = true;
                    _punctOffset = 0;
                } else if ((evFlags & kPitchBoundry_Flg) != 0) {
                    _punctOffset = evAmp;
                } else {
                    FireAccentEvent((int16_t)evFlags, evDuration);
                }
            } else {
                collect = false;
            }
        }
        while (collect);
    }

    // One frame of intonation: declination + userPitch + Fujisaki phrase/accent
    // + smoothed boundary tone + clause-final shaping (raw log-pitch offset).
    int32_t PitchInterpolator::ComputeIntonationOffset(int32_t userPitch) {
        // Declination ramp (linear in log-pitch = exponential F0 decay)
        int32_t baseLineOffset = _baselineStartOffset - (int32_t)(_downRampOffset >> 16) + userPitch;
        _dbgBaselineOffset = baseLineOffset - userPitch;
        if (baseLineOffset > _baselineEndOffset) {
            _downRampOffset += _downRampStep;
        }

        // Fujisaki phrase + accent filter responses (nepers -> pitch units)
        double ySum = _fuji.Step();
        int32_t fujiExcursion = (int32_t)(ySum * kPitchUnitsPerNeper);
        _dbgPhraseResp = (int32_t)(_fuji.PhraseResp() * kPitchUnitsPerNeper);
        _dbgAccentResp = (int32_t)(_fuji.AccentResp() * kPitchUnitsPerNeper);

        // Statement clauses blend a drop across the last vowel; question/tilde
        // rises arrive via _punctOffset instead.
        int32_t clauseShaping = 0;
        {
            int16_t endPunct = _plan.EndPunctuation;
            bool isStatement = (endPunct == 0 || endPunct == _Period_);
            if (isStatement && _vpFujiFinalDropPitch != 0
                && _clauseFrame >= _lastVowelStartFrame
                && _clauseFrame < _lastVowelStartFrame + _lastVowelDur) {
                int32_t intoVowel = _clauseFrame - _lastVowelStartFrame;
                int32_t frac = (_lastVowelDur > 0) ? ((intoVowel << 16) / _lastVowelDur) : 0;
                // Blend the (already negative) drop across the vowel
                clauseShaping = (_vpFujiFinalDropPitch * frac) >> 16;
            }
        }

        _punctSmooth = (_punctSmooth * 7 + (_punctOffset + clauseShaping)) >> 3;
        int32_t excursion = fujiExcursion + _punctSmooth;
        _dbgFujiExcursion = excursion;

        return baseLineOffset + excursion;
    }

    // Precomputes per-word vowel data, the last-vowel window, and the clause
    // mean offset by replaying the intonation profile.
    void PitchInterpolator::BuildFujiProsody() {
        const int32_t n = _plan.PhonBufInIndex;
        _wordNuclei.assign((size_t)n, 0);
        _wordHadPrimary.assign((size_t)n, false);
        _phonStartFrame.assign((size_t)n + 1, 0);
        {
            int32_t acc = 0;
            for (int32_t i = 0; i < n; ++i) {
                _phonStartFrame[i] = acc;
                acc += (i < (int32_t)_plan.DurBuf.size()) ? _plan.DurBuf[i] : 0;
            }
            _phonStartFrame[n] = acc;
        }
        _lastVowelStartFrame = 0;
        _lastVowelDur = 0;

        int32_t lastVowelIdx = -1;
        size_t wordBegin = 0;
        for (int32_t i = 0; i <= n; ++i) {
            bool isEnd = (i == n) ||
                (i > 0 && (_plan.PhonCtrlBuf[i] & kBoundryTypeField) == kWord_Start
                      && _plan.PhonBuf[i] != _SIL_);
            if (isEnd) {
                int32_t nuclei = 0;
                for (int32_t j = (int32_t)wordBegin; j < i; ++j) {
                    if ((Tables::GetFeatureFlags(GetPhon(j)) & kVowelF) != 0) {
                        ++nuclei;
                    }
                }
                bool primarySoFar = false;
                for (int32_t j = (int32_t)wordBegin; j < i; ++j) {
                    _wordNuclei[j] = nuclei;
                    _wordHadPrimary[j] = primarySoFar;
                    int16_t p = GetPhon(j);
                    if ((Tables::GetFeatureFlags(p) & kVowelF) != 0) {
                        lastVowelIdx = j;
                        if ((GetPhonCtrl(j) & kPrimOrEmphStress) != 0) {
                            primarySoFar = true;
                        }
                    }
                }
                if (i < n) {
                    wordBegin = (size_t)i;
                }
            }
        }

        if (lastVowelIdx >= 0 && lastVowelIdx < (int32_t)_plan.DurBuf.size()) {
            for (int32_t i = 0; i < lastVowelIdx; ++i) {
                if (i < (int32_t)_plan.DurBuf.size()) {
                    _lastVowelStartFrame += _plan.DurBuf[i];
                }
            }
            _lastVowelDur = _plan.DurBuf[lastVowelIdx];
        }

        // Replay the clause's intonation profile (no user pitch) and average it
        // over voiced frames.  Subtracting this mean before Intonation/PitchRange
        // scaling keeps PitchHz the average pitch.  Must advance _curPitchBufTime
        // exactly like the live loop or the event timeline stalls and Intonation
        // leaks into the level.
        {
            const int32_t totalFrames = _phonStartFrame[(size_t)_plan.PhonBufInIndex];
            int64_t sum = 0;
            int32_t voicedCount = 0;
            if (totalFrames > 0) {
                _fuji.Reset();
                _fuji.Phrase((double)_vpFujiPhraseAmp / k100percent,
                             FujisakiPitchModel::kDefaultPhraseLenFrames);
                int32_t phonIdx = 0;
                for (int32_t f = 0; f < totalFrames; ++f) {
                    FirePitchEvents();
                    int32_t off = ComputeIntonationOffset(0);
                    _curPitchBufTime++;
                    _clauseFrame++;
                    // Frame -> phoneme walk (monotonic)
                    while (phonIdx + 1 < (int32_t)_phonStartFrame.size()
                           && _phonStartFrame[(size_t)phonIdx + 1] <= f) {
                        phonIdx++;
                    }
                    if (phonIdx < _plan.PhonBufInIndex
                        && (Tables::GetFeatureFlags(GetPhon(phonIdx)) & kVoicedF) != 0) {
                        sum += off;
                        voicedCount++;
                    }
                }
                _fujiMeanOffset = (voicedCount > 0)
                    ? (int32_t)std::llround((double)sum / voicedCount) : 0;
            } else {
                _fujiMeanOffset = 0;
            }

            // Restore pre-synthesis state; the ctor fires the initial phrase
            const PitchState& ps = _plan.Pitch;
            _fuji.Reset();
            _punctSmooth = 0;
            _punctOffset = 0;
            _curPitchBufTime = ps.CurPitchBufTime;
            _nextPitchBufTime = ps.NextPitchBufTime;
            _pitchBufOutIndex = ps.PitchBufOutIndex;
            _downRampOffset = ps.DownRampOffset;
            _curRamp = ps.CurRamp;
            _downRampStep = ps.DownRampStep;
            _clauseFrame = 0;
            _phraseFireFrame = 0;
            _f0Smooth = 0;
            _f0SmoothPrime = true;
        }
    }

    // Fires one accent command from a pitch-buffer event (type from flags)
    void PitchInterpolator::FireAccentEvent(int16_t flags, int32_t durationFrames) {
        // Resolve the owning phoneme; events fire one frame before their buffer
        // time, so the nominal event frame is _clauseFrame + 1 (keeps time-0
        // events attributed to their vowel, not the preceding consonant).
        int32_t phon = 0;
        {
            int32_t evFrame = _clauseFrame + 1;
            size_t lo = 0, hi = _phonStartFrame.size();
            while (lo + 1 < hi) {
                size_t mid = (lo + hi) >> 1;
                if (_phonStartFrame[mid] <= evFrame) {
                    lo = mid;
                } else {
                    hi = mid;
                }
            }
            phon = (int32_t)lo;
        }

        // Event type -> accent amplitude (nepers) + prominence for curve
        // shaping (accentAmp = baseAmp * pow(prominence, pitchExp)).  Head
        // accents stay near-nuclear so the clause doesn't ride flat.
        double amp = 0.0;
        double prominence = 1.0;
        if ((flags & kPitchRiseFall_Flg) != 0) {
            amp = (double)_vpFujiPrimaryAccentAmp / k100percent;
        } else if ((flags & kPitchRiseFall1_Flg) != 0) {
            amp = (double)_vpFujiHeadAccentAmp / k100percent;
            prominence = 0.85;
        } else if ((flags & kPitchStress_Flg) != 0) {
            // Emphatic vs pronoun accent, told apart via the phoneme's controls
            int64_t ctrl = GetPhonCtrl(phon);
            if ((ctrl & kEmphaticStress) != 0) {
                amp = (double)_vpFujiEmphaticAccentAmp / k100percent;
            } else {
                amp = (double)_vpFujiSecondaryAccentAmp / k100percent;
            }
        } else {
            return;
        }

        double expo = _vpFujiPitchCurveExp;
        if (expo < 0.1) expo = 0.1;
        amp *= std::pow(prominence, expo);
        if (amp <= 0.0) return;

        // Clause-start de-accenting: while the phrase hump is active, scale the
        // accent down so the first word doesn't stack phrase + accent into a
        // near-octave spike.  humpFrac is the phrase response, or (for accents
        // before the filter has risen) a taper since the phrase fired.
        {
            double phraseAmp = (double)_vpFujiPhraseAmp / k100percent;
            if (phraseAmp > 0.0) {
                double humpFrac = _fuji.PhraseResp() / phraseAmp;
                if (humpFrac > 1.0) humpFrac = 1.0;
                int32_t sincePhrase = _clauseFrame - _phraseFireFrame;
                double timeFrac = 1.0 - (double)sincePhrase / kPhraseHumpFrames;
                if (timeFrac < 0.0) timeFrac = 0.0;
                if (timeFrac > 1.0) timeFrac = 1.0;
                if (humpFrac < timeFrac) humpFrac = timeFrac;
                if (humpFrac > 0.0) {
                    amp *= (1.0 - kPhraseStartAccentScale * humpFrac);
                }
            }
        }
        if (amp <= 0.0) return;

        // Post-primary vowels in a word step down slightly
        if (_vpFujiCompoundStep > 0.0
            && phon >= 0 && phon < (int32_t)_wordHadPrimary.size()
            && _wordHadPrimary[phon] && prominence < 0.9) {
            amp -= _vpFujiCompoundStep;
            if (amp < 0.0) amp = 0.0;
        }
        if (amp <= 0.0) return;

        // Pulse length = vowel duration, clamped to a window (minimum so short
        // vowels drive the filter; the tail links accents together), scaled for
        // single-nucleus words.
        int32_t dur = durationFrames;
        if (dur < kMinAccentPulseFrames) dur = kMinAccentPulseFrames;
        if (dur > kMaxAccentPulseFrames) dur = kMaxAccentPulseFrames;
        if (_vpFujiMonoAccentDurScale > 0.0 && _vpFujiMonoAccentDurScale < 1.0
            && phon >= 0 && phon < (int32_t)_wordNuclei.size()
            && _wordNuclei[phon] == 1) {
            dur = (int32_t)(dur * _vpFujiMonoAccentDurScale);
            if (dur < 12) dur = 12;
        }

        // Attack scales with the pulse so short accents stay punchy
        int32_t attack = dur / 4;
        if (attack < 4) attack = 4;
        if (attack > FujisakiPitchModel::kDefaultAccentLenFrames) {
            attack = FujisakiPitchModel::kDefaultAccentLenFrames;
        }

        _fuji.Accent(amp, dur, attack);
    }

    void PitchInterpolator::Interpolate_Pitch() {
        FirePitchEvents();

        if (!_singing) {
            int32_t userPitch = (_phonIndexTarg >= 0 && _phonIndexTarg < (int32_t)_plan.UserPitchBuf.size())
                                ? _plan.UserPitchBuf[_phonIndexTarg] : 0;
            int32_t intonBase = ComputeIntonationOffset(userPitch);

            // Phoneme target advance (lookahead for phoneme pitch offsets)
            if (_timeIntoPhonTarg > _curPhonDurCc + _phonDurDelay
                && _phonIndexTarg < _plan.PhonBufInIndex) {
                _timeIntoPhonTarg -= _curPhonDurCc;
                _phonIndexTarg++;
                _curPhonDurCc = (_phonIndexTarg < (int32_t)_plan.DurBuf.size()) ? _plan.DurBuf[_phonIndexTarg] : 0;
                _phonDurDelay = 0;

                int32_t curPhon = GetPhon(_phonIndexTarg);
                (void)GetPhonCtrl(_phonIndexTarg);
                uint32_t curFlags = Tables::GetFeatureFlags(curPhon);
                int32_t nextPhon = GetPhon(_phonIndexTarg + 1);
                uint32_t nextFlags = Tables::GetFeatureFlags(nextPhon);

                int32_t phonPitchOffset = Tables::GetPitch(curPhon) >> 1;

                if ((nextFlags & kVoicedF) == 0) {
                    _phonDurDelay = 25 / kFrameTime; // 5
                }

                if ((curFlags & kVoicedF) != 0) {
                    _phonPitchOffset1 = phonPitchOffset << 1;
                    _uvPhonPitchTarg = 0;
                } else {
                    _uvPhonPitchTarg = phonPitchOffset;
                    _phonPitchOffset1 = 0;
                    if ((curFlags & kStopF) != 0) {
                        _phonDurDelay = 30 / kFrameTime; // 6
                    } else {
                        _phonDurDelay = 0;
                    }
                }
            }

            Phon_Boundry_Pitch();

            // Scale around the clause mean so PitchHz stays the average pitch;
            // phoneme micro-features are applied after range scaling to stay
            // constant regardless of the pitch range setting.
            int32_t totalOffset = (int32_t)(((int64_t)(intonBase - _fujiMeanOffset) * _vpIntonation) >> 16);
            totalOffset = (int16_t)totalOffset; // preserve C short-truncation behaviour
            _dbgTotalOffset = totalOffset;
            _controlF0 = (int32_t)((((int64_t)totalOffset * _vpPitchRange) >> 16) + _vpBaselinePitch);

            // Phoneme boundary micro-dip, scaled by intonation so it stays proportional
            // to the pitch range in use. Raw depth is -1 or -10 pitch units at the boundary.
            int32_t pbIndex = _timeIntoPhonCp - _pitchBoundry;
            if (pbIndex < 0) {
                pbIndex = -pbIndex;
            }
            const int32_t kPbWindow = 45 / kFrameTime; // 9
            if (pbIndex <= kPbWindow) {
                int32_t dipDepth = _lowGainCp ? (pbIndex - 1) : (pbIndex - 5);
                _controlF0 += (int32_t)((dipDepth * _vpIntonation) >> 16);
            }

            // Phoneme onset spikes (range-independent, applied post-scaling)
            _controlF0 += _phonPitchOffset1;
            _phonPitchOffset1 = (int32_t)(((int64_t)_phonPitchOffset1 * 98 * pct) >> 16);
            _controlF0 += _uvPhonPitchTarg;
            _uvPhonPitchTarg = (int32_t)(((int64_t)_uvPhonPitchTarg * 98 * pct) >> 16);

            // Vibrato
            _vibratoPhase1 = (int32_t)(_vibratoPhase1 + _vibratoFreq) & 0x00FFFFFF;
            double phaseNorm = (double)_vibratoPhase1 / 16777216.0;
            double angle = phaseNorm * 2.0 * M_PI;
            int32_t vibrato = (int32_t)(std::sin(angle) * 128.0);

            if (_speechRate >= 100) {
                _controlF0 += (int32_t)((vibrato * _vibratoDepth1) >> 16);
            } else {
                _controlF0 += (int32_t)((vibrato * _vibratoDepth2) >> 16);
            }

            // Flutter: difference of 5 Hz and 3 Hz oscillators, ~0.5-1 Hz peak deviation
            _flutterPhaseA = (_flutterPhaseA + 419430) & 0x00FFFFFF;
            _flutterPhaseB = (_flutterPhaseB + 251659) & 0x00FFFFFF;
            {
                double phA = (double)_flutterPhaseA / 16777216.0 * 2.0 * M_PI;
                double phB = (double)_flutterPhaseB / 16777216.0 * 2.0 * M_PI;
                _controlF0 += (int32_t)((std::sin(phA) - std::sin(phB)) * 2.0);
            }

            // Final backstop smoother (alpha = 0.75, tau ~ 14ms). Primed on the first frame
            // of each phrase so the smoother starts at the correct value rather than
            // ramping up from 0.
            if (_f0SmoothPrime) {
                _f0Smooth = _controlF0;
                _f0SmoothPrime = false;
            } else {
                _f0Smooth = (_f0Smooth * 3 + _controlF0) >> 2;
            }
            _controlF0 = _f0Smooth;
        } else {
            // Singing mode - portamento between notes
            if (_newSentence) {
                _portamentoAccum = (int64_t)_vpBaselinePitch << 16;
                _newSentence = false;
                _newPortaTarget = false;
            } else if (_newPortaTarget) {
                if (_portamentoStep > 0) {
                    _portamentoAccum += _portamentoStep;
                    if ((_portamentoAccum >> 16) >= _vpBaselinePitch) {
                        _portamentoAccum = (int64_t)_vpBaselinePitch << 16;
                        _newPortaTarget = false;
                    }
                } else if (_portamentoStep < 0) {
                    _portamentoAccum += _portamentoStep;
                    if ((_portamentoAccum >> 16) < _vpBaselinePitch) {
                        _portamentoAccum = (int64_t)_vpBaselinePitch << 16;
                        _newPortaTarget = false;
                    }
                } else if (_singing) {
                    int64_t target = (int64_t)_vpBaselinePitch << 16;
                    int64_t diff = target - _portamentoAccum;
                    _portamentoAccum += diff >> 2;
                    if (diff > -0x10000L && diff < 0x10000L) {
                        _portamentoAccum = target;
                        _newPortaTarget = false;
                    }
                } else {
                    _portamentoAccum = (int64_t)_vpBaselinePitch << 16;
                    _newPortaTarget = false;
                }
            }

            _controlF0 = (int32_t)(_portamentoAccum >> 16);

            _vibratoPhase1 = (int32_t)((_vibratoPhase1 + _vibratoFreq) & 0xFFFFFF);
            double phaseNorm = (double)_vibratoPhase1 / 16777216.0;
            double angle = phaseNorm * 2.0 * M_PI;
            int32_t vibrato = (int32_t)(std::sin(angle) * 128.0);

            if (!_hzGlide && _musicalNoteActive) {
                int64_t depth = (_curPhonCtrlSinging & kLowVibrato) != 0 ? _vibratoDepth2 : _vibratoDepth1;
                _controlF0 += (int32_t)((vibrato * depth) >> 16);
            }
        }

        if (_controlF0 < 0) {
            _controlF0 = 0;
        }

        _curPitchBufTime++;
        _timeIntoPhonTarg++;
        _timeIntoPhonCp++;
        _clauseFrame++;
    }

}  // namespace SharpVox
