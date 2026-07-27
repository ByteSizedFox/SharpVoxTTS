#ifndef SHARPVOX_VOICE_DATA_H
#define SHARPVOX_VOICE_DATA_H

#include <cstdint>

namespace SharpVox {

struct VoiceData {
    int16_t PitchHz = 208;
    float TractScale = 1.0f;
    int16_t PitchRange = 120;
    int16_t StressGain = 60;
    int16_t Rate = 160;
    int16_t VoiceType = 0;
    int16_t VGain = 60;
    int16_t AGain = 0;
    int16_t ACycle = 192;

    int16_t TremoloDepth = 0;   // 0-100; maps to Frame.TremDepth byte (/100 -> 0.0-1.0)
    int16_t TremoloRate = 0;    // 0-200; maps to Frame.TremRate byte (/10 -> 0-20 Hz)

    int16_t Jitter = 0;        // 0-100: random cycle-to-cycle F0 perturbation (0=none, 100=+/-5% period)
    int16_t Shimmer = 0;       // 0-100: random cycle-to-cycle amplitude perturbation (0=none, 100=+/-20%)
    int16_t Diplophonia = 0;   // 0-100: alternating strong/weak pulse pattern -> subharmonic at F0/2
    int16_t FryAmount = 0;     // 0-100: vocal fry - random period extension creating irregular creak
    int16_t SubglottalAmt = 0; // 0-100: subglottal resonance coupling (~350 Hz chest cavity texture)
    int16_t BreathAmt = 0;     // 0-100: cycle-synchronous breathiness - open-phase noise via glottal waveform envelope
    int16_t OpenQuotient = 50; // 0=pressed/bright (short open phase), 50=neutral, 100=breathy/dark (long open phase)
    int16_t OQStressLink = 0;  // 0-100: effort->pressed (stressed syllables push OQ down - brighter/more harmonic)
    int16_t OQF0Link = 0;      // 0-100: F0->breathy (higher pitch pushes OQ up - models head voice / falsetto)
    int16_t LarynxOffset = 0;     // Hz: shifts F1-F6 up/down coherently; >0 raised larynx (brighter), <0 lowered (darker/operatic)
    int16_t PharyngealAmt = 0;   // 0-100: pharyngeal constriction - F1 up (+1 Hz/unit), F2 dn (-2 Hz/unit)
    int16_t PitchOffsetHz = 0;   // Hz: shifts F0 up/down for all speech and singing; transposes explicit notes
    int16_t LipRounding = 0;     // -100=spread (F1 up,F2++,F3 up), 0=neutral, +100=rounded (F1 dn,F2--,F3 dn)
    int16_t OnsetHardness = 50;  // 0=soft breathy onset (slow ramp), 50=natural, 100=hard glottal attack (instant)

    int16_t F4Freq = 4825;
    int16_t F4BW = 650;
    int16_t F5Freq = 4000;
    int16_t F5BW = 750;
    int16_t F4pFreq = 3650;
    int16_t F4pBW = 150;
    int16_t F5pFreq = 4200;
    int16_t F5pBW = 100;
    int16_t F6pFreq = 6000;
    int16_t F6pBW = 700;

    int16_t NasalBase = 330;
    int16_t NasalTarg = 400;
    int16_t NasalBW = 60;

    int16_t Locus = 55;
    int16_t BwGain1 = 135;
    int16_t BwGain2 = 110;
    int16_t BwGain3 = 100;
    int16_t F1_Offset = 5;
    int16_t F2_Offset = 15;
    int16_t F3_Offset = 15;
    int16_t Chorus = 0;
    int16_t NGain = 100;

    int16_t SPitchMidi = 0;
    int16_t SGain = 0;
    int16_t AsperW = 2;

    int16_t NasalAmt = 0;
    int16_t EmphVoice = 1;
    int16_t RvbDelay = 35;
    int16_t RvbDepth = 0;

    int16_t RiseAmt = 29;
    int16_t FallAmt = -29;
    int16_t RiseAmt1 = 100;    // accent peak target above the phrase floor, pitch units
    int16_t FallAmt1 = -29;
    int32_t Assertiveness = 0x10000;
    int16_t BaselineFall = 25;
    int32_t Quickness = 7200;
    int32_t DownRampStep = 15360;
    int16_t StressDurTime = 50;
    int16_t VibratoDepth1Raw = 14;
    int16_t VibratoDepth2Raw = 16;
    int16_t VibratoFreqRaw = 65; // 6.5hz
    int16_t Intonation = 100;

    int16_t UptalkAmt = 0;        // 0-100: sentence-final rising tendency (0=natural fall, 100=strong uptalk/rise)
    int16_t StressEarly = 0;      // -50 to +50: stress peak alignment (-50=early/assertive, 0=natural, +50=late/hesitant)
    int16_t BreakStrength = 50;   // 0-100: phrase boundary reset strength (0=smooth carry-over, 50=natural, 100=hard reset)
    int16_t EmphasisBoost = 0;    // 0-100: extra pitch height for emphatic vs primary stress
    int16_t VocalConfidence = 0;  // 0-100: pronoun emphasis - subject pronouns (I/you/he/she/it/we/they) get a rise-fall pitch accent and vowel lengthening

    static VoiceData baseline_voice() {
        return VoiceData{};
    }

    static VoiceData whisper_voice() {
        VoiceData v{};
        v.PitchHz = 220;
        v.StressGain = 70;
        v.Rate = 140;
        v.VGain = 0;
        v.AGain = 400;
        v.ACycle = 16;
        v.F4Freq = 3500;
        v.F4BW = 50;
        v.F5Freq = 4500;
        v.F5BW = 250;
        v.F4pFreq = 4500;
        v.BwGain1 = 100;
        v.BwGain3 = 50;
        v.NGain = 200;
        return v;
    }
};

}  // namespace SharpVox

#endif  // SHARPVOX_VOICE_DATA_H
