#ifndef SHARPVOX_H
#define SHARPVOX_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../../include/TtsEngine.h"

namespace SharpVox {

enum class VoicePreset { Baseline, Whisper, Custom };

class SharpVoxSpeaker {
public:
    SharpVoxSpeaker();

    int32_t SampleRate = 22050;

    bool IsSpeaking() const { return _isSpeaking; }

    void Speak(const std::string& text,
               void (*onBuffer)(SharpVoxSpeaker* speaker, const int16_t* buf, int32_t len, void* userdata),
               void* userdata = nullptr);
    void SpeakWithEvents(const std::string& text,
                         void (*onBuffer)(SharpVoxSpeaker* speaker, const int16_t* buf, int32_t len, void* userdata),
                         void (*onEventsReady)(SharpVoxSpeaker* speaker, const PhonemeEvent* events, int32_t count, void* userdata),
                         void* userdata = nullptr);

    // Polls pending phoneme events up to absoluteSeconds and fires OnPhoneme for each
    void PollAbsolute(float absoluteSeconds);

    // Callback fired by PollAbsolute for each due phoneme event
    std::function<void(const PhonemeEvent&)> OnPhoneme;

    const std::vector<PhonemeEvent>& PhonemeEvents() const { return _phonemeEvents; }

    void SetVoice(VoiceData voice);
    void ApplyVoice();
    void ApplyVoiceInPlace();
    void ApplyVoiceData(const VoiceData& v);

    int32_t Rate = 200;
    int32_t PitchHz = 122;
    float AudioVolume = 1.0f;

    bool KlattschMode = false;
    float KlBaseF0 = 120.0f;
    float KlRate = 110.0f;
    float KlVibrato = 0.0f;
    float KlVibRate = 5.0f;
    float KlAsp = 0.0f;
    float KlTilt = 0.0f;
    float KlEffort = 0.5f;

    // yep, it's a preset
    VoicePreset GetPreset() const { return _preset; }
    void SetPreset(VoicePreset value);

    float OutputVolume = 1.0f;

    // Voice Definition
    bool GetFemale() const { return _female; }
    void SetFemale(bool v) { _female = v; MarkCustom(); }

    int32_t GetVoicingGain() const { return _voicingGain; }
    void SetVoicingGain(int32_t v) { _voicingGain = v; MarkCustom(); }

    int32_t GetAspirationGain() const { return _aspirationGain; }
    void SetAspirationGain(int32_t v) { _aspirationGain = v; MarkCustom(); }

    int32_t GetAspirationCycle() const { return _aspirationCycle; }
    void SetAspirationCycle(int32_t v) { _aspirationCycle = v; MarkCustom(); }

    int32_t GetTremoloDepth() const { return _tremoloDepth; }
    void SetTremoloDepth(int32_t v) { _tremoloDepth = v; MarkCustom(); }

    int32_t GetTremoloRate() const { return _tremoloRate; }
    void SetTremoloRate(int32_t v) { _tremoloRate = v; MarkCustom(); }

    // Glottal source
    int32_t GetJitter() const { return _jitter; }
    void SetJitter(int32_t v) { _jitter = v; MarkCustom(); }

    int32_t GetShimmer() const { return _shimmer; }
    void SetShimmer(int32_t v) { _shimmer = v; MarkCustom(); }

    int32_t GetDiplophonia() const { return _diplophonia; }
    void SetDiplophonia(int32_t v) { _diplophonia = v; MarkCustom(); }

    int32_t GetFryAmount() const { return _fryAmount; }
    void SetFryAmount(int32_t v) { _fryAmount = v; MarkCustom(); }

    int32_t GetSubglottalAmt() const { return _subglottalAmt; }
    void SetSubglottalAmt(int32_t v) { _subglottalAmt = v; MarkCustom(); }

    int32_t GetBreathAmt() const { return _breathAmt; }
    void SetBreathAmt(int32_t v) { _breathAmt = v; MarkCustom(); }

    int32_t GetOpenQuotient() const { return _openQuotient; }
    void SetOpenQuotient(int32_t v) { _openQuotient = v; MarkCustom(); }

    int32_t GetOQStressLink() const { return _oqStressLink; }
    void SetOQStressLink(int32_t v) { _oqStressLink = v; MarkCustom(); }

