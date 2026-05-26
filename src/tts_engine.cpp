#include "../include/tts_engine.h"
#include "../include/audio_processor.h"
#include "../include/speech_renderer.h"
#include "../include/klatt_synthesizer.h"
#include "../include/klattsch_parser.h"
#include "../include/text_commands.h"
#include "../include/phonemizer.h"
#include "../include/voice_data.h"
#include "../include/synth_data.h"
#include "../include/pitch_interpolator.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace SharpTalk {

    const std::vector<int32_t>& TtsEngine::SupportedSampleRates() {
        static std::vector<int32_t> rates = KlattSynthesizer::SupportedSampleRates();
        return rates;
    }

    TtsEngine::TtsEngine(const uint8_t* dictData, size_t dictSize,
                         std::function<const uint8_t*(const std::string&, size_t&)> symbolsTable,
                         int32_t sampleRate)
        : TtsEngine(VoiceData::baseline_voice(), dictData, dictSize, std::move(symbolsTable), sampleRate)
    {}

    TtsEngine::TtsEngine(VoiceData voice,
                         const uint8_t* dictData, size_t dictSize,
                         std::function<const uint8_t*(const std::string&, size_t&)> symbolsTable,
                         int32_t sampleRate)
        : SampleRate(sampleRate),
          _fe(dictData, dictSize, std::move(symbolsTable)),
          _voice(voice),
          _be(voice),
          _renderer(voice),
          _synth(sampleRate)
    {
        RebuildPipeline();
    }

    std::vector<std::string> TtsEngine::PhonemizeWord(const std::string& word) {
        std::vector<std::string> result;
        for (auto& [tokens, endPunct] : _fe.TextToSentenceTokens(word)) {
            for (const auto& tok : tokens) {
                if (tok.Phon == AudioProcessor::_SIL_) { continue; }
                const char* name = AudioProcessor::PhonemeNamesTable[tok.Phon];
                if (name != nullptr) {
                    std::string nameStr = name;
                    for (char& c : nameStr) { c = (char)std::tolower((unsigned char)c); }
                    result.push_back(nameStr);
                }
            }
        }
        return result;
    }

    std::vector<int16_t> TtsEngine::Speak(const std::string& text) {
        std::vector<int16_t> samples;
        Speak(text, [&](const int16_t* buf, int32_t len) {
            samples.insert(samples.end(), buf, buf + len);
        });
        return samples;
    }

    // Returns one record per synthesis frame (5 ms each) with pitch and tilt diagnostics.
    std::vector<TtsEngine::PitchFrameRecord> TtsEngine::DumpPitchFrames(const std::string& text) {
        std::vector<PitchFrameRecord> records;
        for (const auto& seg : EmbeddedCmd::ParseSegments(text)) {
            if (seg.IsCommand()) {
                continue;
            }

            std::vector<PhonemeToken> tokens;
            int16_t endPunct = 0;

            if (seg.IsKlattsch()) {
                tokens = KlattschParser::CompileToTokens(KlattschParser::Tokenize(seg.klattschText));
            } else if (seg.IsSinging()) {
                tokens = seg.singing;
            } else {
                // Just take the first sentence for now if multi-sentence
                for (auto& [t, ep] : _fe.TextToSentenceTokens(seg.plainText)) {
                    tokens = t;
                    endPunct = ep;
                    break;
                }
                if (tokens.empty()) { continue; }
            }

            auto dump = _be.Process(tokens, endPunct);
            PitchInterpolator pi(dump);

            int32_t phonIdx = 0, frameInPhon = 0;
            int32_t totalFrames = 0;
            for (int32_t i = 0; i < dump.PhonBuf2InIndex; i++) {
                totalFrames += dump.DurBuf[i];
            }

            for (int32_t f = 0; f < totalFrames; f++) {
                if (frameInPhon == 0) {
                    pi.DoNote(phonIdx);
                }
                pi.Step();
                int16_t p = dump.PhonBuf2[phonIdx];
                std::string name = "?";
                const char* namePtr = AudioProcessor::PhonemeNamesTable[p];
                if (namePtr != nullptr) {
                    name = namePtr;
                }
                records.emplace_back(name, frameInPhon, pi.DbgF0(),
                    pi.DbgTiltExcursion(), pi.DbgTiltSmooth(), pi.DbgTiltHeld(), pi.DbgTiltPhase(),
                    pi.DbgBaselineOffset(), pi.DbgTotalOffset());
                frameInPhon++;
                if (frameInPhon >= dump.DurBuf[phonIdx]) {
                    phonIdx++;
                    frameInPhon = 0;
                }
            }
        }
        return records;
    }

    void TtsEngine::Speak(const std::string& text,
                          std::function<void(const int16_t*, int32_t)> onBuffer) {
        EmbeddedCmd::KlattschMode = false;
        for (const auto& seg : EmbeddedCmd::ParseSegments(text)) {
            if (seg.IsCommand()) {
                ApplyCommand(*seg.cmd);
                continue;
            }
            if (seg.IsKlattsch()) {
                ProcessKlattsch(seg.klattschText, onBuffer);
                continue;
            }
            if (seg.IsSinging()) {
                int32_t dummy = 0;
                ProcessSentence(seg.singing, 0, onBuffer, nullptr, dummy);
                continue;
            }
            auto sentences = _fe.TextToSentenceTokens(seg.plainText);
            for (auto& [tokens, endPunct] : sentences) {
                int32_t dummy = 0;
                ProcessSentence(tokens, endPunct, onBuffer, nullptr, dummy);
            }
        }
    }

    /// Like Speak, but also returns a timeline of phoneme events with start times
    /// in seconds relative to the start of the returned audio.
    std::pair<std::vector<int16_t>, std::vector<PhonemeEvent>>
    TtsEngine::SpeakWithEvents(const std::string& text) {
        std::vector<int16_t> samples;
        std::vector<PhonemeEvent> events;
        int32_t sampleOffset = 0;
        EmbeddedCmd::KlattschMode = false;
        for (const auto& seg : EmbeddedCmd::ParseSegments(text)) {
            if (seg.IsCommand()) {
                ApplyCommand(*seg.cmd);
                continue;
            }
            if (seg.IsKlattsch()) {
                auto klattTokens = KlattschParser::CompileToTokens(KlattschParser::Tokenize(seg.klattschText));
                if (!klattTokens.empty()) {
                    auto dump = _be.Process(klattTokens, 0);
                    int32_t frameOff = 0;
                    for (int32_t i = 0; i < dump.PhonBuf2InIndex; i++) {
                        int16_t phon = dump.PhonBuf2[i];
                        bool emitSil = phon == AudioProcessor::_SIL_ && (i == 0 || dump.PhonBuf2[i - 1] != AudioProcessor::_SIL_);
                        if (phon != AudioProcessor::_SIL_ || emitSil) {
                            events.emplace_back(phon,
                                (float)(sampleOffset + frameOff * _synth.SampFrameLen) / SampleRate);
                        }
                        frameOff += dump.DurBuf[i];
                    }
                    auto audio = ProcessSentenceToBuffer(klattTokens, 0);
                    samples.insert(samples.end(), audio.begin(), audio.end());
                    sampleOffset += (int32_t)audio.size();
                }
                continue;
            }
            if (seg.IsSinging()) {
                ProcessSentence(seg.singing, 0,
                    [&](const int16_t* buf, int32_t len) {
                        samples.insert(samples.end(), buf, buf + len);
                    },
                    &events, sampleOffset);
                continue;
            }
            for (auto& [tokens, endPunct] : _fe.TextToSentenceTokens(seg.plainText)) {
                ProcessSentence(tokens, endPunct,
                    [&](const int16_t* buf, int32_t len) {
                        samples.insert(samples.end(), buf, buf + len);
                    },
                    &events, sampleOffset);
            }
        }
        return { samples, events };
    }

    // Internal helpers

    // Synchronous streaming: synthesizes in chunks, invoking onBuffer for each.
    // C# async Task SpeakAsync -> synchronous in C++ (no async/await).
    void TtsEngine::SpeakAsync(const std::string& text,
                               std::function<void(const int16_t*, int32_t)> onBuffer) {
        EmbeddedCmd::KlattschMode = false;
        for (const auto& seg : EmbeddedCmd::ParseSegments(text)) {
            if (seg.IsCommand()) {
                ApplyCommand(*seg.cmd);
                continue;
            }
            if (seg.IsKlattsch()) {
                auto tokens = KlattschParser::CompileToTokens(KlattschParser::Tokenize(seg.klattschText));
                if (!tokens.empty()) {
                    ProcessSentenceStreaming(tokens, 0, onBuffer);
                }
                continue;
            }
            if (seg.IsSinging()) {
                ProcessSentenceStreaming(seg.singing, 0, onBuffer);
                continue;
            }
            for (auto& [tokens, endPunct] : _fe.TextToSentenceTokens(seg.plainText)) {
                ProcessSentenceStreaming(tokens, endPunct, onBuffer);
            }
        }
    }

    void TtsEngine::ProcessKlattsch(const std::string& text,
                                    std::function<void(const int16_t*, int32_t)> onBuffer) {
        auto tokens = KlattschParser::CompileToTokens(KlattschParser::Tokenize(text));
        if (!tokens.empty()) {
            int32_t dummy = 0;
            ProcessSentence(tokens, 0, onBuffer, nullptr, dummy);
        }
    }

    void TtsEngine::ProcessSentenceStreaming(const std::vector<PhonemeToken>& tokens, int16_t endPunct,
                                             std::function<void(const int16_t*, int32_t)> onBuffer) {
        auto dump = _be.Process(tokens, endPunct);
        ProcessSentenceStreamingFromDump(dump, onBuffer);
    }

    void TtsEngine::ProcessSentenceStreamingFromDump(const SynthInputDump& dump,
                                                     std::function<void(const int16_t*, int32_t)> onBuffer) {
        const int32_t framesPerChunk = 10;
        std::vector<int16_t> audioChunk(framesPerChunk * _synth.SampFrameLen, 0);
        int32_t frameInChunk = 0;

        _renderer.RenderStreaming(dump, [&](const Frame& frame) {
            _synth.SynthesizeFrame(frame, audioChunk.data(), frameInChunk * _synth.SampFrameLen);
            frameInChunk++;

            if (frameInChunk >= framesPerChunk) {
                onBuffer(audioChunk.data(), (int32_t)audioChunk.size());
                audioChunk.assign(framesPerChunk * _synth.SampFrameLen, 0);
                frameInChunk = 0;
            }
        });

        if (frameInChunk > 0) {
            int32_t finalLen = frameInChunk * _synth.SampFrameLen;
            onBuffer(audioChunk.data(), finalLen);
        }
    }

    // Phase 1, fast (_be.Process per sentence) -> collect events + dumps.
    // Calls onEventsReady before any audio is rendered so the UI can set up
    // tracking while the first frame hasn't been synthesized yet.
    // Phase 2, stream formant frames from pre-computed dumps.
    void TtsEngine::SpeakAsyncWithEvents(
        const std::string& text,
        std::function<void(const int16_t*, int32_t)> onBuffer,
        std::function<void(std::vector<PhonemeEvent>&)> onEventsReady) {

        std::vector<PhonemeEvent> events;
        std::vector<SynthInputDump> workItems;
        int32_t sampleOffset = 0;
        EmbeddedCmd::KlattschMode = false;

        for (const auto& seg : EmbeddedCmd::ParseSegments(text)) {
            if (seg.IsCommand()) {
                ApplyCommand(*seg.cmd);
                continue;
            }

            if (seg.IsKlattsch()) {
                auto tokens = KlattschParser::CompileToTokens(KlattschParser::Tokenize(seg.klattschText));
                if (tokens.empty()) {
                    continue;
                }
                auto dump = _be.Process(tokens, 0);
                int32_t frameOff = 0;
                for (int32_t i = 0; i < dump.PhonBuf2InIndex; i++) {
                    int16_t phon = dump.PhonBuf2[i];
                    bool emitSil = phon == AudioProcessor::_SIL_ && (i == 0 || dump.PhonBuf2[i - 1] != AudioProcessor::_SIL_);
                    if (phon != AudioProcessor::_SIL_ || emitSil) {
                        events.emplace_back(phon,
                            (float)(sampleOffset + frameOff * _synth.SampFrameLen) / SampleRate);
                    }
                    frameOff += dump.DurBuf[i];
                }
                sampleOffset += frameOff * _synth.SampFrameLen;
                workItems.push_back(dump);
                continue;
            }

            if (seg.IsSinging()) {
                auto dump = _be.Process(seg.singing, 0);
                int32_t frameOff = 0;
                for (int32_t i = 0; i < dump.PhonBuf2InIndex; i++) {
                    int16_t phon = dump.PhonBuf2[i];
                    bool emitSil = phon == AudioProcessor::_SIL_ && (i == 0 || dump.PhonBuf2[i - 1] != AudioProcessor::_SIL_);
                    if (phon != AudioProcessor::_SIL_ || emitSil) {
                        events.emplace_back(phon,
                            (float)(sampleOffset + frameOff * _synth.SampFrameLen) / SampleRate);
                    }
                    frameOff += dump.DurBuf[i];
                }
                sampleOffset += frameOff * _synth.SampFrameLen;
                workItems.push_back(dump);
                continue;
            }

            for (auto& [tokens, endPunct] : _fe.TextToSentenceTokens(seg.plainText)) {
                auto dump = _be.Process(tokens, endPunct);
                int32_t frameOff = 0;
                for (int32_t i = 0; i < dump.PhonBuf2InIndex; i++) {
                    int16_t phon = dump.PhonBuf2[i];
                    bool emitSil = phon == AudioProcessor::_SIL_ && (i == 0 || dump.PhonBuf2[i - 1] != AudioProcessor::_SIL_);
                    if (phon != AudioProcessor::_SIL_ || emitSil) {
                        events.emplace_back(phon,
                            (float)(sampleOffset + frameOff * _synth.SampFrameLen) / SampleRate,
                            phon != AudioProcessor::_SIL_ && (dump.PhonCtrlBuf2[i] & AudioProcessor::kWord_Start) != 0);
                    }
                    frameOff += dump.DurBuf[i];
                }
                sampleOffset += frameOff * _synth.SampFrameLen;
                workItems.push_back(dump);
            }
        }

        onEventsReady(events);

        for (const auto& dump : workItems) {
            ProcessSentenceStreamingFromDump(dump, onBuffer);
        }
    }

    std::vector<int16_t> TtsEngine::RenderDumpToBuffer(const SynthInputDump& dump) {
        std::vector<int16_t> audio;
        _renderer.RenderStreaming(dump, [&](const Frame& frame) {
            std::vector<int16_t> frameAudio(_synth.SampFrameLen, 0);
            _synth.SynthesizeFrame(frame, frameAudio.data(), 0);
            audio.insert(audio.end(), frameAudio.begin(), frameAudio.end());
        });
        return audio;
    }

    std::vector<int16_t> TtsEngine::ProcessSentenceToBuffer(const std::vector<PhonemeToken>& tokens,
                                                             int16_t endPunct) {
        return RenderDumpToBuffer(_be.Process(tokens, endPunct));
    }

    void TtsEngine::ProcessSentence(const std::vector<PhonemeToken>& tokens, int16_t endPunct,
                                    std::function<void(const int16_t*, int32_t)> onBuffer,
                                    std::vector<PhonemeEvent>* events, int32_t& sampleOffset) {
        auto dump = _be.Process(tokens, endPunct);

        if (events != nullptr) {
            int32_t frameOffset = 0;
            for (int32_t i = 0; i < dump.PhonBuf2InIndex; i++) {
                int16_t phon = dump.PhonBuf2[i];
                bool emitSil = phon == AudioProcessor::_SIL_ && (i == 0 || dump.PhonBuf2[i - 1] != AudioProcessor::_SIL_);
                if (phon != AudioProcessor::_SIL_ || emitSil) {
                    float t = (float)(sampleOffset + frameOffset * _synth.SampFrameLen) / SampleRate;
                    events->emplace_back(phon, t,
                        phon != AudioProcessor::_SIL_ && (dump.PhonCtrlBuf2[i] & AudioProcessor::kWord_Start) != 0);
                }
                frameOffset += dump.DurBuf[i];
            }
        }

        auto audio = ProcessSentenceToBuffer(tokens, endPunct);
        onBuffer(audio.data(), (int32_t)audio.size());
        sampleOffset += (int32_t)audio.size();
    }

    void TtsEngine::ApplyCommand(const EmbeddedCmd::VoiceCommand& cmd) {
        switch (cmd.Type) {
            case EmbeddedCmd::VoiceCommand::Kind::Rate:
                _voice.Rate = (int16_t)std::clamp(cmd.Value, 40, 600);
                _be = AudioProcessor(_voice);
                break;
            case EmbeddedCmd::VoiceCommand::Kind::Pitch:
                _voice.PitchHz = (int16_t)std::clamp(cmd.Value, 40, 500);
                _be = AudioProcessor(_voice);
                break;
            case EmbeddedCmd::VoiceCommand::Kind::Volume:
                _voice.VGain = (int16_t)std::clamp(cmd.Value, 0, 100);
                _synth.InvDFT(_voice.VWave, _voice.VWave1, (int16_t)_voice.VGain);
                break;
        }
    }

    void TtsEngine::RebuildPipeline() {
        _be = AudioProcessor(_voice);
        _renderer = SpeechRenderer(_voice);
        _synth = KlattSynthesizer(SampleRate);
        int16_t lo = _voice.LarynxOffset;
        _synth.SetVoice(_voice.NGain, true,
            (int16_t)std::clamp((int32_t)(_voice.F4Freq + lo),  100, 8000), _voice.F4BW,
            (int16_t)std::clamp((int32_t)(_voice.F5Freq + lo),  100, 8000), _voice.F5BW,
            (int16_t)std::clamp((int32_t)(_voice.F4pFreq + lo), 100, 8000), _voice.F4pBW,
            (int16_t)std::clamp((int32_t)(_voice.F5pFreq + lo), 100, 8000), _voice.F5pBW,
            (int16_t)std::clamp((int32_t)(_voice.F6pFreq + lo), 100, 8000), _voice.F6pBW,
            _voice.NasalBase, _voice.NasalBW,
            _voice.AGain, _voice.ACycle);
        _synth.Jitter          = _voice.Jitter;
        _synth.Shimmer         = _voice.Shimmer;
        _synth.Diplophonia     = _voice.Diplophonia;
        _synth.FryAmount       = _voice.FryAmount;
        _synth.SubglottalAmt   = _voice.SubglottalAmt;
        _synth.BreathAmt       = _voice.BreathAmt;
        _synth.OpenQuotient_set(_voice.OpenQuotient);
        _synth.OQStressLink    = _voice.OQStressLink;
        _synth.OQF0Link        = _voice.OQF0Link;
        _synth.BasePitchHz     = _voice.PitchHz;
        _synth.LarynxOffset    = lo;
        _synth.PharyngealAmt   = _voice.PharyngealAmt;
        _synth.PitchOffsetHz   = _voice.PitchOffsetHz;
        _synth.LipRounding     = _voice.LipRounding;
        _synth.InvDFT(_voice.VWave, _voice.VWave1, (int16_t)_voice.VGain);
    }

}  // namespace SharpTalk
