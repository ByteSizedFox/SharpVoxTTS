#ifndef SHARPVOX_PITCH_INTERPOLATOR_H
#define SHARPVOX_PITCH_INTERPOLATOR_H

#include <cstdint>
#include <vector>
#include "SynthData.h"
#include "VoiceData.h"

namespace SharpVox {

    // Generates F0 values via Taylor (2000) Tilt model for speech or portamento for singing.
    class PitchInterpolator {
    public:
        explicit PitchInterpolator(const SynthInputDump& dump);

        int16_t Step();

        void DoNote(int32_t phonIndex);

        // Debug accessors - valid immediately after Step().
        int32_t DbgF0() const { return _controlF0; }
        int32_t DbgTiltExcursion() const { return _dbgTiltExcursion; }
        int32_t DbgTiltSmooth() const { return _tiltSmooth; }
        int32_t DbgTiltHeld() const { return _tiltHeldLevel; }
        int32_t DbgTiltPhase() const { return _tiltPhase; }
        int32_t DbgBaselineOffset() const { return _dbgBaselineOffset; }
        int32_t DbgTotalOffset() const { return _dbgTotalOffset; }

    private:
        const SynthInputDump& _dump;

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

        // Declination ramp
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

        // Vibrato
        int64_t _vibratoDepth1;
        int64_t _vibratoDepth2;
        int64_t _vibratoFreq;
        int32_t _vibratoPhase1;

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

        // Tilt synthesis state (Taylor 2000) including held levels, phase, and smoothing IIR filters.
        int32_t _tiltPhase;
        int32_t _tiltSmooth;
        int32_t _tiltFrame;
        int32_t _tiltPhaseDur;
        int32_t _tiltA;    // A parameter for TiltSynth (negative=rise, positive=fall)
        int32_t _tiltAbs;  // A_abs: end value of current phase, becomes new _tiltHeldLevel
        int32_t _tiltHeldLevel;
        int32_t _f0Smooth;
        bool _f0SmoothPrime;

        // Pending fall component (queued when a combined rise+fall event fires)
        bool _tiltFallPending;
        int32_t _tiltFallA;
        int32_t _tiltFallDur;
        int32_t _tiltFallAbs;

        // Boundary tone pitch offset (comma, question, tilde patterns).
        // Set directly from kPitchBoundry_Flg events; fed into tiltExcursion so
        // the existing IIR smoother handles transitions between boundary targets.
        int32_t _punctOffset;

        int32_t _controlF0;
        int32_t _voiceNaturalPitch;
        int64_t _curPhonCtrlSinging;

        // Debug snapshot populated each Step() - zero during singing.
        int32_t _dbgTiltExcursion;
        int32_t _dbgBaselineOffset;
        int32_t _dbgTotalOffset;

        // Constants
        static constexpr int32_t kStepSizeRes = 3;
        static constexpr int32_t kNeverHappens = -10000;
        static constexpr int32_t kFrameTime = 5;
        static constexpr int32_t pct = 655;
        static constexpr int32_t k100percent = 0x10000;

        // Pitch buffer event flags (must match AudioProcessor.cs)
        static constexpr int32_t kResetDecline = 0x8;
        static constexpr int32_t kPhraseReset = 0x10;
        static constexpr int32_t kPitchRiseFall_Flg = 0x2;
        static constexpr int32_t kPitchRiseFall1_Flg = 0x20;
        static constexpr int32_t kPitchStress_Flg = 0x1;
        static constexpr int32_t kPitchBoundry_Flg = 0x4;

        // Phoneme flags
        static constexpr uint32_t kVoicedF = (1u << 2);
        static constexpr uint32_t kVowelF = (1u << 0);
        static constexpr uint32_t kVowel1F = (1u << 3);
        static constexpr uint32_t kGStopF = (1u << 20);
        static constexpr uint32_t kStopF = (1u << 12);

        // PhonCtrl field masks
        static constexpr int64_t kSyllableTypeField = 0x0F;
        static constexpr int64_t kWord_End = 0x0001;
        static constexpr int64_t kPrep_End = 0x0002;
        static constexpr int64_t kMid_Syllable_In_Word = 0x0200;
        static constexpr int64_t kPrimOrEmphStress = 0x1400;

        static constexpr int32_t _SIL_ = 23;
        static constexpr int32_t _YU_ = 17;

        static constexpr int64_t kLowVibrato = 0x10L;
        static constexpr int64_t kSingingDuration = 0x40000000L;
        static constexpr int64_t kSingingPhon = 0x20000000L;
        static constexpr int64_t kSilenceDuration = 0x01000000L;

        static int32_t HzToPitch(int32_t hz);

        int16_t GetPhon(int32_t index) const;
        int64_t GetPhonCtrl(int32_t index) const;

        void Phon_Boundry_Pitch();

        // Parabolic synthesis for one rise or fall component (Taylor 2000, eq. 12).
        //
        //   f0(t) = A_abs + A - 2A*(t/D)^2    for 0 <= t < D/2
        //   f0(t) = A_abs + 2A*(1-t/D)^2      for D/2 <= t <= D
        //
        // The curve always starts at (A_abs + A) and ends at A_abs.
        // A < 0: ascending (rise) curve;  A > 0: descending (fall) curve.
        // 8-bit fixed point is used for t/D to avoid overflow while preserving precision.
        static int32_t TiltSynth(int32_t a, int32_t aAbs, int32_t frame, int32_t dur);

        // Returns the current Tilt excursion (pitch units above/below baseline) and
        // advances the Tilt state machine by one frame.
        int32_t StepTilt();

        // Starts a Tilt event from the pitch buffer.
        // amplitude:  A_event in pitch units (positive)
        // tiltX64:    tilt * 64 in range [-64, +64]
        // duration:   total event duration in frames
        // flags:      event type (determines whether held level is updated or restored)
        void FireTiltEvent(int32_t amplitude, int32_t tiltX64, int32_t duration, int32_t flags);

        void Interpolate_Pitch();
    };

}  // namespace SharpVox

#endif  // SHARPVOX_PITCH_INTERPOLATOR_H
