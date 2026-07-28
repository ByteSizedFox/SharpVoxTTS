#include <emscripten.h>
#include <emscripten/bind.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <stdexcept>

#include "../../platform/lib/SharpVox.h"
#include "../../include/AudioProcessor.h"

using namespace SharpVox;
using namespace emscripten;

//  JS callbacks (all run in worker context  communicate via postMessage) 

EM_JS(void, js_init_audio, (int sr), {
    self.postMessage({ type: 'initAudio', sr: sr });
});

EM_JS(void, js_play_pcm, (const int16_t* ptr, int numSamples, int sr), {
    const pcm = HEAPU8.slice(ptr, ptr + numSamples * 2);
    self.postMessage({ type: 'playPcm', pcm: pcm, sr: sr }, [pcm.buffer]);
});

EM_JS(void, js_stop_audio, (), {
    self.postMessage({ type: 'stopAudio' });
});

EM_JS(void, js_stop_phoneme_tracking, (), {
    self.postMessage({ type: 'stopPhonemeTracking' });
});

EM_JS(void, js_start_phoneme_tracking, (const char* codes, const char* times, double playAt), {
    // playAt is determined on the main thread from when initAudio was received
    self.postMessage({ type: 'startPhonemeTracking', codes: UTF8ToString(codes), times: UTF8ToString(times) });
});

EM_JS(void, js_update_status, (const char* msg), {
    self.postMessage({ type: 'updateStatus', msg: UTF8ToString(msg) });
});

EM_JS(void, js_update_phonemes, (const char* json, int idx), {
    self.postMessage({ type: 'updatePhonemes', json: UTF8ToString(json), idx: idx });
});

EM_JS(void, js_update_all_params, (const char* json), {
    self.postMessage({ type: 'updateAllParams', json: UTF8ToString(json) });
});

EM_JS(void, js_download_bytes, (const uint8_t* ptr, int len, const char* filename, const char* mime), {
    const data = HEAPU8.slice(ptr, ptr + len);
    self.postMessage({ type: 'downloadBytes', data: data, filename: UTF8ToString(filename), mime: UTF8ToString(mime) }, [data.buffer]);
});

EM_JS(void, js_download_file, (const char* filename, const char* content), {
    self.postMessage({ type: 'downloadFile', filename: UTF8ToString(filename), content: UTF8ToString(content) });
});

EM_JS(void, js_render_result, (int requestId, const uint8_t* pcm, int pcmLen, int sr,
                               const char* codesJson, const char* timesJson), {
    const data = HEAPU8.slice(pcm, pcm + pcmLen);
    self.postMessage({
        type: 'renderResult', requestId: requestId, pcm: data, sr: sr,
        codesJson: UTF8ToString(codesJson), timesJson: UTF8ToString(timesJson)
    }, [data.buffer]);
});

EM_JS(void, js_render_error, (int requestId, const char* msg), {
    self.postMessage({ type: 'renderResult', requestId: requestId, error: UTF8ToString(msg) });
});

EM_JS(void, js_start_video_export, (const uint8_t* pcm, int pcmLen, int sr,
                                     const char* eventsJson, const char* timesJson,
                                     const char* wordTimesJson, float duration,
                                     const char* sourceText,
                                     const char* lipsyncTimesJson,
                                     const char* lipsyncV1Json,
                                     const char* lipsyncV2Json), {
    const data = HEAPU8.slice(pcm, pcm + pcmLen);
    self.postMessage({
        type: 'startVideoExport',
        pcm: data, sr: sr,
        eventsJson: UTF8ToString(eventsJson),
        timesJson: UTF8ToString(timesJson),
        wordTimesJson: UTF8ToString(wordTimesJson),
        duration: duration,
        sourceText: UTF8ToString(sourceText),
        lipsyncTimesJson: UTF8ToString(lipsyncTimesJson),
        lipsyncV1Json: UTF8ToString(lipsyncV1Json),
        lipsyncV2Json: UTF8ToString(lipsyncV2Json)
    }, [data.buffer]);
});

//  Helpers 

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

