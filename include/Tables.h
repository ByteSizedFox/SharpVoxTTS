#ifndef SHARPVOX_TABLES_H
#define SHARPVOX_TABLES_H

#include <cstdint>

namespace SharpVox {

class Tables {
public:
    // Control block types
    static constexpr int32_t kFreqType = 0;
    static constexpr int32_t kBWType = 1;
    static constexpr int32_t kFNZType = 2;
    static constexpr int32_t kSourceAmpType = 3;
    static constexpr int32_t kResonAmpType = 4;

    // Rank types
    static constexpr int32_t kFrontR = 0;
    static constexpr int32_t kMiddleR = 1;
    static constexpr int32_t kBackR = 2;
    static constexpr int32_t kRoundR = 3;
    static constexpr int32_t kConsonantR = 4;

    // Phoneme flags
    static constexpr uint32_t kVowelF = (1 << 0);   // 0x1
    static constexpr uint32_t kVoicedF = (1 << 2);   // 0x4
    static constexpr uint32_t kVowel1F = (1 << 3);   // 0x8
    static constexpr uint32_t kSonorantF = (1 << 4);   // 0x10
    static constexpr uint32_t kNasalF = (1 << 6);   // 0x40
    static constexpr uint32_t kPlosiveF = (1 << 9);   // 0x200
    static constexpr uint32_t kStopF = (1 << 12);  // 0x1000
    static constexpr uint32_t kGStopF = (1 << 20);  // 0x100000
    static constexpr uint32_t kAffricateF = (1 << 24);  // 0x1000000

    // Misc constants
    static constexpr int32_t k100pct = 0x10000;
    static constexpr int32_t UseEnvList = 0x8000;

    // Coarticulation types
    static constexpr int32_t kControlType = 0;
    static constexpr int32_t kStressType = 1;
    static constexpr int32_t kPDType = 2;
    static constexpr int32_t kBoundryType = 3;
    static constexpr int32_t kWordBoundType = 4;
    static constexpr int32_t kTerminatorType = 5;

    // Language identifiers
    static constexpr int32_t kLangEnglish = 0;
    static constexpr int32_t kLangJapanese = 1;
    static constexpr int32_t kLangSpanish = 2;   // reserved
    static constexpr int32_t kLangFrench = 3;   // reserved

    // Phoneme block layout
    static constexpr int32_t kEnglishBase = 0;
    static constexpr int32_t kEnglishCoreSize = 56;   // 0-55 active
    static constexpr int32_t kEnglishBlockSize = 56;   // exactly 56 English phonemes
    static constexpr int32_t kJapaneseBase = 56;
    static constexpr int32_t kJapaneseCoreSize = 5;    // JP_A..JP_O
    static constexpr int32_t kJapaneseBlockSize = 32;   // 56-87 with spare

    static int32_t PhonemeLanguage(int32_t id);
    static int32_t PhonemeLocalIndex(int32_t id);

    // Per-language table dispatch helpers (do not index arrays directly in engine code).
    static uint32_t GetFeatureFlags(int32_t id);
    static int16_t GetMinimumDuration(int32_t id);
    static int16_t GetMaximumDuration(int32_t id);
    static int16_t GetForwardRank(int32_t id);
    static int16_t GetBackwardRank(int32_t id);
    static int16_t GetPitch(int32_t id);
    static int16_t GetAmplitudeVoicing(bool male, int32_t id);
    static int16_t GetNoiseIndex(int32_t id);

    // Per-language formant dispatch helpers
    static int16_t GetMaleFormant1(int32_t id);
    static int16_t GetMaleFormant2(int32_t id);
    static int16_t GetMaleFormant3(int32_t id);
    static int16_t GetMaleBandwidth1(int32_t id);
    static int16_t GetMaleBandwidth2(int32_t id);
    static int16_t GetMaleBandwidth3(int32_t id);
    static int16_t GetFemaleFormant1(int32_t id);
    static int16_t GetFemaleFormant2(int32_t id);
    static int16_t GetFemaleFormant3(int32_t id);
    static int16_t GetFemaleBandwidth1(int32_t id);
    static int16_t GetFemaleBandwidth2(int32_t id);
    static int16_t GetFemaleBandwidth3(int32_t id);

    // Packed phoneme feature flags indexed by phoneme ID.
    // 56 entries (0-55 English); see kVowelF etc. for flag layout.
    static constexpr int32_t PhonemeFeatureFlagsLength = 56;
    static constexpr int32_t JapanesePhonemeFeatureFlagsLength = 5;
    static const uint32_t PhonemeFeatureFlags[];
    static const uint32_t JapanesePhonemeFeatureFlags[];

