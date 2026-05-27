#ifndef SHARPVOX_SPEECH_RENDERER_H
#define SHARPVOX_SPEECH_RENDERER_H

#include <cstdint>
#include <functional>
#include <vector>
#include "../include/SynthData.h"
#include "../include/VoiceData.h"
#include "../include/Tables.h"
#include "../include/KlattSynthesizer.h"
#include "../include/PitchInterpolator.h"

namespace SharpVox {

// Converts phonemes into KlattSynthesizer frames using Klatt (1980) and Klatt & Klatt (1990) models.
//
// Parallel control blocks with HEAD and TAIL ramps ensure continuous parameter trajectory blending.
//
// Acoustic locus theory (Liberman 1954, Delattre 1955) used for F2 formant transitions.
//
// Diphthong and glide nuclei (Lehiste & Peterson 1961) modeled as 4-point formant trajectories.
//
// Plosive release voicing timed using Voice-Onset-Time (Lisker & Abramson 1964, Klatt 1975).
class SpeechRenderer {
public:
    // Control block indices
    static constexpr int32_t kF1 = 0;  static constexpr int32_t kF2 = 1;  static constexpr int32_t kF3 = 2;
    static constexpr int32_t kBW1 = 3; static constexpr int32_t kBW2 = 4; static constexpr int32_t kBW3 = 5;
    static constexpr int32_t kFNZ = 6; static constexpr int32_t kAV = 7;  static constexpr int32_t kAF = 8;
    static constexpr int32_t kAp2 = 9; static constexpr int32_t kAp3 = 10; static constexpr int32_t kAp4 = 11;
    static constexpr int32_t kAp5 = 12; static constexpr int32_t kAp6 = 13; static constexpr int32_t kAB = 14;

    // Block type constants (match Tables.ControlBlockTypeTable)
    static constexpr int32_t kFreqType = 0;    static constexpr int32_t kBWType = 1;
    static constexpr int32_t kFNZType = 2;     static constexpr int32_t kSourceAmpType = 3;
    static constexpr int32_t kResonAmpType = 4;

    explicit SpeechRenderer(const VoiceData& voice);

    // Converts 6-bit log-domain amplitude (Klatt 1980) to a linear multiplier via lookup.
    static int16_t LogToLin(int16_t v);

    void RenderStreaming(const SynthInputDump& dump, std::function<void(const Frame&)> callback);
    std::vector<Frame> Render(const SynthInputDump& dump);

private:
    struct ControlBlock {
        int16_t curP_START_Targ  = 0;
        int16_t curP_END_Targ    = 0;
        int16_t prevP_END_Targ   = 0;
        int16_t nextP_START_Targ = 0;
        int32_t curTarget_TIME   = 0;
        int32_t curTarget_STEP   = 0;
        int32_t curTarget_OFFS   = 0;
        int32_t HEAD_offs        = 0;
        int32_t HEAD_step        = 0;
        int32_t TAIL_offs        = 0;
        int32_t TAIL_step        = 0;
        int32_t TAIL_START_time  = 0;
        int32_t onset_END_TIME   = 0;
        int16_t onset_VAL        = 0;
        int32_t ptrToTargetList  = -1; // index into _diphEntries; -1 = no list
    };

    VoiceData _voice;
    const SynthInputDump* _dump = nullptr;

    ControlBlock _cb[15];
    int16_t _controlData[15]  = {};
    int16_t _diphEntries[4096] = {};
    int32_t _nextDiphEntryIdx  = 0;

    // Current-phoneme context
    int32_t _curPhon = 0, _prevPhon = 0, _nextPhon = 0, _prev2Phon = 0;
    uint32_t _curPhonFlags = 0, _prevPhonFlags = 0, _nextPhonFlags = 0, _prev2PhonFlags = 0;
    int32_t _curPhonCtrl = 0, _prevPhonCtrl = 0, _nextPhonCtrl = 0, _prev2PhonCtrl = 0;
    int32_t _curPhonDur    = 0;
    int32_t _curPhonMaxDur = 0;
    int64_t _curPhonPctOfMaxDur  = 0;
    int64_t _curPhonPctOfMaxDur1 = 0;
    int64_t _curPhonPctOfMaxDur2 = 0;