    int32_t GetOQF0Link() const { return _oqF0Link; }
    void SetOQF0Link(int32_t v) { _oqF0Link = v; MarkCustom(); }

    int32_t GetOnsetHardness() const { return _onsetHardness; }
    void SetOnsetHardness(int32_t v) { _onsetHardness = v; MarkCustom(); }

    // Tract articulation
    int32_t GetPitchOffsetHz() const { return _pitchOffsetHz; }
    void SetPitchOffsetHz(int32_t v) { _pitchOffsetHz = v; MarkCustom(); }

    int32_t GetLarynxOffset() const { return _larynxOffset; }
    void SetLarynxOffset(int32_t v) { _larynxOffset = v; MarkCustom(); }

    int32_t GetPharyngealAmt() const { return _pharyngealAmt; }
    void SetPharyngealAmt(int32_t v) { _pharyngealAmt = v; MarkCustom(); }

    int32_t GetLipRounding() const { return _lipRounding; }
    void SetLipRounding(int32_t v) { _lipRounding = v; MarkCustom(); }

    float GetTractScale() const { return _tractScale; }
    void SetTractScale(float v) { _tractScale = v; MarkCustom(); }

    // Formants
    int32_t GetF5Freq() const { return _f5Freq; }
    void SetF5Freq(int32_t v) { _f5Freq = v; MarkCustom(); }

    int32_t GetF5BW() const { return _f5BW; }
    void SetF5BW(int32_t v) { _f5BW = v; MarkCustom(); }

    int32_t GetF4Freq() const { return _f4Freq; }
    void SetF4Freq(int32_t v) { _f4Freq = v; MarkCustom(); }

    int32_t GetF4BW() const { return _f4BW; }
    void SetF4BW(int32_t v) { _f4BW = v; MarkCustom(); }

    int32_t GetF4pFreq() const { return _f4pFreq; }
    void SetF4pFreq(int32_t v) { _f4pFreq = v; MarkCustom(); }

    int32_t GetF4pBW() const { return _f4pBW; }
    void SetF4pBW(int32_t v) { _f4pBW = v; MarkCustom(); }

    int32_t GetF5pFreq() const { return _f5pFreq; }
    void SetF5pFreq(int32_t v) { _f5pFreq = v; MarkCustom(); }

    int32_t GetF5pBW() const { return _f5pBW; }
    void SetF5pBW(int32_t v) { _f5pBW = v; MarkCustom(); }

    int32_t GetF6pFreq() const { return _f6pFreq; }
    void SetF6pFreq(int32_t v) { _f6pFreq = v; MarkCustom(); }

    int32_t GetF6pBW() const { return _f6pBW; }
    void SetF6pBW(int32_t v) { _f6pBW = v; MarkCustom(); }

    int32_t GetBwGain1() const { return _bwGain1; }
    void SetBwGain1(int32_t v) { _bwGain1 = v; MarkCustom(); }

    int32_t GetBwGain2() const { return _bwGain2; }
    void SetBwGain2(int32_t v) { _bwGain2 = v; MarkCustom(); }

    int32_t GetBwGain3() const { return _bwGain3; }
    void SetBwGain3(int32_t v) { _bwGain3 = v; MarkCustom(); }

    // Nasal
    int32_t GetNasalBase() const { return _nasalBase; }
    void SetNasalBase(int32_t v) { _nasalBase = v; MarkCustom(); }

    int32_t GetNasalTarg() const { return _nasalTarg; }
    void SetNasalTarg(int32_t v) { _nasalTarg = v; MarkCustom(); }

    int32_t GetNasalBW() const { return _nasalBW; }
    void SetNasalBW(int32_t v) { _nasalBW = v; MarkCustom(); }

    int32_t GetNGain() const { return _nGain; }
    void SetNGain(int32_t v) { _nGain = v; MarkCustom(); }

    // Intonation
    int32_t GetPitchRange() const { return _pitchRange; }
    void SetPitchRange(int32_t v) { _pitchRange = v; MarkCustom(); }

    int32_t GetStressGain() const { return _stressGain; }
    void SetStressGain(int32_t v) { _stressGain = v; MarkCustom(); }

    int32_t GetIntonation() const { return _intonation; }
    void SetIntonation(int32_t v) { _intonation = v; MarkCustom(); }