static void visemeFor(int16_t ph, const char*& v1, const char*& v2) {
    v1 = ""; v2 = "";
    if (ph == _IY_ || ph == _IH_ || ph == _AX_ || ph == _IX_) { v1 = "vrc.v_ih"; return; }
    if (ph == _EH_ || ph == _AE_ || ph == _EY_) { v1 = "vrc.v_e"; return; }
    if (ph == _AH_ || ph == _AA_) { v1 = "vrc.v_aa"; return; }
    if (ph == _AO_) { v1 = "vrc.v_oh"; return; }
    if (ph == _UH_ || ph == _UW_) { v1 = "vrc.v_ou"; return; }
    if (ph == _ER_ || ph == _XR_ || ph == _RX_) { v1 = "vrc.v_nn"; return; }
    if (ph == _AY_) { v1 = "vrc.v_aa"; v2 = "vrc.v_ih"; return; }
    if (ph == _OY_) { v1 = "vrc.v_oh"; v2 = "vrc.v_ih"; return; }
    if (ph == _AW_) { v1 = "vrc.v_aa"; v2 = "vrc.v_ou"; return; }
    if (ph == _OW_) { v1 = "vrc.v_oh"; v2 = "vrc.v_ou"; return; }
    if (ph == _YU_) { v1 = "vrc.v_nn"; v2 = "vrc.v_ou"; return; }
    if (ph == _IR_) { v1 = "vrc.v_ih"; v2 = "vrc.v_nn"; return; }
    if (ph == _AR_) { v1 = "vrc.v_aa"; v2 = "vrc.v_nn"; return; }
    if (ph == _OR_) { v1 = "vrc.v_oh"; v2 = "vrc.v_nn"; return; }
    if (ph == _UR_) { v1 = "vrc.v_ou"; v2 = "vrc.v_nn"; return; }
    if (ph == _M_  || ph == _P_ || ph == _B_) { v1 = "vrc.v_pp"; return; }
    if (ph == _F_  || ph == _V_) { v1 = "vrc.v_ff"; return; }
    if (ph == _TH_ || ph == _DH_) { v1 = "vrc.v_th"; return; }
    if (ph == _S_  || ph == _Z_  || ph == _T_ ||
        ph == _D_  || ph == _DX_) { v1 = "vrc.v_dd"; return; }
    if (ph == _SH_ || ph == _ZH_ || ph == _CH_ || ph == _JH_) { v1 = "vrc.v_ch"; return; }
    if (ph == _N_  || ph == _NG_ || ph == _K_ ||
        ph == _G_  || ph == _Y_  || ph == _R_ ||
        ph == _HH_ || ph == _EN_) { v1 = "vrc.v_nn"; return; }
    if (ph == _L_  || ph == _LX_ || ph == _EL_) { v1 = "vrc.v_dd"; return; }
    if (ph == _W_) { v1 = "vrc.v_ou"; return; }
    if (ph == _JP_A_) { v1 = "vrc.v_aa"; return; }
    if (ph == _JP_I_) { v1 = "vrc.v_ih"; return; }
    if (ph == _JP_U_) { v1 = "vrc.v_ou"; return; }
    if (ph == _JP_E_) { v1 = "vrc.v_e";  return; }
    if (ph == _JP_O_) { v1 = "vrc.v_oh"; return; }
}

//  Interop class 

class SharpVoxInterop {
public:
    SharpVoxInterop() {}

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
            else if (name == "VibratoDepth")   { _speaker.SetVibratoDepth(iv); }
            else if (name == "VibratoDepth1")  { _speaker.SetVibratoDepth1(iv); }
            else if (name == "VibratoDepth2")  { _speaker.SetVibratoDepth2(iv); }
            else if (name == "VibratoRate")    { _speaker.SetVibratoRate(iv); }
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
            else if (name == "BwGain1")        { _speaker.SetBwGain1(iv); }
            else if (name == "BwGain2")        { _speaker.SetBwGain2(iv); }
            else if (name == "BwGain3")        { _speaker.SetBwGain3(iv); }
            else if (name == "NasalBase")      { _speaker.SetNasalBase(iv); }
            else if (name == "NasalTarg")      { _speaker.SetNasalTarg(iv); }
            else if (name == "NasalBW")        { _speaker.SetNasalBW(iv); }
            else if (name == "PitchRange")     { _speaker.SetPitchRange(iv); }
            else if (name == "StressGain")     { _speaker.SetStressGain(iv); }
            else if (name == "Intonation")     { _speaker.SetIntonation(iv); }
            else if (name == "RiseAmt1")       { _speaker.SetRiseAmt1(iv); }
            else if (name == "Assertiveness")  { _speaker.SetAssertiveness(iv); }
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
            prepareEngine();
            _speaker.KlattschMode = false;

