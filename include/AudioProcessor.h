#ifndef SHARPVOX_AUDIO_PROCESSOR_H
#define SHARPVOX_AUDIO_PROCESSOR_H

#include <cstdint>
#include <vector>
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

    // Converts PhonemeTokens into a SynthInputDump via a multi-stage pipeline (allophones, duration, pitch).
    class AudioProcessor {
    public:
        // Phoneme IDs — must match C# AudioProcessor exactly; tables are indexed by these values.
        static constexpr int16_t _IY_ = 0;  static constexpr int16_t _IH_ = 1;   // close/near-close front
        static constexpr int16_t _EH_ = 2;  static constexpr int16_t _AE_ = 3;   // open-mid/near-open front
        static constexpr int16_t _IX_ = 4;  static constexpr int16_t _AX_ = 5;   // central: reduced-i, schwa
        static constexpr int16_t _ER_ = 6;  static constexpr int16_t _AH_ = 7;   // r-colored mid-central, open-mid back
        static constexpr int16_t _AA_ = 8;  static constexpr int16_t _AO_ = 9;   // open back, open-mid back rounded
        static constexpr int16_t _UH_ = 10; static constexpr int16_t _UW_ = 11;  // near-close/close back rounded
        static constexpr int16_t _EY_ = 12; static constexpr int16_t _AY_ = 13;  // diphthongs
        static constexpr int16_t _OY_ = 14; static constexpr int16_t _AW_ = 15;
        static constexpr int16_t _OW_ = 16; static constexpr int16_t _YU_ = 17;
        static constexpr int16_t _IR_ = 18; static constexpr int16_t _XR_ = 19;  // r-colored vowels
        static constexpr int16_t _AR_ = 20; static constexpr int16_t _OR_ = 21;
        static constexpr int16_t _UR_ = 22; static constexpr int16_t _SIL_ = 23; // silence
        static constexpr int16_t _M_  = 24; static constexpr int16_t _N_  = 25;  // nasals
        static constexpr int16_t _NG_ = 26; static constexpr int16_t _W_  = 27;  // velar nasal; bilabial-velar approx.
        static constexpr int16_t _Y_  = 28; static constexpr int16_t _R_  = 29;  // approximants
        static constexpr int16_t _L_  = 30; static constexpr int16_t _RX_ = 31;  // lateral; syllabic r
        static constexpr int16_t _LX_ = 32; static constexpr int16_t _EL_ = 33;  // syllabic consonants
        static constexpr int16_t _EN_ = 34; static constexpr int16_t _F_  = 35;  // syllabic n; labiodental fric.
        static constexpr int16_t _V_  = 36; static constexpr int16_t _TH_ = 37;  // fricatives
        static constexpr int16_t _DH_ = 38; static constexpr int16_t _S_  = 39;
        static constexpr int16_t _Z_  = 40; static constexpr int16_t _SH_ = 41;
        static constexpr int16_t _ZH_ = 42; static constexpr int16_t _HH_ = 43;  // glottal fricative
        static constexpr int16_t _P_  = 44; static constexpr int16_t _B_  = 45;  // plosives
        static constexpr int16_t _T_  = 46; static constexpr int16_t _D_  = 47;
        static constexpr int16_t _K_  = 48; static constexpr int16_t _G_  = 49;
        static constexpr int16_t _CH_ = 50; static constexpr int16_t _JH_ = 51;  // affricates
        static constexpr int16_t _TX_ = 52; static constexpr int16_t _DX_ = 53;  // allophones
        static constexpr int16_t _QX_ = 54; static constexpr int16_t _DD_ = 55;
        // Japanese phonemes: block 56-60 (local indices 0-4 = JP_A..JP_O)
        static constexpr int16_t _JP_A_ = 56; static constexpr int16_t _JP_I_ = 57;
        static constexpr int16_t _JP_U_ = 58; static constexpr int16_t _JP_E_ = 59; static constexpr int16_t _JP_O_ = 60;
        static constexpr int16_t _Comma_    = 67;
        static constexpr int16_t _Period_   = 68;
        static constexpr int16_t _Quest_    = 69;
        static constexpr int16_t _Exclam_   = 70;
        static constexpr int16_t _Tilde_    = 71;
        static constexpr int16_t _Ellipsis_ = 72;
        // Phoneme stream opcodes - byte-typed constants shared by all files that build phoneme arrays
        static constexpr uint8_t kOpStress1   = 56;
        static constexpr uint8_t kOpStress2   = 57;
        static constexpr uint8_t kOpEmphStress = 58;
        static constexpr uint8_t kOpSyll      = 63;
        static constexpr uint8_t kOpWord      = 64;
        static constexpr uint8_t kOpPrep      = 65;
        static constexpr uint8_t kOpVerb      = 66;
        static constexpr uint8_t kOpComma     = 67;
        static constexpr uint8_t kOpPeriod    = 68;
        static constexpr uint8_t kOpQuest     = 69;
        static constexpr uint8_t kOpExclam    = 70;

        // PhonemeNamesTable: maps phoneme ID to a short name string.
        // Indexed by phoneme ID; returns nullptr for unmapped IDs.
        static const char* const PhonemeNamesTable[];

        // control buffer flags for PhonemeToken::Ctrl
        static constexpr int64_t kSyllableTypeField             = 0x0F;
        static constexpr int64_t kWord_End                      = 0x0001;
        static constexpr int64_t kPrep_End                      = 0x0002;
        static constexpr int64_t kVerb_End                      = 0x0004;
        static constexpr int64_t kTerm_End                      = 0x0008;
        static constexpr int64_t kWord_Initial_Consonant        = 0x0080;
        static constexpr int64_t kSyllableOrderField            = 0x0300;
        static constexpr int64_t kFirst_Syllable_In_Word        = 0x0100;
        static constexpr int64_t kMid_Syllable_In_Word          = 0x0200;
        static constexpr int64_t kLast_Syllable_In_Word         = 0x0300;
        static constexpr int64_t kMore_Than_One_Syllable_In_Word = 0x0300;
        static constexpr int64_t kPrimaryStress                 = 0x0400;
        static constexpr int64_t kSecondaryStress               = 0x0800;
        static constexpr int64_t kEmphaticStress                = 0x1000;
        static constexpr int64_t kStressField                   = 0x1C00;
        static constexpr int64_t kIsStressed                    = 0x1C00;
        static constexpr int64_t kPrimOrEmphStress              = 0x1400;
        static constexpr int64_t kContent_Word                  = 0x2000;
        static constexpr int64_t kPronounWord                   = 0x80000000L;
        static constexpr int64_t kBoundryTypeField              = 0xF0000L;
        static constexpr int64_t kWord_Start                    = 0x10000L;
        static constexpr int64_t kPrep_Start                    = 0x20000L;
        static constexpr int64_t kVerb_Start                    = 0x40000L;
        static constexpr int64_t kTerm_Bound                    = 0x80000L;
        static constexpr int64_t kSilenceTypeField              = 0x00F00000L;
        static constexpr int32_t kSilenceTypeShift              = 20;
        static constexpr int64_t kSilenceDuration               = 0x01000000L;
        static constexpr int64_t kSingingDuration               = 0x40000000L;
        static constexpr int64_t kSingingPhon                   = 0x20000000L;
        static constexpr int64_t kSyllable_Start                = 0x10000000L;
        static constexpr int64_t kPitchRise                     = 0x0020L;
        static constexpr int64_t kPitchFall                     = 0x0040L;
        static constexpr int64_t kPitchRise1                    = 0x04000000L;
        static constexpr int64_t kPitchFall1                    = 0x08000000L;
        static constexpr int64_t kLowVibrato                    = 0x10L;
        static constexpr int64_t kNoteDur                       = 0x0F00L;
        static constexpr int32_t kNoteDurShift                  = 8;
        static constexpr int64_t kNotePitch                     = 0x00FFL;
        static constexpr int64_t kCompoundNoun                  = 0x8000L;
        static constexpr int64_t kStressedWInitial              = kIsStressed | kWord_Initial_Consonant;
        static constexpr int64_t kSampleMarker                  = 0x02000000L;

        // BND types for silence, index into BoundaryDurationTable
        static constexpr int32_t kBND_Pause    = 1;
        static constexpr int32_t kBND_Decl     = 2;
        static constexpr int32_t kBND_Quest    = 3;
        static constexpr int32_t kBND_Emph     = 4;
        static constexpr int32_t kBND_Ellipsis = 14;

        // Constructor
        explicit AudioProcessor(const VoiceData& voice);

        // Full pipeline: text phonemes → SynthInputDump.
        SynthInputDump Process(const std::vector<PhonemeToken>& tokens,
                               int16_t endPunctuation = _Period_);

        // Streaming path for singing segments.
        // Bypasses all large working buffers — builds the SynthInputDump directly
        // from the token list in O(n) time and O(n) memory.  All tokens are expected
        // to carry kSingingPhon; unrecognised SIL tokens are handled gracefully.
        SynthInputDump ProcessSinging(const std::vector<PhonemeToken>& tokens);

    private:
        // Private implementation constants
        static constexpr int32_t kFrameTime          = 5;
        static constexpr int32_t kNormalPitch        = 579;
        static constexpr int32_t kPhonBufSize        = 16384;
        static constexpr int32_t kPhonBuf_Red_Zone   = kPhonBufSize - 10;
        static constexpr int32_t kPitchBufSize       = kPhonBufSize * 6;
        static constexpr int32_t kMaxRamps           = 256;
        static constexpr int32_t kStepSizeRes        = 3;
        static constexpr int32_t kNeverHappens       = -10000;
        static constexpr int32_t kDur_One            = 0x100;
        static constexpr int32_t kDurStepRes         = 8;
        static constexpr int32_t kNormal_Speech_Rate = 180;
        static constexpr int32_t kMinRate            = 40;
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

        // Phoneme flags from phonFlags2 table
        static constexpr uint32_t kVowelF       = 1u << 0;
        static constexpr uint32_t kConsonantF   = 1u << 1;
        static constexpr uint32_t kVoicedF      = 1u << 2;
        static constexpr uint32_t kVowel1F      = 1u << 3;
        static constexpr uint32_t kSonorantF    = 1u << 4;
        static constexpr uint32_t kNasalF       = 1u << 6;
        static constexpr uint32_t kSonorConsonF = 1u << 8;
        static constexpr uint32_t kPlosFricF    = 1u << 10;
        static constexpr uint32_t kStopF        = 1u << 12;
        static constexpr uint32_t kGStopF       = 1u << 20;
        static constexpr uint32_t kAffricateF   = 1u << 24;
        static constexpr uint32_t kVocLiq       = 1u << 26;
        static constexpr uint32_t kFric         = 1u << 27;

        // Pitch buffer event flags
        static constexpr int16_t kPitchStress_Flg    = 0x1;
        static constexpr int16_t kPitchRiseFall_Flg  = 0x2;
        static constexpr int16_t kPitchBoundry_Flg   = 0x4;
        static constexpr int16_t kResetDecline       = 0x8;
        static constexpr int16_t kPhraseReset        = 0x10;
        static constexpr int16_t kPitchRiseFall1_Flg = 0x20;

        // InsertPlosiveRelease constants
        static constexpr uint32_t kHasReleaseF  = 1u << 23;
        static constexpr uint32_t kFrontF_BE    = 1u << 21;
        static constexpr int64_t  kPlosive_Release = 0x4000;

        // Internal buffers
        int16_t  _phonBuf1[kPhonBufSize];
        int64_t  _phonCtrlBuf1[kPhonBufSize];
        int16_t  _userPitchBuf1[kPhonBufSize];
        int16_t  _userDurBuf1[kPhonBufSize];
        int16_t  _userNoteBuf1[kPhonBufSize];
        int16_t  _userRateBuf1[kPhonBufSize];
        uint8_t  _aspirationBuf1[kPhonBufSize];
        uint8_t  _tiltBuf1[kPhonBufSize];
        uint8_t  _effortBuf1[kPhonBufSize];
        uint8_t  _vibDepthBuf1[kPhonBufSize];
        uint8_t  _vibRateBuf1[kPhonBufSize];
        uint8_t  _tremDepthBuf1[kPhonBufSize];
        uint8_t  _tremRateBuf1[kPhonBufSize];

        int16_t  _phonBuf2[kPhonBufSize];
        int64_t  _phonCtrlBuf2[kPhonBufSize];
        int16_t  _userPitchBuf2[kPhonBufSize];
        int16_t  _userDurBuf2[kPhonBufSize];
        int16_t  _userNoteBuf2[kPhonBufSize];
        int16_t  _userRateBuf2[kPhonBufSize];
        uint8_t  _aspirationBuf2[kPhonBufSize];
        uint8_t  _tiltBuf2[kPhonBufSize];
        uint8_t  _effortBuf2[kPhonBufSize];
        uint8_t  _vibDepthBuf2[kPhonBufSize];
        uint8_t  _vibRateBuf2[kPhonBufSize];
        uint8_t  _tremDepthBuf2[kPhonBufSize];
        uint8_t  _tremRateBuf2[kPhonBufSize];

        int16_t  _durBuf[kPhonBufSize];
        int16_t  _pitchBufFreq[kPitchBufSize];
        int16_t  _pitchBufTime[kPitchBufSize];
        int16_t  _pitchBufFlags[kPitchBufSize];
        int16_t  _pitchBufTiltX64[kPitchBufSize];
        int16_t  _pitchBufDuration[kPitchBufSize];
        int64_t  _rampSteps[kMaxRamps];

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
        void ClearBuffers();
        void InitRateParams();
        void InitPitchParams();
        static int16_t HzToPitch(int16_t hz);
        SynthInputDump BuildSynthInputDump();

        // Inline helpers
        int16_t  GetPhon2(int32_t i);
        int64_t  GetCtrl2(int32_t i);
        uint32_t GetPhonFlags1(int32_t i);

        // Phonemes pipeline (AudioProcessor.Phonemes.cs)
        void LoadPhonemes(const std::vector<PhonemeToken>& tokens);
        void FlagPhonBuf1();
        void MarkSyllable();
        void MarkBoundry();
        void MarkSyllableStart();
        int32_t FindNextWordBound(int32_t index);
        static bool IfConsonantCluster(int16_t c1, int16_t c2);
        void FillPhonBuf2();

        // Duration pipeline (AudioProcessor.Duration.cs)
        void ModDuration();

        // Pitch pipeline (AudioProcessor.Pitch.cs)
        void PitchRaiseAndFall();
        int32_t CountVowelsTillBoundry(int64_t boundary, int32_t curIndex);
        int32_t CountStressVowelsTillBoundry(int64_t boundary, int32_t curIndex);
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