    // MinimumDurationTable[phon] = min duration in ms
    static constexpr int32_t MinimumDurationTableLength = 56;
    static constexpr int32_t JapaneseMinimumDurationTableLength = 5;
    static const int16_t MinimumDurationTable[];
    static const int16_t JapaneseMinimumDurationTable[];

    // MaximumDurationTable[phon] = max duration in ms
    static constexpr int32_t MaximumDurationTableLength = 56;
    static constexpr int32_t JapaneseMaximumDurationTableLength = 5;
    static const int16_t MaximumDurationTable[];
    static const int16_t JapaneseMaximumDurationTable[];

    static constexpr int32_t BoundaryDurationTableLength = 21;
    static constexpr int32_t ForwardRankTableLength = 56;
    static constexpr int32_t JapaneseForwardRankTableLength = 5;
    static constexpr int32_t BackwardRankTableLength = 56;
    static constexpr int32_t JapaneseBackwardRankTableLength = 5;
    static constexpr int32_t DefaultTargetFrequenciesTableLength = 6;
    static constexpr int32_t BurstDurationTableLength = 56;
    static const int16_t BoundaryDurationTable[];
    static const int16_t ForwardRankTable[];
    static const int16_t JapaneseForwardRankTable[];
    static const int16_t BackwardRankTable[];
    static const int16_t JapaneseBackwardRankTable[];
    static const int16_t DefaultTargetFrequenciesTable[];
    static const int16_t BurstDurationTable[];

    static constexpr int32_t PhonemePitchTableLength = 56;
    static constexpr int32_t JapanesePhonemePitchTableLength = 5;
    static const int16_t PhonemePitchTable[];
    static const int16_t JapanesePhonemePitchTable[];

    static constexpr int32_t NoiseIndexTableLength = 56;
    static constexpr int32_t MaleFormant1FrequencyTableLength = 56;
    static constexpr int32_t JapaneseMaleFormant1FrequencyTableLength = 5;
    static constexpr int32_t MaleFormant2FrequencyTableLength = 56;
    static constexpr int32_t JapaneseMaleFormant2FrequencyTableLength = 5;
    static constexpr int32_t MaleFormant3FrequencyTableLength = 56;
    static constexpr int32_t JapaneseMaleFormant3FrequencyTableLength = 5;
    static constexpr int32_t MaleBandwidth1FrequencyTableLength = 56;
    static constexpr int32_t JapaneseMaleBandwidth1FrequencyTableLength = 5;
    static constexpr int32_t MaleBandwidth2FrequencyTableLength = 56;
    static constexpr int32_t JapaneseMaleBandwidth2FrequencyTableLength = 5;
    static constexpr int32_t MaleBandwidth3FrequencyTableLength = 56;
    static constexpr int32_t JapaneseMaleBandwidth3FrequencyTableLength = 5;
    static constexpr int32_t MaleAmplitudeVoicingVolumeTableLength = 56;
    static constexpr int32_t JapaneseMaleAmplitudeVoicingVolumeTableLength = 5;
    static constexpr int32_t FemaleFormant1FrequencyTableLength = 56;
    static constexpr int32_t JapaneseFemaleFormant1FrequencyTableLength = 5;
    static constexpr int32_t FemaleFormant2FrequencyTableLength = 56;
    static constexpr int32_t JapaneseFemaleFormant2FrequencyTableLength = 5;
    static constexpr int32_t FemaleFormant3FrequencyTableLength = 56;
    static constexpr int32_t JapaneseFemaleFormant3FrequencyTableLength = 5;
    static constexpr int32_t FemaleBandwidth1FrequencyTableLength = 56;
    static constexpr int32_t JapaneseFemaleBandwidth1FrequencyTableLength = 5;
    static constexpr int32_t FemaleBandwidth2FrequencyTableLength = 56;
    static constexpr int32_t JapaneseFemaleBandwidth2FrequencyTableLength = 5;
    static constexpr int32_t FemaleBandwidth3FrequencyTableLength = 56;
    static constexpr int32_t JapaneseFemaleBandwidth3FrequencyTableLength = 5;
    static constexpr int32_t FemaleAmplitudeVoicingVolumeTableLength = 56;
    static constexpr int32_t JapaneseFemaleAmplitudeVoicingVolumeTableLength = 5;
    static constexpr int32_t LogarithmicToLinearTableLength = 32;
    static constexpr int32_t LogarithmBase2TableLength = 512;
    // Maps each of the 15 control block indices (kF1..kAB) to its type.
    static constexpr int32_t ControlBlockTypeTableLength = 15;
    static const int16_t NoiseIndexTable[];
    static const int16_t MaleFormant1FrequencyTable[];
    static const int16_t JapaneseMaleFormant1FrequencyTable[];
    static const int16_t MaleFormant2FrequencyTable[];
    static const int16_t JapaneseMaleFormant2FrequencyTable[];
    static const int16_t MaleFormant3FrequencyTable[];
    static const int16_t JapaneseMaleFormant3FrequencyTable[];
    static const int16_t MaleBandwidth1FrequencyTable[];
    static const int16_t JapaneseMaleBandwidth1FrequencyTable[];
    static const int16_t MaleBandwidth2FrequencyTable[];
    static const int16_t JapaneseMaleBandwidth2FrequencyTable[];
    static const int16_t MaleBandwidth3FrequencyTable[];
    static const int16_t JapaneseMaleBandwidth3FrequencyTable[];
    static const int16_t MaleAmplitudeVoicingVolumeTable[];
    static const int16_t JapaneseMaleAmplitudeVoicingVolumeTable[];
    static const int16_t FemaleFormant1FrequencyTable[];
    static const int16_t JapaneseFemaleFormant1FrequencyTable[];
    static const int16_t FemaleFormant2FrequencyTable[];
    static const int16_t JapaneseFemaleFormant2FrequencyTable[];
    static const int16_t FemaleFormant3FrequencyTable[];
    static const int16_t JapaneseFemaleFormant3FrequencyTable[];
    static const int16_t FemaleBandwidth1FrequencyTable[];
    static const int16_t JapaneseFemaleBandwidth1FrequencyTable[];
    static const int16_t FemaleBandwidth2FrequencyTable[];
    static const int16_t JapaneseFemaleBandwidth2FrequencyTable[];
    static const int16_t FemaleBandwidth3FrequencyTable[];
    static const int16_t JapaneseFemaleBandwidth3FrequencyTable[];
    static const int16_t FemaleAmplitudeVoicingVolumeTable[];
    static const int16_t JapaneseFemaleAmplitudeVoicingVolumeTable[];
    static const int16_t LogarithmicToLinearTable[];
    static const int16_t LogarithmBase2Table[];
    static const int16_t ControlBlockTypeTable[];

