#include "../include/pitch_interpolator.h"
#include "../include/synth_data.h"
#include "../include/voice_data.h"
#include "../include/tables.h"
#include <cmath>
#include <cstdint>

namespace SharpTalk {

    PitchInterpolator::PitchInterpolator(const SynthInputDump& dump)
        : _dump(dump)
    {
        const PitchState& s = dump.Pitch;

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

        _controlF0 = 0;
        _curPhonCtrlSinging = 0;
        _dbgTiltExcursion = 0;
        _dbgBaselineOffset = 0;
        _dbgTotalOffset = 0;
        _punctOffset = 0;
    }

    int16_t PitchInterpolator::Step() {
        Interpolate_Pitch();
        return (int16_t)_controlF0;
    }

    void PitchInterpolator::DoNote(int32_t phonIndex) {
        _hzGlide = false;
        _curPhonCtrlSinging = GetPhonCtrl(phonIndex);

        int64_t ctrl = (phonIndex >= 0 && phonIndex < (int32_t)_dump.PhonCtrlBuf2.size())
                       ? _dump.PhonCtrlBuf2[phonIndex] : 0;

        if ((ctrl & kSingingPhon) == 0) {
            _musicalNoteActive = false;
        }

        int16_t note = (phonIndex >= 0 && phonIndex < (int32_t)_dump.UserNoteBuf2.size())
                       ? _dump.UserNoteBuf2[phonIndex] : (int16_t)0;

        if (note != 0 && (ctrl & kSilenceDuration) == 0) {
            if ((ctrl & kSingingPhon) != 0) {
                if (note < 0) {
                    int32_t targetPitch = HzToPitch(-note);
                    int32_t curPitch = (int32_t)(_portamentoAccum >> 16);
                    int32_t frames = (phonIndex < (int32_t)_dump.DurBuf.size()) ? _dump.DurBuf[phonIndex] : 1;
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
        static constexpr int32_t kLogTableLen = 512; // matches LogarithmBase2Table length in Tables.cs
        if (ratio >= kLogTableLen) {
            ratio = kLogTableLen - 1;
        }
        return Tables::LogarithmBase2Table[ratio] + fk;
    }

    int16_t PitchInterpolator::GetPhon(int32_t index) const {
        if (index >= 0 && index < _dump.PhonBuf2InIndex) {
            return _dump.PhonBuf2[index];
        }
        return _SIL_;
    }

    int64_t PitchInterpolator::GetPhonCtrl(int32_t index) const {
        if (index >= 0 && index < _dump.PhonBuf2InIndex) {
            return _dump.PhonCtrlBuf2[index];
        }
        return 0;
    }

    void PitchInterpolator::Phon_Boundry_Pitch() {
        if (_timeIntoPhonCp >= _curPhonDurCp) {
            _timeIntoPhonCp -= _curPhonDurCp;
            _phonIndexCp++;
            _curPhonDurCp = (_phonIndexCp < (int32_t)_dump.DurBuf.size()) ? _dump.DurBuf[_phonIndexCp] : 0;

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

    // Parabolic synthesis for one rise or fall component (Taylor 2000, eq. 12).
    //
    //   f0(t) = A_abs + A - 2A*(t/D)^2    for 0 <= t < D/2
    //   f0(t) = A_abs + 2A*(1-t/D)^2      for D/2 <= t <= D
    //
    // The curve always starts at (A_abs + A) and ends at A_abs.
    // A < 0: ascending (rise) curve;  A > 0: descending (fall) curve.
    // 8-bit fixed point is used for t/D to avoid overflow while preserving precision.
    int32_t PitchInterpolator::TiltSynth(int32_t a, int32_t aAbs, int32_t frame, int32_t dur) {
        if (dur <= 0) {
            return aAbs;
        }
        int32_t tD8 = (frame << 8) / dur;   // t/D in 8.8 fixed point (0..255)
        int32_t twoA = a * 2;
        if (frame * 2 < dur) {
            int32_t tD2 = (tD8 * tD8) >> 8; // (t/D)^2 in 8.8 fixed point
            return aAbs + a - ((twoA * tD2) >> 8);
        } else {
            int32_t omtD = (1 << 8) - tD8; // (1-t/D) in 8.8 fixed point
            int32_t omtD2 = (omtD * omtD) >> 8;
            return aAbs + ((twoA * omtD2) >> 8);
        }
    }

    // Returns the current Tilt excursion (pitch units above/below baseline) and
    // advances the Tilt state machine by one frame.
    int32_t PitchInterpolator::StepTilt() {
        if (_tiltPhase == 0) {
            return _tiltHeldLevel;
        }

        int32_t excursion = TiltSynth(_tiltA, _tiltAbs, _tiltFrame, _tiltPhaseDur);
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
    void PitchInterpolator::FireTiltEvent(int32_t amplitude, int32_t tiltX64, int32_t duration, int32_t flags) {
        // Convert Tilt -> RFC components (Taylor 2000, eqs 8-11)
        int32_t aRise = amplitude * (64 + tiltX64) / 128;
        int32_t aFall = amplitude * (64 - tiltX64) / 128;
        int32_t dRise = duration * (64 + tiltX64) / 128;
        int32_t dFall = duration * (64 - tiltX64) / 128;

        // Nuclear events (kPitchRiseFall_Flg) update _tiltHeldLevel permanently.
        // Transient events (stress, head, boundary) restore _tiltHeldLevel afterwards.
        bool isNuclear = (flags & kPitchRiseFall_Flg) != 0;

        int32_t held = _tiltHeldLevel;

        // Sample the current excursion so new events can start from wherever the
        // curve is right now.  This eliminates audible discontinuities when a new
        // event fires mid-curve, regardless of whether the incoming event is nuclear
        // or transient.  Endpoints (_tiltAbs / _tiltFallAbs) are always anchored to
        // _tiltHeldLevel so the nuclear accent level is preserved after completion.
        int32_t curExcursion = _tiltPhase != 0
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

    void PitchInterpolator::Interpolate_Pitch() {
        // Pitch buffer event collection loop: fire all events due this frame.
        bool collect = true;
        do {
            if (_curPitchBufTime >= _nextPitchBufTime
                && _pitchBufOutIndex < (int32_t)_dump.PitchBufInIndex) {
                int32_t evAmp = _dump.PitchBufFreq[_pitchBufOutIndex];
                int32_t evFlags = _dump.PitchBufFlags[_pitchBufOutIndex];
                int32_t evTiltX64 = _dump.PitchBufTiltX64[_pitchBufOutIndex];
                int32_t evDuration = _dump.PitchBufDuration[_pitchBufOutIndex];

                _curPitchBufTime -= _nextPitchBufTime;
                _pitchBufOutIndex++;
                _nextPitchBufTime = _dump.PitchBufTime[_pitchBufOutIndex];

                if ((evFlags & kResetDecline) != 0) {
                    _downRampOffset = 0;
                } else if ((evFlags & kPhraseReset) != 0) {
                    _downRampOffset = (int64_t)(_baselineStartOffset - _baselineEndOffset) << 14;
                    if (_curRamp < (int32_t)_rampSteps.size() - 1) {
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
            int32_t userPitch = (_phonIndexTarg >= 0 && _phonIndexTarg < (int32_t)_dump.UserPitchBuf2.size())
                                ? _dump.UserPitchBuf2[_phonIndexTarg] : 0;
            int32_t baseLineOffset = _baselineStartOffset - (int32_t)(_downRampOffset >> 16) + userPitch;
            if (baseLineOffset > _baselineEndOffset) {
                _downRampOffset += _downRampStep;
            }

            // Tilt excursion for this frame, smoothed with a one-pole IIR (alpha = 0.875, tau ~ 38ms)
            // to approximate the linear connections between events (Taylor 2000, eq. 13).
            // _punctOffset (boundary tone target) is folded in here so the same smoother
            // handles boundary transitions without a separate filter.
            int32_t tiltExcursion = StepTilt() + _punctOffset;
            _dbgTiltExcursion = tiltExcursion;
            _tiltSmooth = (_tiltSmooth * 7 + tiltExcursion) >> 3;

            // Phoneme target advance (lookahead for phoneme pitch offsets)
            if (_timeIntoPhonTarg > _curPhonDurCc + _phonDurDelay
                && _phonIndexTarg < _dump.PhonBuf2InIndex) {
                _timeIntoPhonTarg -= _curPhonDurCc;
                _phonIndexTarg++;
                _curPhonDurCc = (_phonIndexTarg < (int32_t)_dump.DurBuf.size()) ? _dump.DurBuf[_phonIndexTarg] : 0;
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

            // Scale the intonation contour by pitch range.
            // Only the tilt excursion and declination baseline are intentional intonation
            // gestures and should grow with pitch range. Phoneme-level micro-features
            // (_phonPitchOffset1, _uvPhonPitchTarg, micro-dip) are acoustic side effects
            // of articulation and must be applied after range scaling so they stay
            // constant in magnitude regardless of the voice's pitch range setting.
            _dbgBaselineOffset = baseLineOffset;
            int32_t totalOffset = (int32_t)(((int64_t)(_tiltSmooth + baseLineOffset) * _vpIntonation) >> 16);
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

            // Phoneme timbre offsets (range-independent): onset spike decaying across phoneme.
            // Voiced phonemes use _phonPitchOffset1; unvoiced use _uvPhonPitchTarg.
            // Both decay at the same rate and are applied after range scaling so they
            // contribute a fixed pitch-unit magnitude regardless of _vpPitchRange.
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
    }

}  // namespace SharpTalk
