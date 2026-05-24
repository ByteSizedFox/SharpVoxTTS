#nullable enable

namespace SharpTalk {

    partial class SpeechRenderer {
        // Initializes per-phoneme duration ratios used to scale transition times.
        //
        // The ratio of actual duration to canonical maximum duration is a key factor in
        // vowel reduction: shorter (unstressed) phonemes undershoot their formant targets
        // because the articulators do not have time to fully reach them.
        private void FillPhonTargets() {
            for (int i = 0; i < kNumOfBlocks; i++) {
                _cb[i].onset_END_TIME = 0;
            }
            _nextDiphEntryIdx = 0;

            if ((_curPhonFlags & kPlosFricF) != 0 || _curPhon == _SIL_) {
                return;
            }

            int maxDur = Tables.GetMaximumDuration(_curPhon) / kFrameTime;
            _curPhonMaxDur = maxDur > 0 ? maxDur : 1;

            // 16.16 fixed-point ratio of actual duration to canonical max duration.
            // Used to scale head/tail ramp times so stressed vowels get longer transitions
            // and unstressed (short) ones get proportionally shorter ones.
            _curPhonPctOfMaxDur = ((long)_curPhonDur << 16) / _curPhonMaxDur;
            _curPhonPctOfMaxDur1 = (_curPhonPctOfMaxDur >> 1) + kOneHalf;
            _curPhonPctOfMaxDur2 = _curPhonPctOfMaxDur1 - (10L * k1pct);
        }

        // Returns the raw table value for the current block at the given phoneme index.
        // Values >= 0 are direct Hz targets; kNoValue (-1) means no target for this phoneme;
        // values < kNoValue are diphthong envelope indices encoded as negative offsets.
        private short GetTargetRaw(int index) {
            int bt = Tables.ControlBlockTypeTable[_curBlockIndex];
            int cur = GP(index); uint cf = PF(cur); int ctrl = PC(index);
            int next = GP(index + 1); uint nf = PF(next);
            int prev = GP(index - 1); uint pf = PF(prev);
            short tv = -1;

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
                                tv = Tables.DefaultTargetFrequenciesTable[_curBlockIndex];
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
                if ((cur == _N_ || cur == _EN_) && _curBlockIndex == kBW2 && Tables.GetForwardRank(next) != kFrontR) {
                    tv += 60;
                }
                // Before/after a Y-glide, NG raises BW3 to full nasal bandwidth
                if ((cur == _N_ || cur == _EN_ || cur == _NG_) && _curBlockIndex == kBW3 &&
                    ((nf & kYGlideStartF) != 0 || (pf & kYGlideEndF) != 0)) {
                    tv = (short)kMaxBandWidth;
                }
            } else if (bt == kFNZType) {
                tv = (short)(((cf & kNasalF) != 0) ? _nasalTargFreq : _nasalBaseFreq);
            } else if (bt == kSourceAmpType) {
                if (_curBlockIndex == kAV) {
                    tv = Tables.GetAmplitudeVoicing(_male, cur);
                    if ((ctrl & kPlosive_Release) != 0) {
                        tv -= (short)(((pf & kNasalF) != 0) ? 6 : 20);
                    }
                    if ((cf & kStopF) != 0 && (pf & kVoicedF) == 0) {
                        tv = 0;
                    }
                    if (cur == _HH_ && (pf & kVoicedF) != 0 && (ctrl & kPrimOrEmphStress) == 0) {
                        tv = 54;
                    }
                } else if (cur == _HH_) {
                    tv = (short)(Tables.GetForwardRank(next) == kFrontR ? 58 : 62);
                    if ((ctrl & kStressField) == 0) {
                        tv -= 1;
                    }
                } else {
                    tv = 0;
                }
            } else if (bt == kResonAmpType) {
                tv = Tables.GetNoiseIndex(cur);
                if (tv == kNoValue) {
                    tv = 0;
                } else {
                    int rank = (next == _SIL_) ? Tables.GetBackwardRank(prev) : Tables.GetForwardRank(next);
                    if (rank == kRoundR) {
                        rank = kBackR;
                    }
                    int idx2 = tv + (_curBlockIndex - kAp2) + rank * 6;
                    tv = _voiceNoiseAmplitudeTable[idx2];
                    if ((PC(index + 1) & kPlosive_Release) != 0 && tv >= 4) {
                        tv -= 4;
                    }
                }
            }
            return tv;
        }

        // Returns the onset (first) target Hz value for the given phoneme index
        private int GetFirstTarget(int index) {
            short t = GetTargetRaw(index);
            if (t < kNoValue) {
                int i = t & 0x7FFF;
                t = _envelopeListTable[i];
                if (Tables.ControlBlockTypeTable[_curBlockIndex] == kFreqType) {
                    t += (short)AdjustColored(index, 0);
                }
            }
            return t;
        }

