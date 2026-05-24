#nullable enable
using System;

namespace SharpTalk {

    // Generates F0 values via Taylor (2000) Tilt model for speech or portamento for singing.
    public sealed class PitchInterpolator {
        private readonly SynthInputDump _dump;

        // Pitch buffer tracking
        private short _nextPitchBufTime;
        private int _pitchBufOutIndex;
        private int _curPitchBufTime;

        // Phoneme advance (Targ path - lookahead for phoneme pitch offsets)
        private int _phonIndexTarg;
        private int _timeIntoPhonTarg;
        private int _curPhonDurCc;
        private int _phonDurDelay;

        // Phoneme advance (Cp path - phoneme boundary micro-dip)
        private int _phonIndexCp;
        private int _timeIntoPhonCp;
        private int _curPhonDurCp;

        // Phoneme pitch offsets
        private int _uvPhonPitchTarg;
        private int _phonPitchOffset1;

        // Declination ramp
        private int _baselineStartOffset;
        private int _baselineEndOffset;
        private long _downRampOffset;
        private long _downRampStep;
        private long[] _rampSteps;
        private int _curRamp;

        // Voice parameters
        private long _vpIntonation;
        private long _vpPitchRange;
        private int _vpBaselinePitch;

        // Vibrato
        private long _vibratoDepth1;
        private long _vibratoDepth2;
        private long _vibratoFreq;
        private int _vibratoPhase1;

        // Singing state
        private bool _singing;
        private bool _hzGlide;
        private bool _musicalNoteActive;
        private long _portamentoAccum;
        private long _portamentoStep;
        private bool _newPortaTarget;
        private bool _newSentence;
        private int _speechRate;

        // Phoneme boundary micro-dip state
        private int _pitchBoundry;
        private bool _lowGainCp;
        private int _pbHold;
        private bool _pbLowGain;

        // Tilt synthesis state (Taylor 2000) including held levels, phase, and smoothing IIR filters.
        private int _tiltPhase;
        private int _tiltSmooth;
        private int _tiltFrame;
        private int _tiltPhaseDur;
        private int _tiltA;    // A parameter for TiltSynth (negative=rise, positive=fall)
        private int _tiltAbs;  // A_abs: end value of current phase, becomes new _tiltHeldLevel
        private int _tiltHeldLevel;
        private int _f0Smooth;
        private bool _f0SmoothPrime;

        // Pending fall component (queued when a combined rise+fall event fires)
        private bool _tiltFallPending;
        private int _tiltFallA;
        private int _tiltFallDur;
        private int _tiltFallAbs;

        // Boundary tone pitch offset (comma, question, tilde patterns).
        // Set directly from kPitchBoundry_Flg events; fed into tiltExcursion so
        // the existing IIR smoother handles transitions between boundary targets.
        private int _punctOffset;

        // Constants
        private const int kStepSizeRes = 3;
        private const int kNeverHappens = -10000;
        private const int kFrameTime = 5;
        private const int pct = 655;
        private const int k100percent = 0x10000;

        // Pitch buffer event flags (must match AudioProcessor.cs)
        private const int kResetDecline = 0x8;
        private const int kPhraseReset = 0x10;
        private const int kPitchRiseFall_Flg = 0x2;
        private const int kPitchRiseFall1_Flg = 0x20;
        private const int kPitchStress_Flg = 0x1;
        private const int kPitchBoundry_Flg = 0x4;

        // Phoneme flags
        private const uint kVoicedF = (1 << 2);
        private const uint kVowelF = (1 << 0);
        private const uint kVowel1F = (1 << 3);
        private const uint kGStopF = (1 << 20);
        private const uint kStopF = (1 << 12);

        // PhonCtrl field masks
        private const long kSyllableTypeField = 0x0F;
        private const long kWord_End = 0x0001;
        private const long kPrep_End = 0x0002;
        private const long kMid_Syllable_In_Word = 0x0200;
        private const long kPrimOrEmphStress = 0x1400;

        private const int _SIL_ = AudioProcessor._SIL_;
        private const int _YU_ = AudioProcessor._YU_;

        public PitchInterpolator(SynthInputDump dump) {
            _dump = dump;
            PitchState s = dump.Pitch;

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
            _rampSteps = s.RampSteps;
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

            // Tilt state starts at rest
            _tiltPhase = 0;
            _tiltFrame = 0;
            _tiltPhaseDur = 0;
            _tiltA = 0;
            _tiltAbs = 0;
            _tiltHeldLevel = 0;
            _tiltFallPending = false;
            _tiltSmooth = 0;
            _f0Smooth = 0;
            _f0SmoothPrime = true;
        }

