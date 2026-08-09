#include "../include/TtsEngine.h"
#include "../include/AudioProcessor.h"
#include "../include/SpeechRenderer.h"
#include "../include/KlattschParser.h"
#include "../include/TextCommands.h"
#include "../include/Phonemizer.h"
#include "../include/VoiceData.h"
#include "../include/SynthData.h"
#include "../include/PitchInterpolator.h"
#include "../include/VoicePresets.h"
#include <algorithm>
#include <cmath>
#include <cstring>


namespace { template<class T> T clamp11(T v, T lo, T hi) { return v < lo ? lo : v > hi ? hi : v; } }

namespace SharpVox {

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
        _fe.TextToSentenceTokens(word, [&](const std::vector<PhonemeToken>& tokens, int16_t, bool) {
            for (const auto& tok : tokens) {
                if (tok.Phon == _SIL_) { continue; }
                const char* name = AudioProcessor::PhonemeNamesTable[tok.Phon];
                if (name != nullptr) {
                    std::string nameStr = name;
                    for (char& c : nameStr) { c = (char)std::tolower((unsigned char)c); }
                    result.push_back(nameStr);
                }
            }
        });
        return result;
    }

    // Walks a plan frame by frame through PitchInterpolator and appends one diagnostic record per frame.
    static void AppendPitchFrameRecords(const ClausePlan& plan,
                                        std::vector<TtsEngine::PitchFrameRecord>& records) {
        PitchInterpolator pi(plan);
        int32_t phonIdx = 0, frameInPhon = 0;
        int32_t totalFrames = 0;
        for (int32_t i = 0; i < plan.PhonBufInIndex; i++) {
            totalFrames += plan.DurBuf[i];
        }
        for (int32_t f = 0; f < totalFrames; f++) {
            if (frameInPhon == 0) { pi.DoNote(phonIdx); }
            pi.Step();
            const char* namePtr = AudioProcessor::PhonemeNamesTable[plan.PhonBuf[phonIdx]];
            records.emplace_back(namePtr ? std::string(namePtr) : std::string("?"),
                frameInPhon, pi.DbgF0(), pi.DbgFujiExcursion(), pi.DbgPhraseResp(),
                pi.DbgAccentResp(), pi.DbgBaselineOffset(), pi.DbgTotalOffset());
            frameInPhon++;
            if (frameInPhon >= plan.DurBuf[phonIdx]) { phonIdx++; frameInPhon = 0; }
        }
    }

    // Returns one record per synthesis frame (5 ms each) with pitch and tilt diagnostics.
    std::vector<TtsEngine::PitchFrameRecord> TtsEngine::DumpPitchFrames(const std::string& text) {
        std::vector<PitchFrameRecord> records;
        for (const auto& seg : EmbeddedCmd::ParseSegments(text, &_klattsch)) {
            if (seg.IsCommand()) {
                continue;
            }

            std::vector<PhonemeToken> tokens;
            int16_t endPunct = 0;

            if (seg.IsKlattsch()) {
                tokens = _klattsch.CompileToTokens(KlattschParser::Tokenize(seg.klattschText));
            } else if (seg.IsSinging()) {
                AppendPitchFrameRecords(_be.ProcessSinging(seg.singing), records);
                continue;
            } else {
                // Just take the first sentence for now if multi-sentence
                bool gotFirst = false;
                _fe.TextToSentenceTokens(seg.plainText,
                        [&](const std::vector<PhonemeToken>& toks, int16_t ep, bool) {
                    if (gotFirst) { return; }
                    tokens = toks;
                    endPunct = ep;
                    gotFirst = true;
                });
                if (tokens.empty()) { continue; }
            }

            AppendPitchFrameRecords(_be.Process(tokens, endPunct), records);
        }
        return records;
    }

    void TtsEngine::Speak(const std::string& text,
                          void (*onBuffer)(const int16_t*, int32_t, void*),
                          void* userdata) {
        auto cb = [onBuffer, userdata](const int16_t* buf, int32_t len) {
            onBuffer(buf, len, userdata);
        };
        _klattsch.Reset(_voice);
        auto segments = EmbeddedCmd::ParseSegments(text, &_klattsch);
        size_t lastContent = segments.size();
        for (size_t i = segments.size(); i-- > 0; )
            if (!segments[i].IsCommand()) { lastContent = i; break; }
        for (size_t si = 0; si < segments.size(); si++) {
            const auto& seg = segments[si];
            if (seg.IsCommand()) {
                ApplyCommand(seg.cmd);
                continue;
            }
            if (seg.IsKlattsch()) {
                auto tokens = _klattsch.CompileToTokens(KlattschParser::Tokenize(seg.klattschText));
                if (!tokens.empty()) {
                    ProcessSentenceStreaming(tokens, 0, cb);
                }
                continue;
            }
            if (seg.IsSinging()) {
                auto plan = _be.ProcessSinging(seg.singing);
                ProcessSentenceStreamingFromPlan(plan, cb);
                continue;
            }
            _fe.TextToSentenceTokens(seg.plainText,
                    [&](const std::vector<PhonemeToken>& tokens, int16_t ep, bool isLast) {
                if (ep == 0 && si == lastContent && isLast)
                    ep = _Period_;
                ProcessSentenceStreaming(tokens, ep, cb);
            });
        }
    }

    void TtsEngine::ProcessSentenceStreaming(const std::vector<PhonemeToken>& tokens, int16_t endPunct,
                                             std::function<void(const int16_t*, int32_t)> onBuffer) {
        auto plan = _be.Process(tokens, endPunct);
        ProcessSentenceStreamingFromPlan(plan, onBuffer);
    }

    void TtsEngine::ProcessSentenceStreamingFromPlan(const ClausePlan& plan,
                                                     std::function<void(const int16_t*, int32_t)> onBuffer,
                                                     std::function<void(int32_t, const Frame&)> onFrame) {
        const int32_t framesPerChunk = 10;
        std::vector<int16_t> audioChunk(framesPerChunk * _synth.SampFrameLen, 0);
        int32_t frameInChunk = 0;
        int32_t frameIndex = 0;

        _renderer.RenderStreaming(plan, [&](const Frame& frame) {
            if (onFrame) {
                onFrame(frameIndex, frame);
            }
            _synth.SynthesizeFrame(frame, audioChunk.data(), frameInChunk * _synth.SampFrameLen);
            frameInChunk++;
            frameIndex++;

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
        void (*onChunk)(const int16_t*, int32_t, const PhonemeEvent*, int32_t,
                        const FormantEvent*, int32_t, void*),
        void* userdata) {
        int32_t sampleOffset = 0;
        bool rendered = false;
        // 60 Hz tick counter, kept across clauses so the formant grid and its
        // timestamps stay absolute from the start of the utterance.
        int32_t tickIndex = 0;

        // Renders one clause plan, slicing its events into the chunks they fall in.
        auto speakPlan = [&](const ClausePlan& plan, bool wordStarts) {
            int32_t clauseStart = sampleOffset;
            std::vector<PhonemeEvent> events;
            std::vector<int32_t> onsets;
            int32_t frameOff = 0;
            for (int32_t i = 0; i < plan.PhonBufInIndex; i++) {
                int16_t phon = plan.PhonBuf[i];
                bool emitSil = phon == _SIL_ && (i == 0 || plan.PhonBuf[i - 1] != _SIL_);
                if (phon != _SIL_ || emitSil) {
                    int32_t onset = sampleOffset + frameOff * _synth.SampFrameLen;
                    bool ws = wordStarts && phon != _SIL_ &&
                        (plan.PhonCtrlBuf[i] & kWord_Start) != 0;
                    events.emplace_back(phon, (float)onset / SampleRate, ws);
                    onsets.push_back(onset);
                }
                frameOff += plan.DurBuf[i];
            }
            int32_t planEnd = sampleOffset + frameOff * _synth.SampFrameLen;
            size_t cursor = 0;

            std::vector<FormantEvent> formantEvents;
            std::vector<int32_t> formantSamples;
            size_t formantCursor = 0;

            // Fresh renderer/synth per clause, matching the previous batched behavior.
            ResetSynthVoice();
            rendered = true;
            ProcessSentenceStreamingFromPlan(plan, [&](const int16_t* buf, int32_t len) {
                size_t first = cursor;
                size_t fFirst = formantCursor;
                // zero-duration tail events sit exactly on planEnd; flush with the final chunk
                int32_t limit = sampleOffset + len;
                if (limit >= planEnd) { limit = planEnd + 1; }
                while (cursor < onsets.size() && onsets[cursor] < limit) { cursor++; }
                while (formantCursor < formantSamples.size() && formantSamples[formantCursor] < limit) {
                    formantCursor++;
                }
                onChunk(buf, len, events.data() + first, (int32_t)(cursor - first),
                        formantEvents.data() + fFirst, (int32_t)(formantCursor - fFirst), userdata);
                sampleOffset += len;
            }, [&](int32_t frameIndex, const Frame& frame) {
                // emit the formant events
                int32_t frameStart = clauseStart + frameIndex * _synth.SampFrameLen;
                int32_t frameEnd = frameStart + _synth.SampFrameLen;
                float f1 = (float)PitchToHz(frame.F1);
                float f2 = (float)PitchToHz(frame.F2);
                float f3 = (float)PitchToHz(frame.F3);
                while (true) {
                    int32_t tickSample = (int32_t)(((int64_t)tickIndex * SampleRate + 30) / 60);
                    if (tickSample >= frameEnd) {
                        break;
                    }
                    formantEvents.emplace_back(tickIndex / 60.0f, f1, f2, f3);
                    formantSamples.push_back(tickSample);
                    tickIndex++;
                }
            });
            if (cursor < onsets.size() || formantCursor < formantSamples.size()) {
                // plan produced no audio frames; deliver its events with an empty chunk
                static const int16_t kEmpty = 0;
                onChunk(&kEmpty, 0, events.data() + cursor, (int32_t)(onsets.size() - cursor),
                        formantEvents.data() + formantCursor,
                        (int32_t)(formantSamples.size() - formantCursor), userdata);
            }
        };

        _klattsch.Reset(_voice);
        auto segments = EmbeddedCmd::ParseSegments(text, &_klattsch);
        size_t lastContent = segments.size();
        for (size_t i = segments.size(); i-- > 0; )
            if (!segments[i].IsCommand()) { lastContent = i; break; }
        for (size_t si = 0; si < segments.size(); si++) { const auto& seg = segments[si];
            if (seg.IsCommand()) {
                ApplyCommand(seg.cmd);
                continue;
            }

            if (seg.IsKlattsch()) {
                auto tokens = _klattsch.CompileToTokens(KlattschParser::Tokenize(seg.klattschText));
                if (!tokens.empty()) {
                    speakPlan(_be.Process(tokens, 0), false);
                }
                continue;
            }

            if (seg.IsSinging()) {
                speakPlan(_be.ProcessSinging(seg.singing), false);
                continue;
            }

            _fe.TextToSentenceTokens(seg.plainText,
                    [&](const std::vector<PhonemeToken>& tokens, int16_t endPunct, bool isLast) {
                if (endPunct == 0 && si == lastContent && isLast)
                    endPunct = _Period_;
                speakPlan(_be.Process(tokens, endPunct), true);
            });
        }

        // the batched path left a fresh AudioProcessor behind; keep that post-state
        if (rendered) { _be = AudioProcessor(_voice); }
    }

    void TtsEngine::ApplyCommand(const EmbeddedCmd::VoiceCommand& cmd) {
        switch (cmd.Type) {
            case EmbeddedCmd::VoiceCommand::Kind::Rate:
                _voice.Rate = (int16_t)clamp11<int32_t>(cmd.Value, 40, 600);
                _be = AudioProcessor(_voice);
                _klattsch.Reset(_voice);
                break;
            case EmbeddedCmd::VoiceCommand::Kind::Pitch:
                _voice.PitchHz = (int16_t)clamp11<int32_t>(cmd.Value, 40, 500);
                _be = AudioProcessor(_voice);
                _synth.BasePitchHz = _voice.PitchHz;
                _klattsch.Reset(_voice);
                break;
            case EmbeddedCmd::VoiceCommand::Kind::Volume:
                _voice.VGain = (int16_t)clamp11<int32_t>(cmd.Value, 0, 100);
                _synth.ComputeGlotWave((int16_t)_voice.VGain);
                break;
            case EmbeddedCmd::VoiceCommand::Kind::Voice:
                if (VoicePresets::TryGet(cmd.VoiceName, _voice)) {
                    RebuildPipeline();
                    _klattsch.Reset(_voice);
                }
                break;
            case EmbeddedCmd::VoiceCommand::Kind::Custom: {
                bool changed = false;
                for (const auto& p : cmd.Params)
                    changed |= VoicePresets::SetParam(_voice, p.first, p.second);
                if (changed) {
                    RebuildPipeline();
                    _klattsch.Reset(_voice);
                }
                break;
            }
        }
    }

#ifdef SHARPVOX_SAMPLED_GLOT
    void TtsEngine::SetGlottalSample(const float* pcm, int32_t len, int32_t srcRate, float naturalPitchHz) {
        _glotPcm.assign(pcm, pcm + len);
        _glotSrcRate = srcRate;
        _glotNatHz   = naturalPitchHz;
        _synth.SetGlottalSample(pcm, len, srcRate, naturalPitchHz);
    }
    void TtsEngine::ClearGlottalSample() {
        _glotPcm.clear();
        _glotPcm.shrink_to_fit();
        _glotSrcRate = 0;
        _glotNatHz   = 0.0f;
        _synth.ClearGlottalSample();
    }
    void TtsEngine::SetGlottalPitchShift(bool enabled) {
        _glotPitchShift = enabled;
        _synth.SgPitchShift = enabled;
    }
#endif

    void TtsEngine::RebuildPipeline() {
        _be = AudioProcessor(_voice);
        ResetSynthVoice();
    }

    void TtsEngine::ResetSynthVoice() {
        _renderer = SpeechRenderer(_voice);
        _synth = KlattSynthesizerFP(SampleRate);
        int16_t lo = _voice.LarynxOffset;
        // Parallel frication bank: baseline formants scaled by TractScale (small
        // tract -> higher frication). Not voice-tunable; user control was retired
        // since it harms intelligibility. F6pBW=700 is the wide anti-whistle shelf.
        float ts = _voice.TractScale > 0 ? _voice.TractScale : 1.0f;
        int16_t f4p = (int16_t)clamp11<int32_t>((int32_t)(3650 * ts) + lo, 100, 8000);
        int16_t f5p = (int16_t)clamp11<int32_t>((int32_t)(4200 * ts) + lo, 100, 8000);
        int16_t f6p = (int16_t)clamp11<int32_t>((int32_t)(6000 * ts) + lo, 100, 8000);
        _synth.SetVoice(_voice.NGain, true,
            (int16_t)clamp11<int32_t>(_voice.F4Freq + lo,  100, 8000), _voice.F4BW,
            (int16_t)clamp11<int32_t>(_voice.F5Freq + lo,  100, 8000), _voice.F5BW,
            f4p, 150,
            f5p, 100,
            f6p, 700,
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
        _synth.ComputeGlotWave((int16_t)_voice.VGain);
#ifdef SHARPVOX_SAMPLED_GLOT
        _synth.SgPitchShift = _glotPitchShift;
        if (!_glotPcm.empty())
            _synth.SetGlottalSample(_glotPcm.data(), (int32_t)_glotPcm.size(), _glotSrcRate, _glotNatHz);
#endif
    }

}  // namespace SharpVox
