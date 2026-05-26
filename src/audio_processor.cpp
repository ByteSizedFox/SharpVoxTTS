#include "../include/audio_processor.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace SharpTalk {

    // PhonemeNamesTable: indexed by phoneme ID.
    // Slots that have no name are nullptr.
    const char* const AudioProcessor::PhonemeNamesTable[] = {
        /* 0  _IY_ */ "IY",
        /* 1  _IH_ */ "IH",
        /* 2  _EH_ */ "EH",
        /* 3  _AE_ */ "AE",
        /* 4  _IX_ */ "IX",
        /* 5  _AX_ */ "AX",
        /* 6  _ER_ */ "ER",
        /* 7  _AH_ */ "AH",
        /* 8  _AA_ */ "AA",
        /* 9  _AO_ */ "AO",
        /* 10 _UH_ */ "UH",
        /* 11 _UW_ */ "UW",
        /* 12 _EY_ */ "EY",
        /* 13 _AY_ */ "AY",
        /* 14 _OY_ */ "OY",
        /* 15 _AW_ */ "AW",
        /* 16 _OW_ */ "OW",
        /* 17 _YU_ */ "YU",
        /* 18 _IR_ */ "IR",
        /* 19 _XR_ */ "XR",
        /* 20 _AR_ */ "AR",
        /* 21 _OR_ */ "OR",
        /* 22 _UR_ */ "UR",
        /* 23 _SIL_*/ nullptr,
        /* 24 _M_  */ "M",
        /* 25 _N_  */ "N",
        /* 26 _NG_ */ "NG",
        /* 27 _W_  */ "W",
        /* 28 _Y_  */ "Y",
        /* 29 _R_  */ "R",
        /* 30 _L_  */ "L",
        /* 31 _RX_ */ "RX",
        /* 32 _LX_ */ "LX",
        /* 33 _EL_ */ "EL",
        /* 34 _EN_ */ "EN",
        /* 35 _F_  */ "F",
        /* 36 _V_  */ "V",
        /* 37 _TH_ */ "TH",
        /* 38 _DH_ */ "DH",
        /* 39 _S_  */ "S",
        /* 40 _Z_  */ "Z",
        /* 41 _SH_ */ "SH",
        /* 42 _ZH_ */ "ZH",
        /* 43 _HH_ */ "HH",
        /* 44 _P_  */ "P",
        /* 45 _B_  */ "B",
        /* 46 _T_  */ "T",
        /* 47 _D_  */ "D",
        /* 48 _K_  */ "K",
        /* 49 _G_  */ "G",
        /* 50 _CH_ */ "CH",
        /* 51 _JH_ */ "JH",
        /* 52 _TX_ */ "TX",
        /* 53 _DX_ */ "DX",
        /* 54 _QX_ */ "QX",
        /* 55 _DD_ */ "DD",
        /* 56 */      nullptr,
        /* 57 */      nullptr,
        /* 58 */      nullptr,
        /* 59 */      nullptr,
        /* 60 */      nullptr,
        /* 61 */      nullptr,
        /* 62 */      nullptr,
        /* 63 */      nullptr,
        /* 64 _JP_A_*/ "JP_A",
        /* 65 _JP_I_*/ "JP_I",
        /* 66 _JP_U_*/ "JP_U",
        /* 67 _JP_E_*/ "JP_E",
        /* 68 _JP_O_*/ "JP_O",
    };

    // Constructor

    AudioProcessor::AudioProcessor(const VoiceData& voice) {
        InitFromVoice(voice);
    }

    void AudioProcessor::InitFromVoice(const VoiceData& vd) {
        _speechRate       = vd.Rate;
        _vpRiseAmt        = vd.RiseAmt;
        _vpFallAmt        = vd.FallAmt;
        _vpRiseAmt1       = vd.RiseAmt1;
        _vpFallAmt1       = vd.FallAmt1;
        _vpAssertiveness  = vd.Assertiveness;
        _vpBaselineFall   = vd.BaselineFall;
        _stressDurTime    = vd.StressDurTime;
        _vpPitchRange     = ((int64_t)vd.PitchRange << 16) / 100;
        _vpStressGain     = ((int64_t)vd.StressGain << 16) / 100;
        _vibratoDepth1    = ((int64_t)vd.VibratoDepth1Raw << 16) / 1000;
        _vibratoDepth2    = ((int64_t)vd.VibratoDepth2Raw << 16) / 1000;
        int64_t vf        = ((int64_t)vd.VibratoFreqRaw << 16) / 10;
        _vibratoFreq      = (vf * 256) / 200;
        _vpIntonation     = ((int64_t)vd.Intonation << 16) / 100;
        _voiceNaturalPitch = HzToPitch(vd.PitchHz);
        _vpUptalkAmt      = vd.UptalkAmt;
        _vpStressEarly    = vd.StressEarly;
        _vpBreakStrength  = vd.BreakStrength;
        _vpEmphasisBoost  = vd.EmphasisBoost;
        _vocalConfidence  = vd.VocalConfidence;
    }

    // Public entry point.
    // Runs the full pipeline and returns a SynthInputDump ready for SpeechRenderer.

    SynthInputDump AudioProcessor::Process(const std::vector<PhonemeToken>& tokens,
                                           int16_t endPunctuation) {
        _endPunctuation = endPunctuation;
        _singing = false;

        ClearBuffers();
        InitRateParams();
        InitPitchParams();

        LoadPhonemes(tokens);
        FlagPhonBuf1();
        FillPhonBuf2();
        PitchRaiseAndFall();
        ModDuration();
        StretchLastWordForTilde();
        CalcRampSteps();
        FillPitchBuf();
        StartNewPitchClause();
        InsertPlosiveRelease();

        return BuildSynthInputDump();
    }

    // Pipeline setup helpers

    void AudioProcessor::ClearBuffers() {
        for (int32_t i = 0; i < kPhonBufSize; i++) {
            _phonBuf1[i] = _SIL_;
            _phonBuf2[i] = _SIL_;
            _phonCtrlBuf1[i] = 0;
            _phonCtrlBuf2[i] = 0;
            _userDurBuf1[i] = kDur_One;
            _userDurBuf2[i] = kDur_One;
            _userPitchBuf1[i] = 0;
            _userNoteBuf1[i] = 0;
            _userRateBuf1[i] = 0;
        }
    }

    void AudioProcessor::InitRateParams() {
        if (_speechRate < kMinRate) {
            _speechRate = kMinRate;
        }
        // _rateRatio: how much to scale durations relative to 180 wpm normal rate (16.16 fixed).
        _rateRatio = ((int64_t)kNormal_Speech_Rate << 16) / _speechRate;
        // _rateRatioLowGain: compressed ratio used for the rate-sensitive part of durations
        // (phoneme intrinsic durations compress less than fixed additions at extreme speeds).
        int64_t denominator = (((_speechRate - kNormal_Speech_Rate) * (int64_t)(k1pct * 60)) >> 16) + kNormal_Speech_Rate;
        _rateRatioLowGain = ((int64_t)kNormal_Speech_Rate << 16) / denominator;
        _stressDuration = (int16_t)((_rateRatio * _stressDurTime) >> 16);
    }

    void AudioProcessor::InitPitchParams() {
        _vpBaselinePitch = _voiceNaturalPitch;
        _baselineFallStart = (int16_t)(kHZ_7 + _vpBaselineFall);
        _baselineFallEnd = (int16_t)(kHZ_7 - _vpBaselineFall);
        _pitchClauseStartTime = (int16_t)(10 / kFrameTime);
        _pitchBoundry = kNeverHappens;
    }

    int16_t AudioProcessor::HzToPitch(int16_t hz) {
        const int32_t ratioK = 2621;
        if (hz <= 0) {
            return 0;
        }
        int64_t freq, fk;
        if (hz < 100) {
            freq = hz << 3;
            fk = 0x000;
        } else if (hz < 200) {
            freq = hz << 2;
            fk = 0x100;
        } else if (hz < 400) {
            freq = hz << 1;
            fk = 0x200;
        } else if (hz < 800) {
            freq = hz;
            fk = 0x300;
        } else if (hz < 1600) {
            freq = hz >> 1;
            fk = 0x400;
        } else if (hz < 3200) {
            freq = hz >> 2;
            fk = 0x500;
        } else {
            freq = hz >> 3;
            fk = 0x600;
        }
        int64_t ratio = ((freq - 400) * ratioK) >> 11;
        if (ratio < 0) {
            ratio = 0;
        }
        if (ratio >= (int64_t)Tables::LogarithmBase2TableLength) {
            ratio = (int64_t)Tables::LogarithmBase2TableLength - 1;
        }
        return (int16_t)(Tables::LogarithmBase2Table[ratio] + fk);
    }

    // Snapshots computed buffers and PitchState into a SynthInputDump for minimal allocation replay.
    SynthInputDump AudioProcessor::BuildSynthInputDump() {
        int32_t count = _phonBuf2InIndex + 1; // +1 for lookahead SIL slot

        std::vector<int16_t>  phonBuf2(count);
        std::vector<int64_t>  controls(count);
        std::vector<int16_t>  durBuf(count);
        std::vector<int16_t>  userPitchBuf2(count);
        std::vector<int16_t>  userNoteBuf2(count);
        std::vector<uint8_t>  aspirationBuf2(count);
        std::vector<uint8_t>  tiltBuf2(count);
        std::vector<uint8_t>  effortBuf2(count);
        std::vector<uint8_t>  vibDepthBuf2(count);
        std::vector<uint8_t>  vibRateBuf2(count);
        std::vector<uint8_t>  tremDepthBuf2(count);
        std::vector<uint8_t>  tremRateBuf2(count);

        for (int32_t i = 0; i < count; i++) {
            phonBuf2[i]      = _phonBuf2[i];
            controls[i]      = _phonCtrlBuf2[i];
            durBuf[i]        = _durBuf[i];
            userPitchBuf2[i] = _userPitchBuf2[i];
            userNoteBuf2[i]  = _userNoteBuf2[i];
            aspirationBuf2[i] = _aspirationBuf2[i];
            tiltBuf2[i]      = _tiltBuf2[i];
            effortBuf2[i]    = _effortBuf2[i];
            vibDepthBuf2[i]  = _vibDepthBuf2[i];
            vibRateBuf2[i]   = _vibRateBuf2[i];
            tremDepthBuf2[i] = _tremDepthBuf2[i];
            tremRateBuf2[i]  = _tremRateBuf2[i];
        }

        int32_t pitchCount = _pitchBufInIndex + 1;
        std::vector<int16_t> pitchFreq(pitchCount);
        std::vector<int16_t> pitchTime(pitchCount);
        std::vector<int16_t> pitchFlags(pitchCount);
        std::vector<int16_t> pitchTiltX64(pitchCount);
        std::vector<int16_t> pitchDuration(pitchCount);
        for (int32_t i = 0; i < pitchCount; i++) {
            pitchFreq[i]     = _pitchBufFreq[i];
            pitchTime[i]     = _pitchBufTime[i];
            pitchFlags[i]    = _pitchBufFlags[i];
            pitchTiltX64[i]  = _pitchBufTiltX64[i];
            pitchDuration[i] = _pitchBufDuration[i];
        }

        std::vector<int64_t> rampStepsCopy(kMaxRamps);
        std::copy(_rampSteps, _rampSteps + kMaxRamps, rampStepsCopy.begin());

        PitchState pitch;
        pitch.NextPitchBufTime  = pitchCount > 0 ? pitchTime[0] : (int16_t)0;
        pitch.PitchBufOutIndex  = 0;
        pitch.CurPitchBufTime   = (int16_t)(_pitchClauseStartTime >> 1);
        pitch.CurPitchBufPitch  = 0;
        pitch.CurPitchBufFlags  = 0;

        pitch.PhonIndexTarg     = -1;
        pitch.PhonIndexCp       = -1;
        pitch.TimeIntoPhonTarg  = _pitchClauseStartTime;
        pitch.TimeIntoPhonCp    = 0;
        pitch.CurPhonDurCc      = 0;
        pitch.CurPhonDurCp      = 0;
        pitch.PhonDurDelay      = 0;

        pitch.UvPhonPitchTarg   = 0;
        pitch.PhonPitchOffset   = 0;
        pitch.PhonPitchOffset1  = 0;

        pitch.BaseLineOffset     = 0;
        pitch.BasePitchOffset    = 0;
        pitch.PitchBoundry       = (int16_t)_pitchBoundry;
        pitch.LowGainCp          = 0;

        pitch.BaselineFallStart  = _baselineFallStart;
        pitch.BaselineFallEnd    = _baselineFallEnd;
        pitch.BaselineStartOffset = _baselineStartOffset;
        pitch.BaselineEndOffset   = _baselineEndOffset;

        pitch.DownRampOffset     = 0;
        pitch.DownRampStep       = (kMaxRamps > 0) ? _rampSteps[0] : 0;
        std::copy(rampStepsCopy.begin(), rampStepsCopy.end(), pitch.RampSteps.begin());
        pitch.CurRamp            = _curRamp;

        pitch.VpIntonation       = _vpIntonation;
        pitch.VpPitchRange       = _vpPitchRange;
        pitch.VpBaselinePitch    = _vpBaselinePitch;

        pitch.VibratoDepth1      = _vibratoDepth1;
        pitch.VibratoDepth2      = _vibratoDepth2;
        pitch.VibratoFreq        = _vibratoFreq;
        pitch.VibratoPhase1      = 0;

        pitch.Singing            = (int16_t)(_singing ? 1 : 0);
        pitch.HzGlide            = 0;
        pitch.MusicalNoteActive  = 0;
        pitch.PortamentoAccum    = 0;
        pitch.PortamentoStep     = 0;
        pitch.NewPortaTarget     = 0;
        pitch.NewSentence        = 1;
        pitch.SpeechRate         = _speechRate;

        return SynthInputDump::Create(
            /*phonBuf2InIndex=*/ _phonBuf2InIndex,
            /*phonBuf2=*/        std::move(phonBuf2),
            /*controls=*/        std::move(controls),
            /*durBuf=*/          std::move(durBuf),
            /*userPitchBuf2=*/   std::move(userPitchBuf2),
            /*userNoteBuf2=*/    std::move(userNoteBuf2),
            /*aspirationBuf2=*/  std::move(aspirationBuf2),
            /*tiltBuf2=*/        std::move(tiltBuf2),
            /*effortBuf2=*/      std::move(effortBuf2),
            /*vibDepthBuf2=*/    std::move(vibDepthBuf2),
            /*vibRateBuf2=*/     std::move(vibRateBuf2),
            /*tremDepthBuf2=*/   std::move(tremDepthBuf2),
            /*tremRateBuf2=*/    std::move(tremRateBuf2),
            /*pitchBufInIndex=*/ (uint32_t)_pitchBufInIndex,
            /*pitchBufFreq=*/    std::move(pitchFreq),
            /*pitchBufTime=*/    std::move(pitchTime),
            /*pitchBufFlags=*/   std::move(pitchFlags),
            /*pitchBufTiltX64=*/ std::move(pitchTiltX64),
            /*pitchBufDuration=*/std::move(pitchDuration),
            /*pitch=*/           std::move(pitch)
        );
    }

    // Inline helpers

    int16_t AudioProcessor::GetPhon2(int32_t i) {
        if (i < 0 || i >= _phonBuf2InIndex) {
            return _SIL_;
        }
        return _phonBuf2[i];
    }

    int64_t AudioProcessor::GetCtrl2(int32_t i) {
        if (i < 0 || i >= _phonBuf2InIndex) {
            return 0;
        }
        return _phonCtrlBuf2[i];
    }

    uint32_t AudioProcessor::GetPhonFlags1(int32_t i) {
        if (i < 0 || i >= _phonBuf1InIndex) {
            return 0;
        }
        return Tables::GetFeatureFlags(_phonBuf1[i]);
    }

    // -------------------------------------------------------------------------
    // LoadPhonemes
    // -------------------------------------------------------------------------

    void AudioProcessor::LoadPhonemes(const std::vector<PhonemeToken>& tokens) {
        // Slot 0 is the initial SIL which is already filled by ClearBuffers
        _phonBuf1InIndex = 1;

        for (const auto& tok : tokens) {
            if (_phonBuf1InIndex >= kPhonBuf_Red_Zone) {
                break;
            }
            _phonBuf1[_phonBuf1InIndex]       = tok.Phon;
            _phonCtrlBuf1[_phonBuf1InIndex]   = tok.Ctrl;
            _userPitchBuf1[_phonBuf1InIndex]  = tok.UserPitch;
            _userDurBuf1[_phonBuf1InIndex]    = tok.UserDur == 0 ? (int16_t)kDur_One : tok.UserDur;
            _userNoteBuf1[_phonBuf1InIndex]   = tok.UserNote;
            _userRateBuf1[_phonBuf1InIndex]   = tok.UserRate;
            _aspirationBuf1[_phonBuf1InIndex] = tok.Aspiration;
            _tiltBuf1[_phonBuf1InIndex]       = tok.Tilt;
            _effortBuf1[_phonBuf1InIndex]     = tok.Effort;
            _vibDepthBuf1[_phonBuf1InIndex]   = tok.VibDepth;
            _vibRateBuf1[_phonBuf1InIndex]    = tok.VibRate;
            _tremDepthBuf1[_phonBuf1InIndex]  = tok.TremDepth;
            _tremRateBuf1[_phonBuf1InIndex]   = tok.TremRate;
            if ((tok.Ctrl & kSingingDuration) != 0) {
                _singing = true;
            }
            _phonBuf1InIndex++;
        }

        // Add trailing boundary SIL, but only if the last phoneme isn't already a
        // terminal SIL (sentence-final comma from FrontEnd). If one is already
        // there, upgrade its boundary type to match the sentence-ending punctuation.
        if (_phonBuf1InIndex < kPhonBuf_Red_Zone && _endPunctuation != 0) {
            int32_t bndType;
            if (_endPunctuation == _Period_) {
                bndType = kBND_Decl;
            } else if (_endPunctuation == _Quest_) {
                bndType = kBND_Quest;
            } else if (_endPunctuation == _Exclam_) {
                bndType = kBND_Emph;
            } else if (_endPunctuation == _Ellipsis_) {
                bndType = kBND_Ellipsis;
            } else {
                bndType = kBND_Pause;
            }
            int32_t lastIdx = _phonBuf1InIndex - 1;
            bool lastIsSilBoundary = lastIdx >= 1 &&
                _phonBuf1[lastIdx] == _SIL_ &&
                (_phonCtrlBuf1[lastIdx] & kTerm_Bound) != 0;

            if (lastIsSilBoundary) {
                // Replace the existing boundary type with the sentence-final type
                _phonCtrlBuf1[lastIdx] = (_phonCtrlBuf1[lastIdx] & ~kSilenceTypeField)
                    | ((int64_t)bndType << kSilenceTypeShift);
            } else {
                _phonCtrlBuf1[_phonBuf1InIndex] |= kTerm_Bound;
                _phonCtrlBuf1[_phonBuf1InIndex] |= ((int64_t)bndType << kSilenceTypeShift);
                // _phonBuf1[index] is already _SIL_ from ClearBuffers
                _phonBuf1InIndex++;
            }
        }

        // compute kWord_Initial_Consonant for each word.
        // Any boundary bit (kWord_Start, kTerm_Bound, kVerb_Start, kPrep_Start) resets
        // wordInitial=true. kWord_Start is on the first real phoneme of a word, so we
        // must NOT skip it, it may itself be a word-initial consonant. SIL phonemes
        // (sentence boundaries with kTerm_Bound) are skipped since they're not consonants.
        bool wordInitial = true;
        for (int32_t i = 1; i < _phonBuf1InIndex; i++) {
            int64_t ctrl = _phonCtrlBuf1[i];
            if ((ctrl & kBoundryTypeField) != 0) {
                wordInitial = true;
            }
            if (_phonBuf1[i] == _SIL_) {
                continue;
            }
            uint32_t flags = GetPhonFlags1(i);
            if (wordInitial) {
                if ((flags & kVowelF) != 0) {
                    wordInitial = false;
                } else {
                    _phonCtrlBuf1[i] |= kWord_Initial_Consonant;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Flag_PhonBuf_1
    // -------------------------------------------------------------------------

    void AudioProcessor::FlagPhonBuf1() {
        _isCompoundNoun = false;
        for (_scanIndex = 0; _scanIndex < _phonBuf1InIndex; _scanIndex++) {
            int64_t ctrl = _phonCtrlBuf1[_scanIndex];
            if ((ctrl & kCompoundNoun) != 0) {
                _isCompoundNoun = true;
            } else if ((ctrl & kBoundryTypeField) != 0) {
                _isCompoundNoun = false;
            }

            uint32_t phonFlags = GetPhonFlags1(_scanIndex);
            if ((phonFlags & kVowelF) != 0) {
                MarkSyllable();
            }

            MarkBoundry();
        }
        MarkSyllableStart();
    }

    void AudioProcessor::MarkSyllable() {
        int64_t order = 0;

        // scan backward for another vowel in same word
        for (int32_t idx = _scanIndex - 1; idx > 0; idx--) {
            int64_t syl = _phonCtrlBuf1[idx] & kSyllableTypeField;
            if (syl >= kWord_End) {
                break;
            }
            uint32_t flags = GetPhonFlags1(idx);
            if ((flags & kVowelF) != 0) {
                order = kLast_Syllable_In_Word;
                break;
            }
        }

        // scan forward for another vowel in same word
        for (int32_t idx = _scanIndex + 1; idx < _phonBuf1InIndex; idx++) {
            int64_t bnd = _phonCtrlBuf1[idx] & kBoundryTypeField;
            uint32_t flags = GetPhonFlags1(idx);
            if (bnd != 0) {
                _phonCtrlBuf1[_scanIndex] |= order;
                break;
            }
            if ((flags & kVowelF) != 0) {
                if (order == kLast_Syllable_In_Word) {
                    order = kMid_Syllable_In_Word;
                } else if (order == 0) {
                    order = kFirst_Syllable_In_Word;
                }
            }
        }
    }

    void AudioProcessor::MarkBoundry() {
        for (int32_t idx = _scanIndex + 1; idx < _phonBuf1InIndex; idx++) {
            int64_t bnd = _phonCtrlBuf1[idx] & kBoundryTypeField;
            uint32_t flags = GetPhonFlags1(idx);
            if (bnd != 0) {
                int64_t boundType = 0;
                if ((bnd & kTerm_Bound) != 0) {
                    boundType |= kTerm_End | kWord_End;
                }
                if ((bnd & kPrep_Start) != 0) {
                    boundType |= kPrep_End | kWord_End;
                }
                if ((bnd & kVerb_Start) != 0) {
                    boundType |= kVerb_End | kWord_End;
                }
                if ((bnd & kWord_Start) != 0) {
                    boundType |= kWord_End;
                }
                _phonCtrlBuf1[_scanIndex] |= boundType;
                break;
            }
            if ((flags & kVowelF) != 0) {
                break;
            }
        }
    }

    void AudioProcessor::MarkSyllableStart() {
        int32_t syllIdx = 0;
        int32_t idx = 0;
        while (idx < _phonBuf1InIndex) {
            while (idx < _phonBuf1InIndex && _phonBuf1[idx] == _SIL_) {
                syllIdx++;
                idx++;
            }
            if (idx >= _phonBuf1InIndex) {
                break;
            }

            uint32_t flags = GetPhonFlags1(idx);
            if ((flags & kVowelF) != 0) {
                _phonCtrlBuf1[syllIdx] |= kSyllable_Start;
                int64_t syllOrder = _phonCtrlBuf1[idx] & kSyllableOrderField;
                if (syllOrder == 0 || syllOrder == kLast_Syllable_In_Word) {
                    idx = FindNextWordBound(idx);
                    syllIdx = idx;
                } else {
                    // scan forward to next vowel counting consonants
                    int32_t dist = -1;
                    int32_t startIdx = idx; (void)startIdx;
                    do {
                        idx++;
                        dist++;
                        if (idx >= _phonBuf1InIndex) {
                            goto SYLL_DONE;
                        }
                    } while ((GetPhonFlags1(idx) & kVowelF) == 0);

                    if (dist == 0) {
                        syllIdx = idx;
                    } else if (dist == 1) {
                        idx--; syllIdx = idx;
                    } else if (dist == 2) {
                        int16_t p2 = _phonBuf1[idx - 1];
                        int16_t p1 = _phonBuf1[idx - 2];
                        if (IfConsonantCluster(p1, p2)) {
                            idx -= 2;
                        } else {
                            idx--;
                        }
                        syllIdx = idx;
                    } else if (dist == 3) {
                        int16_t p2 = _phonBuf1[idx - 1];
                        int16_t p1 = _phonBuf1[idx - 2];
                        if (IfConsonantCluster(p1, p2)) {
                            if (_phonBuf1[idx - 3] == _S_) {
                                idx -= 3;
                            } else {
                                idx -= 2;
                            }
                        } else {
                            idx--;
                        }
                        syllIdx = idx;
                    } else {
                        int16_t p1 = _phonBuf1[idx - dist];
                        int16_t p2 = _phonBuf1[idx - dist + 1];
                        if (IfConsonantCluster(p1, p2)) {
                            idx -= dist - 2;
                        } else {
                            idx -= dist >> 1;
                        }
                        syllIdx = idx;
                    }
                }
            } else {
                idx++;
            }
        }
    SYLL_DONE:;
    }

    int32_t AudioProcessor::FindNextWordBound(int32_t index) {
        for (int32_t i = index + 1; i < _phonBuf1InIndex; i++) {
            if ((_phonCtrlBuf1[i] & (kBoundryTypeField | kWord_Start)) != 0) {
                return i;
            }
        }
        return _phonBuf1InIndex;
    }

    bool AudioProcessor::IfConsonantCluster(int16_t c1, int16_t c2) {
        if (c1 == _F_  && (c2 == _R_ || c2 == _L_)) { return true; }
        if (c1 == _V_  && (c2 == _R_ || c2 == _L_)) { return true; }
        if (c1 == _TH_ && (c2 == _R_ || c2 == _W_)) { return true; }
        if (c1 == _S_  && (c2 == _W_ || c2 == _L_ || c2 == _P_ || c2 == _T_ || c2 == _K_
                        || c2 == _M_ || c2 == _N_ || c2 == _F_)) { return true; }
        if (c1 == _SH_ && (c2 == _W_ || c2 == _L_ || c2 == _P_ || c2 == _T_
                        || c2 == _R_ || c2 == _M_ || c2 == _N_)) { return true; }
        if (c1 == _P_  && (c2 == _R_ || c2 == _L_)) { return true; }
        if (c1 == _B_  && (c2 == _R_ || c2 == _L_)) { return true; }
        if (c1 == _T_  && (c2 == _R_ || c2 == _W_)) { return true; }
        if (c1 == _D_  && (c2 == _R_ || c2 == _W_)) { return true; }
        if (c1 == _K_  && (c2 == _R_ || c2 == _L_ || c2 == _W_)) { return true; }
        if (c1 == _G_  && (c2 == _R_ || c2 == _L_ || c2 == _W_)) { return true; }
        return false;
    }

    // Applies allophone substitution rules to convert the canonical phoneme stream (PhonBuf1)
    // into the allophone stream (PhonBuf2) used for synthesis. This is where context-sensitive
    // phonological rules are applied: syllabic consonant collapsing, flapping, glide insertion,
    // vowel assimilation, DH vowel harmony, and glottal stop insertion.
    //
    // CompiledLetterToSoundRules are applied in priority order. Many rules merge or delete the current phoneme
    // (delFwd=true) or insert an extra phoneme, so the output index (_phonBuf2InIndex) can
    // diverge from the input index (outIdx).
    void AudioProcessor::FillPhonBuf2() {
        _phonBuf2InIndex = 0;
        int16_t lastStoredPhon = _SIL_;
        int16_t lastUserPitch = 0;

        for (int32_t outIdx = 0; outIdx < _phonBuf1InIndex; outIdx++) {
            int16_t  curPhon  = _phonBuf1[outIdx];
            int64_t  curCtrl  = _phonCtrlBuf1[outIdx];
            uint32_t curFlags = Tables::GetFeatureFlags(curPhon);

            // next
            int16_t nextPhon, next2Phon, next3Phon;
            int64_t nextCtrl, next2Ctrl;
            if (outIdx < _phonBuf1InIndex - 1) {
                nextPhon = _phonBuf1[outIdx + 1];
                nextCtrl = _phonCtrlBuf1[outIdx + 1];
            } else {
                nextPhon = _SIL_;
                nextCtrl = 0;
            }
            if (outIdx < _phonBuf1InIndex - 2) {
                next2Phon = _phonBuf1[outIdx + 2];
                next2Ctrl = _phonCtrlBuf1[outIdx + 2];
            } else {
                next2Phon = _SIL_;
                next2Ctrl = 0;
            }
            next3Phon = outIdx < _phonBuf1InIndex - 3 ? _phonBuf1[outIdx + 3] : _SIL_;

            uint32_t nextFlags  = Tables::GetFeatureFlags(nextPhon);
            uint32_t next2Flags = Tables::GetFeatureFlags(next2Phon); (void)next2Flags;

            // prev
            int16_t prevPhon, prev2Phon, prev3Phon;
            int64_t prevCtrl;
            if (outIdx > 0) {
                prevPhon = _phonBuf1[outIdx - 1];
                prevCtrl = _phonCtrlBuf1[outIdx - 1];
            } else {
                prevPhon = _SIL_;
                prevCtrl = 0;
            }
            prev2Phon = outIdx > 1 ? _phonBuf1[outIdx - 2] : _SIL_;
            prev3Phon = outIdx > 2 ? _phonBuf1[outIdx - 3] : _SIL_;

            uint32_t prevFlags  = Tables::GetFeatureFlags(prevPhon);
            uint32_t prev2Flags = Tables::GetFeatureFlags(prev2Phon);
            uint32_t prev3Flags = Tables::GetFeatureFlags(prev3Phon);

            if (_phonBuf2InIndex == 0) {
                lastStoredPhon = _SIL_;
            } else {
                lastStoredPhon = _phonBuf2[_phonBuf2InIndex - 1];
            }
            uint32_t lastPhonFlags = Tables::GetFeatureFlags(lastStoredPhon);

            int16_t  userPitch  = _userPitchBuf1[outIdx];
            int16_t  userDur    = _userDurBuf1[outIdx];
            int16_t  userNote   = _userNoteBuf1[outIdx];
            int16_t  userRate   = _userRateBuf1[outIdx];
            uint8_t  aspiration = _aspirationBuf1[outIdx];
            uint8_t  tilt       = _tiltBuf1[outIdx];
            uint8_t  effort     = _effortBuf1[outIdx];
            uint8_t  vibDepth   = _vibDepthBuf1[outIdx];
            uint8_t  vibRate    = _vibRateBuf1[outIdx];
            uint8_t  tremDepth  = _tremDepthBuf1[outIdx];
            uint8_t  tremRate   = _tremRateBuf1[outIdx];

            int16_t  targetPhon = curPhon;
            bool     delFwd     = false;
            bool     insertGlot = false;

            // EN rule: IX+N -> EN (syllabic N). "button" IX N -> EN, collapses the
            // schwa into the syllabic nasal. Not applied when the stop before IX is B or G,
            // or when the stop is D preceded by a vowel (prevents "hidden" flap context).
            if (curPhon == _N_ && prevPhon == _IX_) {
                if ((prev2Flags & kPlosFricF) != 0 && prev2Phon != _B_ && prev2Phon != _G_) {
                    if (!(prev2Phon == _D_ && (prev3Flags & kVowelF) != 0)) {
                        _phonBuf2[_phonBuf2InIndex - 1] = _EN_;
                        delFwd = true;
                    }
                }
            }

            // EL rule: AX/UH + L -> EL (syllabic L) in unstressed non-word-initial position.
            // "bottle" AX L -> EL; "petal" UH L -> EL.
            if (curPhon == _L_ && (curCtrl & (kPrimOrEmphStress | kWord_Initial_Consonant)) == 0) {
                if (prevPhon == _AX_ || prevPhon == _UH_) {
                    _phonBuf2[_phonBuf2InIndex - 1] = _EL_;
                    delFwd = true;
                    goto STUFF_BUFF;
                }
            }

            // LX / RX rules: post-vocalic L and R in unstressed non-initial position become
            // dark-L (LX) and syllabic-R (RX). The R rule also merges vowel+R into a rhotic
            // vowel (UR, OR, AR, ER, IR, XR) so the resulting segment has a single set of formant targets.
            if ((curCtrl & (kPrimOrEmphStress | kWord_Initial_Consonant)) == 0 &&
                (prevFlags & kVowel1F) != 0) {
                if (curPhon == _L_) {
                    targetPhon = _LX_;
                } else if (curPhon == _R_) {
                    targetPhon = _RX_;
                    if (prevPhon == _UW_ || prevPhon == _UH_) {
                        _phonBuf2[_phonBuf2InIndex - 1] = _UR_; delFwd = true;
                    } else if (prevPhon == _AO_ || prevPhon == _OW_) {
                        _phonBuf2[_phonBuf2InIndex - 1] = _OR_; delFwd = true;
                    } else if (prevPhon == _AA_) {
                        _phonBuf2[_phonBuf2InIndex - 1] = _AR_; delFwd = true;
                    } else if (prevPhon == _AH_ || prevPhon == _AX_) {
                        _phonBuf2[_phonBuf2InIndex - 1] = _ER_; delFwd = true;
                    } else if (prevPhon == _IH_ || prevPhon == _IY_) {
                        _phonBuf2[_phonBuf2InIndex - 1] = _IR_; delFwd = true;
                    } else if (prevPhon == _AE_ || prevPhon == _EH_ || prevPhon == _EY_) {
                        _phonBuf2[_phonBuf2InIndex - 1] = _XR_; delFwd = true;
                    }
                }
            }

            // yUW -> YU rule. Skip the merge when the vowel is stressed so the
            // diphthong trajectory has the full two-phoneme duration to complete.
            if ((prevCtrl & kWord_Initial_Consonant) != 0 && prevPhon == _Y_ &&
                curPhon == _UW_ && nextPhon != _R_ &&
                (curCtrl & kSyllableTypeField) >= kWord_End &&
                (curCtrl & kPrimOrEmphStress) == 0) {
                _phonBuf2[_phonBuf2InIndex - 1] = _YU_;
                _phonCtrlBuf2[_phonBuf2InIndex - 1] = curCtrl;
                delFwd = true;
            }

            // DHAH -> DHIY rule: "the" before a stressed vowel is pronounced "thee" not "thuh".
            if ((nextFlags & kVowelF) != 0 && curPhon == _AH_ &&
                (curCtrl & kSyllableTypeField) != 0 && prevPhon == _DH_ &&
                (prevCtrl & kWord_Initial_Consonant) != 0 &&
                (nextCtrl & kPrimOrEmphStress) != 0) {
                targetPhon = _IY_;
            }

            // EHnd -> AEnd rule: stressed "end" is commonly realized as [AEnd] in connected speech.
            if (curPhon == _SIL_ && nextPhon == _EH_ && next2Phon == _N_ &&
                next3Phon == _D_ && (nextCtrl & kPrimOrEmphStress) != 0) {
                _phonBuf1[outIdx + 1] = _AE_;
                nextPhon  = _AE_;
                nextFlags = Tables::GetFeatureFlags(_AE_);
            }

            // Glottal stop insertion at a stressed vowel-initial word following a vowel-final word
            // (e.g., "see|it" -> see + glot + it). The glottal stop marks a clear syllable
            // boundary and prevents the two vowels from merging into a diphthong.
            if ((curFlags & kVowelF) != 0 && (nextFlags & kVowelF) != 0 &&
                (nextCtrl & kPrimOrEmphStress) != 0 && (curCtrl & kWord_End) != 0) {
                insertGlot = true;
            }

            // D -> JH before YU or Y (yod coalescence): "did you" -> "did-JHoo".
            // Only applies in unstressed context; stressed YU keeps the D separate.
            if ((nextPhon == _YU_ || nextPhon == _Y_) && (nextCtrl & kPrimOrEmphStress) == 0) {
                if (curPhon == _D_) {
                    targetPhon = _JH_;
                    goto STUFF_BUFF;
                }
            }

            // T rules: glottalization and flap selection.
            if (curPhon == _T_) {
                // tUH -> tUW
                if (nextPhon == _UW_ && (nextCtrl & kSyllableTypeField) >= kWord_End &&
                    (curCtrl & kPrimOrEmphStress) == 0 &&
                    (next2Phon == _SIL_ || (Tables::GetFeatureFlags(next2Phon) & kVowelF) != 0)) {
                    _phonBuf1[outIdx + 1] = _UW_;
                } else {
                    // Glottalize t before l or DH
                    if (nextPhon == _L_ || nextPhon == _DH_) {
                        goto SUB_T_GLOT;
                    } else if ((curCtrl & kSyllableTypeField) >= kWord_End) { // At word end before sonorant/h

                        if (((nextFlags & kSonorConsonF) != 0 && nextPhon != _EN_) || nextPhon == _HH_) {
                            goto SUB_T_GLOT;
                        }
                    } else if (nextPhon == _EN_ || (nextPhon == _IX_ && next2Phon == _N_)) {
                        goto SUB_T_GLOT;
                    }
                    goto SKIP_T_GLOT;
                SUB_T_GLOT:
                    targetPhon = (lastPhonFlags & kSonorantF) != 0 ? _TX_ : _D_;
                    goto STUFF_BUFF;
                SKIP_T_GLOT:;
                }
            }

            // Flapping: T or D -> DX (alveolar tap) between a sonorant and a vowel in
            // unstressed position. "butter" T -> DX, "ladder" D -> DX. Not triggered
            // before syllabic N (to avoid collapsing the burst into the nasal onset).
            if (curPhon == _D_ || curPhon == _T_) {
                // Don't flap before syllabic n
                if (nextPhon == _IX_ && next2Phon == _N_) {
                    if (curPhon == _T_) {
                        goto SKIP_FLAP;
                    }
                    if ((prevFlags & kVowelF) == 0) {
                        goto SKIP_FLAP;
                    }
                }

                if ((nextFlags & kVowelF) != 0 &&
                    (lastPhonFlags & kSonorantF) != 0 && (lastPhonFlags & kNasalF) == 0) {
                    if ((nextCtrl & kWord_Start) != 0) {
                        targetPhon = _DX_;
                    } else if ((curCtrl & kPrimOrEmphStress) == 0) {
                        if ((curCtrl & kWord_Initial_Consonant) != 0) {
                            if (nextPhon == _AX_ || nextPhon == _IX_ || nextPhon == _UH_) {
                                targetPhon = _DX_;
                            }
                        } else if (curPhon == _T_) {
                            // T flap rules
                            if (nextPhon == _OW_) {
                                if ((_phonCtrlBuf2[_phonBuf2InIndex - 1] & kStressField) != 0 &&
                                    (next2Phon != _R_ || (next2Ctrl & kWord_Initial_Consonant) != 0)) {
                                    targetPhon = _DX_;
                                }
                            } else if ((nextPhon == _AH_ || nextPhon == _AX_) &&
                                     next2Phon == _R_ && (nextCtrl & kPrimaryStress) == 0) {
                                if ((curCtrl & kWord_Initial_Consonant) == 0 && (nextCtrl & kPrimaryStress) == 0) {
                                    targetPhon = _DX_;
                                }
                            } else if (nextPhon == _ER_) {
                                if ((curCtrl & kWord_Initial_Consonant) == 0) {
                                    targetPhon = _DX_;
                                }
                            } else if ((nextPhon == _AX_ || nextPhon == _IY_ || nextPhon == _IX_ || nextPhon == _EL_) && (next2Phon != _R_ || (next2Ctrl & kWord_Initial_Consonant) != 0) && (nextCtrl & kPrimaryStress) == 0) {
                                targetPhon = _DX_;
                            }
                        } else { // curPhon == _D_
                            if (nextPhon == _OW_) {
                                if ((_phonCtrlBuf2[_phonBuf2InIndex - 1] & kStressField) != 0) {
                                    targetPhon = _DX_;
                                }
                            } else if (nextPhon == _AX_ || nextPhon == _IY_ || nextPhon == _IX_ ||
                                     nextPhon == _EL_ || nextPhon == _ER_ || nextPhon == _IH_ ||
                                     nextPhon == _AH_ || nextPhon == _AA_) {
                                targetPhon = _DX_;
                            }
                        }
                    }
                }
            }
        SKIP_FLAP:

            // DH assimilation: unstressed DH assimilates to the place of an immediately
            // preceding alveolar (T/TX/D -> DD, the voiced dental with nasal release) or
            // nasal (N -> N, absorbed into the nasal). Prevents the DH from producing a
            // distracting dental friction after an alveolar closure.
            if (curPhon == _DH_ && (curCtrl & kPrimaryStress) == 0) {
                if (lastStoredPhon == _T_ || lastStoredPhon == _TX_ || lastStoredPhon == _D_) {
                    targetPhon = _DD_;
                } else if (lastStoredPhon == _N_) {
                    targetPhon = _N_;
                }
            }

            // H deletion (Hertz 1982 rule 9): non-word-initial HH before an unstressed vowel
            // is typically elided in connected speech ("tell him" -> "tell-im").
            if (curPhon == _HH_ && (curCtrl & kWord_Initial_Consonant) == 0
                && (nextFlags & kVowelF) != 0 && (nextCtrl & kPrimOrEmphStress) == 0) {
                delFwd = true;
                goto STUFF_BUFF;
            }

        STUFF_BUFF:
            if (!delFwd) {
                _phonBuf2[_phonBuf2InIndex]       = targetPhon;
                _phonCtrlBuf2[_phonBuf2InIndex]   = curCtrl;
                _userPitchBuf2[_phonBuf2InIndex]  = (int16_t)(userPitch + lastUserPitch);
                _userDurBuf2[_phonBuf2InIndex]    = userDur;
                _userNoteBuf2[_phonBuf2InIndex]   = userNote;
                _userRateBuf2[_phonBuf2InIndex]   = userRate;
                _aspirationBuf2[_phonBuf2InIndex] = aspiration;
                _tiltBuf2[_phonBuf2InIndex]       = tilt;
                _effortBuf2[_phonBuf2InIndex]     = effort;
                _vibDepthBuf2[_phonBuf2InIndex]   = vibDepth;
                _vibRateBuf2[_phonBuf2InIndex]    = vibRate;
                _tremDepthBuf2[_phonBuf2InIndex]  = tremDepth;
                _tremRateBuf2[_phonBuf2InIndex]   = tremRate;

                if (_phonBuf2InIndex < kPhonBuf_Red_Zone) {
                    _phonBuf2InIndex++;
                }

                if (insertGlot) {
                    _phonBuf2[_phonBuf2InIndex]       = _QX_;
                    _phonCtrlBuf2[_phonBuf2InIndex]   = 0;
                    _userPitchBuf2[_phonBuf2InIndex]  = _userPitchBuf2[_phonBuf2InIndex - 1];
                    _userDurBuf2[_phonBuf2InIndex]    = kDur_One;
                    _userNoteBuf2[_phonBuf2InIndex]   = 0;
                    _userRateBuf2[_phonBuf2InIndex]   = 0;
                    _aspirationBuf2[_phonBuf2InIndex] = aspiration;
                    _tiltBuf2[_phonBuf2InIndex]       = tilt;
                    _effortBuf2[_phonBuf2InIndex]     = effort;
                    _vibDepthBuf2[_phonBuf2InIndex]   = vibDepth;
                    _vibRateBuf2[_phonBuf2InIndex]    = vibRate;
                    _tremDepthBuf2[_phonBuf2InIndex]  = tremDepth;
                    _tremRateBuf2[_phonBuf2InIndex]   = tremRate;

                    if (_phonBuf2InIndex < kPhonBuf_Red_Zone) {
                        _phonBuf2InIndex++;
                    }
                }
            } else {
                _userPitchBuf2[_phonBuf2InIndex - 1] += userPitch;
                if (userDur != kDur_One) {
                    _userDurBuf2[_phonBuf2InIndex - 1] = userDur;
                }
                if (userRate != 0) {
                    _userRateBuf2[_phonBuf2InIndex - 1] = userRate;
                }

                if (aspiration > 0) {
                    _aspirationBuf2[_phonBuf2InIndex - 1] = aspiration;
                }
                if (tilt > 0) {
                    _tiltBuf2[_phonBuf2InIndex - 1] = tilt;
                }
                if (effort > 0) {
                    _effortBuf2[_phonBuf2InIndex - 1] = effort;
                }
                if (vibDepth > 0) {
                    _vibDepthBuf2[_phonBuf2InIndex - 1] = vibDepth;
                }
                if (vibRate > 0) {
                    _vibRateBuf2[_phonBuf2InIndex - 1] = vibRate;
                }
                if (tremDepth > 0) {
                    _tremDepthBuf2[_phonBuf2InIndex - 1] = tremDepth;
                }
                if (tremRate > 0) {
                    _tremRateBuf2[_phonBuf2InIndex - 1] = tremRate;
                }

                if ((curCtrl & kSyllable_Start) != 0) {
                    _phonCtrlBuf1[outIdx + 1] |= kSyllable_Start;
                }
            }

            lastUserPitch += userPitch;
        }
    }

    // -------------------------------------------------------------------------
    // Duration model combines Klatt (1976) and Hertz (1982) rules for boundaries, stress, and clusters.
    // -------------------------------------------------------------------------

    void AudioProcessor::ModDuration() {
        _durBuf[0] = 1; // initial SIL = 5ms (1 frame)

        int16_t prevPhon = _SIL_;
        int64_t prevCtrl = 0;
        bool eFlag = false;

        for (int32_t i = 1; i < _phonBuf2InIndex; i++) {
            int16_t  curPhon    = GetPhon2(i);
            int64_t  curCtrl    = GetCtrl2(i);
            int64_t  curSylType = curCtrl & kSyllableTypeField;
            int64_t  curStress  = curCtrl & kStressField;
            uint32_t curFlags   = Tables::GetFeatureFlags(curPhon);
            bool     curIsVowel = (curFlags & kVowelF) != 0;

            prevPhon = GetPhon2(i - 1);
            prevCtrl = GetCtrl2(i - 1);
            uint32_t prevFlags = Tables::GetFeatureFlags(prevPhon);

            int16_t  nextPhon   = GetPhon2(i + 1);
            int64_t  nextCtrl   = GetCtrl2(i + 1);
            uint32_t nextFlags  = Tables::GetFeatureFlags(nextPhon);

            int16_t  next2Phon  = GetPhon2(i + 2);
            int64_t  next2Ctrl  = GetCtrl2(i + 2);
            uint32_t next2Flags = Tables::GetFeatureFlags(next2Phon);

            int32_t percent  = k100pct_Dur;
            int32_t fixedDur = 0;
            int32_t maxDur   = Tables::GetMaximumDuration(curPhon);
            int32_t minDur   = Tables::GetMinimumDuration(curPhon);

            // Singing duration
            if ((curCtrl & kSingingPhon) != 0) {
                if (curPhon == _SIL_ && (curCtrl & kSingingDuration) == 0) {
                    // A SIL inside a singing phoneme group (from an unrecognised phoneme letter) must not insert a full 200 ms pause - give it one frame.
                    _durBuf[i] = 1;
                } else if ((curCtrl & kSingingDuration) != 0) {
                    // Note-timed vowels use exact user duration
                    int32_t dd = _userDurBuf2[i] / kFrameTime;
                    if (dd < 1) {
                        dd = 1;
                    }
                    _durBuf[i] = (int16_t)dd;
                } else {
                    // Consonants before/after the note vowel use minimum duration
                    // so consonants are brief and the vowel sustains the note.
                    int32_t dd = minDur / kFrameTime;
                    if (dd < 1) {
                        dd = 1;
                    }
                    _durBuf[i] = (int16_t)dd;
                }
                goto DURATION_DONE_END;
            }

            // Pause insertion
            if (curPhon == _SIL_ && (curCtrl & kSingingDuration) == 0) {
                int32_t tempS = (int32_t)((curCtrl & kSilenceTypeField) >> kSilenceTypeShift);
                int32_t durHold = tempS != 0
                    ? Tables::BoundaryDurationTable[std::min(tempS, (int32_t)Tables::BoundaryDurationTableLength - 1)]
                    : 200;

                durHold = (int32_t)((durHold * _rateRatio) >> 16);

                if (!_singing && (curCtrl & kSilenceDuration) != 0) {
                    durHold = _userNoteBuf2[i];
                }

                if (durHold < 10) {
                    durHold = 10;
                }
                {
                    int32_t d = (durHold * _userDurBuf2[i]) >> kDurStepRes;
                    d /= kFrameTime;
                    if (curPhon != _SIL_ && d < 8 / kFrameTime) {
                        d = 8 / kFrameTime;
                    }
                    _durBuf[i] = (int16_t)std::max(d, 1);
                    goto DURATION_DONE_END;
                }
            }

            // Clause-final lengthening (Klatt 1976, Table III Rule 4).
            if ((curSylType & kTerm_End) != 0) {
                if ((curFlags & kStopF) != 0) {
                    fixedDur = 0;
                } else if ((curFlags & kVoicedF) != 0 && (curFlags & kFric) != 0) {
                    fixedDur = 20;
                } else if ((curFlags & kVocLiq) != 0 && (nextFlags & kPlosFricF) != 0 && (nextFlags & kVoicedF) == 0) {
                    fixedDur = 15;
                } else {
                    fixedDur = 40;
                }

                if ((nextFlags & kSonorantF) != 0) {
                    fixedDur -= 20;
                }

                if (_phonBuf2InIndex < 10 && curStress != 0 && curIsVowel) {
                    fixedDur += (10 - _phonBuf2InIndex) * 5;
                }
            }

            if (curIsVowel) {
                // Non-phrase-final shortening (Klatt 1976, Table II Rule 2, K=0.60).
                if (curSylType < kVerb_End) {
                    percent = (int32_t)((int64_t)percent * 60 * pct >> 16);
                }

                // Non-word-final shortening (Klatt 1976, Table II Rule 3): non-final syllables
                // in polysyllabic words only. Monosyllabic words are word-final by definition
                // so the rule does not apply; unstressed shortening below handles them instead.
                if ((curCtrl & kMore_Than_One_Syllable_In_Word) != 0 && (curSylType & kSyllableTypeField) < kWord_End && (curStress & kPrimOrEmphStress) == 0) {
                    if ((curCtrl & kSyllableOrderField) <= kFirst_Syllable_In_Word) {
                        percent = (int32_t)((int64_t)percent * 85 * pct >> 16);
                    } else {
                        percent = (int32_t)((int64_t)percent * 80 * pct >> 16);
                    }
                }

                // Polysyllabic shortening (Klatt 1976, Table II Rule 4, K=0.78).
                if ((curCtrl & kMore_Than_One_Syllable_In_Word) != 0) {
                    percent = (int32_t)((int64_t)percent * 78 * pct >> 16);
                }
            }

            // Non-word-initial consonant shortening (Klatt 1976, Table III Rule 1, K=0.70).
            if (!curIsVowel && (curCtrl & kWord_Initial_Consonant) == 0) {
                if ((curFlags & kFric) != 0 && (curSylType & kWord_End) != 0) {
                    fixedDur += 20;
                } else {
                    percent = (int32_t)((int64_t)percent * 70 * pct >> 16);
                }
            }

            // Unstressed shortening
            if ((curStress & kPrimOrEmphStress) == 0) {
                if ((curFlags & kPlosFricF) == 0 && (curFlags & kGStopF) == 0) {
                    minDur -= minDur >> 2;
                }

                if (curIsVowel) {
                    if ((curCtrl & kSyllableOrderField) == kMid_Syllable_In_Word) {
                        percent = (int32_t)((int64_t)percent * 55 * pct >> 16);
                    } else {
                        percent = (int32_t)((int64_t)percent * 70 * pct >> 16);
                    }
                } else {
                    if (curPhon >= _W_ && curPhon <= _L_) {
                        percent = (int32_t)((int64_t)percent * 60 * pct >> 16);
                    } else {
                        percent = (int32_t)((int64_t)percent * 70 * pct >> 16);
                    }
                }
            }

            // Emphatic lengthening
            if ((curCtrl & kWord_Initial_Consonant) != 0 || (curIsVowel && curStress != kEmphaticStress)) {
                eFlag = false;
            }
            if (curStress == kEmphaticStress) {
                eFlag = true;
            }
            if (eFlag) {
                fixedDur += curIsVowel ? 60 : 20;
            }

            // Postvocalic context (House & Fairbanks 1953, JASA 25:105; Chen 1970, Phonetica 22:129; Hertz 1982 rules 14/15)
            {
                bool  vocFlag   = false;
                int16_t theObstr = _SIL_;
                int64_t num1    = k100percent;

                if (curIsVowel ||
                    (((curFlags & kVocLiq) != 0 || (curFlags & kNasalF) != 0) &&
                     (curCtrl & kStressedWInitial) == 0 && (nextFlags & kPlosFricF) != 0)) {
                    if ((nextFlags & kVowelF) == 0 && (nextCtrl & kStressedWInitial) == 0) {
                        theObstr = nextPhon;
                        if (((nextFlags & kVocLiq) != 0 || (nextFlags & kNasalF) != 0) &&
                            (next2Ctrl & kStressedWInitial) == 0 && (next2Flags & kPlosFricF) != 0) {
                            vocFlag  = true;
                            theObstr = next2Phon;
                        }

                        if (theObstr != _SIL_) {
                            uint32_t obFlags = Tables::GetFeatureFlags(theObstr);
                            if ((obFlags & kVoicedF) == 0) {
                                fixedDur -= fixedDur >> 1;
                                num1 = k1pct * 80;
                                if ((obFlags & (kStopF | kAffricateF)) != 0) {
                                    num1 = k1pct * 55;
                                }
                            } else if ((obFlags & kPlosFricF) != 0) {
                                // Hertz 1982 rules 14/15: pre-fricative 135%, pre-voiced stop 130%
                                num1 = (obFlags & kFric) != 0 ? k1pct * 135 : k1pct * 130;
                                if ((obFlags & kStopF) == 0 && theObstr != _DX_ && (curCtrl & kPrimOrEmphStress) != 0) {
                                    fixedDur += 25;
                                }
                            } else if ((obFlags & kNasalF) != 0) {
                                num1 = k1pct * 85;
                            }
                            // Hertz 1982 (.20)/(.80) windowing: voicing-context scaling applies only to the segment nucleus.
                            percent = (int32_t)((2 * k100pct_Dur + 3 * percent) / 5);
                        }
                    }

                    if (curSylType < kTerm_End || vocFlag) {
                        num1 = (num1 >> 1) + kOneHalf;
                    }

                    percent = (int32_t)((int64_t)percent * num1 >> 16);
                }
            }

            // Cluster shortening
            if (curIsVowel) {
                if ((nextFlags & kVowelF) != 0) {
                    fixedDur += 30;
                }
                if ((curCtrl & kSyllableOrderField) == kFirst_Syllable_In_Word &&
                    (curCtrl & kPrimOrEmphStress) != 0 &&
                    (prevCtrl & kWord_Initial_Consonant) == 0) {
                    fixedDur += 25;
                }
                if (nextPhon == _LX_) {
                    fixedDur -= 20;
                }
            } else if ((curFlags & kConsonantF) != 0) {
                if ((nextFlags & kConsonantF) != 0 && curSylType < kTerm_End) {
                    int64_t num1 = k1pct * 55;
                    if ((curFlags & kNasalF) != 0 && (nextCtrl & kWord_Initial_Consonant) != 0) {
                        num1 = k1pct * 150;
                    }
                    minDur -= minDur >> 2;
                    if (curPhon == _S_ || curPhon == _TH_) {
                        if ((nextFlags & kStopF) != 0) {
                            num1 = k1pct * 50;
                        }
                        if (nextPhon == _SH_) {
                            int32_t dh = 12;
                            int32_t dd = (dh * _userDurBuf2[i]) >> kDurStepRes;
                            dd /= kFrameTime;
                            if (dd < 1) {
                                dd = 1;
                            }
                            _durBuf[i] = (int16_t)dd;
                            goto DURATION_DONE_END;
                        }
                    }
                    percent = (int32_t)((int64_t)percent * num1 >> 16);
                }

                if ((prevFlags & kConsonantF) != 0) {
                    int64_t num1 = k1pct * 55;
                    minDur -= minDur >> 2;
                    if ((curFlags & kStopF) != 0) {
                        if (prevPhon == _S_) {
                            num1 = k1pct * 60;
                        } else if ((prevFlags & kNasalF) != 0 && curStress == 0) {
                            num1 = k1pct * 10;
                        }
                    }
                    percent = (int32_t)((int64_t)percent * num1 >> 16);
                }
            }

            // Plosive aspiration lengthening (Hertz 1991, sec.2.2: aspiration aligns with the CV transition).
            if ((curFlags & kSonorantF) != 0 && (prevFlags & kVoicedF) == 0 && (prevFlags & kStopF) != 0) {
                fixedDur += 20;
            }

            // Glide lengthening
            if ((curFlags & kVowel1F) != 0 && (prevFlags & kSonorConsonF) != 0 && (prevFlags & kNasalF) == 0) {
                if (fixedDur == 0) {
                    fixedDur = 20;
                }
            }

            // Short phrase lengthening
            if (_phonBuf2InIndex < 10 && minDur != maxDur) {
                fixedDur += (5 - (_phonBuf2InIndex >> 1)) * kFrameTime;
            }

            // Pronoun emphasis lengthening (VocalConfidence, VoiceData).
            // Applies only to the vowel portion so consonant onsets stay crisp.
            if (_vocalConfidence > 0 && (curCtrl & kPronounWord) != 0 && curIsVowel) {
                fixedDur += _vocalConfidence * 40 / 100;
            }

            // Rate change (Klatt 1979).
            {
                int16_t rateVal = _userRateBuf2[i];
                if (rateVal != 0) {
                    _speechRate = rateVal;
                    InitRateParams();
                }
            }

            {
                // Incompressibility (Klatt 1976, eq. 1): D_j = K*(D_i - D_min) + D_min,
                // where D_min is the minimum physiologically achievable duration.
                int32_t durHold = (percent * (maxDur - minDur) >> 7) + minDur;
                if (_speechRate != kNormal_Speech_Rate && durHold != 0) {
                    durHold  = (int32_t)((durHold * _rateRatioLowGain) >> 16);
                    fixedDur = (int32_t)((fixedDur * _rateRatio) >> 16);
                }
                durHold += fixedDur;

                int32_t d = (durHold * _userDurBuf2[i]) >> kDurStepRes;
                d /= kFrameTime;
                if (curPhon != _SIL_ && d < 8 / kFrameTime) {
                    d = 1;
                }
                _durBuf[i] = (int16_t)std::max(d, 1);
            }

        DURATION_DONE_END:;
        }
    }

    // -------------------------------------------------------------------------
    // Taylor (2000) Tilt model synthesis.
    // Each intonational event is described by three parameters:
    //   amplitude  - total F0 excursion in pitch units (A_event)
    //   tiltX64    - tilt * 64, range [-64, +64]; +64 = pure rise, -64 = pure fall
    //   duration   - total event duration in frames
    //
    // These are converted to RFC components (eqs 8-11) and then to F0 via eq. 12
    // in PitchInterpolator.TiltSynth.
    // -------------------------------------------------------------------------

    // Assigns rise/fall pitch markers to vowels in the phoneme buffer.
    // The first stressed vowel in the clause starts the nuclear rise.
    // The last stressed vowel (or final vowel if none stressed) gets the nuclear fall.
    // Content-word vowels in the head get kPitchRise1; function words get kPitchFall1.
    // These markers are consumed by FillPitchBuf to place Tilt events.
    void AudioProcessor::PitchRaiseAndFall() {
        const int32_t kFallen = 0, kRaised = 1, kStart = 2, kFinished = 3;

        int32_t pState = kStart, lastState = kStart;
        int32_t wdIndex = 0, firstWord = 0, lastWord = 0;
        int64_t wdType[64];
        int32_t stressCount = 1;

        for (int32_t index = 0; index < _phonBuf2InIndex; index++) {
            int16_t  curPhon  = _phonBuf2[index];
            int64_t  curCtrl  = _phonCtrlBuf2[index];
            uint32_t curFlags = Tables::GetFeatureFlags(curPhon);

            if ((curCtrl & kSilenceTypeField) != 0) {
                pState = kStart;
                wdIndex = 0; firstWord = 0; lastWord = 0; stressCount = 1;
                continue;
            }

            if (pState == kRaised && (curCtrl & kBoundryTypeField) == kWord_Start) {
                wdType[wdIndex] = (curCtrl & kContent_Word) != 0 ? kPitchRise1 : kPitchFall1;
                if (wdIndex < 63) {
                    wdIndex++;
                }
                stressCount = 0;
                lastWord = index;
                if (lastState == kStart && pState == kRaised) {
                    lastState = kRaised;
                    firstWord = index;
                }
            }

            if ((curFlags & kVowelF) != 0) {
                if (pState == kStart) {
                    if (CountVowelsTillBoundry(kTerm_End, index) == 0) {
                        if (_endPunctuation != _Comma_) {
                            _phonCtrlBuf2[index] |= kPitchFall;
                        }
                        pState = kFinished;
                        break;
                    } else if (CountStressVowelsTillBoundry(kTerm_End, index) == 0) {
                        if (_endPunctuation != _Comma_) {
                            _phonCtrlBuf2[index] |= kPitchFall;
                        }
                        pState = kFinished;
                    } else if ((curCtrl & kIsStressed) != 0) {
                        _phonCtrlBuf2[index] |= kPitchRise;
                        pState = kRaised;
                    }
                } else if (pState == kRaised) {
                    if ((curCtrl & kPrimOrEmphStress) != 0) {
                        stressCount++;
                    }

                    if (CountVowelsTillBoundry(kTerm_End, index) == 0) {
                        if (_endPunctuation != _Comma_) {
                            _phonCtrlBuf2[index] |= kPitchFall;
                        }
                        pState = kFallen;
                    } else if ((curCtrl & kPrimOrEmphStress) != 0 &&
                             CountStressVowelsTillBoundry(kTerm_End, index) == 0) {
                        if (_endPunctuation != _Comma_) {
                            _phonCtrlBuf2[index] |= kPitchFall;
                        }
                        pState = kFallen;
                    }
                }
            }
        }

        wdIndex -= 2;
        if (wdIndex >= 1 && pState != kFinished) {
            pState = kFallen;
            for (int32_t i = 0; i < wdIndex; i++) {
                if (pState == kFallen) {
                    wdType[i] = kPitchRise1;
                    pState = kRaised;
                } else {
                    wdType[i] = kPitchFall1;
                    pState = kFallen;
                }
            }
            if (pState == kRaised) {
                wdType[wdIndex] = kPitchFall1;
                wdIndex++;
            }

            bool action = false;
            int32_t wi = 0;
            for (int32_t index = firstWord; index < lastWord; index++) {
                int16_t  curPhon  = _phonBuf2[index];
                int64_t  curCtrl  = _phonCtrlBuf2[index];
                uint32_t curFlags = Tables::GetFeatureFlags(curPhon);

                if ((curCtrl & kBoundryTypeField) == kWord_Start) {
                    action = true;
                }

                if ((curFlags & kVowelF) != 0 && action) {
                    if (!AnyStressVowelsRemain(index)) {
                        action = false;
                        if (wi < wdIndex) {
                            _phonCtrlBuf2[index] |= wdType[wi];
                        }
                        wi++;
                    }
                }
            }
        }
    }

    int32_t AudioProcessor::CountVowelsTillBoundry(int64_t boundary, int32_t curIndex) {
        int32_t count = 0;
        for (int32_t i = curIndex; i < _phonBuf2InIndex; i++) {
            if (i != curIndex && (PhonemeFeatureFlagsSafe(_phonBuf2[i]) & kVowelF) != 0) {
                count++;
            }
            if ((_phonCtrlBuf2[i] & kSyllableTypeField) >= boundary) {
                break;
            }
        }
        return count;
    }

    int32_t AudioProcessor::CountStressVowelsTillBoundry(int64_t boundary, int32_t curIndex) {
        int32_t count = 0;
        for (int32_t i = curIndex; i < _phonBuf2InIndex; i++) {
            if (i != curIndex &&
                (_phonCtrlBuf2[i] & kPrimOrEmphStress) != 0 &&
                (PhonemeFeatureFlagsSafe(_phonBuf2[i]) & kVowelF) != 0) {
                count++;
            }
            if ((_phonCtrlBuf2[i] & kSyllableTypeField) >= boundary) {
                break;
            }
        }
        return count;
    }

    bool AudioProcessor::AnyStressVowelsRemain(int32_t curIndex) {
        for (int32_t i = curIndex + 1; i < _phonBuf2InIndex; i++) {
            if ((_phonCtrlBuf2[i] & kBoundryTypeField) == kWord_Start) {
                break;
            }
            if ((_phonCtrlBuf2[i] & kPrimOrEmphStress) != 0 &&
                (PhonemeFeatureFlagsSafe(_phonBuf2[i]) & kVowelF) != 0) {
                return true;
            }
        }
        return false;
    }

    uint32_t AudioProcessor::PhonemeFeatureFlagsSafe(int16_t p) {
        return Tables::GetFeatureFlags(p);
    }

    // -------------------------------------------------------------------------
    // Calc_Ramp_Steps
    // -------------------------------------------------------------------------

    void AudioProcessor::CalcRampSteps() {
        const int32_t kRampMode = 0;
        int32_t rampIndex = 0, mode = kRampMode, accum = 1;

        for (int32_t i = 0; i < _phonBuf2InIndex; i++) {
            int64_t curCtrl    = GetCtrl2(i);
            int64_t curSylType = curCtrl & kSyllableTypeField;
            int16_t curDur     = _durBuf[i];

            if (mode == kRampMode) {
                if ((curCtrl & kSilenceTypeField) != 0 || (curSylType & kTerm_End) != 0) {
                    int64_t step = ((int64_t)(_baselineFallStart - _baselineFallEnd) << 16) / accum;
                    if ((curSylType & kTerm_End) != 0) {
                        if (_endPunctuation == _Comma_ || _endPunctuation == _Quest_ ||
                            _endPunctuation == _Tilde_ || _endPunctuation == _Ellipsis_) {
                            step >>= 1;
                        }
                    }
                    if (rampIndex < kMaxRamps) {
                        _rampSteps[rampIndex++] = step;
                    }
                    accum = 1;
                } else {
                    accum += curDur;
                }
            }
        }

        _curRamp = 0;
    }

    // Fill_Pitch_Buf - Taylor (2000) Tilt model.
    //
    // Events emitted into the pitch buffer carry (amplitude, tiltX64, duration) so that
    // PitchInterpolator can synthesize the parabolic F0 contour for each event.
    //
    // Nuclear accent (kPitchRise / kPitchFall):
    //   The vowel bearing kPitchRise starts the nuclear accent (rise component).
    //   The vowel bearing kPitchFall completes it (fall component).
    //   Together they form one Tilt event with the rise on the way up and the fall on the way down.
    //
    // Pre-nuclear head (kPitchRise1 / kPitchFall1):
    //   Smaller Tilt events on stressed vowels in the pre-nuclear region.
    //
    // Stress accent (kPrimOrEmphStress):
    //   Upward Tilt excursion at each stressed syllable.
    //
    // Boundary tones (at kTerm_End):
    //   Rising or falling boundary, implemented as a Tilt event with high |tilt|.

    void AudioProcessor::FillPitchBuf() {
        bool pitchIsFallen = true;
        _pitchBufInIndex  = 0;
        int32_t stressCounter = 0;
        int32_t curBaseline   = 0;
        _pitchTimeOffset = 0;
        _lastEventTime   = 0;

        for (int32_t i = 0; i < _phonBuf2InIndex; i++) {
            int16_t  curPhon    = GetPhon2(i);
            int64_t  curCtrl    = GetCtrl2(i);
            uint32_t curFlags   = Tables::GetFeatureFlags(curPhon);
            int64_t  curStress  = curCtrl & kStressField;
            int64_t  curSylType = curCtrl & kSyllableTypeField;
            int16_t  curDur     = _durBuf[i];

            int64_t prevCtrl = GetCtrl2(i - 1);

            // Phrase reset after silence - resets the baseline accumulator in PitchInterpolator.
            if (((prevCtrl & kSilenceTypeField) >> kSilenceTypeShift) != 0) {
                int16_t resetAmt = (int16_t)((0 - curBaseline) * _vpBreakStrength / 50);
                StoreTiltEvent(resetAmt, 0, 0, 0, kPhraseReset);
                curBaseline += resetAmt;
                pitchIsFallen = true;
            }

            if ((curFlags & kVowelF) != 0) {
                // NUCLEAR RISE - begin of nuclear accent
                if ((curCtrl & kPitchRise) != 0 && pitchIsFallen) {
                    int16_t riseAmt = _vpRiseAmt;
                    if (_endPunctuation == _Quest_ || _endPunctuation == _Tilde_) {
                        riseAmt >>= 1;
                    }
                    // Pure-rise event (tilt = +64): only the rise component fires here;
                    // the matching fall fires at the kPitchFall vowel below.
                    int16_t timeT = (curCtrl & kPitchFall) != 0
                        ? (int16_t)((-80) / kFrameTime)
                        : (int16_t)0;
                    StoreTiltEvent(riseAmt, +64, curDur, timeT, kPitchRiseFall_Flg);
                    curBaseline += riseAmt;
                    pitchIsFallen = false;
                }

                // PRE-NUCLEAR HEAD (kPitchRise1 / kPitchFall1)
                if ((curCtrl & kPitchRise1) != 0) {
                    int16_t raiseAmt1 = _vpRiseAmt1;
                    if (_endPunctuation == _Quest_ || _endPunctuation == _Tilde_) {
                        raiseAmt1 >>= 1;
                    }
                    // Slight rise-fall shape: tilt +32 (mild rise dominance)
                    StoreTiltEvent(raiseAmt1, +32, curDur, 0, kPitchRiseFall1_Flg);
                } else if ((curCtrl & kPitchFall1) != 0) {
                    int16_t fallAmt1 = _vpFallAmt1;
                    // Slight fall shape: tilt -32
                    StoreTiltEvent(fallAmt1, -32, curDur, 0, kPitchRiseFall1_Flg);
                }

                // STRESS ACCENT (primary or emphatic)
                if ((curStress & kPrimOrEmphStress) != 0) {
                    int16_t pitchT;
                    if (curStress == kEmphaticStress) {
                        pitchT = (int16_t)(kHZ_28 + (_vpEmphasisBoost * kHZ_14 / 100));
                    } else {
                        pitchT = kHZ_14;
                    }

                    switch (stressCounter) {
                        case 0:  pitchT += kHZ_10; break;
                        case 1:  pitchT += kHZ_9;  break;
                        case 2:  pitchT += kHZ_6;  break;
                        case 3:  pitchT += kHZ_4;  break;
                        default: break;
                    }

                    if (_endPunctuation == _Quest_ || _endPunctuation == _Tilde_) {
                        pitchT >>= 1;
                    }

                    int16_t timeT;
                    if ((curCtrl & kPitchFall) != 0 || (curSylType & kTerm_End) != 0) {
                        timeT = (int16_t)((-60) / kFrameTime);
                    } else if (curStress == kEmphaticStress) {
                        timeT = 0;
                    } else {
                        timeT = (int16_t)(curDur * (25 + _vpStressEarly / 2) / 100);
                    }

                    pitchT = (int16_t)((_vpStressGain * pitchT) >> 16);

                    if ((curSylType & kTerm_End) != 0 && curStress != kEmphaticStress) {
                        pitchT = (int16_t)(0 - kHZ_4);
                    }

                    // Stress accent: brief rise-fall, tilt 0 (symmetric shape), amplitude pitchT
                    StoreTiltEvent(pitchT, 0, curDur, timeT, kPitchStress_Flg);
                    stressCounter++;
                }

                // NUCLEAR FALL
                if ((curCtrl & kPitchFall) != 0) {
                    int16_t timeT = (int16_t)(curDur - (160 / kFrameTime));
                    if (timeT < 25 / kFrameTime) {
                        timeT = (int16_t)(25 / kFrameTime);
                    }

                    int16_t fallAmt;
                    if ((curSylType & kTerm_End) != 0) {
                        if (_endPunctuation == _Comma_) {
                            fallAmt = (int16_t)0;
                        } else if (_endPunctuation == _Period_) {
                            fallAmt = (int16_t)(0 - kHZ_20);
                        } else if (_endPunctuation == _Quest_) {
                            fallAmt = (int16_t)(0 - kHZ_7);
                        } else if (_endPunctuation == _Exclam_) {
                            fallAmt = (int16_t)(0 - kHZ_20);
                        } else if (_endPunctuation == _Tilde_) {
                            fallAmt = (int16_t)(0 - kHZ_4);
                        } else if (_endPunctuation == _Ellipsis_) {
                            fallAmt = (int16_t)(0 - kHZ_14);
                        } else {
                            fallAmt = (int16_t)(0 - kHZ_12);
                        }
                        if (_endPunctuation == _Period_ || _endPunctuation == _Exclam_) {
                            fallAmt += (int16_t)(_vpUptalkAmt * (kHZ_20 + kHZ_4) / 100);
                        }
                    } else if ((curSylType & kVerb_End) != 0) {
                        fallAmt = 0;
                    } else {
                        fallAmt = _vpFallAmt;
                    }

                    fallAmt = (int16_t)(((int64_t)_vpAssertiveness * fallAmt >> 16) - _vpRiseAmt);

                    // Pure-fall event (tilt = -64)
                    StoreTiltEvent((int16_t)(-fallAmt), -64, curDur, timeT, kPitchRiseFall_Flg);
                    curBaseline += fallAmt;
                    pitchIsFallen = true;
                }

                // PRONOUN ACCENT: mild rise-fall on pronoun vowels, scaled by VocalConfidence.
                // Fires regardless of stress - confident speakers mark I/you/he/she/it/we/they
                // with a subtle peak even when unstressed.
                if (_vocalConfidence > 0 && (curCtrl & kPronounWord) != 0) {
                    int16_t pronounAmt = (int16_t)(kHZ_10 * _vocalConfidence / 100);
                    if (pronounAmt > 0) {
                        StoreTiltEvent(pronounAmt, 0, curDur, (int16_t)(curDur / 2), kPitchStress_Flg);
                    }
                }

                // BOUNDARY TONES (question, tilde, comma).
                // Stored as pitch-offset targets (kPitchBoundry_Flg), not Tilt excursions.
                // PitchInterpolator accumulates them into _punctOffset and adds the offset
                // directly to the pitch target - the same mechanism as the original IIR path.
                // Multiple events per clause let the offset ramp across the vowel duration;
                // the last event before the boundary wins and persists until phrase reset.
                if ((curSylType & kTerm_End) != 0 &&
                    (_endPunctuation == _Comma_ || _endPunctuation == _Quest_ || _endPunctuation == _Tilde_)) {
                    if (_endPunctuation == _Quest_) {
                        StoreTiltEvent(kHZ_18, 0, 0, 0, kPitchBoundry_Flg);
                        StoreTiltEvent(kHZ_25, 0, 0, curDur, kPitchBoundry_Flg);
                    } else if (_endPunctuation == _Tilde_) {
                        StoreTiltEvent(kHZ_7, 0, 0, 0, kPitchBoundry_Flg);
                        StoreTiltEvent(kHZ_18, 0, 0, (int16_t)(curDur * 2 / 5), kPitchBoundry_Flg);
                        StoreTiltEvent(kHZ_4, 0, 0, curDur, kPitchBoundry_Flg);
                    } else {
                        StoreTiltEvent(kHZ_10, 0, 0, 0, kPitchBoundry_Flg);
                        StoreTiltEvent(kHZ_20, 0, 0, curDur, kPitchBoundry_Flg);
                    }
                }
            }

            _pitchTimeOffset += curDur;
        }
    }

    // Stores one Tilt event into the pitch buffer.
    //
    // amplitude: A_event in pitch units (positive = excursion upward from baseline)
    // tiltX64:   tilt parameter * 64; +64 = pure rise, -64 = pure fall, 0 = symmetric
    // duration:  total event duration in frames
    // time:      frame offset relative to current phoneme start (may be negative for early placement)
    // flags:     kPitchRiseFall_Flg, kPitchStress_Flg, kPitchBoundry_Flg, kPhraseReset, etc.
    void AudioProcessor::StoreTiltEvent(int16_t amplitude, int16_t tiltX64, int32_t duration,
                                        int16_t time, int16_t flags) {
        int32_t absTime = _pitchTimeOffset + time;
        int32_t relTime = absTime - _lastEventTime;
        _pitchBufTime[_pitchBufInIndex]     = (int16_t)(relTime >= 0 ? relTime : 0);
        _lastEventTime                      = absTime;
        _pitchBufFreq[_pitchBufInIndex]     = amplitude;
        _pitchBufTiltX64[_pitchBufInIndex]  = tiltX64;
        _pitchBufDuration[_pitchBufInIndex] = (int16_t)std::min(duration, (int32_t)INT16_MAX);
        _pitchBufFlags[_pitchBufInIndex]    = flags;
        if (_pitchBufInIndex < kPitchBufSize - 1) {
            _pitchBufInIndex++;
        }
    }

    // -------------------------------------------------------------------------
    // StartNew_PitchClause
    // -------------------------------------------------------------------------

    void AudioProcessor::StartNewPitchClause() {
        _baselineStartOffset = _baselineFallStart;
        _baselineEndOffset   = _baselineFallEnd;
    }

    void AudioProcessor::StretchLastWordForTilde() {
        int32_t pct = _endPunctuation == _Tilde_    ? 110
                    : _endPunctuation == _Ellipsis_ ? 125
                    : 0;
        if (pct == 0) {
            return;
        }

        int32_t end = _phonBuf2InIndex - 1;
        while (end > 0 && _phonBuf2[end] == _SIL_) {
            end--;
        }

        int32_t start = 0;
        for (int32_t i = end; i >= 0; i--) {
            if ((_phonCtrlBuf2[i] & kBoundryTypeField) == kWord_Start) {
                start = i;
                break;
            }
        }

        for (int32_t i = start; i <= end; i++) {
            _durBuf[i] = (int16_t)std::max(1, (_durBuf[i] * pct + 50) / 100);
        }
    }

    void AudioProcessor::InsertPlosiveRelease() {
        if (_singing) {
            return;
        }
        for (int32_t i = 0; i < _phonBuf2InIndex; i++) {
            int16_t cur  = _phonBuf2[i];
            int16_t next = i + 1 < _phonBuf2InIndex ? _phonBuf2[i + 1] : _SIL_;
            if (next != _SIL_) {
                continue;
            }

            uint32_t curFlags = Tables::GetFeatureFlags(cur);
            if ((curFlags & kHasReleaseF) == 0) {
                continue;
            }
            if (_phonBuf2InIndex >= kPhonBuf_Red_Zone) {
                break;
            }

            for (int32_t k = _phonBuf2InIndex; k > i + 1; k--) {
                _phonBuf2[k]       = _phonBuf2[k - 1];
                _phonCtrlBuf2[k]   = _phonCtrlBuf2[k - 1];
                _durBuf[k]         = _durBuf[k - 1];
                _userPitchBuf2[k]  = _userPitchBuf2[k - 1];
                _userDurBuf2[k]    = _userDurBuf2[k - 1];
                _userNoteBuf2[k]   = _userNoteBuf2[k - 1];
                _userRateBuf2[k]   = _userRateBuf2[k - 1];
            }
            _phonBuf2InIndex++;

            int16_t  prevPhon  = i > 0 ? _phonBuf2[i - 1] : _SIL_;
            uint32_t prevFlags = Tables::GetFeatureFlags(prevPhon);
            bool     useIX     = (cur == _T_ || cur == _D_) || ((prevFlags & kFrontF_BE) != 0);
            _phonBuf2[i + 1]       = useIX ? _IX_ : _AX_;
            _phonCtrlBuf2[i + 1]   = _phonCtrlBuf2[i] | kPlosive_Release;
            _durBuf[i + 1]         = 25 / kFrameTime;
            _userPitchBuf2[i + 1]  = _userPitchBuf2[i];
            _userDurBuf2[i + 1]    = kDur_One;
            _userNoteBuf2[i + 1]   = 0;
            _userRateBuf2[i + 1]   = 0;

            i++;
        }
    }

}  // namespace SharpTalk