    // Shared state written by InitCtrlsForNewPhon and consumed by HeadRules/TailRules
    int32_t _transLevel    = 0;
    int32_t _transTime     = 0;
    int32_t _curBlockIndex = 0;

    int32_t _durDoneInPhon  = 0;
    int32_t _curPhonBufIndex = 0;
    bool _startingNewPhon    = true;
    bool _bigBang            = true;

    // Klattsch parameters in 16.16 fixed point, linearly interpolated per frame.
    // Parameterization follows the KLSYN88 source model (Klatt & Klatt 1990).
    int32_t _curKlattsch[7]  = {};
    int32_t _klattschStep[7] = {};
    static constexpr int32_t kAspIdx  = 0; static constexpr int32_t kTiltIdx  = 1;
    static constexpr int32_t kEffIdx  = 2; static constexpr int32_t kVibDIdx  = 3;
    static constexpr int32_t kVibRIdx = 4; static constexpr int32_t kTremDIdx = 5;
    static constexpr int32_t kTremRIdx = 6;

    static constexpr int32_t kNumOfBlocks = 15;

    static constexpr int32_t kNoValue     = -1;
    static constexpr int32_t kMaxBandWidth = 1000;
    static constexpr int32_t C_V_type     = 0;
    static constexpr int32_t V_C_type     = 1;
    static constexpr int32_t kFrontR      = 0; static constexpr int32_t kMiddleR = 1;
    static constexpr int32_t kBackR       = 2; static constexpr int32_t kRoundR  = 3;
    static constexpr int32_t kConsonantR  = 4;
    static constexpr int32_t kStepSizeRes = 3;
    static constexpr int32_t k1pct        = 655;
    static constexpr int32_t kFrameTime   = 5;
    static constexpr int32_t ReciprocalTableSize = 100;
    static constexpr int32_t kOneHalf     = 0x8000;

    // Phoneme feature flags
    static constexpr uint32_t kVowelF      = 1u << 0;  static constexpr uint32_t kConsonantF  = 1u << 1;
    static constexpr uint32_t kVoicedF     = 1u << 2;  static constexpr uint32_t kVowel1F     = 1u << 3;
    static constexpr uint32_t kSonorantF   = 1u << 4;  static constexpr uint32_t kSonorant1F  = 1u << 5;
    static constexpr uint32_t kNasalF      = 1u << 6;  static constexpr uint32_t kLiqGlideF   = 1u << 7;
    static constexpr uint32_t kSonorConsonF = 1u << 8; static constexpr uint32_t kPlosiveF    = 1u << 9;
    static constexpr uint32_t kPlosFricF   = 1u << 10; static constexpr uint32_t kObstF       = 1u << 11;
    static constexpr uint32_t kStopF       = 1u << 12; static constexpr uint32_t kAlveolarF   = 1u << 13;
    static constexpr uint32_t kVelar       = 1u << 14; static constexpr uint32_t kLabialF     = 1u << 15;
    static constexpr uint32_t kDentalF     = 1u << 16; static constexpr uint32_t kPalatalF    = 1u << 17;
    static constexpr uint32_t kYGlideStartF = 1u << 18; static constexpr uint32_t kYGlideEndF = 1u << 19;
    static constexpr uint32_t kGStopF      = 1u << 20; static constexpr uint32_t kFrontF      = 1u << 21;
    static constexpr uint32_t kDiphthongF  = 1u << 22; static constexpr uint32_t kAffricateF  = 1u << 24;
    static constexpr uint32_t kLiqGlide2F  = 1u << 25; static constexpr uint32_t kVocLiq      = 1u << 26;
    static constexpr uint32_t kFric        = 1u << 27;

    // Phoneme control field masks
    static constexpr int32_t kPlosive_Release  = 0x4000;
    static constexpr int32_t kPrimOrEmphStress = 0x1400;
    static constexpr int32_t kStressField      = 0x1C00;
    static constexpr int32_t kSyllableTypeField = 0x000F;

    // Voice-dependent tables and parameters
    const int16_t* _envelopeListTable        = nullptr;
    const int16_t* _locusTable               = nullptr;
    const int16_t* _voiceAmplitudeVoicingTable = nullptr;
    const int16_t* _voiceNoiseAmplitudeTable = nullptr;
    int32_t _nasalTargFreq    = 0;
    int32_t _nasalBaseFreq    = 0;
    int32_t _locusOffset      = 0;
    int32_t _voiceBWgain1     = 0;
    int32_t _voiceBWgain2     = 0;
    int32_t _voiceBWgain3     = 0;
    float   _tractScale       = 1.0f;
    uint8_t _voiceTremDepth   = 0;
    uint8_t _voiceTremRate    = 0;
    int32_t _voiceOnsetHardness = 0;
    bool    _male             = false;

