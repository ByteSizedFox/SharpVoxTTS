#ifndef SHARPVOX_PITCH_INTERPOLATOR_H
#define SHARPVOX_PITCH_INTERPOLATOR_H

#include <cstdint>
#include <vector>
#include "PhonemeDefs.h"
#include "SynthData.h"
#include "VoiceData.h"
#include "FujisakiPitchModel.h"

// windows builds lack M_PI
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SharpVox {

    // Generates F0 via the Fujisaki-Bartman model (phrase + accent commands,
    // ported from TGSpeechBox) or portamento for singing.  Pitch-buffer events
    // fire the phrase/accent filters; the clause is centered on its voiced-frame
    // mean so Intonation/PitchRange scale the movement, not the level.
    class PitchInterpolator {
    public:
        explicit PitchInterpolator(const ClausePlan& plan);

        int16_t Step();

        void DoNote(int32_t phonIndex);

        // Debug accessors - valid immediately after Step().
        int32_t DbgF0() const { return _controlF0; }
        int32_t DbgFujiExcursion() const { return _dbgFujiExcursion; }
        int32_t DbgPhraseResp() const { return _dbgPhraseResp; }
        int32_t DbgAccentResp() const { return _dbgAccentResp; }
        int32_t DbgBaselineOffset() const { return _dbgBaselineOffset; }
        int32_t DbgTotalOffset() const { return _dbgTotalOffset; }
        int32_t DbgPitchBufTime() const { return _curPitchBufTime; }
        int32_t DbgNextPitchBufTime() const { return _nextPitchBufTime; }
        int32_t DbgPitchBufOutIndex() const { return _pitchBufOutIndex; }

    private:
        const ClausePlan& _plan;

        // Pitch buffer tracking
        int16_t _nextPitchBufTime;
        int32_t _pitchBufOutIndex;
        int32_t _curPitchBufTime;

        // Phoneme advance (Targ path - lookahead for phoneme pitch offsets)
        int32_t _phonIndexTarg;
        int32_t _timeIntoPhonTarg;
        int32_t _curPhonDurCc;
        int32_t _phonDurDelay;

        // Phoneme advance (Cp path - phoneme boundary micro-dip)
        int32_t _phonIndexCp;
        int32_t _timeIntoPhonCp;
        int32_t _curPhonDurCp;

        // Phoneme pitch offsets
        int32_t _uvPhonPitchTarg;
        int32_t _phonPitchOffset1;

        // Declination ramp (linear in log-pitch = exponential F0 decay)
        int32_t _baselineStartOffset;
        int32_t _baselineEndOffset;
        int64_t _downRampOffset;
        int64_t _downRampStep;
        std::vector<int64_t> _rampSteps;
        int32_t _curRamp;

        // Voice parameters
        int64_t _vpIntonation;
        int64_t _vpPitchRange;
        int32_t _vpBaselinePitch;

        // Fujisaki model knobs (from plan.Pitch)
        int64_t _vpFujiPhraseAmp;             // Q16 nepers
        int64_t _vpFujiPrimaryAccentAmp;      // Q16 nepers
        int64_t _vpFujiHeadAccentAmp;         // Q16 nepers
        int64_t _vpFujiSecondaryAccentAmp;    // Q16 nepers
        int64_t _vpFujiEmphaticAccentAmp;     // Q16 nepers
        double  _vpFujiPitchCurveExp;         // prominence shaping exponent
        double  _vpFujiMonoAccentDurScale;    // mono-syllable accent duration scale
        double  _vpFujiCompoundStep;          // nepers
        int32_t _vpFujiFinalDropPitch;        // pitch units, statement final-vowel drop

        // Vibrato
        int64_t _vibratoDepth1;
        int64_t _vibratoDepth2;
        int64_t _vibratoFreq;
        int32_t _vibratoPhase1;

        // Flutter: two fixed-freq oscillators (~5 Hz, ~3 Hz), difference applied post-range-scale
        int32_t _flutterPhaseA;
        int32_t _flutterPhaseB;

        // Singing state
        bool _singing;
        bool _hzGlide;
        bool _musicalNoteActive;
        int64_t _portamentoAccum;
        int64_t _portamentoStep;
        bool _newPortaTarget;
        bool _newSentence;
        int32_t _speechRate;

        // Phoneme boundary micro-dip state
        int32_t _pitchBoundry;
        bool _lowGainCp;
        int32_t _pbHold;
        bool _pbLowGain;

        // Fujisaki-Bartman filter state (phrase + accent command filters)
        FujisakiPitchModel _fuji;
        // Smoothed boundary-tone offset (one-pole IIR over _punctOffset steps)
        int32_t _punctSmooth;
        int32_t _f0Smooth;
        bool _f0SmoothPrime;

        // Clause-final shaping data: last vowel window for the statement drop,
        // per-word vowel counts / primary tracking, frame->phoneme lookup.
        std::vector<int32_t> _wordNuclei;       // vowel-nucleus count per phoneme's word
        std::vector<bool>    _wordHadPrimary;   // a primary-stressed vowel appeared earlier in the word
        std::vector<int32_t> _phonStartFrame;   // cumulative start frame per phoneme (frame->phoneme lookup)
        int32_t _lastVowelStartFrame;
        int32_t _lastVowelDur;
        int32_t _clauseFrame;
        int32_t _phraseFireFrame;  // clause frame of the last phrase command fire

        // Voiced-frame mean offset, precomputed in BuildFujiProsody; subtracted
        // before Intonation/PitchRange scaling so PitchHz is the clause average.
        int32_t _fujiMeanOffset;

        // Boundary tone pitch offset (comma, question, tilde), smoothed by the IIR
        int32_t _punctOffset;

        int32_t _controlF0;
        int32_t _voiceNaturalPitch;
        int64_t _curPhonCtrlSinging;

        // Debug snapshot populated each Step() - zero during singing.
        int32_t _dbgFujiExcursion;
        int32_t _dbgPhraseResp;
        int32_t _dbgAccentResp;
        int32_t _dbgBaselineOffset;
        int32_t _dbgTotalOffset;

        // Constants
        static constexpr int32_t kStepSizeRes = 3;
        static constexpr int32_t kNeverHappens = -10000;
        static constexpr int32_t kFrameTime = 5;
        static constexpr int32_t pct = 655;
        static constexpr int32_t k100percent = 0x10000;
        // Accent pulse length window: short pulses never reach the filter's
        // amplitude, and the tail past the vowel links accents together.
        static constexpr int32_t kMinAccentPulseFrames = 25;
        static constexpr int32_t kMaxAccentPulseFrames = 60;

        // Clause-start de-accenting: scale early accents by (1 - scale*humpFrac)
        // where humpFrac tracks the phrase hump, so the first word doesn't stack
        // phrase lift + accent into a near-octave spike.
        static constexpr double kPhraseStartAccentScale = 0.70;
        static constexpr int32_t kPhraseHumpFrames = 2 * FujisakiPitchModel::kDefaultPhraseLenFrames;  // 78 = ~390 ms

        // Pitch units per neper: 256/ln(2) ~= 369.3 (256 units/octave)
        static constexpr double kPitchUnitsPerNeper = 369.3246;

        static int32_t HzToPitch(int32_t hz);

        int16_t GetPhon(int32_t index) const;
        int64_t GetPhonCtrl(int32_t index) const;

        void Phon_Boundry_Pitch();

        // Fires all pitch-buffer events due this frame.  Shared by the live
        // loop and the BuildFujiProsody replay so both fire events identically.
        void FirePitchEvents();

        // One frame of the intonation model (declination + phrase/accent +
        // boundary tone + clause shaping); returns the raw log-pitch offset.
        // userPitch is excluded from the mean centering so level shifts survive.
        int32_t ComputeIntonationOffset(int32_t userPitch);

        // Fires one accent command; flags select the accent type/amplitude.
        void FireAccentEvent(int16_t flags, int32_t durationFrames);

        // Precomputes per-word vowel data, the last-vowel window, and the
        // clause mean offset by replaying the intonation profile.
        void BuildFujiProsody();

        void Interpolate_Pitch();
    };

}  // namespace SharpVox

#endif  // SHARPVOX_PITCH_INTERPOLATOR_H