    int32_t GetRiseAmt() const { return _riseAmt; }
    void SetRiseAmt(int32_t v) { _riseAmt = v; MarkCustom(); }

    int32_t GetFallAmt() const { return _fallAmt; }
    void SetFallAmt(int32_t v) { _fallAmt = v; MarkCustom(); }

    int32_t GetBaselineFall() const { return _baselineFall; }
    void SetBaselineFall(int32_t v) { _baselineFall = v; MarkCustom(); }

    int32_t GetUptalkAmt() const { return _uptalkAmt; }
    void SetUptalkAmt(int32_t v) { _uptalkAmt = v; MarkCustom(); }

    int32_t GetStressEarly() const { return _stressEarly; }
    void SetStressEarly(int32_t v) { _stressEarly = v; MarkCustom(); }

    int32_t GetBreakStrength() const { return _breakStrength; }
    void SetBreakStrength(int32_t v) { _breakStrength = v; MarkCustom(); }

    int32_t GetEmphasisBoost() const { return _emphasisBoost; }
    void SetEmphasisBoost(int32_t v) { _emphasisBoost = v; MarkCustom(); }

    int32_t GetVocalConfidence() const { return _vocalConfidence; }
    void SetVocalConfidence(int32_t v) { _vocalConfidence = v; MarkCustom(); }

private:
    TtsEngine _engine;
    bool _isSpeaking = false;

    std::vector<PhonemeEvent> _phonemeEvents;
    int32_t _nextPhonemeIndex = 0;
    float _pollElapsed = 0.0f;

    bool _applyingPreset = false;
    VoicePreset _preset = VoicePreset::Baseline;

    // Voice Definition
    bool _female = false;
    int32_t _voicingGain = 100;
    int32_t _aspirationGain = 0;
    int32_t _aspirationCycle = 192;
    int32_t _tremoloDepth = 0;
    int32_t _tremoloRate = 0;

    // Glottal source
    int32_t _jitter = 0;
    int32_t _shimmer = 0;
    int32_t _diplophonia = 0;
    int32_t _fryAmount = 0;
    int32_t _subglottalAmt = 0;
    int32_t _breathAmt = 0;
    int32_t _openQuotient = 50;
    int32_t _oqStressLink = 0;
    int32_t _oqF0Link = 0;
    int32_t _onsetHardness = 50;

    // Tract articulation
    int32_t _pitchOffsetHz = 0;
    int32_t _larynxOffset = 0;
    int32_t _pharyngealAmt = 0;
    int32_t _lipRounding = 0;

    float _tractScale = 1.0f;

    // Formants
    int32_t _f5Freq = 4500;
    int32_t _f5BW = 250;
    int32_t _f4Freq = 3000;
    int32_t _f4BW = 200;
    int32_t _f4pFreq = 3600;
    int32_t _f4pBW = 150;
    int32_t _f5pFreq = 3750;
    int32_t _f5pBW = 100;
    int32_t _f6pFreq = 4500;
    int32_t _f6pBW = 150;
    int32_t _bwGain1 = 150;
    int32_t _bwGain2 = 100;
    int32_t _bwGain3 = 100;

    // Nasal
    int32_t _nasalBase = 330;
    int32_t _nasalTarg = 400;
    int32_t _nasalBW = 60;
    int32_t _nGain = 100;

    // Intonation
    int32_t _pitchRange = 100;
    int32_t _stressGain = 60;
    int32_t _intonation = 100;
    int32_t _riseAmt = 29;
    int32_t _fallAmt = -29;
    int32_t _baselineFall = 51;
    int32_t _uptalkAmt = 0;
    int32_t _stressEarly = 0;
    int32_t _breakStrength = 50;
    int32_t _emphasisBoost = 0;
    int32_t _vocalConfidence = 0;

    void MarkCustom();
    std::string PrepareText(const std::string& text);

    // yep, make a new one
    VoiceData BuildVoice();

    struct SpeakCtx;
    static void SpeakBufAdapter(const int16_t* buf, int32_t len, void* ud);
    static void SpeakEventsAdapter(const PhonemeEvent* events, int32_t count, void* ud);
};

}  // namespace SharpVox

#endif  // SHARPVOX_H
