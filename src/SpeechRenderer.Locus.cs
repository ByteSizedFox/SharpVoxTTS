#nullable enable

namespace SharpTalk {

    partial class SpeechRenderer {
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
        private void GetLocus(int iCons, int iVowel, int bType) {
            if (_curBlockIndex < kF1 || _curBlockIndex > kF3) {
                return;
            }

            int cons = GP(iCons); int vow = GP(iVowel);
            int vowRank, consRank;

            if (bType == C_V_type) {
                vowRank = Tables.GetForwardRank(vow);
                consRank = Tables.GetBackwardRank(cons);
            } else {
                vowRank = Tables.GetBackwardRank(vow);
                consRank = Tables.GetForwardRank(cons);
            }

            // Only apply locus adjustment when the adjacent segment is a consonant at a
            // known place of articulation and the current segment is a vowel-like sound
            if (consRank != kConsonantR || vowRank == kConsonantR) {
                return;
            }

            uint vf = PF(vow); uint cf = PF(cons);
            bool f2y = (vf & kYGlideStartF) != 0;

            int v1Targ = (bType == C_V_type) ? GetFirstTarget(iVowel) : GetLastTarget(iVowel);

            // Select locus table entry based on vowel height/backness category.
            // Delattre et al. (1955) found that the g/k locus is well-defined only for
            // front vowels; for back vowels the locus relationship breaks down, so separate
            // front, middle, and back entries capture this place-vowel interaction.
            int lociIdx = vowRank switch {
                kFrontR => Tables.FrontLocusTable[cons],
                kMiddleR => Tables.MiddleLocusTable[cons],
                _ => Tables.BackLocusTable[cons]
            };

            if (lociIdx == kNoValue) {
                return;
            }

            // Each entry occupies two consecutive slots per formant: [F1, F2, F3] x 3 fields
            lociIdx = (lociIdx >> 1) + (_curBlockIndex - kF1) * 3;
            int lFreq = _locusTable[lociIdx++] + _locusOffset;
            int lPcnt = _locusTable[lociIdx++];
            _transTime = _locusTable[lociIdx] / kFrameTime;

            if (_curBlockIndex == kF2) {
                // Sussman, McCaffrey & Matthews (1991) Table I group means, N=20 speakers:
                //   F2_onset = slope * F2_nucleus + intercept  (all in Hz)
                //   Labial:   slope=0.89  intercept=99 Hz   (slopeQ15=29163)
                //   Alveolar: slope=0.42  intercept=1211 Hz (slopeQ15=13763)
                //   Velar:    slope=0.71  intercept=792 Hz  (slopeQ15=23266)
                int slopeQ15, intercept;
                if ((cf & kLabialF) != 0) {
                    slopeQ15 = 29163;
                    intercept = 99 + _locusOffset;
                    // Lehiste & Peterson (1961) Table III: labials ~5.1 cs
                    _transTime = 55 / kFrameTime;
                } else if ((cf & kVelar) != 0) {
                    slopeQ15 = 23266;
                    intercept = 792 + _locusOffset;
                    // Lehiste & Peterson (1961) Table III: velars ~7.8-8.8 cs
                    _transTime = 80 / kFrameTime;
                } else {
                    slopeQ15 = 13763;
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
    }

}  // namespace
