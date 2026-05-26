#include <emscripten.h>
#include <emscripten/bind.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <stdexcept>

#include "../../platform/lib/sharptalk_speaker.h"
#include "../../include/audio_processor.h"

using namespace SharpTalk;
using namespace emscripten;

// ── JS callbacks ──────────────────────────────────────────────────────────────

EM_JS(void, js_init_audio, (int sr), {
    if (globalThis.initAudio) globalThis.initAudio(sr);
});

EM_JS(void, js_play_pcm, (const int16_t* ptr, int numSamples, int sr), {
    if (!globalThis.playAudioStream) return;
    globalThis.playAudioStream(HEAPU8.slice(ptr, ptr + numSamples * 2), sr);
});

EM_JS(void, js_stop_audio, (), {
    if (globalThis.stopAudio) globalThis.stopAudio();
});

EM_JS(void, js_stop_phoneme_tracking, (), {
    if (globalThis.stopPhonemeTracking) globalThis.stopPhonemeTracking();
});

EM_JS(double, js_reserve_start_time, (int sr), {
    return globalThis.reserveStartTime ? globalThis.reserveStartTime(sr) : 0;
});

EM_JS(void, js_start_phoneme_tracking, (const char* codes, const char* times, double playAt), {
    if (globalThis.startPhonemeTracking)
        globalThis.startPhonemeTracking(UTF8ToString(codes), UTF8ToString(times), playAt);
});

EM_JS(void, js_update_status, (const char* msg), {
    if (globalThis.ui && globalThis.ui.updateStatus) globalThis.ui.updateStatus(UTF8ToString(msg));
});

EM_JS(void, js_update_phonemes, (const char* json, int idx), {
    if (globalThis.ui && globalThis.ui.updatePhonemes) globalThis.ui.updatePhonemes(UTF8ToString(json), idx);
});

EM_JS(void, js_update_all_params, (const char* json), {
    if (globalThis.ui && globalThis.ui.updateAllParams) globalThis.ui.updateAllParams(UTF8ToString(json));
});

EM_JS(void, js_download_bytes, (const uint8_t* ptr, int len, const char* filename, const char* mime), {
    if (globalThis.downloadBytes)
        globalThis.downloadBytes(HEAPU8.slice(ptr, ptr + len), UTF8ToString(filename), UTF8ToString(mime));
});

EM_JS(void, js_download_file, (const char* filename, const char* content), {
    if (globalThis.downloadFile)
        globalThis.downloadFile(UTF8ToString(filename), UTF8ToString(content));
});

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::vector<uint8_t> buildWav(const std::vector<int16_t>& samples, int sampleRate) {
    int dataBytes = (int)(samples.size() * 2);
    std::vector<uint8_t> buf(44 + dataBytes);
    auto u32 = [&](int off, uint32_t v) {
        buf[off]   = v & 0xFF; buf[off+1] = (v>>8)  & 0xFF;
        buf[off+2] = (v>>16) & 0xFF; buf[off+3] = (v>>24) & 0xFF;
    };
    auto u16 = [&](int off, uint16_t v) {
        buf[off] = v & 0xFF; buf[off+1] = (v>>8) & 0xFF;
    };
    std::memcpy(buf.data() +  0, "RIFF", 4); u32( 4, 36 + dataBytes);
    std::memcpy(buf.data() +  8, "WAVE", 4);
    std::memcpy(buf.data() + 12, "fmt ", 4); u32(16, 16);
    u16(20, 1); u16(22, 1);
    u32(24, sampleRate); u32(28, sampleRate * 2);
    u16(32, 2); u16(34, 16);
    std::memcpy(buf.data() + 36, "data", 4); u32(40, dataBytes);
    std::memcpy(buf.data() + 44, samples.data(), dataBytes);
    return buf;
}

static void applyVolume(std::vector<int16_t>& samples, float volume) {
    if (std::fabs(volume - 1.0f) < 0.001f) return;
    for (auto& s : samples) {
        float v = s * volume;
        if (v >  32767.0f) v =  32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        s = (int16_t)(v >= 0.0f ? v + 0.5f : v - 0.5f);
    }
}

