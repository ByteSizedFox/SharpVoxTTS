#include "Tables.h"

namespace SharpVox {

int32_t Tables::PhonemeLanguage(int32_t id) {
    if (id < kEnglishBlockSize) { return kLangEnglish; }
    if (id < kJapaneseBase + kJapaneseBlockSize) { return kLangJapanese; }
    return -1;
}
int32_t Tables::PhonemeLocalIndex(int32_t id) {
    if (id < kEnglishBlockSize) { return id; }
    return id - kJapaneseBase;
}

// Per-language table dispatch helpers (do not index arrays directly in engine code).
uint32_t Tables::GetFeatureFlags(int32_t id) {
    if (id >= 0 && id < PhonemeFeatureFlagsLength) { return PhonemeFeatureFlags[id]; }
    if (id >= kJapaneseBase) {
        int32_t local = id - kJapaneseBase;
        if (local >= 0 && local < JapanesePhonemeFeatureFlagsLength) { return JapanesePhonemeFeatureFlags[local]; }
    }
    return 0u;
}

int16_t Tables::GetMinimumDuration(int32_t id) {
    if (id >= 0 && id < MinimumDurationTableLength) { return MinimumDurationTable[id]; }
    if (id >= kJapaneseBase) {
        int32_t local = id - kJapaneseBase;
        if (local >= 0 && local < JapaneseMinimumDurationTableLength) { return JapaneseMinimumDurationTable[local]; }
    }
    return 0;
}

int16_t Tables::GetMaximumDuration(int32_t id) {
    if (id >= 0 && id < MaximumDurationTableLength) { return MaximumDurationTable[id]; }
    if (id >= kJapaneseBase) {
        int32_t local = id - kJapaneseBase;
        if (local >= 0 && local < JapaneseMaximumDurationTableLength) { return JapaneseMaximumDurationTable[local]; }
    }
    return 0;
}

int16_t Tables::GetForwardRank(int32_t id) {
    if (id >= 0 && id < ForwardRankTableLength) { return ForwardRankTable[id]; }
    if (id >= kJapaneseBase) {
        int32_t local = id - kJapaneseBase;
        if (local >= 0 && local < JapaneseForwardRankTableLength) { return JapaneseForwardRankTable[local]; }
    }
    return 0;
}

int16_t Tables::GetBackwardRank(int32_t id) {
    if (id >= 0 && id < BackwardRankTableLength) { return BackwardRankTable[id]; }
    if (id >= kJapaneseBase) {
        int32_t local = id - kJapaneseBase;
        if (local >= 0 && local < JapaneseBackwardRankTableLength) { return JapaneseBackwardRankTable[local]; }
    }
    return 0;
}

int16_t Tables::GetPitch(int32_t id) {
    if (id >= 0 && id < PhonemePitchTableLength) { return PhonemePitchTable[id]; }
    if (id >= kJapaneseBase) {
        int32_t local = id - kJapaneseBase;
        if (local >= 0 && local < JapanesePhonemePitchTableLength) { return JapanesePhonemePitchTable[local]; }
    }
    return 0;
}

int16_t Tables::GetAmplitudeVoicing(bool male, int32_t id) {
    if (male) {
        if (id >= 0 && id < MaleAmplitudeVoicingVolumeTableLength) { return MaleAmplitudeVoicingVolumeTable[id]; }
        int32_t local = id - kJapaneseBase;
        return (local >= 0 && local < JapaneseMaleAmplitudeVoicingVolumeTableLength) ? JapaneseMaleAmplitudeVoicingVolumeTable[local] : (int16_t)0;
    } else {
        if (id >= 0 && id < FemaleAmplitudeVoicingVolumeTableLength) { return FemaleAmplitudeVoicingVolumeTable[id]; }
        int32_t local = id - kJapaneseBase;
        return (local >= 0 && local < JapaneseFemaleAmplitudeVoicingVolumeTableLength) ? JapaneseFemaleAmplitudeVoicingVolumeTable[local] : (int16_t)0;
    }
}

int16_t Tables::GetNoiseIndex(int32_t id) {
    return (id >= 0 && id < NoiseIndexTableLength) ? NoiseIndexTable[id] : (int16_t)-1;
}

// Per-language formant dispatch helpers
int16_t Tables::GetMaleFormant1(int32_t id) {
    if (id >= 0 && id < MaleFormant1FrequencyTableLength) { return MaleFormant1FrequencyTable[id]; }
    int32_t local = id - kJapaneseBase;
    return (local >= 0 && local < JapaneseMaleFormant1FrequencyTableLength) ? JapaneseMaleFormant1FrequencyTable[local] : (int16_t)-1;
}
int16_t Tables::GetMaleFormant2(int32_t id) {
    if (id >= 0 && id < MaleFormant2FrequencyTableLength) { return MaleFormant2FrequencyTable[id]; }
    int32_t local = id - kJapaneseBase;
    return (local >= 0 && local < JapaneseMaleFormant2FrequencyTableLength) ? JapaneseMaleFormant2FrequencyTable[local] : (int16_t)-1;
}
int16_t Tables::GetMaleFormant3(int32_t id) {
    if (id >= 0 && id < MaleFormant3FrequencyTableLength) { return MaleFormant3FrequencyTable[id]; }
    int32_t local = id - kJapaneseBase;
    return (local >= 0 && local < JapaneseMaleFormant3FrequencyTableLength) ? JapaneseMaleFormant3FrequencyTable[local] : (int16_t)-1;
}
int16_t Tables::GetMaleBandwidth1(int32_t id) {
    if (id >= 0 && id < MaleBandwidth1FrequencyTableLength) { return MaleBandwidth1FrequencyTable[id]; }
    int32_t local = id - kJapaneseBase;
    return (local >= 0 && local < JapaneseMaleBandwidth1FrequencyTableLength) ? JapaneseMaleBandwidth1FrequencyTable[local] : (int16_t)-1;
}
int16_t Tables::GetMaleBandwidth2(int32_t id) {
    if (id >= 0 && id < MaleBandwidth2FrequencyTableLength) { return MaleBandwidth2FrequencyTable[id]; }
    int32_t local = id - kJapaneseBase;
    return (local >= 0 && local < JapaneseMaleBandwidth2FrequencyTableLength) ? JapaneseMaleBandwidth2FrequencyTable[local] : (int16_t)-1;
}
int16_t Tables::GetMaleBandwidth3(int32_t id) {
    if (id >= 0 && id < MaleBandwidth3FrequencyTableLength) { return MaleBandwidth3FrequencyTable[id]; }
    int32_t local = id - kJapaneseBase;
    return (local >= 0 && local < JapaneseMaleBandwidth3FrequencyTableLength) ? JapaneseMaleBandwidth3FrequencyTable[local] : (int16_t)-1;
}
int16_t Tables::GetFemaleFormant1(int32_t id) {
    if (id >= 0 && id < FemaleFormant1FrequencyTableLength) { return FemaleFormant1FrequencyTable[id]; }
    int32_t local = id - kJapaneseBase;
    return (local >= 0 && local < JapaneseFemaleFormant1FrequencyTableLength) ? JapaneseFemaleFormant1FrequencyTable[local] : (int16_t)-1;
}
int16_t Tables::GetFemaleFormant2(int32_t id) {
    if (id >= 0 && id < FemaleFormant2FrequencyTableLength) { return FemaleFormant2FrequencyTable[id]; }
    int32_t local = id - kJapaneseBase;
    return (local >= 0 && local < JapaneseFemaleFormant2FrequencyTableLength) ? JapaneseFemaleFormant2FrequencyTable[local] : (int16_t)-1;
}
int16_t Tables::GetFemaleFormant3(int32_t id) {
    if (id >= 0 && id < FemaleFormant3FrequencyTableLength) { return FemaleFormant3FrequencyTable[id]; }
    int32_t local = id - kJapaneseBase;
    return (local >= 0 && local < JapaneseFemaleFormant3FrequencyTableLength) ? JapaneseFemaleFormant3FrequencyTable[local] : (int16_t)-1;
}
int16_t Tables::GetFemaleBandwidth1(int32_t id) {
    if (id >= 0 && id < FemaleBandwidth1FrequencyTableLength) { return FemaleBandwidth1FrequencyTable[id]; }
    int32_t local = id - kJapaneseBase;
    return (local >= 0 && local < JapaneseFemaleBandwidth1FrequencyTableLength) ? JapaneseFemaleBandwidth1FrequencyTable[local] : (int16_t)-1;
}
int16_t Tables::GetFemaleBandwidth2(int32_t id) {
    if (id >= 0 && id < FemaleBandwidth2FrequencyTableLength) { return FemaleBandwidth2FrequencyTable[id]; }
    int32_t local = id - kJapaneseBase;
    return (local >= 0 && local < JapaneseFemaleBandwidth2FrequencyTableLength) ? JapaneseFemaleBandwidth2FrequencyTable[local] : (int16_t)-1;
}
int16_t Tables::GetFemaleBandwidth3(int32_t id) {
    if (id >= 0 && id < FemaleBandwidth3FrequencyTableLength) { return FemaleBandwidth3FrequencyTable[id]; }
    int32_t local = id - kJapaneseBase;
    return (local >= 0 && local < JapaneseFemaleBandwidth3FrequencyTableLength) ? JapaneseFemaleBandwidth3FrequencyTable[local] : (int16_t)-1;
}

// Packed phoneme feature flags indexed by phoneme ID.
// 56 entries (0-55 English); see kVowelF etc. for flag layout.
const uint32_t Tables::PhonemeFeatureFlags[] =
{
 2621501u, 2097213u, 2097213u, 2097213u, 2097213u,      61u,      61u,      61u,
      61u,      61u,      61u, 4194365u, 6815805u, 4194365u, 4194365u, 4194365u,
 4194365u, 4456765u, 4194365u, 4194365u, 4194365u, 4194365u, 4194365u,      32u,
 8423798u, 8399222u, 8407414u, 33554870u, 786870u, 33554870u,    438u, 100663478u,
67109046u,     53u,  10613u, 134251522u, 134251526u, 134284290u, 134284294u, 134226946u,
134226950u, 134349826u, 134349830u,     34u, 8429058u, 8429062u, 8404482u, 8404486u,
 8412674u, 8412678u, 16911874u, 16911878u, 1064454u,   3078u, 1050628u,  73222u,
};
const uint32_t Tables::JapanesePhonemeFeatureFlags[] =
{
61u, 61u, 61u, 61u, 61u,
};

// MinimumDurationTable[phon] = min duration in ms
const int16_t Tables::MinimumDurationTable[] =
{
  60,  60,  60,  50,  50,  50,  90,  70,  90, 100,  50,  50, 110, 100, 110, 110,
  90, 130, 120, 120, 120, 120, 120, 200,  60,  35,  50,  15,  30,  30,  40,  70,
  70, 110, 100,  60,  55,  40,  35,  65,  60,  60,  50,  35,  70,  60,  50,  40,
  75,  65, 100,  70,  50,  20,  50,  35,
};
const int16_t Tables::JapaneseMinimumDurationTable[] =
{
80, 60, 60, 60, 80,
};

// MaximumDurationTable[phon] = max duration in ms
const int16_t Tables::MaximumDurationTable[] =
{
 170, 160, 160, 230, 120, 120, 180, 160, 240, 240, 170, 210, 200, 250, 260, 260,
 220, 230, 230, 250, 250, 250, 230, 305,  70,  65,  80,  60,  75,  65,  75, 120,
 100, 160, 170, 100,  70, 100,  60, 115,  75, 115,  70,  70,  85,  80,  85,  80,
  90,  90, 160, 100,  70,  20,  50,  60,
};
const int16_t Tables::JapaneseMaximumDurationTable[] =
{
240, 170, 200, 160, 220,
};

const int16_t Tables::BoundaryDurationTable[] =
{
  0,  // 0: unused
350,  // 1: kBND_Pause (comma)
600,  // 2: kBND_Decl (declarative)
600,  // 3: kBND_Quest (yes-no question)
600,  // 4: kBND_Emph (exclamation)
300,  // 5: kBND_Paren_L
200,  // 6: kBND_Paren_R
155,  // 7: kBND_Sep1
 60,  // 8: kBND_Sep2
 60,  // 9: kBND_Sep3
 60,  // 10: kBND_Sep4
 20,  // 11: kBND_Sep5
  0,  // 12: kBND_Sep6
100,  // 13: kBND_Sep7
850,  // 14: kBND_Ellipsis
 35,  // 15
 60,  // 16
 60,  // 17
 20,  // 18
 10,  // 19
100,  // 20
};

const int16_t Tables::ForwardRankTable[] =
{
0, 0, 0, 0, 0, 1, 2, 1, 1, 2, 2, 2, 0, 1, 2, 1,
2, 0, 0, 0, 1, 2, 2, 4, 4, 4, 4, 3, 0, 3, 3, 2,
3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
4, 4, 4, 4, 4, 4, 4, 4,
};
const int16_t Tables::JapaneseForwardRankTable[] =
{
1, 0, 2, 0, 2,
};

const int16_t Tables::BackwardRankTable[] =
{
0, 0, 0, 0, 0, 1, 2, 1, 1, 2, 2, 3, 0, 0, 0, 3,
3, 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 3, 0, 3, 3, 2,
3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
4, 4, 4, 4, 4, 4, 4, 4,
};
const int16_t Tables::JapaneseBackwardRankTable[] =
{
1, 0, 2, 0, 3,
};

const int16_t Tables::DefaultTargetFrequenciesTable[] =
{
600, 1600, 2600, 100, 150, 250,
};

const int16_t Tables::BurstDurationTable[] =
{
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 13, 13,
20, 20, 70, 40, 0, 0, 0, 13,
};

const int16_t Tables::PhonemePitchTable[] =
{
 29,  18,   6,   0,  18,   9,  15,   6,   0,   0,  18,  29,  12,   0,  15,   0,
  9,  29,  29,  18,   0,   9,  24,  15, -15, -15, -15,  18,  18,   0,   0,   0,
  0,   0, -15,  82, -15,  82, -15,  82, -15,  82, -15,  57,  82, -15,  82, -15,
 82, -15,  82, -15,   0,  -3,   0, -15,
};
const int16_t Tables::JapanesePhonemePitchTable[] =
{
0, 29, 18, 6, 9,
};

const int16_t Tables::NoiseIndexTable[] =
{
 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
 -1, -1, -1,  0, 24, 48, 72, 96,120,144,168, -1,192,216,240,264,
288,312,336,360,384, -1, -1,408,
};

const int16_t Tables::MaleFormant1FrequencyTable[] =
{
   317,  458,  617,  714,  454,  523,  462,  625,  735,  665,  498,-32748,-32768,-32764,-32760,-32756,
-32752,-32744,-32740,-32736,-32732,-32728,-32724,   -1,  400,  405,  403,  295,  267,  395,  355,  475,
   450,  450,  425,  350,  280,  325,  300,  325,  270,  300,  280,   -1,  350,  230,  350,  200,
   350,  200,  350,  250,  200,  350,   -1,  200,
};
const int16_t Tables::JapaneseMaleFormant1FrequencyTable[] =
{
744, 298, 355, 480, 460,
};

const int16_t Tables::MaleFormant2FrequencyTable[] =
{
  2182,-32720,-32716,-32712, 1667, 1375, 1348, 1238, 1200, 1000,-32708,-32684,-32704,-32700,-32696,-32692,
-32688,-32680,-32676,-32672,-32668,-32664,-32660,   -1, 1179, 1500, 1870,  536, 2156, 1112, 1090, 1280,
   800,  800, 1350, 1100, 1100, 1300, 1300, 1430, 1430, 1650, 1650,   -1, 1100, 1034, 1600, 1600,
  1800, 1895, 1750, 1750, 1600, 1600,   -1, 1400,
};
const int16_t Tables::JapaneseMaleFormant2FrequencyTable[] =
{
1240, 2083, 1282, 1857, 857,
};

const int16_t Tables::MaleFormant3FrequencyTable[] =
{
  2860, 2540, 2540, 2470, 2526, 2510, 1863, 2600, 2660, 2635, 2530,-32632,-32652,-32648,-32644,-32640,
-32636,-32628,-32624,-32620,-32616,-32612,-32608,   -1, 2212, 2520, 2470, 2300, 2660, 1490, 2815, 1550,
  3000, 2850, 2500, 2070, 2070, 2520, 2570, 2550, 2600, 2550, 2550,   -1, 2150, 2025, 2600, 2600,
  2250, 2675, 2700, 2700, 2600, 2600,   -1, 2600,
};
const int16_t Tables::JapaneseMaleFormant3FrequencyTable[] =
{
2426, 2954, 2233, 2437, 2405,
};

const int16_t Tables::MaleBandwidth1FrequencyTable[] =
{
    75,   75,   75,   90,   60,   90,   90,   90,  115,  100,   85,   82,   80,-32604,   90,-32600,
    90,   70,   80,   85,  115,   95,   90,  300,  130,  110,  130,   75,   70,   80,   80,   80,
    90,   90,  130,  200,  100,  200,  100,  200,  100,  200,  100,  300,  200,   90,  200,   75,
   200,   75,  200,   70,  110,   90,   -1,   90,
};
const int16_t Tables::JapaneseMaleBandwidth1FrequencyTable[] =
{
182, 67, 55, 65, 61,
};

const int16_t Tables::MaleBandwidth2FrequencyTable[] =
{
   120,  120,  122,-32596,  100,  110,  110,  105,  115,  115,  110,  105,  120,  120,-32592,  107,
   105,-32588,  120,  125,  115,  110,   90,  300,  280,  220,  250,  100,  170,  100,  102,   80,
    90,   90,  300,  120,  120,   90,  120,  180,  140,  240,  160,  200,  180,   80,  150,  100,
   160,  135,  280,  170,  100,  100,   -1,  110,
};
const int16_t Tables::JapaneseMaleBandwidth2FrequencyTable[] =
{
117, 130, 129, 97, 114,
};

const int16_t Tables::MaleBandwidth3FrequencyTable[] =
{
   220,  225,  240,  255,  200,  220,  175,  220,  230,  190,  210,  200,  230,  245,  220,  205,
   220,-32584,  200,  230,  205,  200,  180,  300,  360,  360,  400,  170,  280,  120,  190,  130,
   160,  200,  460,  150,  120,  150,  170,  300,  300,  300,  300,  220,  180,  130,  250,  205,
   280,  230,  250,  250,  170,  170,   -1,  200,
};
const int16_t Tables::JapaneseMaleBandwidth3FrequencyTable[] =
{
222, 145, 135, 257, 134,
};

const int16_t Tables::MaleAmplitudeVoicingVolumeTable[] =
{
61, 62, 62, 60, 63, 60, 62, 61, 61, 61, 62, 62, 62, 61, 62, 61,
62, 65, 65, 62, 61, 62, 65,  0, 60, 59, 60, 58, 58, 58, 59, 64,
63, 63, 60,  0, 56,  0, 56,  0, 56,  0, 56,  0,  0,  0,  0, 26,
 0, 28,  0,  0,  0, 34, 47, 50,
};
const int16_t Tables::JapaneseMaleAmplitudeVoicingVolumeTable[] =
{
61, 61, 61, 61, 61,
};

const int16_t Tables::FemaleFormant1FrequencyTable[] =
{
   322,  533,  667,-32768,  499,  623,  512,  738,  810,  728,-32764,-32740,-32760,-32756,-32752,-32748,
-32744,-32736,-32732,-32728,-32724,-32720,-32716,   -1,  410,  415,  413,  320,  302,  395,  360,  490,
   480,  480,  450,  360,  300,  340,  320,  340,  300,  320,  300,   -1,  370,  240,  370,  210,
   370,  210,  370,  280,  220,  370,   -1,  220,
};
const int16_t Tables::JapaneseFemaleFormant1FrequencyTable[] =
{
834, 334, 398, 538, 515,
};

const int16_t Tables::FemaleFormant2FrequencyTable[] =
{
-32712,-32708,-32704,-32700, 2055, 1650, 1513,-32696, 1325,-32692, 1195,-32668,-32688,-32684,-32680,-32676,
-32672,-32664,-32660,-32656,-32652,-32648,-32644,   -1, 1224, 1575, 2370,  636, 2361, 1100, 1190, 1600,
   930,  990, 1530, 1150, 1150, 1530, 1500, 1640, 1640, 1980, 1980,   -1, 1150, 1059, 1860, 1730,
  2190, 2090, 2100, 2100, 1860, 1860,   -1, 1860,
};
const int16_t Tables::JapaneseFemaleFormant2FrequencyTable[] =
{
1314, 2208, 1359, 1968, 908,
};

const int16_t Tables::FemaleFormant3FrequencyTable[] =
{
  3085, 2890, 2930, 2795, 2864, 2710, 2138, 2800, 2830, 2810, 2780,-32620,-32640,-32636,-32632,-32628,
-32624,-32616,-32612,-32608,-32604,-32600,-32596,   -1, 2395, 2720, 3020, 2495, 3080, 1650, 3055, 1950,
  3320, 3320, 2920, 2460, 2460, 2940, 2940, 2930, 3000, 2930, 2940,   -1, 2470, 2185, 3020, 2810,
  2710, 2780, 3150, 3150, 3000, 3000,   -1, 3020,
};
const int16_t Tables::JapaneseFemaleFormant3FrequencyTable[] =
{
2523, 3072, 2322, 2534, 2501,
};

const int16_t Tables::FemaleBandwidth1FrequencyTable[] =
{
    82,   82,   85,-32592,   80,  115,   90,  100,  135,  115,   90,   90,   82,-32588,   95,-32584,
    95,   80,   80,  105,  110,   90,   80,  300,  120,  105,  120,   85,  130,   80,   90,   90,
    90,   90,  110,  200,  140,  200,  120,  200,  140,  200,  120,  500,  200,  100,  200,   85,
   200,   80,  200,  120,   90,  100,  100,  120,
};
const int16_t Tables::JapaneseFemaleBandwidth1FrequencyTable[] =
{
191, 70, 58, 68, 64,
};

const int16_t Tables::FemaleBandwidth2FrequencyTable[] =
{
   180,  165,  150,  190,  180,  140,  195,  145,  170,  120,  120,  120,  165,  165,-32580,  145,
   125,-32576,  220,  165,  170,  155,  130,  300,  340,  170,  600,  110,  160,  125,  125,  200,
   110,  120,  200,  140,  120,  220,  210,  200,  170,  240,  160,   -1,  180,  110,  150,  120,
   160,  125,  230,  170,  100,  100,   -1,   90,
};
const int16_t Tables::JapaneseFemaleBandwidth2FrequencyTable[] =
{
123, 137, 135, 102, 120,
};

const int16_t Tables::FemaleBandwidth3FrequencyTable[] =
{
   265,  240,  265,  280,  240,  230,  190,  235,  220,  220,  215,  220,  280,  265,  220,  255,
   220,-32572,  200,  205,  210,  195,  140,  500,  250,  305,  550,  185,  230,  125,  190,  150,
   130,  200,  160,  150,  120,  300,  270,  200,  200,  300,  300,   -1,  180,  150,  250,  170,
   280,  215,  350,  350,  130,  130,   -1,  120,
};
const int16_t Tables::JapaneseFemaleBandwidth3FrequencyTable[] =
{
233, 152, 142, 270, 141,
};

const int16_t Tables::FemaleAmplitudeVoicingVolumeTable[] =
{
62, 62, 62, 61, 63, 60, 62, 61, 61, 61, 62, 62, 62, 61, 62, 61,
62, 66, 66, 62, 61, 61, 66,  0, 60, 59, 60, 58, 58, 58, 59, 64,
64, 64, 60,  0, 56,  0, 56,  0, 56,  0, 56,  0,  0,  0,  0, 26,
 0, 28,  0,  0,  0, 34, 47, 50,
};
const int16_t Tables::JapaneseFemaleAmplitudeVoicingVolumeTable[] =
{
62, 62, 62, 62, 62,
};

const int16_t Tables::LogarithmicToLinearTable[] =
{
0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 5, 5,
6, 6, 7, 8, 9, 10, 11, 13, 14, 16, 18, 20, 23, 25, 29, 32,
};
const int16_t Tables::LogarithmBase2Table[] =
{
0, 0, 1, 2, 2, 3, 4, 5, 5, 6, 7, 7, 8, 9, 9, 10,
11, 12, 12, 13, 14, 14, 15, 16, 16, 17, 18, 18, 19, 20, 21, 21,
22, 23, 23, 24, 25, 25, 26, 27, 27, 28, 29, 29, 30, 31, 31, 32,
33, 33, 34, 35, 35, 36, 37, 37, 38, 38, 39, 40, 40, 41, 42, 42,
43, 44, 44, 45, 46, 46, 47, 47, 48, 49, 49, 50, 51, 51, 52, 52,
53, 54, 54, 55, 56, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62,
63, 64, 64, 65, 65, 66, 67, 67, 68, 68, 69, 70, 70, 71, 71, 72,
73, 73, 74, 74, 75, 76, 76, 77, 77, 78, 78, 79, 80, 80, 81, 81,
82, 82, 83, 84, 84, 85, 85, 86, 87, 87, 88, 88, 89, 89, 90, 90,
91, 92, 92, 93, 93, 94, 94, 95, 96, 96, 97, 97, 98, 98, 99, 99,
100, 100, 101, 102, 102, 103, 103, 104, 104, 105, 105, 106, 106, 107, 108, 108,
109, 109, 110, 110, 111, 111, 112, 112, 113, 113, 114, 114, 115, 116, 116, 117,
117, 118, 118, 119, 119, 120, 120, 121, 121, 122, 122, 123, 123, 124, 124, 125,
125, 126, 126, 127, 127, 128, 128, 129, 129, 130, 131, 131, 132, 132, 133, 133,
134, 134, 135, 135, 136, 136, 137, 137, 138, 138, 139, 139, 140, 140, 140, 141,
141, 142, 142, 143, 143, 144, 144, 145, 145, 146, 146, 147, 147, 148, 148, 149,
149, 150, 150, 151, 151, 152, 152, 153, 153, 154, 154, 155, 155, 155, 156, 156,
157, 157, 158, 158, 159, 159, 160, 160, 161, 161, 162, 162, 162, 163, 163, 164,
164, 165, 165, 166, 166, 167, 167, 168, 168, 168, 169, 169, 170, 170, 171, 171,
172, 172, 173, 173, 173, 174, 174, 175, 175, 176, 176, 177, 177, 177, 178, 178,
179, 179, 180, 180, 181, 181, 181, 182, 182, 183, 183, 184, 184, 185, 185, 185,
186, 186, 187, 187, 188, 188, 188, 189, 189, 190, 190, 191, 191, 191, 192, 192,
193, 193, 194, 194, 194, 195, 195, 196, 196, 197, 197, 197, 198, 198, 199, 199,
200, 200, 200, 201, 201, 202, 202, 202, 203, 203, 204, 204, 205, 205, 205, 206,
206, 207, 207, 207, 208, 208, 209, 209, 209, 210, 210, 211, 211, 212, 212, 212,
213, 213, 214, 214, 214, 215, 215, 216, 216, 216, 217, 217, 218, 218, 218, 219,
219, 220, 220, 220, 221, 221, 222, 222, 222, 223, 223, 223, 224, 224, 225, 225,
225, 226, 226, 227, 227, 227, 228, 228, 229, 229, 229, 230, 230, 231, 231, 231,
232, 232, 232, 233, 233, 234, 234, 234, 235, 235, 235, 236, 236, 237, 237, 237,
238, 238, 239, 239, 239, 240, 240, 240, 241, 241, 242, 242, 242, 243, 243, 243,
244, 244, 245, 245, 245, 246, 246, 246, 247, 247, 247, 248, 248, 249, 249, 249,
250, 250, 250, 251, 251, 252, 252, 252, 253, 253, 253, 254, 254, 254, 255, 255,
};
// Maps each of the 15 control block indices (kF1..kAB) to its type.
const int16_t Tables::ControlBlockTypeTable[] =
{
0, 0, 0, 1, 1, 1, 2, 3, 3, 4, 4, 4, 4, 4, 4,
};

// Locus tables: each consonant phoneme has 9 entries [F1_locus,F1_pct,F1_time, F2_locus,F2_pct,F2_time, F3_locus,F3_pct,F3_time].
// locus_freq: Hz value toward which the formant transitions at the C boundary.
// locus_pct: how far (0-100%) the onset/offset moves toward the locus.
// trans_time_ms: transition duration in ms.
// Locus frequencies sourced from:
//   Klatt, D.H. (1980). Software for a cascade/parallel formant synthesizer.
//   J. Acoust. Soc. Am., 67(3), 971-995. Table III.
// locus_pct and trans_time_ms retained from prior empirical tuning.
const int16_t Tables::MaleLocusTable[] =
{
340, 63, 30, 1100, 92, 35, 2080, 35, 30,   // F  Front
340, 60, 30, 1200, 91, 35, 2080, 65, 40,   // F  Mid/Back
220, 50, 30, 1100, 92, 35, 2080, 35, 40,   // V  Front
220, 50, 30, 1200, 91, 40, 2080, 65, 40,   // V  Mid/Back
320, 10, 45, 1290, 20, 50, 2540,  0, 50,   // TH Front
320, 10, 50, 1240, 12, 55, 2540, 11, 55,   // TH Mid/Back
270, 10, 45, 1290, 20, 50, 2540,  0, 50,   // DH Front
270, 10, 50, 1240, 12, 55, 2540, 11, 55,   // DH Mid/Back
320, 40, 40, 1390, 40, 50, 2530,  0, 70,   // S  Front
320, 40, 40, 1390, 40, 50, 2530,  0, 70,   // S  Middle
320, 40, 40, 1470, 15, 60, 2530,  0, 65,   // S  Back
240, 40, 40, 1390, 35, 50, 2530,  0, 70,   // Z  Front
240, 40, 40, 1390, 35, 60, 2530,  0, 70,   // Z  Middle
240, 40, 40, 1470, 15, 60, 2530,  0, 65,   // Z  Back
300, 32, 55, 1840, 30, 70, 2750, 51, 70,   // SH Front
300, 32, 55, 1710, 27, 70, 2750,  0, 85,   // SH Middle
300, 32, 55, 1640, 27, 90, 2750, 20,110,   // SH Back
300, 32, 55, 1840, 30, 70, 2750, 51, 70,   // ZH Front
300, 32, 55, 1710, 27, 70, 2750,  0, 85,   // ZH Middle
300, 32, 55, 1640, 27, 90, 2750, 20,110,   // ZH Back
400, 55, 20, 1100, 56, 30, 2150, 25, 45,   // P  Front
400, 45, 25, 1070, 46, 30, 2150, 40, 50,   // P  Mid/Back
200, 55, 20, 1100, 56, 30, 2150, 25, 45,   // B  Front
200, 45, 25, 1120, 46, 30, 2150, 40, 50,   // B  Mid/Back
400, 43, 35, 1600, 66, 35, 2600, 30, 45,   // T  Front
400, 43, 50, 1500, 40, 75, 2600,  0, 50,   // T  Middle
400, 43, 40, 1500, 40, 95, 2600,  0, 95,   // T  Back
200, 43, 35, 1600, 66, 35, 2600, 30, 45,   // D  Front
200, 43, 50, 1500, 40, 75, 2600,  0, 50,   // D  Middle
200, 43, 40, 1500, 40, 95, 2600,  0, 95,   // D  Back
300, 33, 45, 1990, 20, 50, 2850,117, 50,   // K  Front
300, 33, 50, 1800, 16, 60, 2850,  0, 90,   // K  Middle
300, 33, 40, 1600, 42, 65, 2850, 15, 80,   // K  Back
200, 33, 45, 1990, 20, 50, 2850,113, 50,   // G  Front
200, 33, 50, 1780, 16, 60, 2850,  0, 90,   // G  Middle
200, 45, 40, 1600, 42, 65, 2850, 15, 80,   // G  Back
350, 54, 55, 1800, 25, 70, 2820, 19, 70,   // CH Front
350, 54, 55, 1730, 10, 70, 2820, 10, 70,   // CH Middle
350, 54, 55, 1730, 10, 90, 2820, 10,100,   // CH Back
260, 32, 55, 1800, 25, 70, 2820, 19, 70,   // JH Front
260, 32, 55, 1730, 10, 70, 2820, 10, 70,   // JH Middle
260, 32, 55, 1730, 10, 90, 2820, 10,100,   // JH Back
480, 30, 30, 1270, 10, 35, 2130, 30, 40,   // M  Front
480, 20, 30, 1370, 88, 40, 2130, 80, 25,   // M  Mid/Back
480, 20, 35, 1340, 75, 35, 2470, 40, 45,   // N/EN Front
480, 20, 30, 1410, 25, 75, 2470,  0, 60,   // N/EN Middle
480, 20, 30, 1540, 25, 95, 2470,  0, 95,   // N/EN Back
440, 25, 40, 2200, 15, 60, 3000,105, 60,   // NG Front
440, 25, 40, 1800, 20, 70, 2150, 20, 70,   // NG Middle
440, 25, 40, 1700, 42, 70, 1920, 25, 70,   // NG Back
};
const int16_t Tables::FemaleLocusTable[] =
{
350, 65, 30, 1, 87, 40, 2600, 35, 30, 350, 75, 30, 1, 85, 30, 2000,
65, 40, 330, 65, 30, 1, 85, 40, 2600, 35, 40, 330, 75, 30, 1, 84,
30, 2000, 65, 40, 360, 30, 45, 1300, 40, 70, 3180, 35, 70, 440, 30, 50,
1800, 42, 70, 3060, 11, 70, 340, 30, 45, 1300, 35, 60, 3180, 35, 60, 400,
30, 50, 1800, 42, 70, 3060, 11, 70, 330, 40, 40, 2000, 30, 50, 2980, 0,
70, 390, 40, 40, 1800, 40, 50, 3070, 0, 70, 390, 40, 40, 1900, 15, 60,
2880, 0, 65, 320, 40, 40, 2000, 30, 50, 2980, 0, 70, 320, 35, 40, 1800,
40, 50, 3070, 0, 70, 320, 40, 40, 1900, 15, 60, 2880, 0, 65, 360, 32,
55, 2000, 30, 70, 2940, 51, 70, 360, 32, 55, 1850, 7, 70, 2700, 0, 85,
400, 32, 55, 1840, 7, 90, 2700, 20, 110, 320, 26, 55, 2000, 30, 70, 2940,
51, 70, 320, 26, 55, 1850, 7, 70, 2700, 0, 85, 320, 30, 55, 1840, 7,
90, 2700, 20, 110, 440, 40, 20, 1350, 60, 25, 2690, 55, 35, 440, 40, 25,
1130, 50, 30, 2530, 48, 40, 360, 40, 20, 1350, 60, 25, 2690, 55, 35, 360,
50, 20, 1130, 50, 30, 2530, 48, 40, 470, 33, 35, 2150, 60, 35, 3150, 30,
45, 470, 33, 45, 2050, 0, 80, 2990, 0, 50, 470, 33, 40, 1900, 0, 70,
2900, 20, 70, 370, 33, 35, 2150, 60, 35, 3150, 30, 45, 370, 33, 45, 2050,
0, 80, 2990, 0, 50, 370, 33, 40, 1900, 0, 80, 2900, 20, 80, 440, 33,
40, 2750, 60, 50, 3350, 110, 50, 440, 33, 45, 1900, 0, 60, 2500, 0, 90,
440, 33, 35, 1800, 62, 65, 2520, 15, 80, 320, 33, 45, 2750, 60, 50, 3350,
110, 50, 340, 43, 50, 1900, 0, 60, 2500, 0, 90, 370, 45, 40, 1800, 62,
65, 2520, 15, 80, 400, 34, 55, 2100, 25, 70, 3300, 19, 70, 420, 40, 55,
1900, 10, 70, 3200, 10, 70, 420, 34, 55, 2050, 10, 90, 3100, 10, 100, 370,
32, 55, 2000, 25, 70, 3300, 19, 70, 380, 40, 55, 1900, 10, 70, 3200, 10,
70, 380, 32, 55, 2050, 10, 90, 3100, 10, 100, 470, 30, 30, 1380, 10, 35,
2400, 30, 40, 450, 20, 30, 1, 88, 40, 1750, 80, 30, 450, 24, 35, 1500,
75, 35, 3000, 35, 45, 450, 22, 30, 1900, 25, 75, 3040, 0, 60, 450, 24,
30, 1900, 25, 80, 2890, 0, 80, 460, 20, 40, 2860, 40, 60, 3200, 65, 60,
500, 20, 40, 1970, 20, 70, 2620, 20, 70, 480, 20, 40, 1650, 42, 70, 2500,
25, 70,
};

// FrontLocusTable[phonId] = byte offset into MaleLocusTable/FemaleLocusTable for front-vowel context.
// MiddleLocusTable/BackLocusTable: same layout for mid/back vowel contexts. -1 = no locus data.
const int16_t Tables::FrontLocusTable[] =
{
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,756,792,846, -1, -1, -1, -1, -1,
-1, -1,792,  0, 36, 72,108,144,198,252,306, -1,360,396,432,486,
540,594,648,702,432,486, -1,108,
};
const int16_t Tables::MiddleLocusTable[] =
{
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,774,810,864, -1, -1, -1, -1, -1,
-1, -1,810, 18, 54, 90,126,162,216,270,324, -1,378,414,450,504,
558,612,666,720,450,504, -1,126,
};
const int16_t Tables::BackLocusTable[] =
{
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,774,828,882, -1, -1, -1, -1, -1,
-1, -1,828, 18, 54, 90,126,180,234,288,342, -1,378,414,468,522,
576,630,684,738,468,522, -1,126,
};

const int16_t Tables::MaleNoiseAmplitudeTable[] =
{
0, 0, 0, 0, 0, 51, 0, 0, 0, 0, 0, 51, 0, 0, 0, 0,
0, 49, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 38, 0, 0,
0, 0, 0, 38, 0, 0, 0, 0, 0, 36, 0, 0, 0, 0, 0, 36,
0, 0, 0, 0, 0, 50, 0, 0, 0, 0, 0, 50, 0, 0, 0, 0,
0, 48, 0, 0, 0, 0, 0, 48, 0, 0, 0, 0, 0, 41, 0, 0,
0, 0, 0, 41, 0, 0, 0, 0, 0, 39, 0, 0, 0, 0, 0, 39,
0, 0, 0, 0, 55, 0, 0, 0, 0, 0, 55, 0, 0, 0, 0, 0,
55, 0, 0, 0, 0, 0, 55, 0, 0, 0, 0, 0, 55, 0, 0, 0,
0, 0, 55, 0, 0, 0, 0, 0, 55, 0, 0, 0, 0, 0, 55, 0,
0, 46, 63, 0, 60, 0, 0, 46, 63, 0, 60, 0, 0, 46, 63, 0,
63, 0, 0, 46, 63, 0, 63, 0, 0, 43, 59, 0, 42, 0, 0, 43,
59, 0, 42, 0, 0, 43, 54, 0, 36, 0, 0, 43, 54, 0, 36, 0,
0, 0, 0, 0, 44, 61, 0, 0, 0, 0, 44, 61, 0, 0, 0, 0,
44, 61, 0, 0, 0, 0, 44, 61, 0, 0, 0, 0, 40, 56, 0, 0,
0, 0, 40, 56, 0, 0, 0, 0, 40, 56, 0, 0, 0, 0, 40, 56,
0, 0, 48, 0, 55, 0, 0, 0, 52, 0, 54, 0, 33, 37, 60, 0,
50, 0, 33, 37, 60, 0, 50, 0, 0, 0, 44, 0, 52, 0, 0, 0,
46, 0, 51, 0, 0, 0, 57, 0, 0, 0, 0, 0, 55, 0, 0, 0,
0, 42, 51, 44, 33, 0, 43, 0, 0, 46, 0, 0, 43, 0, 28, 38,
0, 0, 43, 0, 28, 38, 0, 0, 0, 38, 0, 33, 0, 0, 39, 0,
0, 42, 0, 0, 38, 0, 23, 33, 0, 0, 38, 0, 23, 33, 0, 0,
0, 49, 53, 0, 48, 0, 0, 49, 53, 0, 48, 0, 0, 49, 52, 0,
42, 0, 0, 49, 52, 0, 42, 0, 0, 49, 63, 0, 48, 0, 0, 49,
63, 0, 48, 0, 0, 49, 60, 0, 42, 0, 0, 49, 60, 0, 42, 0,
0, 0, 44, 0, 52, 0, 0, 0, 46, 0, 51, 0, 0, 0, 57, 0,
0, 0, 0, 0, 55, 0, 0, 0, 0, 0, 0, 0, 44, 61, 0, 0,
0, 0, 44, 61, 0, 0, 0, 0, 44, 61, 0, 0, 0, 0, 44, 61,
};
const int16_t Tables::FemaleNoiseAmplitudeTable[] =
{
0, 0, 0, 0, 35, 52, 0, 0, 0, 0, 35, 52, 0, 0, 0, 0,
33, 50, 0, 0, 0, 0, 33, 50, 0, 0, 0, 0, 29, 39, 0, 0,
0, 0, 29, 39, 0, 0, 0, 0, 27, 37, 0, 0, 0, 0, 27, 37,
0, 0, 0, 0, 0, 46, 0, 0, 0, 0, 0, 46, 0, 0, 0, 0,
0, 44, 0, 0, 0, 0, 0, 44, 0, 0, 0, 0, 0, 40, 0, 0,
0, 0, 0, 40, 0, 0, 0, 0, 0, 38, 0, 0, 0, 0, 0, 38,
0, 0, 0, 0, 58, 0, 0, 0, 0, 0, 58, 0, 0, 0, 0, 0,
61, 0, 0, 0, 0, 0, 61, 0, 0, 0, 0, 0, 52, 0, 0, 0,
0, 0, 52, 0, 0, 0, 0, 0, 55, 0, 0, 0, 0, 0, 55, 0,
0, 46, 53, 0, 37, 0, 0, 46, 53, 0, 37, 0, 0, 46, 53, 0,
37, 0, 0, 46, 53, 0, 37, 0, 0, 40, 47, 0, 31, 0, 0, 40,
47, 0, 31, 0, 0, 40, 47, 0, 31, 0, 0, 40, 47, 0, 31, 0,
40, 0, 0, 0, 43, 60, 0, 0, 0, 0, 43, 60, 0, 0, 0, 0,
43, 60, 0, 0, 0, 0, 43, 60, 34, 0, 0, 0, 37, 54, 0, 0,
0, 0, 37, 54, 0, 0, 0, 0, 37, 54, 0, 0, 0, 0, 37, 54,
0, 0, 0, 0, 54, 60, 0, 0, 0, 0, 54, 60, 0, 0, 0, 0,
54, 60, 0, 0, 0, 0, 54, 60, 0, 0, 0, 0, 49, 55, 0, 0,
0, 0, 49, 55, 0, 0, 0, 0, 49, 55, 0, 0, 0, 0, 49, 55,
0, 44, 51, 0, 35, 0, 45, 0, 0, 0, 0, 0, 48, 0, 30, 0,
0, 0, 48, 0, 30, 0, 0, 0, 0, 40, 0, 0, 0, 0, 41, 0,
0, 0, 0, 0, 43, 0, 25, 0, 0, 0, 43, 0, 25, 0, 0, 0,
0, 46, 53, 0, 37, 0, 0, 46, 53, 0, 37, 0, 0, 46, 49, 0,
34, 0, 0, 46, 49, 0, 34, 0, 0, 46, 53, 0, 37, 0, 0, 46,
53, 0, 37, 0, 0, 46, 49, 0, 34, 0, 0, 46, 49, 0, 34, 0,
0, 0, 0, 0, 49, 55, 0, 0, 0, 0, 49, 55, 0, 0, 0, 0,
49, 55, 0, 0, 0, 0, 49, 55, 40, 0, 0, 0, 43, 60, 0, 0,
0, 0, 43, 60, 0, 0, 0, 0, 43, 60, 0, 0, 0, 0, 43, 60,
};

const uint32_t Tables::ReciprocalTable[] =
{
65536, 65536, 32768, 21845, 16384, 13107, 10922, 9362, 8192, 7281, 6553, 5957, 5461, 5041, 4681, 4369,
4096, 3855, 3640, 3449, 3276, 3120, 2978, 2849, 2730, 2621, 2520, 2427, 2340, 2259, 2184, 2114,
2048, 1985, 1927, 1872, 1820, 1771, 1724, 1680, 1638, 1598, 1560, 1524, 1489, 1456, 1424, 1394,
1365, 1337, 1310, 1285, 1260, 1236, 1213, 1191, 1170, 1149, 1129, 1110, 1092, 1074, 1057, 1040,
1024, 1008, 992, 978, 963, 949, 936, 923, 910, 897, 885, 873, 862, 851, 840, 829,
819, 809, 799, 789, 780, 771, 762, 753, 744, 736, 728, 720, 712, 704, 697, 689,
682, 675, 668, 661,
};
const int16_t Tables::MaleEnvelopeTable[] =
{
604, 5, 429, 80, 773, 50, 482, 80, 556, 35, 411, 85, 742, 40, 523, 90,
548, 20, 429, 75, 363, 20, 330, 85, 312, 20, 340, 70, 367, 40, 463, 75,
605, 15, 505, 90, 727, 40, 545, 70, 487, 35, 501, 70, 460, 35, 480, 75,
1860, 40, 1730, 90, 1783, 50, 1550, 90, 1507, 20, 1460, 90, 1117, 40, 1175, 75,
1760, 5, 1980, 80, 1230, 40, 1915, 85, 931, 35, 1844, 85, 1260, 40, 860, 85,
1082, 20, 845, 75, 1305, 20, 970, 85, 2111, 20, 1050, 80, 1940, 40, 1330, 75,
1705, 50, 1300, 75, 1139, 40, 1300, 70, 858, 40, 1200, 75, 997, 35, 1190, 75,
1680, 25, 1200, 75, 2524, 5, 2550, 80, 2655, 50, 2445, 70, 2578, 50, 2388, 75,
2635, 50, 2585, 90, 2416, 20, 2331, 80, 2280, 20, 2267, 80, 2660, 20, 2217, 65,
2710, 30, 1660, 90, 2460, 40, 1650, 80, 2605, 40, 1670, 80, 2480, 40, 1650, 80,
2371, 30, 1740, 90, 110, 40, 77, 90, 105, 50, 95, 90, 135, 50, 130, 90,
110, 40, 150, 90, 145, 20, 105, 65, 255, 20, 195, 50,
};
const int16_t Tables::FemaleEnvelopeTable[] =
{
789, 20, 786, 80, 550, 29, 587, 75, 599, 20, 434, 90, 863, 50, 585, 90,
651, 50, 444, 85, 842, 40, 588, 90, 633, 20, 439, 80, 391, 20, 360, 85,
352, 30, 365, 70, 372, 40, 523, 75, 710, 20, 575, 90, 877, 40, 610, 70,
552, 40, 556, 70, 460, 30, 520, 75, 2486, 5, 2625, 90, 2135, 40, 2050, 90,
2108, 50, 1875, 90, 1737, 50, 1670, 80, 1484, 25, 1521, 60, 1080, 50, 1110, 90,
2075, 15, 2280, 85, 1405, 40, 2190, 80, 1021, 40, 2039, 85, 1535, 20, 1075, 65,
1232, 20, 895, 60, 1405, 20, 1125, 90, 2386, 35, 1250, 90, 2365, 40, 1480, 80,
2105, 45, 1450, 80, 1279, 40, 1450, 70, 1048, 40, 1350, 75, 1242, 35, 1340, 75,
2864, 10, 2975, 80, 2855, 60, 2920, 90, 2853, 50, 2738, 90, 2835, 25, 2760, 75,
2666, 20, 2656, 90, 2530, 20, 2467, 85, 2910, 30, 2517, 65, 2910, 30, 1810, 70,
2710, 40, 1800, 80, 2830, 35, 1820, 70, 2680, 35, 1800, 80, 2646, 30, 1890, 80,
95, 20, 120, 60, 135, 50, 95, 90, 120, 50, 95, 90, 120, 40, 160, 80,
145, 30, 120, 60, 275, 30, 195, 70,
};

}  // namespace SharpVox
