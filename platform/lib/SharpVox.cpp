#include "SharpVox.h"

#include <cstdio>
#include <stdexcept>

#include "../../include/TtsEngine.h"
#include "../../include/LibraryData.h"

namespace SharpVox {

static std::function<const uint8_t*(const std::string&, size_t&)> MakeSymbolsLookup() {
    return [](const std::string& key, size_t& outSize) -> const uint8_t* {
        auto it = LibraryData::SymbolsTable.find(key);
        if (it != LibraryData::SymbolsTable.end()) {
            outSize = it->second.size();
            return it->second.data();
        }
        outSize = 0;
        return nullptr;
    };
}

SharpVoxSpeaker::SharpVoxSpeaker()
    : _engine(BuildVoice(), LibraryData::dictionary,
              static_cast<size_t>(LibraryData::dictionarySize),
              MakeSymbolsLookup(), SampleRate) {
}

std::string SharpVoxSpeaker::PrepareText(const std::string& text) {
    if (!KlattschMode) {
        return text;
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "b%.0f r%.0f v%.1f w%.1f h%.2f t%.2f g%.2f",
        (double)KlBaseF0, (double)KlRate, (double)KlVibrato, (double)KlVibRate,
        (double)KlAsp, (double)KlTilt, (double)KlEffort);
    KlattschMode = false;
    return std::string("[:klattsch on] ") + buf + " " + text + " [:klattsch off]";
}

void SharpVoxSpeaker::Speak(const std::string& text,
                             std::function<void(const int16_t*, int32_t)> onBuffer) {
    _isSpeaking = true;
    try {
        _engine.Speak(PrepareText(text), onBuffer);
        _isSpeaking = false;
    } catch (...) {
        _isSpeaking = false;
        throw;
    }
}

void SharpVoxSpeaker::SpeakWithEvents(const std::string& text,
                                       std::function<void(const int16_t*, int32_t)> onBuffer,
                                       std::function<void(const std::vector<PhonemeEvent>&)> onEventsReady) {
    _isSpeaking = true;
    try {
        _engine.SpeakWithEvents(PrepareText(text), onBuffer,
            [&](const std::vector<PhonemeEvent>& events) {
                _phonemeEvents = events;
                _nextPhonemeIndex = 0;
                _pollElapsed = 0.0f;
                onEventsReady(events);
            });
        _isSpeaking = false;
    } catch (...) {
        _isSpeaking = false;
        throw;
    }
}

void SharpVoxSpeaker::PollAbsolute(float absoluteSeconds) {
    if (!OnPhoneme || _nextPhonemeIndex >= static_cast<int32_t>(_phonemeEvents.size())) {
        return;
    }
    _pollElapsed = absoluteSeconds;
    while (_nextPhonemeIndex < static_cast<int32_t>(_phonemeEvents.size()) &&
           _phonemeEvents[_nextPhonemeIndex].TimeSeconds <= _pollElapsed) {
        OnPhoneme(_phonemeEvents[_nextPhonemeIndex++]);
    }
}

void SharpVoxSpeaker::SetVoice(VoiceData voice) {
    voice.Rate = static_cast<int16_t>(Rate);
    voice.PitchHz = static_cast<int16_t>(PitchHz);
    _engine.GetVoice() = voice;
}

void SharpVoxSpeaker::ApplyVoice() {
    _engine = TtsEngine(BuildVoice(), LibraryData::dictionary,
                        static_cast<size_t>(LibraryData::dictionarySize),
                        MakeSymbolsLookup(), SampleRate);
}

void SharpVoxSpeaker::ApplyVoiceInPlace() {
    _engine.SampleRate = SampleRate;
    _engine.SetVoice(BuildVoice());
}

// yep, it's a preset
void SharpVoxSpeaker::SetPreset(VoicePreset value) {
    _preset = value;
    if (value == VoicePreset::Custom) {
        return;
    }
    _applyingPreset = true;
    const VoiceData& v = (value == VoicePreset::Whisper) ? VoiceData::whisper_voice() : VoiceData::baseline_voice();
    SetFemale(v.VoiceType == 1);
    _tractScale = v.TractScale;
    _f5Freq = v.F5Freq;
    _f5BW = v.F5BW;
    _voicingGain = v.VGain;
    _aspirationGain = v.AGain;
    _aspirationCycle = v.ACycle;
    _tremoloDepth = v.TremoloDepth;
    _tremoloRate = v.TremoloRate;
    _jitter = v.Jitter;
    _shimmer = v.Shimmer;
    _diplophonia = v.Diplophonia;
    _fryAmount = v.FryAmount;
    _subglottalAmt = v.SubglottalAmt;
    _breathAmt = v.BreathAmt;
    _openQuotient = v.OpenQuotient;
    _oqStressLink = v.OQStressLink;
    _oqF0Link = v.OQF0Link;
    _onsetHardness = v.OnsetHardness;
    _pitchOffsetHz = v.PitchOffsetHz;
    _larynxOffset = v.LarynxOffset;
    _pharyngealAmt = v.PharyngealAmt;
    _lipRounding = v.LipRounding;
    _f4Freq = v.F4Freq;
    _f4BW = v.F4BW;
    _f4pFreq = v.F4pFreq;
    _f4pBW = v.F4pBW;
    _f5pFreq = v.F5pFreq;
    _f5pBW = v.F5pBW;
    _f6pFreq = v.F6pFreq;
    _f6pBW = v.F6pBW;
    _bwGain1 = v.BwGain1;
    _bwGain2 = v.BwGain2;
    _bwGain3 = v.BwGain3;
    _nasalBase = v.NasalBase;
    _nasalTarg = v.NasalTarg;
    _nasalBW = v.NasalBW;
    _nGain = v.NGain;
    _pitchRange = v.PitchRange;
    _stressGain = v.StressGain;
    _intonation = v.Intonation;
    _riseAmt = v.RiseAmt;
    _fallAmt = v.FallAmt;
    _baselineFall = v.BaselineFall;
    _uptalkAmt = v.UptalkAmt;
    _stressEarly = v.StressEarly;
    _breakStrength = v.BreakStrength;
    _emphasisBoost = v.EmphasisBoost;
    _vocalConfidence = v.VocalConfidence;
    Rate = v.Rate;
    PitchHz = v.PitchHz;
    _applyingPreset = false;
}

void SharpVoxSpeaker::MarkCustom() {
    if (!_applyingPreset) {
        _preset = VoicePreset::Custom;
    }
}

// yep, make a new one
VoiceData SharpVoxSpeaker::BuildVoice() {
    VoiceData v;
    switch (_preset) {
        case VoicePreset::Whisper:
            v = VoiceData::whisper_voice();
            break;
        case VoicePreset::Custom:
            v = VoiceData();
            v.VGain = static_cast<int16_t>(GetVoicingGain());
            v.AGain = static_cast<int16_t>(GetAspirationGain());
            v.ACycle = static_cast<int16_t>(GetAspirationCycle());
            v.TremoloDepth = static_cast<int16_t>(GetTremoloDepth());
            v.TremoloRate = static_cast<int16_t>(GetTremoloRate());
            v.Jitter = static_cast<int16_t>(GetJitter());
            v.Shimmer = static_cast<int16_t>(GetShimmer());
            v.Diplophonia = static_cast<int16_t>(GetDiplophonia());
            v.FryAmount = static_cast<int16_t>(GetFryAmount());
            v.SubglottalAmt = static_cast<int16_t>(GetSubglottalAmt());
            v.BreathAmt = static_cast<int16_t>(GetBreathAmt());
            v.OpenQuotient = static_cast<int16_t>(GetOpenQuotient());
            v.OQStressLink = static_cast<int16_t>(GetOQStressLink());
            v.OQF0Link = static_cast<int16_t>(GetOQF0Link());
            v.OnsetHardness = static_cast<int16_t>(GetOnsetHardness());
            v.PitchOffsetHz = static_cast<int16_t>(GetPitchOffsetHz());
            v.LarynxOffset = static_cast<int16_t>(GetLarynxOffset());
            v.PharyngealAmt = static_cast<int16_t>(GetPharyngealAmt());
            v.LipRounding = static_cast<int16_t>(GetLipRounding());
            v.TractScale = GetTractScale();
            v.F5Freq = static_cast<int16_t>(GetF5Freq());
            v.F5BW = static_cast<int16_t>(GetF5BW());
            v.F4Freq = static_cast<int16_t>(GetF4Freq());
            v.F4BW = static_cast<int16_t>(GetF4BW());
            v.F4pFreq = static_cast<int16_t>(GetF4pFreq());
            v.F4pBW = static_cast<int16_t>(GetF4pBW());
            v.F5pFreq = static_cast<int16_t>(GetF5pFreq());
            v.F5pBW = static_cast<int16_t>(GetF5pBW());
            v.F6pFreq = static_cast<int16_t>(GetF6pFreq());
            v.F6pBW = static_cast<int16_t>(GetF6pBW());
            v.BwGain1 = static_cast<int16_t>(GetBwGain1());
            v.BwGain2 = static_cast<int16_t>(GetBwGain2());
            v.BwGain3 = static_cast<int16_t>(GetBwGain3());
            v.NasalBase = static_cast<int16_t>(GetNasalBase());
            v.NasalTarg = static_cast<int16_t>(GetNasalTarg());
            v.NasalBW = static_cast<int16_t>(GetNasalBW());
            v.NGain = static_cast<int16_t>(GetNGain());
            v.PitchRange = static_cast<int16_t>(GetPitchRange());
            v.StressGain = static_cast<int16_t>(GetStressGain());
            v.Intonation = static_cast<int16_t>(GetIntonation());
            v.RiseAmt = static_cast<int16_t>(GetRiseAmt());
            v.FallAmt = static_cast<int16_t>(GetFallAmt());
            v.BaselineFall = static_cast<int16_t>(GetBaselineFall());
            v.UptalkAmt = static_cast<int16_t>(GetUptalkAmt());
            v.StressEarly = static_cast<int16_t>(GetStressEarly());
            v.BreakStrength = static_cast<int16_t>(GetBreakStrength());
            v.EmphasisBoost = static_cast<int16_t>(GetEmphasisBoost());
            v.VocalConfidence = static_cast<int16_t>(GetVocalConfidence());
            break;
        default:
            v = VoiceData::baseline_voice();
            break;
    }
    v.Rate = static_cast<int16_t>(Rate);
    v.PitchHz = static_cast<int16_t>(PitchHz);
    v.TractScale = GetTractScale();
    v.VoiceType = static_cast<int16_t>(_female ? 1 : 0);
    return v;
}

void SharpVoxSpeaker::ApplyVoiceData(const VoiceData& v) {
    _engine = TtsEngine(v, LibraryData::dictionary,
                        static_cast<size_t>(LibraryData::dictionarySize),
                        MakeSymbolsLookup(), SampleRate);
}

}  // namespace SharpVox
