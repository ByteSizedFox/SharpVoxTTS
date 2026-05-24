#nullable enable

namespace SharpTalk {

    partial class SpeechRenderer {
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
        private void InsertBurst() {
            if ((_curPhonFlags & kPlosiveF) != 0) {
                int burstDur = Tables.BurstDurationTable[_curPhon] / kFrameTime;
                if ((_curPhonFlags & kStopF) != 0 && (_curPhonFlags & kVoicedF) == 0) {
                    if ((_nextPhonFlags & (kStopF | kNasalF)) != 0) {
                        burstDur = (_nextPhonCtrl & kPrimOrEmphStress) != 0 ? 0 : burstDur >> 1;
                    }
                }
                int closureDur = _curPhonDur - burstDur;
                if ((_curPhonFlags & kAffricateF) != 0 && closureDur > 80 / kFrameTime) {
                    closureDur = 80 / kFrameTime;
                }
                for (int i = kAp2; i <= kAB; i++) {
                    _cb[i].onset_END_TIME = closureDur;
                    _cb[i].onset_VAL = 0;
                }
            }

            if ((_prevPhonFlags & kStopF) != 0 && (_prevPhonFlags & kVoicedF) == 0 && (_curPhonFlags & kSonorant1F) != 0) {
                // Aspirated release from a preceding voiceless stop into this sonorant.
                // Lisker & Abramson (1964) Table 6: American English, 4-speaker averages:
                //   /p/ 58ms  /t/ 70ms  /k/ 80ms
                // Front vowels use a lower AF amplitude because the high-frequency spectral
                // tilt of the front vocal tract already emphasizes the aspiration region.
                int rel;
                if ((_prevPhonFlags & kLabialF) != 0) {
                    rel = 58 / kFrameTime;
                } else if ((_prevPhonFlags & kVelar) != 0) {
                    rel = 80 / kFrameTime;
                } else {
                    rel = 70 / kFrameTime;
                }
                _cb[kAV].onset_VAL = 0;
                _cb[kAF].onset_VAL = (short)(Tables.GetForwardRank(_nextPhon) == kFrontR ? 48 : 54);
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
                _cb[kBW1].onset_VAL = (short)(_cb[kBW1].curP_START_Targ + 250);
                _cb[kBW2].onset_VAL = (short)(_cb[kBW2].curP_START_Targ + 70);
            }

            if ((_curPhonFlags & kStopF) != 0 && (_curPhonFlags & kVoicedF) != 0 &&
                (_prevPhonFlags & kVoicedF) != 0 && (_nextPhonFlags & kVoicedF) == 0 && _curPhon != _TX_) {
                // Voiced stop before a devoiced context: pre-devoicing murmur.
                // The arytenoids begin abducting before the stop closure completes, producing
                // weak breathy voicing with raised bandwidths throughout the closure.
                _cb[kAV].onset_END_TIME = _curPhonDur - (10 / kFrameTime);
                _cb[kBW1].onset_END_TIME = _cb[kBW2].onset_END_TIME = _cb[kBW3].onset_END_TIME = _curPhonDur;
                _cb[kAV].onset_VAL = 56;
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
        private void InterpolateControls() {
            // Advance Klattsch parameters by one linear step
            for (int i = 0; i < 7; i++) {
                _curKlattsch[i] += _klattschStep[i];
            }

            // Frequency and FNZ blocks: combined offset accumulator, shifted at end
            for (int i = kF1; i <= kFNZ; i++) {
                var cb = _cb[i];
                if (cb.ptrToTargetList >= 0 && _durDoneInPhon > cb.curTarget_TIME) {
                    int p = cb.ptrToTargetList;
                    cb.curTarget_TIME = _diphEntries[p++];
                    cb.curTarget_STEP = _diphEntries[p++];
                    cb.ptrToTargetList = p;
                    cb.curP_START_Targ += (short)(cb.curTarget_OFFS >> kStepSizeRes);
                    cb.curTarget_OFFS = 0;
                }
                cb.curTarget_OFFS += cb.curTarget_STEP;

                int offset = cb.curTarget_OFFS + cb.HEAD_offs;
                if (cb.HEAD_offs != 0) {
                    cb.HEAD_offs -= cb.HEAD_step;
                }
                if (_durDoneInPhon >= cb.TAIL_START_time) {
                    offset += cb.TAIL_offs;
                    cb.TAIL_offs += cb.TAIL_step;
                }

                _controlData[i] = (short)(cb.curP_START_Targ + (offset >> kStepSizeRes));
            }

            // Amplitude blocks: HEAD and TAIL applied independently
            for (int i = kAV; i <= kAB; i++) {
                var cb = _cb[i];
                int val = cb.curP_START_Targ + (cb.HEAD_offs >> kStepSizeRes);
                if (cb.HEAD_offs != 0) {
                    cb.HEAD_offs -= cb.HEAD_step;
                }
                if (_durDoneInPhon >= cb.TAIL_START_time) {
                    val += cb.TAIL_offs >> kStepSizeRes;
                    cb.TAIL_offs += cb.TAIL_step;
                }
                _controlData[i] = (short)val;

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
        private Frame SaveFrame(short f0, byte phonCtrl) {
            var f = new Frame();
            f.F0 = f0;

            short curF1 = (short)(_controlData[kF1] * _tractScale);
            short curF2 = (short)(_controlData[kF2] * _tractScale);
            short curF3 = (short)(_controlData[kF3] * _tractScale);

            while (curF2 - curF1 < 200) {
                curF1 -= 10;
            }
            while (curF3 - curF2 < 600) {
                curF3 += 10;
            }

            f.F1 = KlattSynthesizer.HzToPitch(curF1);
            f.F2 = KlattSynthesizer.HzToPitch(curF2);
            f.F3 = KlattSynthesizer.HzToPitch(curF3);
            f.Bw1 = (short)((_controlData[kBW1] * _voiceBWgain1) >> 16);
            f.Bw2 = (short)((_controlData[kBW2] * _voiceBWgain2) >> 16);
            f.Bw3 = (short)((_controlData[kBW3] * _voiceBWgain3) >> 16);
            f.FNZ = KlattSynthesizer.HzToPitch((short)(_controlData[kFNZ] * _tractScale));
            f.Av = (short)(LogToLin(_controlData[kAV]) * _tractScale);
            f.Af = LogToLin(_controlData[kAF]);
            f.A2 = LogToLin(_controlData[kAp2]);
            f.A3 = LogToLin(_controlData[kAp3]);
            f.A4 = LogToLin(_controlData[kAp4]);
            f.A5 = LogToLin(_controlData[kAp5]);
            f.A6 = LogToLin(_controlData[kAp6]);
            f.AB = LogToLin(_controlData[kAB]);
            f.PhonEdge = (short)(_durDoneInPhon == 0 ? 1 : 0);

            f.Aspiration = (byte)(_curKlattsch[kAspIdx] >> 16);
            f.Tilt = (byte)(_curKlattsch[kTiltIdx] >> 16);
            f.Effort = (byte)(_curKlattsch[kEffIdx] >> 16);
            f.VibDepth = (byte)(_curKlattsch[kVibDIdx] >> 16);
            f.VibRate = (byte)(_curKlattsch[kVibRIdx] >> 16);
            f.TremDepth = (byte)(_curKlattsch[kTremDIdx] >> 16);
            f.TremRate = (byte)(_curKlattsch[kTremRIdx] >> 16);

            return f;
        }
    }

}  // namespace