    // Phoneme number aliases (populated in constructor from AudioProcessor constants)
    int32_t _IY_, _ER_, _AY_, _OY_, _UW_, _YU_, _SIL_, _LX_, _EL_, _EN_;
    int32_t _W_, _Y_, _R_, _L_, _HH_, _M_, _N_, _NG_;
    int32_t _F_, _V_, _TH_, _DH_, _S_, _Z_, _SH_, _ZH_;
    int32_t _P_, _B_, _T_, _D_, _K_, _G_, _CH_, _JH_;
    int32_t _TX_, _DX_, _QX_, _DD_;

    // --- Internal helpers ---

    void SetPhonContext(int32_t index);

    // Safe phoneme buffer access; returns SIL for out-of-range indices
    int32_t GP(int32_t i) const;

    // Phoneme feature flags; returns 0 for out-of-range phoneme indices
    uint32_t PF(int32_t p) const;

    // Phoneme control field; returns 0 for out-of-range indices
    int32_t PC(int32_t i) const;

    // One-over-x from reciprocal table for small x, or computed directly for large x
    int32_t OvX(int32_t x) const;

    // --- Frame.cs ---

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
    void InsertBurst();

    // Steps the 15 control blocks forward by one frame, applying HEAD and TAIL ramps
    // and diphthong trajectory stepping.
    //
    // Frequency blocks (F1-FNZ) sum the diphthong step, HEAD ramp, and TAIL ramp
    // offsets in the accumulator before a single final shift, preserving fixed-point
    // accuracy. Amplitude blocks (AV-AB) apply HEAD and TAIL independently, then
    // overlay the onset_VAL burst or aspiration window if still active.
    void InterpolateControls();

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
    Frame SaveFrame(int16_t f0, uint8_t phonCtrl);

    // --- Locus.cs ---

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
    void GetLocus(int32_t iCons, int32_t iVowel, int32_t bType);

    // --- Targets.cs ---

    // Initializes per-phoneme duration ratios used to scale transition times.
    //
    // The ratio of actual duration to canonical maximum duration is a key factor in
    // vowel reduction: shorter (unstressed) phonemes undershoot their formant targets
    // because the articulators do not have time to fully reach them.
    void FillPhonTargets();

    // Returns the raw table value for the current block at the given phoneme index.
    // Values >= 0 are direct Hz targets; kNoValue (-1) means no target for this phoneme;
    // values < kNoValue are diphthong envelope indices encoded as negative offsets.
    int16_t GetTargetRaw(int32_t index);

    // Returns the onset (first) target Hz value for the given phoneme index
    int32_t GetFirstTarget(int32_t index);

    // Returns the final target Hz value for the given phoneme index
    int32_t GetLastTarget(int32_t index);

    int16_t GetVoiceFormantValue(int32_t bi, int32_t phonemeId) const;

    // Sets up a 4-point interpolated formant trajectory for diphthong and glide phonemes.
    //
    // Diphthongs are characterized by continuous formant movement between two targets;
    // the trajectory is stored as a pair (p1,t1) -> (p2,t2) in the _diphEntries buffer.
    // [Lehiste & Peterson 1961]
    //
    // Coarticulatory adjustments are applied to both endpoints so that the glide into
    // and out of adjacent phonemes is smooth.
    void GetDiphthongs(int32_t index);

    // Scales a duration percentage by the ratio of actual-to-canonical phoneme duration.
    // Produces a frame count proportional to how long this phoneme actually lasts.
    int32_t ScalePrcnt(int32_t pct);

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
    int32_t AdjustColored(int32_t index, int32_t entry);

    // --- Transitions.cs ---

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
    void InitCtrlsForNewPhon();

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
    void HeadRules(ControlBlock& cb, int32_t bt);

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
    void TailRules(ControlBlock& cb, int32_t bt);
};

}  // namespace SharpVox

#endif  // SHARPVOX_SPEECH_RENDERER_H