    // Locus tables: each consonant phoneme has 9 entries [F1_locus,F1_pct,F1_time, F2_locus,F2_pct,F2_time, F3_locus,F3_pct,F3_time].
    // locus_freq: Hz value toward which the formant transitions at the C boundary.
    // locus_pct: how far (0-100%) the onset/offset moves toward the locus.
    // trans_time_ms: transition duration in ms.
    // Locus frequencies sourced from:
    //   Klatt, D.H. (1980). Software for a cascade/parallel formant synthesizer.
    //   J. Acoust. Soc. Am., 67(3), 971-995. Table III.
    // locus_pct and trans_time_ms retained from prior empirical tuning.
    static constexpr int32_t MaleLocusTableLength = 450;
    static constexpr int32_t FemaleLocusTableLength = 450;
    static const int16_t MaleLocusTable[];
    static const int16_t FemaleLocusTable[];

    // FrontLocusTable[phonId] = byte offset into MaleLocusTable/FemaleLocusTable for front-vowel context.
    // MiddleLocusTable/BackLocusTable: same layout for mid/back vowel contexts. -1 = no locus data.
    static constexpr int32_t FrontLocusTableLength = 56;
    static constexpr int32_t MiddleLocusTableLength = 56;
    static constexpr int32_t BackLocusTableLength = 56;
    static const int16_t FrontLocusTable[];
    static const int16_t MiddleLocusTable[];
    static const int16_t BackLocusTable[];

    static constexpr int32_t MaleNoiseAmplitudeTableLength = 432;
    static constexpr int32_t FemaleNoiseAmplitudeTableLength = 432;
    static constexpr int32_t ReciprocalTableLength = 100;
    static constexpr int32_t MaleEnvelopeTableLength = 188;
    static constexpr int32_t FemaleEnvelopeTableLength = 200;
    static const int16_t MaleNoiseAmplitudeTable[];
    static const int16_t FemaleNoiseAmplitudeTable[];
    static const uint32_t ReciprocalTable[];
    static const int16_t MaleEnvelopeTable[];
    static const int16_t FemaleEnvelopeTable[];
};

}  // namespace SharpVox

#endif  // SHARPVOX_TABLES_H