            struct Ctx { SharpVoxInterop* self; int32_t totalSamples; };
            Ctx ctx { this, 0 };

            js_init_audio(_sampleRate);
            _speaker.SpeakWithEvents(buildSynText(text),
                [](SharpVoxSpeaker* /*speaker*/, const int16_t* buf, int32_t len,
                   const PhonemeEvent* /*events*/, int32_t /*count*/, void* ud) {
                    auto* c = static_cast<Ctx*>(ud);
                    if (std::fabs(c->self->_outputVolume - 1.0f) > 0.001f) {
                        std::vector<int16_t> chunk(buf, buf + len);
                        applyVolume(chunk, c->self->_outputVolume);
                        js_play_pcm(chunk.data(), len, c->self->_sampleRate);
                    } else {
                        js_play_pcm(buf, len, c->self->_sampleRate);
                    }
                    c->totalSamples += len;
                },
                &ctx);

            const auto& allEvents = _speaker.PhonemeEvents();
            buildPhonemeJson(allEvents.data(), (int32_t)allEvents.size());
            js_update_phonemes(_codesJson.c_str(), -1);

            char status[128];
            int ms = _sampleRate > 0 ? (int)((int64_t)ctx.totalSamples * 1000 / _sampleRate) : 0;
            std::snprintf(status, sizeof(status), "ready - %d ms, %d phonemes", ms, (int)_phonCodes.size());
            js_update_status(status);

