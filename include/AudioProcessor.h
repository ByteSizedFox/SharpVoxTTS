#ifndef SHARPVOX_AUDIO_PROCESSOR_H
#define SHARPVOX_AUDIO_PROCESSOR_H

#include <cstdint>
#include <vector>
#include "../include/PhonemeDefs.h"
#include "../include/SynthData.h"
#include "../include/VoiceData.h"
#include "../include/Tables.h"

namespace SharpVox {

    // Input unit to AudioProcessor::Process().
    // Phon is the phoneme ID (_IY_, _T_, etc.); Ctrl carries prosodic control flags.
    // UserPitch/UserDur/UserNote/UserRate are per-phoneme overrides (0 = use defaults).
    // The extra byte fields carry Klattsch voice-quality parameters; all zero in normal TTS.
    struct PhonemeToken {
        int16_t  Phon       = 0;
        int64_t  Ctrl       = 0;     // kWord_Start, stress flags, etc.
        int16_t  UserPitch  = 0;
        int16_t  UserDur    = 0;     // 0 = kDur_One, no scaling
        int16_t  UserNote   = 0;
        int16_t  UserRate   = 0;

        // extra parameters for Klattsch support
        uint8_t  Aspiration = 0;
        uint8_t  Tilt       = 0;
        uint8_t  Effort     = 0;
        uint8_t  VibDepth   = 0;
        uint8_t  VibRate    = 0;
        uint8_t  TremDepth  = 0;
        uint8_t  TremRate   = 0;
    };

    // Converts PhonemeTokens into a ClausePlan via a multi-stage pipeline (allophones, duration, pitch).
    class AudioProcessor {
    public:
        // PhonemeNamesTable: maps phoneme ID to a short name string.
        // Indexed by phoneme ID; returns nullptr for unmapped IDs.
        static const char* const PhonemeNamesTable[];

        // Constructor
        explicit AudioProcessor(const VoiceData& voice);

        // Full pipeline: text phonemes  ClausePlan.
        ClausePlan Process(const std::vector<PhonemeToken>& tokens,
                               int16_t endPunctuation = _Period_);

        // Streaming path for singing segments.
        // Bypasses all large working buffers  builds the ClausePlan directly
        // from the token list in O(n) time and O(n) memory.  All tokens are expected
        // to carry kSingingPhon; unrecognised SIL tokens are handled gracefully.
        ClausePlan ProcessSinging(const std::vector<PhonemeToken>& tokens);

    private:
        // Private implementation constants
        static constexpr int32_t kFrameTime          = 5;
        static constexpr int32_t kNormalPitch        = 579;
        static constexpr int32_t kMaxRamps           = 256;
        static constexpr int32_t kStepSizeRes        = 3;
        static constexpr int32_t kNeverHappens       = -10000;
        static constexpr int32_t kDur_One            = 0x100;
        static constexpr int32_t kDurStepRes         = 8;
        static constexpr int32_t kNormal_Speech_Rate = 180;
        static constexpr int32_t kMinRate            = 40;
        // Post-scale duration floor for sonorant consonants (Klatt 1976
        // incompressibility). Below this a nasal/liquid/glide has no time for its
        // formant target to register at extreme rate. Overridable via -D at build time.
#ifndef SVX_MIN_SONORANT_FRAMES
#define SVX_MIN_SONORANT_FRAMES 2
#endif
        static constexpr int32_t kMinSonorantFrames  = SVX_MIN_SONORANT_FRAMES;
        static constexpr int32_t k1pct               = 655;
        static constexpr int32_t pct                 = 655;
        static constexpr int32_t kOneHalf            = 0x8000;
        static constexpr int32_t k100percent         = 0x10000;
        static constexpr int32_t k100pct_Dur         = 128;