static std::string jsonStr(const char* s) {
    std::string r = "\"";
    for (; s && *s; ++s) {
        switch (*s) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            default:   r += *s;     break;
        }
    }
    return r + '"';
}

static constexpr int kPhonemeTableSize = 69;

static const char* phonemeName(int16_t id) {
    if (id < 0 || id >= kPhonemeTableSize) return nullptr;
    return AudioProcessor::PhonemeNamesTable[id];
}

// ── Interop class ─────────────────────────────────────────────────────────────

class SharpTalkInterop {
public:
    SharpTalkInterop() {}

    void Initialize() { syncAllParamsToUi(); }

    void SetMode(bool klattsch) {
        _klattschMode = klattsch;
        _speaker.KlattschMode = false;
    }

    void UpdateParam(const std::string& name, const std::string& value) {
        try {
            float fv = std::stof(value);
            int   iv = (int)fv;
            // clang-format off
            if      (name == "sampleRate")     { _sampleRate = iv; return; }
            else if (name == "OutputVolume")   { _outputVolume = fv; return; }
            else if (name == "TractScale")     { _speaker.SetTractScale(fv); return; }
            else if (name == "klBaseF0")       { _speaker.KlBaseF0 = fv; return; }
            else if (name == "klRate")         { _speaker.KlRate = fv; return; }
            else if (name == "klVibrato")      { _speaker.KlVibrato = fv; return; }
            else if (name == "klVibRate")      { _speaker.KlVibRate = fv; return; }
            else if (name == "klAsp")          { _speaker.KlAsp = fv; return; }
            else if (name == "klTilt")         { _speaker.KlTilt = fv; return; }
            else if (name == "klEffort")       { _speaker.KlEffort = fv; return; }
            else if (name == "Rate")           { _speaker.Rate = iv; }
            else if (name == "PitchHz")        { _speaker.PitchHz = iv; }
            else if (name == "VoiceType")      { _speaker.SetFemale(iv != 0); }
            else if (name == "VGain")          { _speaker.SetVoicingGain(iv); }
            else if (name == "AGain")          { _speaker.SetAspirationGain(iv); }
            else if (name == "ACycle")         { _speaker.SetAspirationCycle(iv); }
            else if (name == "TremoloDepth")   { _speaker.SetTremoloDepth(iv); }
            else if (name == "TremoloRate")    { _speaker.SetTremoloRate(iv); }
            else if (name == "Jitter")         { _speaker.SetJitter(iv); }
            else if (name == "Shimmer")        { _speaker.SetShimmer(iv); }
            else if (name == "Diplophonia")    { _speaker.SetDiplophonia(iv); }
            else if (name == "FryAmount")      { _speaker.SetFryAmount(iv); }
            else if (name == "SubglottalAmt")  { _speaker.SetSubglottalAmt(iv); }
            else if (name == "BreathAmt")      { _speaker.SetBreathAmt(iv); }
            else if (name == "OpenQuotient")   { _speaker.SetOpenQuotient(iv); }
            else if (name == "OQStressLink")   { _speaker.SetOQStressLink(iv); }
            else if (name == "OQF0Link")       { _speaker.SetOQF0Link(iv); }
            else if (name == "LarynxOffset")   { _speaker.SetLarynxOffset(iv); }
            else if (name == "PharyngealAmt")  { _speaker.SetPharyngealAmt(iv); }
            else if (name == "PitchOffsetHz")  { _speaker.SetPitchOffsetHz(iv); }
            else if (name == "LipRounding")    { _speaker.SetLipRounding(iv); }
            else if (name == "OnsetHardness")  { _speaker.SetOnsetHardness(iv); }
            else if (name == "NGain")          { _speaker.SetNGain(iv); }
            else if (name == "F4Freq")         { _speaker.SetF4Freq(iv); }
            else if (name == "F4BW")           { _speaker.SetF4BW(iv); }
            else if (name == "F5Freq")         { _speaker.SetF5Freq(iv); }
            else if (name == "F5BW")           { _speaker.SetF5BW(iv); }
            else if (name == "F4pFreq")        { _speaker.SetF4pFreq(iv); }
            else if (name == "F4pBW")          { _speaker.SetF4pBW(iv); }
            else if (name == "F5pFreq")        { _speaker.SetF5pFreq(iv); }
            else if (name == "F5pBW")          { _speaker.SetF5pBW(iv); }
            else if (name == "F6pFreq")        { _speaker.SetF6pFreq(iv); }
            else if (name == "F6pBW")          { _speaker.SetF6pBW(iv); }
            else if (name == "BwGain1")        { _speaker.SetBwGain1(iv); }
            else if (name == "BwGain2")        { _speaker.SetBwGain2(iv); }
            else if (name == "BwGain3")        { _speaker.SetBwGain3(iv); }
            else if (name == "NasalBase")      { _speaker.SetNasalBase(iv); }
            else if (name == "NasalTarg")      { _speaker.SetNasalTarg(iv); }
            else if (name == "NasalBW")        { _speaker.SetNasalBW(iv); }
            else if (name == "PitchRange")     { _speaker.SetPitchRange(iv); }
            else if (name == "StressGain")     { _speaker.SetStressGain(iv); }
            else if (name == "Intonation")     { _speaker.SetIntonation(iv); }
            else if (name == "RiseAmt")        { _speaker.SetRiseAmt(iv); }
            else if (name == "FallAmt")        { _speaker.SetFallAmt(iv); }
            else if (name == "BaselineFall")   { _speaker.SetBaselineFall(iv); }
            else if (name == "UptalkAmt")      { _speaker.SetUptalkAmt(iv); }
            else if (name == "StressEarly")    { _speaker.SetStressEarly(iv); }
            else if (name == "BreakStrength")  { _speaker.SetBreakStrength(iv); }
            else if (name == "EmphasisBoost")  { _speaker.SetEmphasisBoost(iv); }
            else if (name == "VocalConfidence"){ _speaker.SetVocalConfidence(iv); }
            // clang-format on
        } catch (...) {}
    }

