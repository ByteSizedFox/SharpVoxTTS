#nullable enable
using System;

namespace SharpTalk {

    // Taylor (2000) Tilt model synthesis.
    // Each intonational event is described by three parameters:
    //   amplitude  - total F0 excursion in pitch units (A_event)
    //   tiltX64    - tilt * 64, range [-64, +64]; +64 = pure rise, -64 = pure fall
    //   duration   - total event duration in frames
    //
    // These are converted to RFC components (eqs 8-11) and then to F0 via eq. 12
    // in PitchInterpolator.TiltSynth.

    public sealed partial class AudioProcessor {
        // Assigns rise/fall pitch markers to vowels in the phoneme buffer.
        // The first stressed vowel in the clause starts the nuclear rise.
        // The last stressed vowel (or final vowel if none stressed) gets the nuclear fall.
        // Content-word vowels in the head get kPitchRise1; function words get kPitchFall1.
        // These markers are consumed by FillPitchBuf to place Tilt events.
        private void PitchRaiseAndFall() {
            const int kFallen = 0, kRaised = 1, kStart = 2, kFinished = 3;

            int pState = kStart, lastState = kStart;
            int wdIndex = 0, firstWord = 0, lastWord = 0;
            long[] wdType = new long[64];
            int stressCount = 1;

            for (int index = 0; index < _phonBuf2InIndex; index++) {
                short curPhon = _phonBuf2[index];
                long curCtrl = _phonCtrlBuf2[index];
                uint curFlags = Tables.GetFeatureFlags(curPhon);

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
                for (int i = 0; i < wdIndex; i++) {
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
                int wi = 0;
                for (int index = firstWord; index < lastWord; index++) {
                    short curPhon = _phonBuf2[index];
                    long curCtrl = _phonCtrlBuf2[index];
                    uint curFlags = Tables.GetFeatureFlags(curPhon);

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

        private int CountVowelsTillBoundry(long boundary, int curIndex) {
            int count = 0;
            for (int i = curIndex; i < _phonBuf2InIndex; i++) {
                if (i != curIndex && (PhonemeFeatureFlagsSafe(_phonBuf2[i]) & kVowelF) != 0) {
                    count++;
                }
                if ((_phonCtrlBuf2[i] & kSyllableTypeField) >= boundary) {
                    break;
                }
            }
            return count;
        }

        private int CountStressVowelsTillBoundry(long boundary, int curIndex) {
            int count = 0;
            for (int i = curIndex; i < _phonBuf2InIndex; i++) {
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

        private bool AnyStressVowelsRemain(int curIndex) {
            for (int i = curIndex + 1; i < _phonBuf2InIndex; i++) {
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

        static uint PhonemeFeatureFlagsSafe(short p) => Tables.GetFeatureFlags(p);

        // Calc_Ramp_Steps

        private void CalcRampSteps() {
            const int kRampMode = 0;
            int rampIndex = 0, mode = kRampMode, accum = 1;

            for (int i = 0; i < _phonBuf2InIndex; i++) {
                long curCtrl = GetCtrl2(i);
                long curSylType = curCtrl & kSyllableTypeField;
                short curDur = _durBuf[i];

                if (mode == kRampMode) {
                    if ((curCtrl & kSilenceTypeField) != 0 || (curSylType & kTerm_End) != 0) {
                        long step = ((long)(_baselineFallStart - _baselineFallEnd) << 16) / accum;
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

        private void FillPitchBuf() {
            bool pitchIsFallen = true;
            _pitchBufInIndex = 0;
            int stressCounter = 0;
            int curBaseline = 0;
            _pitchTimeOffset = 0;
            _lastEventTime = 0;

            for (int i = 0; i < _phonBuf2InIndex; i++) {
                short curPhon = GetPhon2(i);
                long curCtrl = GetCtrl2(i);
                uint curFlags = Tables.GetFeatureFlags(curPhon);
                long curStress = curCtrl & kStressField;
                long curSylType = curCtrl & kSyllableTypeField;
                short curDur = _durBuf[i];

                long prevCtrl = GetCtrl2(i - 1);

                // Phrase reset after silence - resets the baseline accumulator in PitchInterpolator.
                if (((prevCtrl & kSilenceTypeField) >> kSilenceTypeShift) != 0) {
                    short resetAmt = (short)((0 - curBaseline) * _vpBreakStrength / 50);
                    StoreTiltEvent(resetAmt, 0, 0, 0, kPhraseReset);
                    curBaseline += resetAmt;
                    pitchIsFallen = true;
                }

                if ((curFlags & kVowelF) != 0) {
                    // NUCLEAR RISE - begin of nuclear accent
                    if ((curCtrl & kPitchRise) != 0 && pitchIsFallen) {
                        short riseAmt = _vpRiseAmt;
                        if (_endPunctuation == _Quest_ || _endPunctuation == _Tilde_) {
                            riseAmt >>= 1;
                        }
                        // Pure-rise event (tilt = +64): only the rise component fires here;
                        // the matching fall fires at the kPitchFall vowel below.
                        short timeT = (curCtrl & kPitchFall) != 0
                            ? (short)((-80) / kFrameTime)
                            : (short)0;
                        StoreTiltEvent(riseAmt, +64, curDur, timeT, kPitchRiseFall_Flg);
                        curBaseline += riseAmt;
                        pitchIsFallen = false;
                    }

                    // PRE-NUCLEAR HEAD (kPitchRise1 / kPitchFall1)
                    if ((curCtrl & kPitchRise1) != 0) {
                        short raiseAmt1 = _vpRiseAmt1;
                        if (_endPunctuation == _Quest_ || _endPunctuation == _Tilde_) {
                            raiseAmt1 >>= 1;
                        }
                        // Slight rise-fall shape: tilt +32 (mild rise dominance)
                        StoreTiltEvent(raiseAmt1, +32, curDur, 0, kPitchRiseFall1_Flg);
                    } else if ((curCtrl & kPitchFall1) != 0) {
                        short fallAmt1 = _vpFallAmt1;
                        // Slight fall shape: tilt -32
                        StoreTiltEvent(fallAmt1, -32, curDur, 0, kPitchRiseFall1_Flg);
                    }

                    // STRESS ACCENT (primary or emphatic)
                    if ((curStress & kPrimOrEmphStress) != 0) {
                        short pitchT;
                        if (curStress == kEmphaticStress) {
                            pitchT = (short)(kHZ_28 + (_vpEmphasisBoost * kHZ_14 / 100));
                        } else {
                            pitchT = kHZ_14;
                        }

                        pitchT += stressCounter switch {
                            0 => kHZ_10,
                            1 => kHZ_9,
                            2 => kHZ_6,
                            3 => kHZ_4,
                            _ => 0,
                        };

                        if (_endPunctuation == _Quest_ || _endPunctuation == _Tilde_) {
                            pitchT >>= 1;
                        }

                        short timeT;
                        if ((curCtrl & kPitchFall) != 0 || (curSylType & kTerm_End) != 0) {
                            timeT = (short)((-60) / kFrameTime);
                        } else if (curStress == kEmphaticStress) {
                            timeT = 0;
                        } else {
                            timeT = (short)(curDur * (25 + _vpStressEarly / 2) / 100);
                        }

                        pitchT = (short)((_vpStressGain * pitchT) >> 16);

                        if ((curSylType & kTerm_End) != 0 && curStress != kEmphaticStress) {
                            pitchT = (short)(0 - kHZ_4);
                        }

                        // Stress accent: brief rise-fall, tilt 0 (symmetric shape), amplitude pitchT
                        StoreTiltEvent(pitchT, 0, curDur, timeT, kPitchStress_Flg);
                        stressCounter++;
                    }

                    // NUCLEAR FALL
                    if ((curCtrl & kPitchFall) != 0) {
                        short timeT = (short)(curDur - (160 / kFrameTime));
                        if (timeT < 25 / kFrameTime) {
                            timeT = (short)(25 / kFrameTime);
                        }

                        short fallAmt;
                        if ((curSylType & kTerm_End) != 0) {
                            fallAmt = _endPunctuation switch {
                                _Comma_ => (short)0,
                                _Period_ => (short)(0 - kHZ_20),
                                _Quest_ => (short)(0 - kHZ_7),
                                _Exclam_ => (short)(0 - kHZ_20),
                                _Tilde_ => (short)(0 - kHZ_4),
                                _Ellipsis_ => (short)(0 - kHZ_14),
                                _ => (short)(0 - kHZ_12),
                            };
                            if (_endPunctuation == _Period_ || _endPunctuation == _Exclam_) {
                                fallAmt += (short)(_vpUptalkAmt * (kHZ_20 + kHZ_4) / 100);
                            }
                        } else if ((curSylType & kVerb_End) != 0) {
                            fallAmt = 0;
                        } else {
                            fallAmt = _vpFallAmt;
                        }

                        fallAmt = (short)(((long)_vpAssertiveness * fallAmt >> 16) - _vpRiseAmt);

                        // Pure-fall event (tilt = -64)
                        StoreTiltEvent((short)(-fallAmt), -64, curDur, timeT, kPitchRiseFall_Flg);
                        curBaseline += fallAmt;
                        pitchIsFallen = true;
                    }

                    // PRONOUN ACCENT: mild rise-fall on pronoun vowels, scaled by VocalConfidence.
                    // Fires regardless of stress - confident speakers mark I/you/he/she/it/we/they
                    // with a subtle peak even when unstressed.
                    if (_vocalConfidence > 0 && (curCtrl & kPronounWord) != 0) {
                        short pronounAmt = (short)(kHZ_10 * _vocalConfidence / 100);
                        if (pronounAmt > 0) {
                            StoreTiltEvent(pronounAmt, 0, curDur, (short)(curDur / 2), kPitchStress_Flg);
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
                            StoreTiltEvent(kHZ_18, 0, 0, (short)(curDur * 2 / 5), kPitchBoundry_Flg);
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
        private void StoreTiltEvent(short amplitude, short tiltX64, int duration, short time, short flags) {
            int absTime = _pitchTimeOffset + time;
            int relTime = absTime - _lastEventTime;
            _pitchBufTime[_pitchBufInIndex] = (short)(relTime >= 0 ? relTime : 0);
            _lastEventTime = absTime;
            _pitchBufFreq[_pitchBufInIndex] = amplitude;
            _pitchBufTiltX64[_pitchBufInIndex] = tiltX64;
            _pitchBufDuration[_pitchBufInIndex] = (short)Math.Min(duration, short.MaxValue);
            _pitchBufFlags[_pitchBufInIndex] = flags;
            if (_pitchBufInIndex < kPitchBufSize - 1) {
                _pitchBufInIndex++;
            }
        }

        // StartNew_PitchClause

        private void StartNewPitchClause() {
            _baselineStartOffset = _baselineFallStart;
            _baselineEndOffset = _baselineFallEnd;
        }

        private void StretchLastWordForTilde() {
            int pct = _endPunctuation == _Tilde_ ? 110
                    : _endPunctuation == _Ellipsis_ ? 125
                    : 0;
            if (pct == 0) {
                return;
            }

            int end = _phonBuf2InIndex - 1;
            while (end > 0 && _phonBuf2[end] == _SIL_) {
                end--;
            }

            int start = 0;
            for (int i = end; i >= 0; i--) {
                if ((_phonCtrlBuf2[i] & kBoundryTypeField) == kWord_Start) {
                    start = i;
                    break;
                }
            }

            for (int i = start; i <= end; i++) {
                _durBuf[i] = (short)Math.Max(1, (_durBuf[i] * pct + 50) / 100);
            }
        }

        private const uint kHasReleaseF = 1u << 23;
        private const uint kFrontF_BE = 1u << 21;
        private const long kPlosive_Release = 0x4000;

        private void InsertPlosiveRelease() {
            if (_singing) {
                return;
            }
            for (int i = 0; i < _phonBuf2InIndex; i++) {
                short cur = _phonBuf2[i];
                short next = i + 1 < _phonBuf2InIndex ? _phonBuf2[i + 1] : _SIL_;
                if (next != _SIL_) {
                    continue;
                }

                uint curFlags = Tables.GetFeatureFlags(cur);
                if ((curFlags & kHasReleaseF) == 0) {
                    continue;
                }
                if (_phonBuf2InIndex >= kPhonBuf_Red_Zone) {
                    break;
                }

                for (int k = _phonBuf2InIndex; k > i + 1; k--) {
                    _phonBuf2[k] = _phonBuf2[k - 1];
                    _phonCtrlBuf2[k] = _phonCtrlBuf2[k - 1];
                    _durBuf[k] = _durBuf[k - 1];
                    _userPitchBuf2[k] = _userPitchBuf2[k - 1];
                    _userDurBuf2[k] = _userDurBuf2[k - 1];
                    _userNoteBuf2[k] = _userNoteBuf2[k - 1];
                    _userRateBuf2[k] = _userRateBuf2[k - 1];
                }
                _phonBuf2InIndex++;

                short prevPhon = i > 0 ? _phonBuf2[i - 1] : _SIL_;
                uint prevFlags = Tables.GetFeatureFlags(prevPhon);
                bool useIX = (cur == _T_ || cur == _D_) || ((prevFlags & kFrontF_BE) != 0);
                _phonBuf2[i + 1] = useIX ? _IX_ : _AX_;
                _phonCtrlBuf2[i + 1] = _phonCtrlBuf2[i] | kPlosive_Release;
                _durBuf[i + 1] = 25 / kFrameTime;
                _userPitchBuf2[i + 1] = _userPitchBuf2[i];
                _userDurBuf2[i + 1] = kDur_One;
                _userNoteBuf2[i + 1] = 0;
                _userRateBuf2[i + 1] = 0;

                i++;
            }
        }
    }
}  // namespace