            if (!_phonCodes.empty()) {
                js_start_phoneme_tracking(_codesJson.c_str(), _timesJson.c_str(), 0.0);
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
            struct AudCtx { SharpVoxInterop* self; };
            AudCtx audCtx { this };
            js_init_audio(_sampleRate);
            _speaker.Speak(buf, [](SharpVoxSpeaker* /*speaker*/, const int16_t* chunk, int32_t len, void* ud) {
                auto* c = static_cast<AudCtx*>(ud);
                if (std::fabs(c->self->_outputVolume - 1.0f) > 0.001f) {
                    std::vector<int16_t> tmp(chunk, chunk + len);
                    applyVolume(tmp, c->self->_outputVolume);
                    js_play_pcm(tmp.data(), len, c->self->_sampleRate);
                } else {
                    js_play_pcm(chunk, len, c->self->_sampleRate);
                }
            }, &audCtx);
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
            prepareEngine();
            _speaker.KlattschMode = false;
            std::vector<int16_t> samples;
            _speaker.Speak(buildSynText(text), [](SharpVoxSpeaker* /*speaker*/, const int16_t* buf, int32_t len, void* ud) {
                auto* s = static_cast<std::vector<int16_t>*>(ud);
                s->insert(s->end(), buf, buf + len);
            }, &samples);
            applyVolume(samples, _outputVolume);
            auto wav = buildWav(samples, _sampleRate);
            js_download_bytes(wav.data(), (int)wav.size(), "speech.wav", "audio/wav");
        } catch (...) {}
    }

    void ExportVideo(const std::string& text) {
        if (text.empty()) return;
        try {
            prepareEngine();
            _speaker.KlattschMode = false;

            std::vector<int16_t> samples;
            std::string codesJson, timesJson, wordTimesJson;
            std::string lsTimesJson, lsV1Json, lsV2Json;
            codesJson    = "["; timesJson    = "["; wordTimesJson = "[";
            lsTimesJson  = "["; lsV1Json     = "["; lsV2Json      = "[";
            bool firstCode = true, firstWord = true, firstLs = true;

            struct Ctx { SharpVoxInterop* self; std::vector<int16_t>* samples; };
            Ctx ctx { this, &samples };

            _speaker.SpeakWithEvents(buildSynText(text),
                [](SharpVoxSpeaker* /*speaker*/, const int16_t* buf, int32_t len,
                   const PhonemeEvent* /*events*/, int32_t /*count*/, void* ud) {
                    auto* c = static_cast<Ctx*>(ud);
                    c->samples->insert(c->samples->end(), buf, buf + len);
                },
                &ctx);

            // Process phoneme events from the speaker's stored list
            const auto& events = _speaker.PhonemeEvents();
            for (const auto& e : events) {
                const char* v1; const char* v2;
                visemeFor(e.Phoneme, v1, v2);

                char timeBuf[32];
                std::snprintf(timeBuf, sizeof(timeBuf), "%g", (double)e.TimeSeconds);

                // lipsync: all events
                if (!firstLs) { lsTimesJson += ','; lsV1Json += ','; lsV2Json += ','; }
                firstLs = false;
                lsTimesJson += timeBuf;
                lsV1Json    += jsonStr(v1);
                lsV2Json    += jsonStr(v2);

                if (e.Phoneme == _SIL_) { continue; }

                const char* name = phonemeName(e.Phoneme);
                if (!name) { continue; }

                if (!firstCode) { codesJson += ','; timesJson += ','; }
                firstCode = false;
                codesJson += jsonStr(name);
                timesJson += timeBuf;

                if (e.IsWordStart) {
                    if (!firstWord) { wordTimesJson += ','; }
                    firstWord = false;
                    wordTimesJson += timeBuf;
                }
            }

            codesJson += ']'; timesJson += ']'; wordTimesJson += ']';
            lsTimesJson += ']'; lsV1Json += ']'; lsV2Json += ']';

            applyVolume(samples, _outputVolume);
            float duration = _sampleRate > 0
                ? (float)samples.size() / (float)_sampleRate
                : 0.0f;

            js_start_video_export(
                reinterpret_cast<const uint8_t*>(samples.data()),
                (int)(samples.size() * 2),
                _sampleRate,
                codesJson.c_str(), timesJson.c_str(),
                wordTimesJson.c_str(), duration, text.c_str(),
                lsTimesJson.c_str(), lsV1Json.c_str(), lsV2Json.c_str());
        } catch (const std::exception& e) {
            std::string err = std::string("error: ") + e.what();
            js_update_status(err.c_str());
        }
    }

    // Render text to a PCM buffer off the audio path and post it back with
    // all phoneme events (SIL included), so JS callers like the MIDI
    // converter can mix voices and drive per-tile overlays themselves.
    // The text is rendered as-is: callers supply their own [:klattsch on].
    void RenderBuffer(int requestId, const std::string& text) {
        try {
            prepareEngine();
            _speaker.KlattschMode = false;

            std::vector<int16_t> samples;
            _speaker.SpeakWithEvents(text,
                [](SharpVoxSpeaker* /*speaker*/, const int16_t* buf, int32_t len,
                   const PhonemeEvent* /*events*/, int32_t /*count*/, void* ud) {
                    auto* s = static_cast<std::vector<int16_t>*>(ud);
                    s->insert(s->end(), buf, buf + len);
                },
                &samples);

            std::string codesJson = "[", timesJson = "[";
            bool first = true;
            for (const auto& e : _speaker.PhonemeEvents()) {
                const char* name = e.Phoneme == _SIL_ ? "SIL" : phonemeName(e.Phoneme);
                if (!name) { continue; }
                if (!first) { codesJson += ','; timesJson += ','; }
                first = false;
                codesJson += jsonStr(name);
                char timeBuf[32];
                std::snprintf(timeBuf, sizeof(timeBuf), "%g", (double)e.TimeSeconds);
                timesJson += timeBuf;
            }
            codesJson += ']'; timesJson += ']';

            js_render_result(requestId,
                reinterpret_cast<const uint8_t*>(samples.data()),
                (int)(samples.size() * 2), _sampleRate,
                codesJson.c_str(), timesJson.c_str());
        } catch (const std::exception& e) {
            js_render_error(requestId, e.what());
        } catch (...) {
            js_render_error(requestId, "render failed");
        }
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
        KS("VibratoDepth",   _speaker.GetVibratoDepth1());
        KS("VibratoDepth2",  _speaker.GetVibratoDepth2());
        KS("VibratoRate",    _speaker.GetVibratoRate());
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
        KS("BwGain1", _speaker.GetBwGain1()); KS("BwGain2", _speaker.GetBwGain2()); KS("BwGain3", _speaker.GetBwGain3());
        KS("NasalBase", _speaker.GetNasalBase()); KS("NasalTarg", _speaker.GetNasalTarg()); KS("NasalBW", _speaker.GetNasalBW());
        KS("PitchRange",   _speaker.GetPitchRange());
        KS("StressGain",   _speaker.GetStressGain());
        KS("Intonation",   _speaker.GetIntonation());
        KS("RiseAmt1",      _speaker.GetRiseAmt1());
        KS("Assertiveness", _speaker.GetAssertiveness());
        KS("BaselineFall",  _speaker.GetBaselineFall());
        sb += "\n}";
        js_download_file("voice.json", sb.c_str());
    }

    std::string GetCustomString() {
        VoiceData def;
        std::string s = "[:custom";

        auto addI = [&](const char* name, int32_t cur, int32_t defVal) {
            if (cur != defVal) {
                char tmp[64];
                std::snprintf(tmp, sizeof(tmp), " %s %d", name, cur);
                s += tmp;
            }
        };
        auto addF = [&](const char* name, float cur, float defVal) {
            if (cur != defVal) {
                char tmp[80];
                std::snprintf(tmp, sizeof(tmp), " %s %g", name, (double)cur);
                s += tmp;
            }
        };

        addI("pitch",           _speaker.PitchHz,                   (int32_t)def.PitchHz);
        addI("rate",            _speaker.Rate,                      (int32_t)def.Rate);
        addI("voicetype",       _speaker.GetFemale() ? 1 : 0,       (int32_t)def.VoiceType);
        addF("tract",           _speaker.GetTractScale(),           def.TractScale);
        addI("vgain",           _speaker.GetVoicingGain(),          (int32_t)def.VGain);
        addI("again",           _speaker.GetAspirationGain(),       (int32_t)def.AGain);
        addI("acycle",          _speaker.GetAspirationCycle(),      (int32_t)def.ACycle);
        addI("tremolodepth",    _speaker.GetTremoloDepth(),         (int32_t)def.TremoloDepth);
        addI("tremolorate",     _speaker.GetTremoloRate(),          (int32_t)def.TremoloRate);
        addI("vibratodepth1",   _speaker.GetVibratoDepth1(),        (int32_t)def.VibratoDepth1Raw);
        addI("vibratodepth2",   _speaker.GetVibratoDepth2(),        (int32_t)def.VibratoDepth2Raw);
        addI("vibratofreq",     _speaker.GetVibratoRate(),          (int32_t)def.VibratoFreqRaw);
        addI("jitter",          _speaker.GetJitter(),               (int32_t)def.Jitter);
        addI("shimmer",         _speaker.GetShimmer(),              (int32_t)def.Shimmer);
        addI("diplophonia",     _speaker.GetDiplophonia(),          (int32_t)def.Diplophonia);
        addI("fry",             _speaker.GetFryAmount(),            (int32_t)def.FryAmount);
        addI("subglottal",      _speaker.GetSubglottalAmt(),        (int32_t)def.SubglottalAmt);
        addI("breath",          _speaker.GetBreathAmt(),            (int32_t)def.BreathAmt);
        addI("oq",              _speaker.GetOpenQuotient(),         (int32_t)def.OpenQuotient);
        addI("oqstresslink",    _speaker.GetOQStressLink(),         (int32_t)def.OQStressLink);
        addI("oqf0link",        _speaker.GetOQF0Link(),             (int32_t)def.OQF0Link);
        addI("larynx",          _speaker.GetLarynxOffset(),         (int32_t)def.LarynxOffset);
        addI("pharyngeal",      _speaker.GetPharyngealAmt(),        (int32_t)def.PharyngealAmt);
        addI("pitchoffset",     _speaker.GetPitchOffsetHz(),        (int32_t)def.PitchOffsetHz);
        addI("liprounding",     _speaker.GetLipRounding(),          (int32_t)def.LipRounding);
        addI("onset",           _speaker.GetOnsetHardness(),        (int32_t)def.OnsetHardness);
        addI("f4freq",          _speaker.GetF4Freq(),               (int32_t)def.F4Freq);
        addI("f4bw",            _speaker.GetF4BW(),                 (int32_t)def.F4BW);
        addI("f5freq",          _speaker.GetF5Freq(),               (int32_t)def.F5Freq);
        addI("f5bw",            _speaker.GetF5BW(),                 (int32_t)def.F5BW);
        addI("nasalbase",       _speaker.GetNasalBase(),            (int32_t)def.NasalBase);
        addI("nasaltarg",       _speaker.GetNasalTarg(),            (int32_t)def.NasalTarg);
        addI("nasalbw",         _speaker.GetNasalBW(),              (int32_t)def.NasalBW);
        addI("ngain",           _speaker.GetNGain(),                (int32_t)def.NGain);
        addI("bwgain1",         _speaker.GetBwGain1(),              (int32_t)def.BwGain1);
        addI("bwgain2",         _speaker.GetBwGain2(),              (int32_t)def.BwGain2);
        addI("bwgain3",         _speaker.GetBwGain3(),              (int32_t)def.BwGain3);
        addI("pitchrange",      _speaker.GetPitchRange(),           (int32_t)def.PitchRange);
        addI("stressgain",      _speaker.GetStressGain(),           (int32_t)def.StressGain);
        addI("intonation",      _speaker.GetIntonation(),           (int32_t)def.Intonation);
        addI("riseamt1",        _speaker.GetRiseAmt1(),             (int32_t)def.RiseAmt1);
        addI("assertiveness",   _speaker.GetAssertiveness(),        (int32_t)(((int64_t)def.Assertiveness * 100) >> 16));
        addI("baselinefall",    _speaker.GetBaselineFall(),         (int32_t)def.BaselineFall);
        addI("uptalk",          _speaker.GetUptalkAmt(),            (int32_t)def.UptalkAmt);
        addI("stressearly",     _speaker.GetStressEarly(),          (int32_t)def.StressEarly);
        addI("breakstrength",   _speaker.GetBreakStrength(),        (int32_t)def.BreakStrength);
        addI("emphasisboost",   _speaker.GetEmphasisBoost(),        (int32_t)def.EmphasisBoost);
        addI("vocalconfidence", _speaker.GetVocalConfidence(),      (int32_t)def.VocalConfidence);

        s += "]";
        return s;
    }