    void Speak(const std::string& text) {
        if (text.empty()) return;
        js_stop_phoneme_tracking();
        js_stop_audio();
        js_update_status("processing...");
        js_update_phonemes("[]", -1);
        try {
            auto [samples, events] = synthesize(text);
            applyVolume(samples, _outputVolume);
            buildPhonemeJson(events);
            js_init_audio(_sampleRate);
            double playAt = js_reserve_start_time(_sampleRate);
            js_play_pcm(samples.data(), (int)samples.size(), _sampleRate);

            char status[128];
            int ms = _sampleRate > 0 ? (int)((int64_t)samples.size() * 1000 / _sampleRate) : 0;
            std::snprintf(status, sizeof(status), "ready — %d ms, %d phonemes", ms, (int)_phonCodes.size());
            js_update_status(status);
            js_update_phonemes(_codesJson.c_str(), -1);

            if (!_phonCodes.empty()) {
                js_start_phoneme_tracking(_codesJson.c_str(), _timesJson.c_str(), playAt);
            }
        } catch (const std::exception& e) {
            std::string err = std::string("error: ") + e.what();
            js_update_status(err.c_str());
        }
    }

    void AuditionPhoneme(const std::string& code) {
        js_stop_phoneme_tracking();
        js_stop_audio();
        try {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "[:klattsch on] b%.0f r%.0f %s [:klattsch off]",
                (double)_speaker.KlBaseF0, (double)_speaker.KlRate, code.c_str());
            prepareEngine();
            _speaker.KlattschMode = false;
            auto [samples, _ignored] = _speaker.SpeakWithEvents(buf);
            applyVolume(samples, _outputVolume);
            js_init_audio(_sampleRate);
            js_play_pcm(samples.data(), (int)samples.size(), _sampleRate);
        } catch (...) {}
    }

    void StopBtn() {
        js_stop_phoneme_tracking();
        js_stop_audio();
        js_update_status("stopped");
    }

    void DownloadWav(const std::string& text) {
        if (text.empty()) return;
        try {
            auto [samples, _ignored] = synthesize(text);
            applyVolume(samples, _outputVolume);
            auto wav = buildWav(samples, _sampleRate);
            js_download_bytes(wav.data(), (int)wav.size(), "speech.wav", "audio/wav");
        } catch (...) {}
    }

    void OnPresetChange(const std::string& val) {
        if (val == "baseline") {
            _speaker.SetPreset(VoicePreset::Baseline);
        } else if (val == "whisper") {
            _speaker.SetPreset(VoicePreset::Whisper);
        }
        syncAllParamsToUi();
    }

    void ExportPreset() {
        std::string sb = "{\n";
        bool first = true;
        auto KS = [&](const char* k, int v) {
            if (!first) sb += ",\n"; first = false;
            char tmp[64];
            std::snprintf(tmp, sizeof(tmp), "  \"%s\": %d", k, v);
            sb += tmp;
        };
        auto KF = [&](const char* k, float v) {
            if (!first) sb += ",\n"; first = false;
            char tmp[80];
            std::snprintf(tmp, sizeof(tmp), "  \"%s\": %g", k, (double)v);
            sb += tmp;
        };
        KS("Rate", _speaker.Rate);
        KS("PitchHz", _speaker.PitchHz);
        KF("TractScale", _speaker.GetTractScale());
        KS("VoiceType", _speaker.GetFemale() ? 1 : 0);
        KS("VoicingGain",    _speaker.GetVoicingGain());
        KS("AspirationGain", _speaker.GetAspirationGain());
        KS("AspirationCycle",_speaker.GetAspirationCycle());
        KS("TremoloDepth",   _speaker.GetTremoloDepth());
        KS("TremoloRate",    _speaker.GetTremoloRate());
        KS("Jitter",         _speaker.GetJitter());
        KS("Shimmer",        _speaker.GetShimmer());
        KS("Diplophonia",    _speaker.GetDiplophonia());
        KS("FryAmount",      _speaker.GetFryAmount());
        KS("SubglottalAmt",  _speaker.GetSubglottalAmt());
        KS("BreathAmt",      _speaker.GetBreathAmt());
        KS("OpenQuotient",   _speaker.GetOpenQuotient());
        KS("OQStressLink",   _speaker.GetOQStressLink());
        KS("OQF0Link",       _speaker.GetOQF0Link());
        KS("LarynxOffset",   _speaker.GetLarynxOffset());
        KS("PharyngealAmt",  _speaker.GetPharyngealAmt());
        KS("PitchOffsetHz",  _speaker.GetPitchOffsetHz());
        KS("LipRounding",    _speaker.GetLipRounding());
        KS("OnsetHardness",  _speaker.GetOnsetHardness());
        KS("NGain",   _speaker.GetNGain());
        KS("F4Freq",  _speaker.GetF4Freq());  KS("F4BW",  _speaker.GetF4BW());
        KS("F5Freq",  _speaker.GetF5Freq());  KS("F5BW",  _speaker.GetF5BW());
        KS("F4pFreq", _speaker.GetF4pFreq()); KS("F4pBW", _speaker.GetF4pBW());
        KS("F5pFreq", _speaker.GetF5pFreq()); KS("F5pBW", _speaker.GetF5pBW());
        KS("F6pFreq", _speaker.GetF6pFreq()); KS("F6pBW", _speaker.GetF6pBW());
        KS("BwGain1", _speaker.GetBwGain1()); KS("BwGain2", _speaker.GetBwGain2()); KS("BwGain3", _speaker.GetBwGain3());
        KS("NasalBase", _speaker.GetNasalBase()); KS("NasalTarg", _speaker.GetNasalTarg()); KS("NasalBW", _speaker.GetNasalBW());
        KS("PitchRange",   _speaker.GetPitchRange());
        KS("StressGain",   _speaker.GetStressGain());
        KS("Intonation",   _speaker.GetIntonation());
        KS("RiseAmt",      _speaker.GetRiseAmt());
        KS("FallAmt",      _speaker.GetFallAmt());
        KS("BaselineFall", _speaker.GetBaselineFall());
        sb += "\n}";
        js_download_file("voice.json", sb.c_str());
    }

    void HandleImport(const std::string& json) {
        if (json.empty()) return;
        try {
            auto getInt = [&](const char* key, int def) -> int {
                std::string pat = std::string("\"") + key + "\":";
                auto pos = json.find(pat);
                if (pos == std::string::npos) return def;
                pos += pat.size();
                while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
                try { return (int)std::stod(json.substr(pos)); } catch (...) { return def; }
            };
            auto getFloat = [&](const char* key, float def) -> float {
                std::string pat = std::string("\"") + key + "\":";
                auto pos = json.find(pat);
                if (pos == std::string::npos) return def;
                pos += pat.size();
                while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
                try { return std::stof(json.substr(pos)); } catch (...) { return def; }
            };

            _speaker.Rate    = (int32_t)getInt("Rate",    _speaker.Rate);
            _speaker.PitchHz = (int32_t)getInt("PitchHz", _speaker.PitchHz);
            _speaker.SetTractScale(getFloat("TractScale", _speaker.GetTractScale()));
            _speaker.SetFemale(getInt("VoiceType", 0) != 0);
            _speaker.SetVoicingGain(getInt("VoicingGain",     _speaker.GetVoicingGain()));
            _speaker.SetAspirationGain(getInt("AspirationGain",  _speaker.GetAspirationGain()));
            _speaker.SetAspirationCycle(getInt("AspirationCycle", _speaker.GetAspirationCycle()));
            _speaker.SetTremoloDepth(getInt("TremoloDepth", 0));
            _speaker.SetTremoloRate(getInt("TremoloRate", 0));
            _speaker.SetJitter(getInt("Jitter", 0));
            _speaker.SetShimmer(getInt("Shimmer", 0));
            _speaker.SetDiplophonia(getInt("Diplophonia", 0));
            _speaker.SetFryAmount(getInt("FryAmount", 0));
            _speaker.SetSubglottalAmt(getInt("SubglottalAmt", 0));
            _speaker.SetBreathAmt(getInt("BreathAmt", 0));
            _speaker.SetOpenQuotient(getInt("OpenQuotient", 50));
            _speaker.SetOQStressLink(getInt("OQStressLink", 0));
            _speaker.SetOQF0Link(getInt("OQF0Link", 0));
            _speaker.SetLarynxOffset(getInt("LarynxOffset", 0));
            _speaker.SetPharyngealAmt(getInt("PharyngealAmt", 0));
            _speaker.SetPitchOffsetHz(getInt("PitchOffsetHz", 0));
            _speaker.SetLipRounding(getInt("LipRounding", 0));
            _speaker.SetOnsetHardness(getInt("OnsetHardness", 50));
            _speaker.SetNGain(getInt("NGain",  _speaker.GetNGain()));
            _speaker.SetF4Freq(getInt("F4Freq", _speaker.GetF4Freq())); _speaker.SetF4BW(getInt("F4BW", _speaker.GetF4BW()));
            _speaker.SetF5Freq(getInt("F5Freq", _speaker.GetF5Freq())); _speaker.SetF5BW(getInt("F5BW", _speaker.GetF5BW()));
            _speaker.SetF4pFreq(getInt("F4pFreq", _speaker.GetF4pFreq())); _speaker.SetF4pBW(getInt("F4pBW", _speaker.GetF4pBW()));
            _speaker.SetF5pFreq(getInt("F5pFreq", _speaker.GetF5pFreq())); _speaker.SetF5pBW(getInt("F5pBW", _speaker.GetF5pBW()));
            _speaker.SetF6pFreq(getInt("F6pFreq", _speaker.GetF6pFreq())); _speaker.SetF6pBW(getInt("F6pBW", _speaker.GetF6pBW()));
            _speaker.SetBwGain1(getInt("BwGain1", _speaker.GetBwGain1()));
            _speaker.SetBwGain2(getInt("BwGain2", _speaker.GetBwGain2()));
            _speaker.SetBwGain3(getInt("BwGain3", _speaker.GetBwGain3()));
            _speaker.SetNasalBase(getInt("NasalBase", _speaker.GetNasalBase()));
            _speaker.SetNasalTarg(getInt("NasalTarg", _speaker.GetNasalTarg()));
            _speaker.SetNasalBW(getInt("NasalBW", _speaker.GetNasalBW()));
            _speaker.SetPitchRange(getInt("PitchRange",   _speaker.GetPitchRange()));
            _speaker.SetStressGain(getInt("StressGain",   _speaker.GetStressGain()));
            _speaker.SetIntonation(getInt("Intonation",   _speaker.GetIntonation()));
            _speaker.SetRiseAmt(getInt("RiseAmt",         _speaker.GetRiseAmt()));
            _speaker.SetFallAmt(getInt("FallAmt",         _speaker.GetFallAmt()));
            _speaker.SetBaselineFall(getInt("BaselineFall", _speaker.GetBaselineFall()));

            syncAllParamsToUi();
            js_update_status("preset imported");
        } catch (...) {
            js_update_status("import error");
        }
    }

    std::string ConvertUst(const std::string& /*text*/, const std::string& /*language*/,
                           int /*offset*/, const std::string& /*bank*/) {
        return "{\"klattsch\":\"\",\"diagnostics\":\"UST conversion is not available in the C++ build\"}";
    }