        // Hz-based pitch offsets from kNormalPitch
        static constexpr int32_t kHZ_4  = 591 - kNormalPitch;  // 12
        static constexpr int32_t kHZ_6  = 597 - kNormalPitch;  // 18
        static constexpr int32_t kHZ_7  = 600 - kNormalPitch;  // 21
        static constexpr int32_t kHZ_8  = 603 - kNormalPitch;  // 24
        static constexpr int32_t kHZ_9  = 606 - kNormalPitch;  // 27
        static constexpr int32_t kHZ_10 = 608 - kNormalPitch;  // 29
        static constexpr int32_t kHZ_12 = 614 - kNormalPitch;  // 35
        static constexpr int32_t kHZ_14 = 620 - kNormalPitch;  // 41
        static constexpr int32_t kHZ_18 = 630 - kNormalPitch;  // 51
        static constexpr int32_t kHZ_20 = 636 - kNormalPitch;  // 57
        static constexpr int32_t kHZ_25 = 649 - kNormalPitch;  // 70
        static constexpr int32_t kHZ_28 = 656 - kNormalPitch;  // 77

        // Working buffers  sized per-call to actual clause length in Process().
        // Buf1: raw phoneme stream from the front-end (LoadPhonemes).
        // Buf2: allophone-transformed stream (FillPhonBuf2).  Allocated at 2 buf1
        //       capacity to absorb any glottal-stop / plosive-release insertions.
        // PitchBuf: prosody events (FillPitchBuf) for PitchInterpolator.  6 buf1.
        // RampSteps: baseline declination steps across phrase resets.  Fixed at
        //            kMaxRamps because that bound is independent of clause length.
        std::vector<int16_t>  _phonBuf1;
        std::vector<int64_t>  _phonCtrlBuf1;
        std::vector<int16_t>  _userPitchBuf1;
        std::vector<int16_t>  _userDurBuf1;
        std::vector<int16_t>  _userNoteBuf1;
        std::vector<int16_t>  _userRateBuf1;
        std::vector<uint8_t>  _aspirationBuf1;
        std::vector<uint8_t>  _tiltBuf1;
        std::vector<uint8_t>  _effortBuf1;
        std::vector<uint8_t>  _vibDepthBuf1;
        std::vector<uint8_t>  _vibRateBuf1;
        std::vector<uint8_t>  _tremDepthBuf1;
        std::vector<uint8_t>  _tremRateBuf1;

        std::vector<int16_t>  _phonBuf2;
        std::vector<int64_t>  _phonCtrlBuf2;
        std::vector<int16_t>  _userPitchBuf2;
        std::vector<int16_t>  _userDurBuf2;
        std::vector<int16_t>  _userNoteBuf2;
        std::vector<int16_t>  _userRateBuf2;
        std::vector<uint8_t>  _aspirationBuf2;
        std::vector<uint8_t>  _tiltBuf2;
        std::vector<uint8_t>  _effortBuf2;
        std::vector<uint8_t>  _vibDepthBuf2;
        std::vector<uint8_t>  _vibRateBuf2;
        std::vector<uint8_t>  _tremDepthBuf2;
        std::vector<uint8_t>  _tremRateBuf2;

        std::vector<int16_t>  _durBuf;
        std::vector<int16_t>  _pitchBufFreq;
        std::vector<int16_t>  _pitchBufTime;
        std::vector<int16_t>  _pitchBufFlags;
        std::vector<int16_t>  _pitchBufTiltX64;
        std::vector<int16_t>  _pitchBufDuration;
        int64_t               _rampSteps[kMaxRamps];

        // Per-call buffer limits: set by ClearBuffers(), replace the old static
        // kPhonBuf_Red_Zone / (kPitchBufSize-1) guards.
        int32_t  _phonBuf1Limit = 0;
        int32_t  _phonBuf2Limit = 0;
        int32_t  _pitchBufLimit = 0;

        // Voice params, set from VoiceData
        int16_t  _speechRate;
        int64_t  _vpPitchRange;    // 16.16 fixed
        int64_t  _vpStressGain;    // 16.16 fixed
        int16_t  _vpRiseAmt;
        int16_t  _vpFallAmt;
        int16_t  _vpRiseAmt1;
        int16_t  _vpFallAmt1;
        int32_t  _vpAssertiveness; // 16.16 fixed
        int16_t  _vpBaselineFall;
        int16_t  _stressDurTime;   // frames (already >>1 from raw)
        int64_t  _vibratoDepth1;
        int64_t  _vibratoDepth2;
        int64_t  _vibratoFreq;
        int64_t  _vpIntonation;
        int16_t  _voiceNaturalPitch;
        int16_t  _vpUptalkAmt;
        int16_t  _vpStressEarly;
        int16_t  _vpBreakStrength;
        int16_t  _vpEmphasisBoost;
        int32_t  _vocalConfidence;

