#include "../include/TtsEngine.h"
#include "../include/AudioProcessor.h"
#include "../include/SpeechRenderer.h"
#include "../include/KlattSynthesizer.h"
#include "../include/KlattschParser.h"
#include "../include/TextCommands.h"
#include "../include/Phonemizer.h"
#include "../include/VoiceData.h"
#include "../include/SynthData.h"
#include "../include/PitchInterpolator.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace { template<class T> T clamp11(T v, T lo, T hi) { return v < lo ? lo : v > hi ? hi : v; } }

namespace SharpVox {

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
        for (auto& pair : _fe.TextToSentenceTokens(word)) {
            const std::vector<PhonemeToken>& tokens = pair.first;
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
                auto dump = _be.ProcessSinging(seg.singing);
                PitchInterpolator pi(dump);
                int32_t phonIdx2 = 0, frameInPhon2 = 0;
                int32_t totalFrames2 = 0;
                for (int32_t i = 0; i < dump.PhonBuf2InIndex; i++) {
                    totalFrames2 += dump.DurBuf[i];
                }
                for (int32_t f = 0; f < totalFrames2; f++) {
                    if (frameInPhon2 == 0) { pi.DoNote(phonIdx2); }
                    pi.Step();
                    int16_t p2 = dump.PhonBuf2[phonIdx2];
                    const char* namePtr2 = AudioProcessor::PhonemeNamesTable[p2];
                    records.emplace_back(namePtr2 ? std::string(namePtr2) : std::string("?"),
                        frameInPhon2, pi.DbgF0(), pi.DbgTiltExcursion(), pi.DbgTiltSmooth(),
                        pi.DbgTiltHeld(), pi.DbgTiltPhase(), pi.DbgBaselineOffset(), pi.DbgTotalOffset());
                    frameInPhon2++;
                    if (frameInPhon2 >= dump.DurBuf[phonIdx2]) { phonIdx2++; frameInPhon2 = 0; }
                }
                continue;
            } else {
                // Just take the first sentence for now if multi-sentence
                for (auto& p : _fe.TextToSentenceTokens(seg.plainText)) {
                    tokens = p.first;
                    endPunct = p.second;
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
                          void (*onBuffer)(const int16_t*, int32_t, void*),
                          void* userdata) {
        auto cb = [onBuffer, userdata](const int16_t* buf, int32_t len) {
            onBuffer(buf, len, userdata);
        };
        EmbeddedCmd::KlattschMode = false;
        for (const auto& seg : EmbeddedCmd::ParseSegments(text)) {
            if (seg.IsCommand()) {
                ApplyCommand(seg.cmd);
                continue;
            }
            if (seg.IsKlattsch()) {
                auto tokens = KlattschParser::CompileToTokens(KlattschParser::Tokenize(seg.klattschText));
                if (!tokens.empty()) {
                    ProcessSentenceStreaming(tokens, 0, cb);
                }
                continue;
            }
            if (seg.IsSinging()) {
                auto dump = _be.ProcessSinging(seg.singing);
                ProcessSentenceStreamingFromDump(dump, cb);
                continue;
            }
            for (auto& p : _fe.TextToSentenceTokens(seg.plainText)) {
                ProcessSentenceStreaming(p.first, p.second, cb);
            }
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

    void TtsEngine::SpeakWithEvents(
        const std::string& text,
        void (*onBuffer)(const int16_t*, int32_t, void*),
        void (*onEventsReady)(const PhonemeEvent*, int32_t, void*),
        void* userdata) {
        auto cb = [onBuffer, userdata](const int16_t* buf, int32_t len) {
            onBuffer(buf, len, userdata);
        };

        std::vector<PhonemeEvent> events;
        std::vector<SynthInputDump> workItems;
        int32_t sampleOffset = 0;
        EmbeddedCmd::KlattschMode = false;

        for (const auto& seg : EmbeddedCmd::ParseSegments(text)) {
            if (seg.IsCommand()) {
                ApplyCommand(seg.cmd);
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
                workItems.push_back(std::move(dump));
                continue;
            }

            if (seg.IsSinging()) {
                auto dump = _be.ProcessSinging(seg.singing);
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
                workItems.push_back(std::move(dump));
                continue;
            }

            for (auto& p : _fe.TextToSentenceTokens(seg.plainText)) {
                const std::vector<PhonemeToken>& tokens = p.first;
                int16_t endPunct = p.second;
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
                workItems.push_back(std::move(dump));
            }
        }

        onEventsReady(events.data(), (int32_t)events.size(), userdata);

        for (const auto& dump : workItems) {
            ProcessSentenceStreamingFromDump(dump, cb);
        }
    }

    void TtsEngine::ApplyCommand(const EmbeddedCmd::VoiceCommand& cmd) {
        switch (cmd.Type) {
            case EmbeddedCmd::VoiceCommand::Kind::Rate:
                _voice.Rate = (int16_t)clamp11(cmd.Value, 40, 600);
                _be = AudioProcessor(_voice);
                break;
            case EmbeddedCmd::VoiceCommand::Kind::Pitch:
                _voice.PitchHz = (int16_t)clamp11(cmd.Value, 40, 500);
                _be = AudioProcessor(_voice);
                break;
            case EmbeddedCmd::VoiceCommand::Kind::Volume:
                _voice.VGain = (int16_t)clamp11(cmd.Value, 0, 100);
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
            (int16_t)clamp11((int32_t)(_voice.F4Freq + lo),  100, 8000), _voice.F4BW,
            (int16_t)clamp11((int32_t)(_voice.F5Freq + lo),  100, 8000), _voice.F5BW,
            (int16_t)clamp11((int32_t)(_voice.F4pFreq + lo), 100, 8000), _voice.F4pBW,
            (int16_t)clamp11((int32_t)(_voice.F5pFreq + lo), 100, 8000), _voice.F5pBW,
            (int16_t)clamp11((int32_t)(_voice.F6pFreq + lo), 100, 8000), _voice.F6pBW,
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

}  // namespace SharpVox