private:
    SharpTalkSpeaker _speaker;
    int   _sampleRate   = 48000;
    float _outputVolume = 1.0f;
    bool  _klattschMode = false;

    std::string _codesJson;
    std::string _timesJson;
    std::vector<std::string> _phonCodes;

    void prepareEngine() {
        _speaker.SampleRate = _sampleRate;
        _speaker.ApplyVoiceInPlace();
    }

    std::string buildSynText(const std::string& text) {
        if (!_klattschMode) return text;
        char buf[256];
        std::snprintf(buf, sizeof(buf), "b%.0f r%.0f v%.1f w%.1f h%.2f t%.2f g%.2f",
            (double)_speaker.KlBaseF0, (double)_speaker.KlRate,
            (double)_speaker.KlVibrato, (double)_speaker.KlVibRate,
            (double)_speaker.KlAsp, (double)_speaker.KlTilt, (double)_speaker.KlEffort);
        return std::string("[:klattsch on] ") + buf + " " + text + " [:klattsch off]";
    }

    std::pair<std::vector<int16_t>, std::vector<PhonemeEvent>>
    synthesize(const std::string& text) {
        prepareEngine();
        _speaker.KlattschMode = false;
        return _speaker.SpeakWithEvents(buildSynText(text));
    }

    void buildPhonemeJson(const std::vector<PhonemeEvent>& events) {
        _phonCodes.clear();
        _codesJson = "[";
        _timesJson = "[";
        bool first = true;

        for (const auto& e : events) {
            if (e.Phoneme == AudioProcessor::_SIL_) continue;
            const char* name = phonemeName(e.Phoneme);
            if (!name) continue;

            if (!first) { _codesJson += ','; _timesJson += ','; }
            first = false;

            _phonCodes.push_back(name);
            _codesJson += jsonStr(name);

            char timeBuf[32];
            std::snprintf(timeBuf, sizeof(timeBuf), "%g", (double)e.TimeSeconds);
            _timesJson += timeBuf;
        }

        _codesJson += ']';
        _timesJson += ']';
    }

    void syncAllParamsToUi() {
        std::string sb = "{";
        bool first = true;
        auto KS = [&](const char* k, int v) {
            if (!first) sb += ','; first = false;
            char tmp[64];
            std::snprintf(tmp, sizeof(tmp), "\"%s\":%d", k, v);
            sb += tmp;
        };
        auto KF = [&](const char* k, float v) {
            if (!first) sb += ','; first = false;
            char tmp[80];
            std::snprintf(tmp, sizeof(tmp), "\"%s\":%g", k, (double)v);
            sb += tmp;
        };
        KS("Rate",    _speaker.Rate);
        KS("PitchHz", _speaker.PitchHz);
        KF("TractScale", _speaker.GetTractScale());
        KS("VoiceType", _speaker.GetFemale() ? 1 : 0);
        KS("VGain",  _speaker.GetVoicingGain());
        KS("AGain",  _speaker.GetAspirationGain());
        KS("ACycle", _speaker.GetAspirationCycle());
        KS("TremoloDepth", _speaker.GetTremoloDepth());
        KS("TremoloRate",  _speaker.GetTremoloRate());
        KS("Jitter",       _speaker.GetJitter());
        KS("Shimmer",      _speaker.GetShimmer());
        KS("Diplophonia",  _speaker.GetDiplophonia());
        KS("FryAmount",    _speaker.GetFryAmount());
        KS("SubglottalAmt",_speaker.GetSubglottalAmt());
        KS("BreathAmt",    _speaker.GetBreathAmt());
        KS("OpenQuotient", _speaker.GetOpenQuotient());
        KS("OQStressLink", _speaker.GetOQStressLink());
        KS("OQF0Link",     _speaker.GetOQF0Link());
        KS("LarynxOffset", _speaker.GetLarynxOffset());
        KS("PharyngealAmt",_speaker.GetPharyngealAmt());
        KS("PitchOffsetHz",_speaker.GetPitchOffsetHz());
        KS("LipRounding",  _speaker.GetLipRounding());
        KS("OnsetHardness",_speaker.GetOnsetHardness());
        KS("NGain",   _speaker.GetNGain());
        KS("F4Freq",  _speaker.GetF4Freq());  KS("F4BW",  _speaker.GetF4BW());
        KS("F5Freq",  _speaker.GetF5Freq());  KS("F5BW",  _speaker.GetF5BW());
        KS("F4pFreq", _speaker.GetF4pFreq()); KS("F4pBW", _speaker.GetF4pBW());
        KS("F5pFreq", _speaker.GetF5pFreq()); KS("F5pBW", _speaker.GetF5pBW());
        KS("F6pFreq", _speaker.GetF6pFreq()); KS("F6pBW", _speaker.GetF6pBW());
        KS("BwGain1", _speaker.GetBwGain1()); KS("BwGain2", _speaker.GetBwGain2()); KS("BwGain3", _speaker.GetBwGain3());
        KS("NasalBase", _speaker.GetNasalBase()); KS("NasalTarg", _speaker.GetNasalTarg()); KS("NasalBW", _speaker.GetNasalBW());
        KS("PitchRange",   _speaker.GetPitchRange());
        KS("StressGain",   _speaker.GetStressGain());
        KS("Intonation",   _speaker.GetIntonation());
        KS("RiseAmt",      _speaker.GetRiseAmt());
        KS("FallAmt",      _speaker.GetFallAmt());
        KS("BaselineFall", _speaker.GetBaselineFall());
        KF("klBaseF0",  _speaker.KlBaseF0);
        KF("klRate",    _speaker.KlRate);
        KF("klVibrato", _speaker.KlVibrato);
        KF("klVibRate", _speaker.KlVibRate);
        KF("klAsp",     _speaker.KlAsp);
        KF("klTilt",    _speaker.KlTilt);
        KF("klEffort",  _speaker.KlEffort);
        KF("OutputVolume", _outputVolume);
        KS("sampleRate",   _sampleRate);
        sb += '}';
        js_update_all_params(sb.c_str());
    }
};

// ── Embind bindings ───────────────────────────────────────────────────────────

EMSCRIPTEN_BINDINGS(sharptalk_interop) {
    class_<SharpTalkInterop>("SharpTalkInterop")
        .constructor()
        .function("Initialize",      &SharpTalkInterop::Initialize)
        .function("SetMode",         &SharpTalkInterop::SetMode)
        .function("UpdateParam",     &SharpTalkInterop::UpdateParam)
        .function("Speak",           &SharpTalkInterop::Speak)
        .function("AuditionPhoneme", &SharpTalkInterop::AuditionPhoneme)
        .function("StopBtn",         &SharpTalkInterop::StopBtn)
        .function("DownloadWav",     &SharpTalkInterop::DownloadWav)
        .function("OnPresetChange",  &SharpTalkInterop::OnPresetChange)
        .function("ExportPreset",    &SharpTalkInterop::ExportPreset)
        .function("HandleImport",    &SharpTalkInterop::HandleImport)
        .function("ConvertUst",      &SharpTalkInterop::ConvertUst);
}