        private int _controlF0;
        private int _voiceNaturalPitch;
        private long _curPhonCtrlSinging;

        // Debug snapshot populated each Step() - zero during singing.
        private int _dbgTiltExcursion;
        private int _dbgBaselineOffset;
        private int _dbgTotalOffset;

        public short Step() {
            Interpolate_Pitch();
            return (short)_controlF0;
        }

        // Debug accessors - valid immediately after Step().
        public int DbgF0 => _controlF0;
        public int DbgTiltExcursion => _dbgTiltExcursion;
        public int DbgTiltSmooth => _tiltSmooth;
        public int DbgTiltHeld => _tiltHeldLevel;
        public int DbgTiltPhase => _tiltPhase;
        public int DbgBaselineOffset => _dbgBaselineOffset;
        public int DbgTotalOffset => _dbgTotalOffset;

        private const long kLowVibrato = 0x10L;
        private const long kSingingDuration = 0x40000000L;
        private const long kSingingPhon = 0x20000000L;
        private const long kSilenceDuration = 0x01000000L;

        public void DoNote(int phonIndex) {
            _hzGlide = false;
            _curPhonCtrlSinging = GetPhonCtrl(phonIndex);

            long ctrl = (phonIndex >= 0 && phonIndex < _dump.PhonCtrlBuf2.Length)
                        ? _dump.PhonCtrlBuf2[phonIndex] : 0;

            if ((ctrl & kSingingPhon) == 0) {
                _musicalNoteActive = false;
            }

            short note = (phonIndex >= 0 && phonIndex < _dump.UserNoteBuf2.Length)
                         ? _dump.UserNoteBuf2[phonIndex] : (short)0;

            if (note != 0 && (ctrl & kSilenceDuration) == 0) {
                if ((ctrl & kSingingPhon) != 0) {
                    if (note < 0) {
                        int targetPitch = HzToPitch(-note);
                        int curPitch = (int)(_portamentoAccum >> 16);
                        int frames = (phonIndex < _dump.DurBuf.Length) ? _dump.DurBuf[phonIndex] : 1;
                        if (frames < 1) {
                            frames = 1;
                        }
                        _vpBaselinePitch = targetPitch;
                        _portamentoStep = ((long)(targetPitch - curPitch) << 16) / frames;
                        _newPortaTarget = true;
                        _hzGlide = true;
                    } else {
                        int targetPitch = HzToPitch(note);
                        _vpBaselinePitch = targetPitch;
                        _portamentoStep = 0;
                        _newPortaTarget = true;
                        _musicalNoteActive = true;
                    }
                } else {
                    int n = (note & 0xFF) << 8;
                    if (n != 0x7F00) {
                        _vpBaselinePitch = _voiceNaturalPitch + ((n * 0x1555) >> 16);
                        if (_vpBaselinePitch < 0) {
                            _vpBaselinePitch = 0;
                        }
                    }
                }
            }
        }

        private static int HzToPitch(int hz) {
            if (hz <= 0) {
                return 0;
            }
            int freq, fk;
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

            int ratio = ((freq - 400) * 2621) >> 11;
            if (ratio < 0) {
                ratio = 0;
            }
            if (ratio >= Tables.LogarithmBase2Table.Length) {
                ratio = Tables.LogarithmBase2Table.Length - 1;
            }
            return Tables.LogarithmBase2Table[ratio] + fk;
        }

        private short GetPhon(int index) {
            if (index >= 0 && index < _dump.PhonBuf2InIndex) {
                return _dump.PhonBuf2[index];
            }
            return _SIL_;
        }

        private long GetPhonCtrl(int index) {
            if (index >= 0 && index < _dump.PhonBuf2InIndex) {
                return _dump.PhonCtrlBuf2[index];
            }
            return 0;
        }

