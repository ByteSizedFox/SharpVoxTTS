#nullable enable

namespace SharpTalk {

    partial class SpeechRenderer {
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
        private void InitCtrlsForNewPhon() {
            SetPhonContext(_curPhonBufIndex);
            FillPhonTargets();

            int dur = _curPhonDur;
            if (dur < 1) {
                dur = 1;
            }

            void SetKStep(int idx, byte targetVal) {
                int target = targetVal << 16;
                _klattschStep[idx] = (target - _curKlattsch[idx]) / dur;
            }

            if (_curPhonBufIndex < _dump.PhonBuf2InIndex) {
                SetKStep(kAspIdx, _dump.AspirationBuf2[_curPhonBufIndex]);
                SetKStep(kTiltIdx, _dump.TiltBuf2[_curPhonBufIndex]);
                SetKStep(kEffIdx, _dump.EffortBuf2[_curPhonBufIndex]);
                SetKStep(kVibDIdx, _dump.VibDepthBuf2[_curPhonBufIndex]);
                SetKStep(kVibRIdx, _dump.VibRateBuf2[_curPhonBufIndex]);
                // Tremolo falls back to voice-level baseline when no explicit value is set
                byte tremD = _dump.TremDepthBuf2[_curPhonBufIndex] > 0 ? _dump.TremDepthBuf2[_curPhonBufIndex] : _voiceTremDepth;
                byte tremR = (_voiceTremDepth > 0 && _dump.TremRateBuf2[_curPhonBufIndex] == 0) ? _voiceTremRate : _dump.TremRateBuf2[_curPhonBufIndex];
                SetKStep(kTremDIdx, tremD);
                SetKStep(kTremRIdx, tremR);
            } else {
                for (int i = 0; i < 7; i++) {
                    _klattschStep[i] = (0 - _curKlattsch[i]) / dur;
                }
            }

            for (_curBlockIndex = 0; _curBlockIndex < kNumOfBlocks; _curBlockIndex++) {
                var cb = _cb[_curBlockIndex];
                int bt = Tables.ControlBlockTypeTable[_curBlockIndex];

                cb.prevP_END_Targ = cb.curP_END_Targ;
                cb.nextP_START_Targ = (short)GetFirstTarget(_curPhonBufIndex + 1);
                cb.curTarget_OFFS = 0;
                cb.ptrToTargetList = -1;

                short rawTarg = GetTargetRaw(_curPhonBufIndex);
                if (rawTarg < kNoValue) {
                    // Diphthong: store the multi-point trajectory rather than a scalar target
                    GetDiphthongs(rawTarg & 0x7FFF);
                } else {
                    cb.curP_START_Targ = rawTarg;
                    cb.curTarget_STEP = 0;
                    cb.curTarget_TIME = _curPhonDur;

                    if (bt == kFreqType) {
                        // Coarticulatory undershoot: blend the target toward the midpoint of
                        // adjacent targets. Stressed phonemes undershoot less because they
                        // receive more precise articulation.
                        int artic = k1pct * 10;
                        if ((_curPhonCtrl & kStressField) != 0) {
                            artic = (_curBlockIndex == kF2) ? k1pct * 25 : k1pct * 15;
                        }
                        cb.curP_START_Targ += (short)((((cb.prevP_END_Targ + cb.nextP_START_Targ) >> 1) - cb.curP_START_Targ) * artic >> 16);
                    }
                    cb.curP_END_Targ = cb.curP_START_Targ;
                }

                if (bt == kFreqType) {
                    // Pull nextP_START_Targ 10% back toward the current end target so the
                    // tail ramp meets the following phoneme at a blend rather than a hard step
                    cb.nextP_START_Targ += (short)((cb.curP_END_Targ - cb.nextP_START_Targ) * (k1pct * 10) >> 16);
                }

                // HEAD envelope: onset ramp from transLevel to curP_START_Targ over transTime
                _transLevel = (cb.prevP_END_Targ + cb.curP_START_Targ) >> 1;
                _transTime = 32 / kFrameTime;
                HeadRules(cb, bt);

                // Onset hardness scales the AV ramp: 0 = soft (slow ramp from deep below),
                // 50 = neutral, 100 = hard (near-instant onset at target level)
                if (_curBlockIndex == kAV && _voiceOnsetHardness != 50) {
                    float scale = (100 - _voiceOnsetHardness) / 50.0f;
                    _transLevel = cb.curP_START_Targ + (int)((_transLevel - cb.curP_START_Targ) * scale);
                    _transTime = (int)(_transTime * scale);
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
                        int hs = (int)(((long)OvX(_transTime) * cb.HEAD_offs) >> 16);
                        cb.HEAD_step = hs;
                        cb.HEAD_offs = hs * _transTime;
                    }
                }

                // TAIL envelope: offset ramp from curP_END_Targ toward the following target
                _transLevel = (cb.curP_END_Targ + cb.nextP_START_Targ) >> 1;
                _transTime = 25 / kFrameTime;
                TailRules(cb, bt);

                cb.TAIL_offs = 0; cb.TAIL_step = 0;
                if (_transTime > 0) {
                    int ts = (_transLevel - cb.curP_END_Targ) << kStepSizeRes;
                    if (ts != 0) {
                        cb.TAIL_step = (int)(((long)OvX(_transTime) * ts) >> 16);
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
        private void HeadRules(ControlBlock cb, int bt) {
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
                        _transTime = 32 / kFrameTime;
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
                        if ((_curPhon == _N_ || _curPhon == _EN_) && Tables.GetBackwardRank(_prevPhon) == kFrontR) {
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

                if ((_curPhonFlags & kPlosFricF) == 0 && Tables.GetBackwardRank(_prevPhon) != kConsonantR && _transTime > 0) {
                    // Scale transition time by duration ratio: shorter vowels have faster
                    // formant transitions because there is less time to reach the target.
                    _transTime = 1 + (int)((_curPhonPctOfMaxDur1 * _transTime) >> 16);
                }
            } else if (bt == kFNZType) {
                if ((_prevPhonFlags & kNasalF) != 0 && (_curPhonFlags & kNasalF) == 0) {
                    // Coming from a nasal: FNZ starts halfway between nasal base and target
                    _transLevel = _nasalBaseFreq + ((_nasalTargFreq - _nasalBaseFreq) >> 1);
                    _transTime = 80 / kFrameTime;
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
                        _transTime = 50 / kFrameTime;
                        _transLevel = (_cb[kF1].curP_START_Targ >> 3) + cb.curP_START_Targ;
                    } else {
                        _transTime = 40 / kFrameTime;
                    }
                } else {
                    _transTime = 20 / kFrameTime;
                }

                if (_prevPhon == _SIL_) {
                    _transLevel = (kBW3 - bt) * 50 + cb.curP_START_Targ;
                    _transTime = 50 / kFrameTime;
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
                    if (_curBlockIndex == kBW2 && (_prevPhon == _N_ || _prevPhon == _EN_) && Tables.GetForwardRank(_curPhon) != kFrontR) {
                        _transLevel += 60;
                        _transTime = 60 / kFrameTime;
                    } else if (_curBlockIndex == kBW1) {
                        _transLevel += 70;
                        _transTime = 100 / kFrameTime;
                    }
                }
                if ((_curPhonFlags & kNasalF) != 0) {
                    // Nasal bandwidth is set directly by the target; no onset ramp needed
                    _transTime = 0;
                }
            } else {
                // Amplitude blocks: ramp from slightly below target to model finite adduction time
                int ampT = cb.curP_START_Targ - 10;
                if (_transLevel < ampT || (_prevPhonFlags & kStopF) != 0 || _prevPhon == _JH_) {
                    _transLevel = ampT;
                    if ((_curPhonFlags & kPlosFricF) == 0) {
                        _transTime = 20 / kFrameTime;
                    }
                    if (_curBlockIndex == kAV) {
                        if (_prevPhon == _SIL_ && (_curPhonFlags & kVoicedF) != 0) {
                            // Voiced onset from silence: slow ramp models gradual fold adduction
                            _transLevel -= 8;
                            _transTime = 45 / kFrameTime;
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
                    _transTime = _curPhonDur - 2;
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
                                _transTime = 80 / kFrameTime;
                                _transLevel = 52;
                            } else {
                                _transTime = 45 / kFrameTime;
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
        private void TailRules(ControlBlock cb, int bt) {
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
                            _transTime = 20 / kFrameTime;
                        } else {
                            _transLevel = (cb.curP_END_Targ + _transLevel) >> 1;
                            _transTime = 40 / kFrameTime;
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
                        if ((_curPhon == _N_ || _curPhon == _EN_) && Tables.GetForwardRank(_nextPhon) == kFrontR) {
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

                if ((_curPhonFlags & kPlosFricF) == 0 && Tables.GetForwardRank(_nextPhon) != kConsonantR && _transTime > 0) {
                    _transTime = 1 + (int)((_curPhonPctOfMaxDur2 * _transTime) >> 16);
                }
            } else if (bt == kFNZType) {
                if ((_nextPhonFlags & kNasalF) != 0 && (_curPhonFlags & kNasalF) == 0) {
                    _transLevel = _nasalTargFreq;
                    _transTime = 80 / kFrameTime;
                }
            } else if (bt == kBWType) {
                if ((_curPhonFlags & kVoicedF) != 0) {
                    _transTime = 40 / kFrameTime;
                    if ((_nextPhonFlags & kVoicedF) == 0 && _curBlockIndex == kBW1) {
                        _transTime = 50 / kFrameTime;
                        _transLevel = (_cb[kF1].curP_START_Targ >> 3) + cb.curP_END_Targ;
                    }
                } else {
                    _transTime = 20 / kFrameTime;
                }
                if (_nextPhon == _SIL_) {
                    _transLevel = (kBW3 - bt) * 50 + cb.curP_END_Targ;
                    _transTime = 50 / kFrameTime;
                } else if (_curPhon == _SIL_) {
                    _transLevel = (kBW3 - bt) * 50 + cb.nextP_START_Targ;
                    _transTime = 50 / kFrameTime;
                }
                if ((_nextPhonFlags & kNasalF) != 0) {
                    _transLevel = cb.curP_END_Targ;
                    if (_curBlockIndex == kBW2 && (_nextPhon == _N_ || _nextPhon == _EN_) && Tables.GetForwardRank(_curPhon) != kFrontR) {
                        _transLevel += 60;
                        _transTime = 60 / kFrameTime;
                    } else if (_curBlockIndex == kBW1) {
                        _transLevel += 100;
                        _transTime = 100 / kFrameTime;
                    }
                }
                if ((_curPhonFlags & kNasalF) != 0) {
                    _transTime = 0;
                }
            } else {
                int ampT = cb.nextP_START_Targ - 10;
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
                                _transTime = 45 / kFrameTime;
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
                        _transTime = 20 / kFrameTime;
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
                                _transTime = 40 / kFrameTime;
                                _transLevel = 52;
                            }
                        }
                        if ((_curPhonFlags & kVowelF) != 0 && _nextPhon == _SIL_) {
                            _transTime = 130 / kFrameTime;
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
    }

}  // namespace
