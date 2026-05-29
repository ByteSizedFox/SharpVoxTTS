#include "../include/VoicePresets.h"
#include <algorithm>
#include <cctype>

namespace SharpVox {

bool VoicePresets::TryGet(const std::string& name, VoiceData& outVoice) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (lower == "beth") {
        outVoice = VoiceData::baseline_voice();
        outVoice.Rate = 162;
        outVoice.PitchHz = 476;
        outVoice.VoiceType = 1;
        outVoice.TractScale = 1.09f;
        outVoice.VGain = 60;
        outVoice.AGain = 30;
        outVoice.ACycle = 192;
        outVoice.NGain = 107;
        outVoice.F4Freq = 4380;
        outVoice.F4BW = 262;
        outVoice.F5Freq = 2510;
        outVoice.F5BW = 2025;
        outVoice.F4pFreq = 4384;
        outVoice.F4pBW = 154;
        outVoice.F5pFreq = 4199;
        outVoice.F5pBW = 97;
        outVoice.F6pFreq = 4493;
        outVoice.F6pBW = 149;
        outVoice.BwGain1 = 134;
        outVoice.BwGain2 = 120;
        outVoice.BwGain3 = 105;
        outVoice.NasalBase = 323;
        outVoice.NasalTarg = 405;
        outVoice.NasalBW = 53;
        outVoice.PitchRange = 148;
        outVoice.StressGain = 60;
        outVoice.Intonation = 100;
        outVoice.RiseAmt = 38;
        outVoice.FallAmt = -38;
        outVoice.BaselineFall = 53;
        return true;
    }
    if (lower == "chris") {
        outVoice = VoiceData::baseline_voice();
        outVoice.Rate = 162;
        outVoice.PitchHz = 276;
        outVoice.VoiceType = 0;
        outVoice.TractScale = 0.99f;
        outVoice.VGain = 58;
        outVoice.AGain = 175;
        outVoice.ACycle = 192;
        outVoice.NGain = 103;
        outVoice.F4Freq = 3580;
        outVoice.F4BW = 255;
        outVoice.F5Freq = 4050;
        outVoice.F5BW = 290;
        outVoice.F4pFreq = 3574;
        outVoice.F4pBW = 148;
        outVoice.F5pFreq = 4194;
        outVoice.F5pBW = 104;
        outVoice.F6pFreq = 4513;
        outVoice.F6pBW = 156;
        outVoice.BwGain1 = 148;
        outVoice.BwGain2 = 109;
        outVoice.BwGain3 = 107;
        outVoice.NasalBase = 331;
        outVoice.NasalTarg = 398;
        outVoice.NasalBW = 64;
        outVoice.PitchRange = 145;
        outVoice.StressGain = 60;
        outVoice.Intonation = 101;
        outVoice.RiseAmt = 35;
        outVoice.FallAmt = -38;
        outVoice.BaselineFall = 43;
        return true;
    }
    if (lower == "deborah") {
        outVoice = VoiceData::baseline_voice();
        outVoice.Rate = 148;
        outVoice.PitchHz = 336;
        outVoice.VoiceType = 1;
        outVoice.TractScale = 1.05f;
        outVoice.VGain = 60;
        outVoice.AGain = 200;
        outVoice.ACycle = 192;
        outVoice.NGain = 99;
        outVoice.F4Freq = 4020;
        outVoice.F4BW = 250;
        outVoice.F5Freq = 2503;
        outVoice.F5BW = 2052;
        outVoice.F4pFreq = 4026;
        outVoice.F4pBW = 142;
        outVoice.F5pFreq = 4196;
        outVoice.F5pBW = 101;
        outVoice.F6pFreq = 4499;
        outVoice.F6pBW = 142;
        outVoice.BwGain1 = 136;
        outVoice.BwGain2 = 130;
        outVoice.BwGain3 = 100;
        outVoice.NasalBase = 336;
        outVoice.NasalTarg = 395;
        outVoice.NasalBW = 58;
        outVoice.PitchRange = 72;
        outVoice.StressGain = 60;
        outVoice.Intonation = 96;
        outVoice.RiseAmt = 22;
        outVoice.FallAmt = -25;
        outVoice.BaselineFall = 62;
        return true;
    }
    if (lower == "jack") {
        outVoice = VoiceData::baseline_voice();
        outVoice.Rate = 155;
        outVoice.PitchHz = 310;
        outVoice.VoiceType = 0;
        outVoice.TractScale = 1.02f;
        outVoice.VGain = 58;
        outVoice.AGain = 185;
        outVoice.ACycle = 192;
        outVoice.NGain = 101;
        outVoice.F4Freq = 3720;
        outVoice.F4BW = 275;
        outVoice.F5Freq = 4280;
        outVoice.F5BW = 315;
        outVoice.F4pFreq = 3714;
        outVoice.F4pBW = 157;
        outVoice.F5pFreq = 4210;
        outVoice.F5pBW = 92;
        outVoice.F6pFreq = 4503;
        outVoice.F6pBW = 150;
        outVoice.BwGain1 = 126;
        outVoice.BwGain2 = 107;
        outVoice.BwGain3 = 118;
        outVoice.NasalBase = 329;
        outVoice.NasalTarg = 404;
        outVoice.NasalBW = 67;
        outVoice.PitchRange = 80;
        outVoice.StressGain = 60;
        outVoice.Intonation = 106;
        outVoice.RiseAmt = 34;
        outVoice.FallAmt = -17;
        outVoice.BaselineFall = 47;
        return true;
    }
    if (lower == "jess") {
        outVoice = VoiceData::baseline_voice();
        outVoice.Rate = 166;
        outVoice.PitchHz = 436;
        outVoice.VoiceType = 1;
        outVoice.TractScale = 1.07f;
        outVoice.VGain = 60;
        outVoice.AGain = 0;
        outVoice.ACycle = 192;
        outVoice.NGain = 97;
        outVoice.F4Freq = 4460;
        outVoice.F4BW = 270;
        outVoice.F5Freq = 2508;
        outVoice.F5BW = 2042;
        outVoice.F4pFreq = 4464;
        outVoice.F4pBW = 149;
        outVoice.F5pFreq = 4203;
        outVoice.F5pBW = 106;
        outVoice.F6pFreq = 4507;
        outVoice.F6pBW = 153;
        outVoice.BwGain1 = 138;
        outVoice.BwGain2 = 120;
        outVoice.BwGain3 = 103;
        outVoice.NasalBase = 321;
        outVoice.NasalTarg = 408;
        outVoice.NasalBW = 50;
        outVoice.PitchRange = 228;
        outVoice.StressGain = 60;
        outVoice.Intonation = 104;
        outVoice.RiseAmt = 40;
        outVoice.FallAmt = -35;
        outVoice.BaselineFall = 47;
        return true;
    }
    if (lower == "john") {
        outVoice = VoiceData::baseline_voice();
        outVoice.Rate = 158;
        outVoice.PitchHz = 242;
        outVoice.VoiceType = 0;
        outVoice.TractScale = 0.96f;
        outVoice.VGain = 60;
        outVoice.AGain = 62;
        outVoice.ACycle = 192;
        outVoice.TremoloDepth = 0;
        outVoice.TremoloRate = 0;
        outVoice.Jitter = 0;
        outVoice.Shimmer = 0;
        outVoice.Diplophonia = 0;
        outVoice.FryAmount = 0;
        outVoice.SubglottalAmt = 0;
        outVoice.BreathAmt = 0;
        outVoice.OpenQuotient = 50;
        outVoice.OQStressLink = 0;
        outVoice.OQF0Link = 0;
        outVoice.LarynxOffset = 0;
        outVoice.PharyngealAmt = 0;
        outVoice.PitchOffsetHz = 0;
        outVoice.LipRounding = 0;
        outVoice.OnsetHardness = 50;
        outVoice.NGain = 100;
        outVoice.F4Freq = 2392;
        outVoice.F4BW = 240;
        outVoice.F5Freq = 3780;
        outVoice.F5BW = 265;
        outVoice.F4pFreq = 3415;
        outVoice.F4pBW = 152;
        outVoice.F5pFreq = 4198;
        outVoice.F5pBW = 100;
        outVoice.F6pFreq = 4508;
        outVoice.F6pBW = 150;
        outVoice.BwGain1 = 136;
        outVoice.BwGain2 = 112;
        outVoice.BwGain3 = 101;
        outVoice.NasalBase = 327;
        outVoice.NasalTarg = 402;
        outVoice.NasalBW = 60;
        outVoice.PitchRange = 118;
        outVoice.StressGain = 60;
        outVoice.Intonation = 94;
        outVoice.RiseAmt = 37;
        outVoice.FallAmt = -34;
        outVoice.BaselineFall = 47;
        return true;
    }
    if (lower == "matt") {
        outVoice = VoiceData::baseline_voice();
        outVoice.Rate = 154;
        outVoice.PitchHz = 172;
        outVoice.VoiceType = 0;
        outVoice.TractScale = 0.92f;
        outVoice.VGain = 60;
        outVoice.AGain = 95;
        outVoice.ACycle = 192;
        outVoice.NGain = 100;
        outVoice.F4Freq = 3150;
        outVoice.F4BW = 200;
        outVoice.F5Freq = 3620;
        outVoice.F5BW = 238;
        outVoice.F4pFreq = 3142;
        outVoice.F4pBW = 155;
        outVoice.F5pFreq = 4192;
        outVoice.F5pBW = 97;
        outVoice.F6pFreq = 4496;
        outVoice.F6pBW = 147;
        outVoice.BwGain1 = 142;
        outVoice.BwGain2 = 111;
        outVoice.BwGain3 = 109;
        outVoice.NasalBase = 326;
        outVoice.NasalTarg = 401;
        outVoice.NasalBW = 70;
        outVoice.PitchRange = 65;
        outVoice.StressGain = 60;
        outVoice.Intonation = 90;
        outVoice.RiseAmt = 34;
        outVoice.FallAmt = -27;
        outVoice.BaselineFall = 63;
        return true;
    }
    if (lower == "pirate") {
        outVoice = VoiceData::baseline_voice();
        outVoice.PitchHz = 282;
        outVoice.TractScale = 0.944f;
        outVoice.VoiceType = 0;
        outVoice.Rate = 178;
        outVoice.OpenQuotient = 27;
        outVoice.BreathAmt = 18;
        outVoice.Jitter = 38;
        outVoice.Shimmer = 25;
        outVoice.FryAmount = 44;
        outVoice.Diplophonia = 41;
        outVoice.SubglottalAmt = 6;
        outVoice.PitchRange = 149;
        outVoice.Intonation = 113;
        outVoice.RiseAmt = 46;
        outVoice.FallAmt = -18;
        outVoice.BaselineFall = 58;
        outVoice.StressGain = 63;
        outVoice.F4Freq = 3100;
        outVoice.F4BW = 294;
        outVoice.F5Freq = 2759;
        outVoice.F5BW = 200;
        outVoice.BwGain1 = 98;
        outVoice.BwGain2 = 78;
        outVoice.BwGain3 = 95;
        outVoice.NasalBase = 331;
        outVoice.NasalTarg = 421;
        outVoice.NasalBW = 68;
        outVoice.AGain = 260;
        outVoice.NGain = 115;
        outVoice.LarynxOffset = 125;
        outVoice.PharyngealAmt = 0;
        outVoice.LipRounding = -22;
        outVoice.OnsetHardness = 74;
        outVoice.ACycle = 192;
        outVoice.OQStressLink = 0;
        outVoice.OQF0Link = 0;
        outVoice.PitchOffsetHz = 0;
        outVoice.F4pFreq = 3661;
        outVoice.F4pBW = 214;
        outVoice.F5pFreq = 3800;
        outVoice.F5pBW = 200;
        outVoice.F6pFreq = 4500;
        outVoice.F6pBW = 150;
        outVoice.NasalAmt = 0;
        outVoice.VoiceVers = 3;
        outVoice.VGain = 60;
        return true;
    }
    if (lower == "tommy") {
        outVoice = VoiceData::baseline_voice();
        outVoice.Rate = 164;
        outVoice.PitchHz = 636;
        outVoice.VoiceType = 1;
        outVoice.TractScale = 1.22f;
        outVoice.VGain = 60;
        outVoice.AGain = 160;
        outVoice.ACycle = 192;
        outVoice.NGain = 96;
        outVoice.F4Freq = 2510;
        outVoice.F4BW = 2030;
        outVoice.F5Freq = 2508;
        outVoice.F5BW = 2038;
        outVoice.F4pFreq = 2514;
        outVoice.F4pBW = 144;
        outVoice.F5pFreq = 4204;
        outVoice.F5pBW = 99;
        outVoice.F6pFreq = 4502;
        outVoice.F6pBW = 151;
        outVoice.BwGain1 = 138;
        outVoice.BwGain2 = 128;
        outVoice.BwGain3 = 109;
        outVoice.NasalBase = 334;
        outVoice.NasalTarg = 404;
        outVoice.NasalBW = 54;
        outVoice.PitchRange = 235;
        outVoice.StressGain = 60;
        outVoice.Intonation = 95;
        outVoice.RiseAmt = 23;
        outVoice.FallAmt = -22;
        outVoice.BaselineFall = 45;
        return true;
    }
    if (lower == "whisper") {
        outVoice = VoiceData::baseline_voice();
        outVoice.Rate = 148;
        outVoice.PitchHz = 350;
        outVoice.VoiceType = 1;
        outVoice.TractScale = 1.0f;
        outVoice.VGain = 0;
        outVoice.AGain = 420;
        outVoice.ACycle = 16;
        outVoice.NGain = 102;
        outVoice.F4Freq = 4480;
        outVoice.F4BW = 420;
        outVoice.F5Freq = 2508;
        outVoice.F5BW = 2057;
        outVoice.F4pFreq = 4492;
        outVoice.F4pBW = 146;
        outVoice.F5pFreq = 4199;
        outVoice.F5pBW = 100;
        outVoice.F6pFreq = 4507;
        outVoice.F6pBW = 150;
        outVoice.BwGain1 = 138;
        outVoice.BwGain2 = 115;
        outVoice.BwGain3 = 111;
        outVoice.NasalBase = 331;
        outVoice.NasalTarg = 403;
        outVoice.NasalBW = 67;
        outVoice.PitchRange = 170;
        outVoice.StressGain = 60;
        outVoice.Intonation = 90;
        outVoice.RiseAmt = 22;
        outVoice.FallAmt = -24;
        outVoice.BaselineFall = 60;
        return true;
    }