#ifdef SHARPVOX_SAMPLED_GLOT
    void SetGlottalSample(emscripten::val floatArray, int32_t srcRate, float naturalPitchHz) {
        int32_t len = floatArray["length"].as<int32_t>();
        std::vector<float> pcm(len);
        auto view = emscripten::val(emscripten::typed_memory_view(len, pcm.data()));
        view.call<void>("set", floatArray);
        _speaker.SetGlottalSample(pcm.data(), len, srcRate, naturalPitchHz);
    }
    void ClearGlottalSample() {
        _speaker.ClearGlottalSample();
    }
    void SetGlottalPitchShift(bool enabled) {
        _speaker.SetGlottalPitchShift(enabled);
    }
#endif

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
            _speaker.SetVibratoDepth(getInt("VibratoDepth", 14));
            _speaker.SetVibratoDepth2(getInt("VibratoDepth2", _speaker.GetVibratoDepth2()));
            _speaker.SetVibratoRate(getInt("VibratoRate", 65));
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
            _speaker.SetBwGain1(getInt("BwGain1", _speaker.GetBwGain1()));
            _speaker.SetBwGain2(getInt("BwGain2", _speaker.GetBwGain2()));
            _speaker.SetBwGain3(getInt("BwGain3", _speaker.GetBwGain3()));
            _speaker.SetNasalBase(getInt("NasalBase", _speaker.GetNasalBase()));
            _speaker.SetNasalTarg(getInt("NasalTarg", _speaker.GetNasalTarg()));
            _speaker.SetNasalBW(getInt("NasalBW", _speaker.GetNasalBW()));
            _speaker.SetPitchRange(getInt("PitchRange",   _speaker.GetPitchRange()));
            _speaker.SetStressGain(getInt("StressGain",   _speaker.GetStressGain()));
            _speaker.SetIntonation(getInt("Intonation",   _speaker.GetIntonation()));
            _speaker.SetRiseAmt1(getInt("RiseAmt1",       _speaker.GetRiseAmt1()));
            _speaker.SetAssertiveness(getInt("Assertiveness", _speaker.GetAssertiveness()));
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
    SharpVoxSpeaker _speaker;
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
        std::snprintf(buf, sizeof(buf), "b%.0f r%.0f v%.2f w%.1f h%.2f t%.2f g%.2f",
            (double)_speaker.KlBaseF0, (double)_speaker.KlRate,
            (double)_speaker.VibratoDepthToKlattschHz(), (double)_speaker.GetVibratoRate() / 10.0,
            (double)_speaker.KlAsp, (double)_speaker.KlTilt, (double)_speaker.KlEffort);
        return std::string("[:klattsch on] ") + buf + " " + text + " [:klattsch off]";
    }

    void buildPhonemeJson(const PhonemeEvent* events, int32_t count) {
        _phonCodes.clear();
        _codesJson = "[";
        _timesJson = "[";
        bool first = true;

        for (int32_t i = 0; i < count; i++) {
            const auto& e = events[i];
            if (e.Phoneme == _SIL_) continue;
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
        KS("VibratoDepth", _speaker.GetVibratoDepth1());
        KS("VibratoDepth2",_speaker.GetVibratoDepth2());
        KS("VibratoRate",  _speaker.GetVibratoRate());
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
        KS("BwGain1", _speaker.GetBwGain1()); KS("BwGain2", _speaker.GetBwGain2()); KS("BwGain3", _speaker.GetBwGain3());
        KS("NasalBase", _speaker.GetNasalBase()); KS("NasalTarg", _speaker.GetNasalTarg()); KS("NasalBW", _speaker.GetNasalBW());
        KS("PitchRange",   _speaker.GetPitchRange());
        KS("StressGain",   _speaker.GetStressGain());
        KS("Intonation",   _speaker.GetIntonation());
        KS("RiseAmt1",      _speaker.GetRiseAmt1());
        KS("Assertiveness", _speaker.GetAssertiveness());
        KS("BaselineFall",  _speaker.GetBaselineFall());
        KF("klBaseF0",  _speaker.KlBaseF0);
        KF("klRate",    _speaker.KlRate);
        KF("klAsp",     _speaker.KlAsp);
        KF("klTilt",    _speaker.KlTilt);
        KF("klEffort",  _speaker.KlEffort);
        KF("OutputVolume", _outputVolume);
        KS("sampleRate",   _sampleRate);
        sb += '}';
        js_update_all_params(sb.c_str());
    }
};