        // Fujisaki intonation model knobs (from VoiceData)
        int64_t  _vpFujiPhraseAmp;            // Q16 nepers
        int64_t  _vpFujiPrimaryAccentAmp;     // Q16 nepers
        int64_t  _vpFujiHeadAccentAmp;        // Q16 nepers
        int64_t  _vpFujiSecondaryAccentAmp;   // Q16 nepers
        int64_t  _vpFujiEmphaticAccentAmp;    // Q16 nepers
        int32_t  _vpFujiPitchCurveExpPct;
        int32_t  _vpFujiMonoAccentDurScalePct;
        int64_t  _vpFujiCompoundStep;         // Q16 nepers
        int32_t  _vpFujiFinalDropPct;

        // State computed during pipeline
        int32_t  _phonBuf1InIndex;
        int32_t  _phonBuf2InIndex;
        int32_t  _pitchBufInIndex;
        int32_t  _scanIndex;
        bool     _isCompoundNoun;
        int16_t  _endPunctuation;
        bool     _singing;

        // Rate params
        int64_t  _rateRatio;
        int64_t  _rateRatioLowGain;
        int16_t  _stressDuration;

        // Pitch params
        int16_t  _vpBaselinePitch;
        int16_t  _baselineFallStart;
        int16_t  _baselineFallEnd;
        int16_t  _pitchClauseStartTime;
        int16_t  _pitchBoundry;

        // Fill_Pitch_Buf helpers
        int32_t  _pitchTimeOffset;
        int32_t  _lastEventTime;

        // Calc_Ramp_Steps result
        int16_t  _curRamp;

        // StartNew_PitchClause output
        int16_t  _baselineStartOffset;
        int16_t  _baselineEndOffset;

        // Pipeline setup helpers
        void InitFromVoice(const VoiceData& vd);
        void ClearBuffers(int32_t tokenCount);
        void InitRateParams();
        void InitPitchParams();
        static int16_t HzToPitch(int16_t hz);
        ClausePlan BuildClausePlan();

        // Inline helpers
        int16_t  GetPhon2(int32_t i);
        int64_t  GetCtrl2(int32_t i);
        uint32_t GetPhonFlags1(int32_t i);

        // Phonemes pipeline
        void LoadPhonemes(const std::vector<PhonemeToken>& tokens);
        void FlagPhonBuf1();
        void MarkSyllable();
        void MarkBoundry();
        void MarkSyllableStart();
        int32_t FindNextWordBound(int32_t index);
        static bool IfConsonantCluster(int16_t c1, int16_t c2);
        void FillPhonBuf2();

        // Duration pipeline
        void ModDuration();

        // Pitch pipeline
        void JapanesePitchAssign();
        void PitchRaiseAndFall();
        int32_t CountVowelsTillBoundry(int64_t boundary, int32_t curIndex);
        int32_t CountStressVowelsTillBoundry(int64_t boundary, int32_t curIndex);
        int32_t CountAnyStressVowelsTillBoundry(int64_t boundary, int32_t curIndex);
        bool AnyStressVowelsRemain(int32_t curIndex);
        static uint32_t PhonemeFeatureFlagsSafe(int16_t p);
        void CalcRampSteps();
        void FillPitchBuf();
        void StoreTiltEvent(int16_t amplitude, int16_t tiltX64, int32_t duration,
                            int16_t time, int16_t flags);
        void StartNewPitchClause();
        void StretchLastWordForTilde();
        void InsertPlosiveRelease();
    };

}  // namespace SharpVox

#endif  // SHARPVOX_AUDIO_PROCESSOR_H