    return false;
}

bool VoicePresets::SetParam(VoiceData& v, const std::string& name, float value) {
    auto i16 = [](float f) { return static_cast<int16_t>(f); };

    if      (name == "pitch")          { v.PitchHz        = i16(value); }
    else if (name == "rate")           { v.Rate           = i16(value); }
    else if (name == "volume"
          || name == "vgain")          { v.VGain          = i16(value); }
    else if (name == "again")          { v.AGain          = i16(value); }
    else if (name == "acycle")         { v.ACycle         = i16(value); }
    else if (name == "tract"
          || name == "tractscale")     { v.TractScale     = value;      }
    else if (name == "pitchrange")     { v.PitchRange     = i16(value); }
    else if (name == "stressgain")     { v.StressGain     = i16(value); }
    else if (name == "voicetype")      { v.VoiceType      = i16(value); }
    else if (name == "tremolodepth")   { v.TremoloDepth   = i16(value); }
    else if (name == "tremolorate")    { v.TremoloRate    = i16(value); }
    else if (name == "jitter")         { v.Jitter         = i16(value); }
    else if (name == "shimmer")        { v.Shimmer        = i16(value); }
    else if (name == "diplophonia")    { v.Diplophonia    = i16(value); }
    else if (name == "fry"
          || name == "fryamount")      { v.FryAmount      = i16(value); }
    else if (name == "subglottal")     { v.SubglottalAmt  = i16(value); }
    else if (name == "breath"
          || name == "breathamt")      { v.BreathAmt      = i16(value); }
    else if (name == "oq"
          || name == "openquotient")   { v.OpenQuotient   = i16(value); }
    else if (name == "oqstresslink")   { v.OQStressLink   = i16(value); }
    else if (name == "oqf0link")       { v.OQF0Link       = i16(value); }
    else if (name == "larynx"
          || name == "larynxoffset")   { v.LarynxOffset   = i16(value); }
    else if (name == "pharyngeal")     { v.PharyngealAmt  = i16(value); }
    else if (name == "pitchoffset")    { v.PitchOffsetHz  = i16(value); }
    else if (name == "liprounding")    { v.LipRounding    = i16(value); }
    else if (name == "onset"
          || name == "onsethard")      { v.OnsetHardness  = i16(value); }
    else if (name == "f4freq")         { v.F4Freq         = i16(value); }
    else if (name == "f4bw")           { v.F4BW           = i16(value); }
    else if (name == "f5freq")         { v.F5Freq         = i16(value); }
    else if (name == "f5bw")           { v.F5BW           = i16(value); }
    else if (name == "f4pfreq")        { v.F4pFreq        = i16(value); }
    else if (name == "f4pbw")          { v.F4pBW          = i16(value); }
    else if (name == "f5pfreq")        { v.F5pFreq        = i16(value); }
    else if (name == "f5pbw")          { v.F5pBW          = i16(value); }
    else if (name == "f6pfreq")        { v.F6pFreq        = i16(value); }
    else if (name == "f6pbw")          { v.F6pBW          = i16(value); }
    else if (name == "nasalbase")      { v.NasalBase      = i16(value); }
    else if (name == "nasaltarg")      { v.NasalTarg      = i16(value); }
    else if (name == "nasalbw")        { v.NasalBW        = i16(value); }
    else if (name == "nasalamt")       { v.NasalAmt       = i16(value); }
    else if (name == "ngain")          { v.NGain          = i16(value); }
    else if (name == "bwgain1")        { v.BwGain1        = i16(value); }
    else if (name == "bwgain2")        { v.BwGain2        = i16(value); }
    else if (name == "bwgain3")        { v.BwGain3        = i16(value); }
    else if (name == "f1offset")       { v.F1_Offset      = i16(value); }
    else if (name == "f2offset")       { v.F2_Offset      = i16(value); }
    else if (name == "f3offset")       { v.F3_Offset      = i16(value); }
    else if (name == "locus")          { v.Locus          = i16(value); }
    else if (name == "chorus")         { v.Chorus         = i16(value); }
    else if (name == "intonation")     { v.Intonation     = i16(value); }
    else if (name == "riseamt")        { v.RiseAmt        = i16(value); }
    else if (name == "fallamt")        { v.FallAmt        = i16(value); }
    else if (name == "riseamt1")       { v.RiseAmt1       = i16(value); }
    else if (name == "fallamt1")       { v.FallAmt1       = i16(value); }
    else if (name == "baselinefall")   { v.BaselineFall   = i16(value); }
    else if (name == "uptalk")         { v.UptalkAmt      = i16(value); }
    else if (name == "stressearly")    { v.StressEarly    = i16(value); }
    else if (name == "breakstrength")  { v.BreakStrength  = i16(value); }
    else if (name == "emphasisboost")  { v.EmphasisBoost  = i16(value); }
    else if (name == "vocalconfidence"){ v.VocalConfidence= i16(value); }
    else if (name == "vibratodepth1")  { v.VibratoDepth1Raw = i16(value); }
    else if (name == "vibratodepth2")  { v.VibratoDepth2Raw = i16(value); }
    else if (name == "vibratofreq")    { v.VibratoFreqRaw = i16(value); }
    else if (name == "rvbdelay")       { v.RvbDelay       = i16(value); }
    else if (name == "rvbdepth")       { v.RvbDepth       = i16(value); }
    else return false;
    return true;
}

} // namespace SharpVox