//  Embind bindings 

EMSCRIPTEN_BINDINGS(sharpvox_interop) {
    class_<SharpVoxInterop>("SharpVoxInterop")
        .constructor()
        .function("Initialize",      &SharpVoxInterop::Initialize)
        .function("SetMode",         &SharpVoxInterop::SetMode)
        .function("UpdateParam",     &SharpVoxInterop::UpdateParam)
        .function("Speak",           &SharpVoxInterop::Speak)
        .function("AuditionPhoneme", &SharpVoxInterop::AuditionPhoneme)
        .function("StopBtn",         &SharpVoxInterop::StopBtn)
        .function("DownloadWav",     &SharpVoxInterop::DownloadWav)
        .function("OnPresetChange",  &SharpVoxInterop::OnPresetChange)
        .function("ExportPreset",    &SharpVoxInterop::ExportPreset)
        .function("GetCustomString", &SharpVoxInterop::GetCustomString)
        .function("HandleImport",    &SharpVoxInterop::HandleImport)
        .function("ConvertUst",      &SharpVoxInterop::ConvertUst)
        .function("ExportVideo",     &SharpVoxInterop::ExportVideo)
        .function("RenderBuffer",    &SharpVoxInterop::RenderBuffer)
#ifdef SHARPVOX_SAMPLED_GLOT
        .function("SetGlottalSample",     &SharpVoxInterop::SetGlottalSample)
        .function("ClearGlottalSample",   &SharpVoxInterop::ClearGlottalSample)
        .function("SetGlottalPitchShift", &SharpVoxInterop::SetGlottalPitchShift)
#endif
        ;
}