        // Returns the final target Hz value for the given phoneme index
        private int GetLastTarget(int index) {
            short t = GetTargetRaw(index);
            if (t < kNoValue) {
                int i = (t & 0x7FFF) + 2;
                t = _envelopeListTable[i];
                if (Tables.ControlBlockTypeTable[_curBlockIndex] == kFreqType) {
                    t += (short)AdjustColored(index, 1);
                }
            }
            return t;
        }

        private short GetVoiceFormantValue(int bi, int phonemeId) {
            bool male = _voice.VoiceType == 0;
            return bi switch {
                kF1 => male ? Tables.GetMaleFormant1(phonemeId) : Tables.GetFemaleFormant1(phonemeId),
                kF2 => male ? Tables.GetMaleFormant2(phonemeId) : Tables.GetFemaleFormant2(phonemeId),
                kF3 => male ? Tables.GetMaleFormant3(phonemeId) : Tables.GetFemaleFormant3(phonemeId),
                kBW1 => male ? Tables.GetMaleBandwidth1(phonemeId) : Tables.GetFemaleBandwidth1(phonemeId),
                kBW2 => male ? Tables.GetMaleBandwidth2(phonemeId) : Tables.GetFemaleBandwidth2(phonemeId),
                kBW3 => male ? Tables.GetMaleBandwidth3(phonemeId) : Tables.GetFemaleBandwidth3(phonemeId),
                _ => throw new System.ArgumentException()
            };
        }

        // Sets up a 4-point interpolated formant trajectory for diphthong and glide phonemes.
        //
        // Diphthongs are characterized by continuous formant movement between two targets;
        // the trajectory is stored as a pair (p1,t1) -> (p2,t2) in the _diphEntries buffer.
        // [Lehiste & Peterson 1961]
        //
        // Coarticulatory adjustments are applied to both endpoints so that the glide into
        // and out of adjacent phonemes is smooth.
        private void GetDiphthongs(int index) {
            var cb = _cb[_curBlockIndex];
            int bt = Tables.ControlBlockTypeTable[_curBlockIndex];

            short p1 = _envelopeListTable[index];
            short t1 = _envelopeListTable[index + 1];
            short p2 = _envelopeListTable[index + 2];
            short t2 = _envelopeListTable[index + 3];

            t1 = (short)ScalePrcnt(t1);
            t2 = (short)ScalePrcnt(t2);

            if (bt == kFreqType) {
                int artic = k1pct * 10;
                if (cb.prevP_END_Targ > 0) {
                    p1 += (short)(((cb.prevP_END_Targ - p1) * artic) >> 16);
                }
                p1 += (short)AdjustColored(_curPhonBufIndex, 0);
                if (cb.nextP_START_Targ > 0) {
                    p2 += (short)(((cb.nextP_START_Targ - p2) * artic) >> 16);
                }
                p2 += (short)AdjustColored(_curPhonBufIndex, 1);
            }

            int rampTime = t2 - t1;
            int diff = (p2 - p1) << kStepSizeRes;
            int step = rampTime > 0
                ? (rampTime < ReciprocalTableSize
                    ? (int)(((long)OvX(rampTime) * diff) >> 16)
                    : diff / rampTime)
                : 0;

            cb.curP_START_Targ = p1;
            cb.curTarget_TIME = t1;
            cb.curTarget_STEP = 0;
            cb.curP_END_Targ = p2;

            cb.ptrToTargetList = _nextDiphEntryIdx;
            _diphEntries[_nextDiphEntryIdx++] = (short)t2;
            _diphEntries[_nextDiphEntryIdx++] = (short)step;
            _diphEntries[_nextDiphEntryIdx++] = (short)_curPhonDur;
            _diphEntries[_nextDiphEntryIdx++] = 0;
        }

        // Scales a duration percentage by the ratio of actual-to-canonical phoneme duration.
        // Produces a frame count proportional to how long this phoneme actually lasts.
        private int ScalePrcnt(int pct) {
            long t = (pct * _curPhonPctOfMaxDur) >> 8;
            t = (_curPhonMaxDur * t / 100) >> 8;
            return t <= 0 ? 1 : (int)t;
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
        private int AdjustColored(int index, int entry) {
            int cur = GP(index); int next = GP(index + 1); int prev = GP(index - 1);
            uint cf = PF(cur); uint nf = PF(next); uint pf = PF(prev);
            int ctrl = PC(index);
            int adj = 0;

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
    }

}  // namespace
