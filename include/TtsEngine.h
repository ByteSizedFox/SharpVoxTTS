#ifndef SHARPVOX_TTS_ENGINE_H
#define SHARPVOX_TTS_ENGINE_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include "../include/AudioProcessor.h"
#include "../include/SpeechRenderer.h"
#include "../include/KlattSynthesizer.h"
#include "../include/TextCommands.h"
#include "../include/Phonemizer.h"
#include "../include/VoiceData.h"
#include "../include/SynthData.h"

namespace SharpVox {

    struct PhonemeEvent {
        int16_t Phoneme;
        float TimeSeconds;
        bool IsWordStart;
        PhonemeEvent(int16_t phoneme, float timeSeconds, bool isWordStart = false)
            : Phoneme(phoneme), TimeSeconds(timeSeconds), IsWordStart(isWordStart) {}
    };

    // Top-level TTS API. Converts text to audio via the full pipeline:
    //   Phonemizer (text -> PhonemeToken[]) -> AudioProcessor (phoneme processing) ->
    //   SpeechRenderer (formant targets) -> KlattSynthesizer (PCM samples).
    //
    // The streaming async path pre-computes all SynthInputDumps upfront so latency to
    // first audio is bounded by the front-end processing time, not the synthesis time.
    // Each sentence or clause gets its own AudioProcessor.Process() call so pitch and
    // duration resets happen at natural boundaries.
    class TtsEngine {
    public:
        static const int32_t DefaultSampleRate = 22050;
        static const std::vector<int32_t>& SupportedSampleRates();
        int32_t SampleRate;

        TtsEngine(const uint8_t* dictData, size_t dictSize,
                  std::function<const uint8_t*(const std::string&, size_t&)> symbolsTable,
                  int32_t sampleRate = DefaultSampleRate);

        TtsEngine(VoiceData voice,
                  const uint8_t* dictData, size_t dictSize,
                  std::function<const uint8_t*(const std::string&, size_t&)> symbolsTable,
                  int32_t sampleRate = DefaultSampleRate);

        VoiceData GetVoice() const { return _voice; }
        void SetVoice(VoiceData voice) { _voice = voice; RebuildPipeline(); }

        struct LookupStats {
            int32_t dict;
            int32_t morph;
            int32_t lts;
        };
        LookupStats GetLookupStats() const {
            return { _fe.StatDict, _fe.StatMorph, _fe.StatLts };
        }
        void ResetLookupStats() { _fe.ResetStats(); }
        DictReader& Dict() { return _fe.Dict(); }

        std::vector<std::string> PhonemizeWord(const std::string& word);

        void ApplyVoice() { RebuildPipeline(); }

        struct PitchFrameRecord {
            std::string Phoneme;
            int32_t FrameInPhon;
            int32_t F0;
            int32_t TiltExcursion;
            int32_t TiltSmooth;
            int32_t TiltHeld;
            int32_t TiltPhase;
            int32_t BaselineOffset;
            int32_t TotalOffset;
            PitchFrameRecord(const std::string& phoneme, int32_t frameInPhon, int32_t f0,
                int32_t tiltExcursion, int32_t tiltSmooth, int32_t tiltHeld, int32_t tiltPhase,
                int32_t baselineOffset, int32_t totalOffset)
                : Phoneme(phoneme), FrameInPhon(frameInPhon), F0(f0),
                  TiltExcursion(tiltExcursion), TiltSmooth(tiltSmooth),
                  TiltHeld(tiltHeld), TiltPhase(tiltPhase),
                  BaselineOffset(baselineOffset), TotalOffset(totalOffset) {}
        };

        // Returns one record per synthesis frame (5 ms each) with pitch and tilt diagnostics.
        std::vector<PitchFrameRecord> DumpPitchFrames(const std::string& text);

        void Speak(const std::string& text,
                   void (*onBuffer)(const int16_t* buf, int32_t len, void* userdata),
                   void* userdata = nullptr);

        void SpeakWithEvents(
            const std::string& text,
            void (*onBuffer)(const int16_t* buf, int32_t len, void* userdata),
            void (*onEventsReady)(const PhonemeEvent* events, int32_t count, void* userdata),
            void* userdata = nullptr);

    private:
        Phonemizer _fe;
        VoiceData _voice;
        AudioProcessor _be;
        SpeechRenderer _renderer;
        KlattSynthesizer _synth;

        void ProcessSentenceStreaming(const std::vector<PhonemeToken>& tokens, int16_t endPunct,
                                     std::function<void(const int16_t*, int32_t)> onBuffer);

        void ProcessSentenceStreamingFromDump(const SynthInputDump& dump,
                                              std::function<void(const int16_t*, int32_t)> onBuffer);


        void ApplyCommand(const EmbeddedCmd::VoiceCommand& cmd);

        void RebuildPipeline();
    };

}  // namespace SharpVox

#endif  // SHARPVOX_TTS_ENGINE_H