        private void Phon_Boundry_Pitch() {
            if (_timeIntoPhonCp >= _curPhonDurCp) {
                _timeIntoPhonCp -= _curPhonDurCp;
                _phonIndexCp++;
                _curPhonDurCp = (_phonIndexCp < _dump.DurBuf.Length) ? _dump.DurBuf[_phonIndexCp] : 0;

                int curPhon = GetPhon(_phonIndexCp);
                uint curFlags = Tables.GetFeatureFlags(curPhon);
                long curCtrl = GetPhonCtrl(_phonIndexCp + 1);

                int nextPhon = GetPhon(_phonIndexCp + 1);
                uint nextFlags = Tables.GetFeatureFlags(nextPhon);
                long nextCtrl = GetPhonCtrl(_phonIndexCp + 1);

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

            int timeAt50 = 50 / kFrameTime; // 10
            int lastFrame = _curPhonDurCp - 1;
            if (_timeIntoPhonCp == timeAt50 || _timeIntoPhonCp == lastFrame) {
                _pitchBoundry = _pbHold;
                _lowGainCp = _pbLowGain;
            }
        }

        // Parabolic synthesis for one rise or fall component (Taylor 2000, eq. 12).
        //
        //   f0(t) = A_abs + A - 2A*(t/D)^2    for 0 <= t < D/2
        //   f0(t) = A_abs + 2A*(1-t/D)^2      for D/2 <= t <= D
        //
        // The curve always starts at (A_abs + A) and ends at A_abs.
        // A < 0: ascending (rise) curve;  A > 0: descending (fall) curve.
        // 8-bit fixed point is used for t/D to avoid overflow while preserving precision.
        private static int TiltSynth(int a, int aAbs, int frame, int dur) {
            if (dur <= 0) {
                return aAbs;
            }
            int tD8 = (frame << 8) / dur;   // t/D in 8.8 fixed point (0..255)
            int twoA = a * 2;
            if (frame * 2 < dur) {
                int tD2 = (tD8 * tD8) >> 8; // (t/D)^2 in 8.8 fixed point
                return aAbs + a - ((twoA * tD2) >> 8);
            } else {
                int omtD = (1 << 8) - tD8; // (1-t/D) in 8.8 fixed point
                int omtD2 = (omtD * omtD) >> 8;
                return aAbs + ((twoA * omtD2) >> 8);
            }
        }

        // Returns the current Tilt excursion (pitch units above/below baseline) and
        // advances the Tilt state machine by one frame.
        private int StepTilt() {
            if (_tiltPhase == 0) {
                return _tiltHeldLevel;
            }

            int excursion = TiltSynth(_tiltA, _tiltAbs, _tiltFrame, _tiltPhaseDur);
            _tiltFrame++;

            if (_tiltFrame >= _tiltPhaseDur) {
                _tiltHeldLevel = _tiltAbs; // settle to end value

                if (_tiltFallPending) {
                    _tiltPhase = 2;
                    _tiltFrame = 0;
                    _tiltA = _tiltFallA;
                    _tiltAbs = _tiltFallAbs;
                    _tiltPhaseDur = _tiltFallDur;
                    _tiltFallPending = false;
                } else {
                    _tiltPhase = 0; // return to hold
                }
            }

            return excursion;
        }

        // Starts a Tilt event from the pitch buffer.
        // amplitude:  A_event in pitch units (positive)
        // tiltX64:    tilt * 64 in range [-64, +64]
        // duration:   total event duration in frames
        // flags:      event type (determines whether held level is updated or restored)
        private void FireTiltEvent(int amplitude, int tiltX64, int duration, int flags) {
            // Convert Tilt -> RFC components (Taylor 2000, eqs 8-11)
            int aRise = amplitude * (64 + tiltX64) / 128;
            int aFall = amplitude * (64 - tiltX64) / 128;
            int dRise = duration * (64 + tiltX64) / 128;
            int dFall = duration * (64 - tiltX64) / 128;

            // Nuclear events (kPitchRiseFall_Flg) update _tiltHeldLevel permanently.
            // Transient events (stress, head, boundary) restore _tiltHeldLevel afterwards.
            bool isNuclear = (flags & kPitchRiseFall_Flg) != 0;

            int held = _tiltHeldLevel;

            // Sample the current excursion so new events can start from wherever the
            // curve is right now.  This eliminates audible discontinuities when a new
            // event fires mid-curve, regardless of whether the incoming event is nuclear
            // or transient.  Endpoints (_tiltAbs / _tiltFallAbs) are always anchored to
            // _tiltHeldLevel so the nuclear accent level is preserved after completion.
            int curExcursion = _tiltPhase != 0
                ? TiltSynth(_tiltA, _tiltAbs, _tiltFrame, _tiltPhaseDur)
                : held;

            if (aRise > 0 && dRise > 0) {
                _tiltPhase = 1; // RISE
                _tiltFrame = 0;
                _tiltPhaseDur = dRise;
                _tiltA = -aRise;
                // Transient events start from the current excursion for continuity.
                // Nuclear events anchor to _tiltHeldLevel to preserve the held-level ceiling.
                _tiltAbs = (isNuclear ? held : curExcursion) + aRise;

                if (aFall > 0 && dFall > 0) {
                    _tiltFallPending = true;
                    _tiltFallA = aFall;
                    _tiltFallDur = dFall;
                    // Nuclear fall ends below old held level; transient fall restores to it
                    _tiltFallAbs = isNuclear ? held + aRise - aFall : held;
                } else {
                    _tiltFallPending = false;
                }
            } else if (aFall > 0 && dFall > 0) {
                _tiltPhase = 2; // FALL (no rise)
                _tiltFrame = 0;
                _tiltPhaseDur = dFall;
                _tiltAbs = isNuclear ? held - aFall : held;
                // Adjust _tiltA so the curve starts at curExcursion and ends at _tiltAbs,
                // giving continuity without changing the intended endpoint.
                _tiltA = curExcursion - _tiltAbs;
                _tiltFallPending = false;
            }
        }

        private void Interpolate_Pitch() {
            // Pitch buffer event collection loop: fire all events due this frame.
            bool collect = true;
            do {
                if (_curPitchBufTime >= _nextPitchBufTime
                    && _pitchBufOutIndex < (int)_dump.PitchBufInIndex) {
                    int evAmp = _dump.PitchBufFreq[_pitchBufOutIndex];
                    int evFlags = _dump.PitchBufFlags[_pitchBufOutIndex];
                    int evTiltX64 = _dump.PitchBufTiltX64[_pitchBufOutIndex];
                    int evDuration = _dump.PitchBufDuration[_pitchBufOutIndex];

                    _curPitchBufTime -= _nextPitchBufTime;
                    _pitchBufOutIndex++;
                    _nextPitchBufTime = _dump.PitchBufTime[_pitchBufOutIndex];

                    if ((evFlags & kResetDecline) != 0) {
                        _downRampOffset = 0;
                    } else if ((evFlags & kPhraseReset) != 0) {
                        _downRampOffset = (long)(_baselineStartOffset - _baselineEndOffset) << 14;
                        if (_curRamp < _rampSteps.Length - 1) {
                            _curRamp++;
                        }
                        _downRampStep = _rampSteps[_curRamp];
                        _tiltHeldLevel = 0;
                        _tiltPhase = 0;
                        _tiltFallPending = false;
                        _tiltSmooth = 0;
                        _f0Smooth = 0;
                        _f0SmoothPrime = true;
                        _punctOffset = 0;
                    } else if ((evFlags & kPitchBoundry_Flg) != 0) {
                        _punctOffset = evAmp;
                    } else {
                        FireTiltEvent(evAmp, evTiltX64, evDuration, evFlags);
                    }
                } else {
                    collect = false;
                }
            }
            while (collect);

            if (!_singing) {
                // Baseline declination ramp
                int userPitch = (_phonIndexTarg >= 0 && _phonIndexTarg < _dump.UserPitchBuf2.Length)
                                ? _dump.UserPitchBuf2[_phonIndexTarg] : 0;
                int baseLineOffset = _baselineStartOffset - (int)(_downRampOffset >> 16) + userPitch;
                if (baseLineOffset > _baselineEndOffset) {
                    _downRampOffset += _downRampStep;
                }

                // Tilt excursion for this frame, smoothed with a one-pole IIR (alpha = 0.875, tau ~ 38ms)
                // to approximate the linear connections between events (Taylor 2000, eq. 13).
                // _punctOffset (boundary tone target) is folded in here so the same smoother
                // handles boundary transitions without a separate filter.
                int tiltExcursion = StepTilt() + _punctOffset;
                _dbgTiltExcursion = tiltExcursion;
                _tiltSmooth = (_tiltSmooth * 7 + tiltExcursion) >> 3;

                // Phoneme target advance (lookahead for phoneme pitch offsets)
                if (_timeIntoPhonTarg > _curPhonDurCc + _phonDurDelay
                    && _phonIndexTarg < _dump.PhonBuf2InIndex) {
                    _timeIntoPhonTarg -= _curPhonDurCc;
                    _phonIndexTarg++;
                    _curPhonDurCc = (_phonIndexTarg < _dump.DurBuf.Length) ? _dump.DurBuf[_phonIndexTarg] : 0;
                    _phonDurDelay = 0;

                    int curPhon = GetPhon(_phonIndexTarg);
                    long curCtrl = GetPhonCtrl(_phonIndexTarg);
                    uint curFlags = Tables.GetFeatureFlags(curPhon);
                    int nextPhon = GetPhon(_phonIndexTarg + 1);
                    uint nextFlags = Tables.GetFeatureFlags(nextPhon);

                    int phonPitchOffset = Tables.GetPitch(curPhon) >> 1;

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

                // Scale the intonation contour by pitch range.
                // Only the tilt excursion and declination baseline are intentional intonation
                // gestures and should grow with pitch range. Phoneme-level micro-features
                // (_phonPitchOffset1, _uvPhonPitchTarg, micro-dip) are acoustic side effects
                // of articulation and must be applied after range scaling so they stay
                // constant in magnitude regardless of the voice's pitch range setting.
                _dbgBaselineOffset = baseLineOffset;
                int totalOffset = (int)(((long)(_tiltSmooth + baseLineOffset) * _vpIntonation) >> 16);
                totalOffset = (short)totalOffset; // preserve C short-truncation behaviour
                _dbgTotalOffset = totalOffset;
                _controlF0 = (int)((((long)totalOffset * _vpPitchRange) >> 16) + _vpBaselinePitch);

                // Phoneme boundary micro-dip, scaled by intonation so it stays proportional
                // to the pitch range in use. Raw depth is -1 or -10 pitch units at the boundary.
                int pbIndex = _timeIntoPhonCp - _pitchBoundry;
                if (pbIndex < 0) {
                    pbIndex = -pbIndex;
                }
                const int kPbWindow = 45 / kFrameTime; // 9
                if (pbIndex <= kPbWindow) {
                    int dipDepth = _lowGainCp ? (pbIndex - 1) : (pbIndex - 5);
                    _controlF0 += (int)((dipDepth * _vpIntonation) >> 16);
                }

                // Phoneme timbre offsets (range-independent): onset spike decaying across phoneme.
                // Voiced phonemes use _phonPitchOffset1; unvoiced use _uvPhonPitchTarg.
                // Both decay at the same rate and are applied after range scaling so they
                // contribute a fixed pitch-unit magnitude regardless of _vpPitchRange.
                _controlF0 += _phonPitchOffset1;
                _phonPitchOffset1 = (int)(((long)_phonPitchOffset1 * 98 * pct) >> 16);
                _controlF0 += _uvPhonPitchTarg;
                _uvPhonPitchTarg = (int)(((long)_uvPhonPitchTarg * 98 * pct) >> 16);

                // Vibrato
                _vibratoPhase1 = (int)(_vibratoPhase1 + _vibratoFreq) & 0x00FFFFFF;
                double phaseNorm = (double)_vibratoPhase1 / 16777216.0;
                double angle = phaseNorm * 2.0 * Math.PI;
                int vibrato = (int)(Math.Sin(angle) * 128.0);

                if (_speechRate >= 100) {
                    _controlF0 += (int)((vibrato * _vibratoDepth1) >> 16);
                } else {
                    _controlF0 += (int)((vibrato * _vibratoDepth2) >> 16);
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
                    _portamentoAccum = (long)_vpBaselinePitch << 16;
                    _newSentence = false;
                    _newPortaTarget = false;
                } else if (_newPortaTarget) {
                    if (_portamentoStep > 0) {
                        _portamentoAccum += _portamentoStep;
                        if ((_portamentoAccum >> 16) >= _vpBaselinePitch) {
                            _portamentoAccum = (long)_vpBaselinePitch << 16;
                            _newPortaTarget = false;
                        }
                    } else if (_portamentoStep < 0) {
                        _portamentoAccum += _portamentoStep;
                        if ((_portamentoAccum >> 16) < _vpBaselinePitch) {
                            _portamentoAccum = (long)_vpBaselinePitch << 16;
                            _newPortaTarget = false;
                        }
                    } else if (_singing) {
                        long target = (long)_vpBaselinePitch << 16;
                        long diff = target - _portamentoAccum;
                        _portamentoAccum += diff >> 2;
                        if (diff > -0x10000L && diff < 0x10000L) {
                            _portamentoAccum = target;
                            _newPortaTarget = false;
                        }
                    } else {
                        _portamentoAccum = (long)_vpBaselinePitch << 16;
                        _newPortaTarget = false;
                    }
                }

                _controlF0 = (int)(_portamentoAccum >> 16);

                _vibratoPhase1 = (int)((_vibratoPhase1 + _vibratoFreq) & 0xFFFFFF);
                double phaseNorm = (double)_vibratoPhase1 / 16777216.0;
                double angle = phaseNorm * 2.0 * Math.PI;
                int vibrato = (int)(Math.Sin(angle) * 128.0);

                if (!_hzGlide && _musicalNoteActive) {
                    long depth = (_curPhonCtrlSinging & kLowVibrato) != 0 ? _vibratoDepth2 : _vibratoDepth1;
                    _controlF0 += (int)((vibrato * depth) >> 16);
                }
            }

            if (_controlF0 < 0) {
                _controlF0 = 0;
            }

            _curPitchBufTime++;
            _timeIntoPhonTarg++;
            _timeIntoPhonCp++;
        }
    }
}  // namespace
