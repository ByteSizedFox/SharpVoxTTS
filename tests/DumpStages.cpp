// dump_stages.cpp — diagnostic: runs AudioProcessor::Process() on "hello" and dumps
// PhonBuf2 phoneme list with durations, then total frame count.
// Build: see 'tests' target in Makefile.

#include <cstdio>
#include <string>
#include <vector>

#include "../include/AudioProcessor.h"
#include "../include/LibraryData.h"
#include "../include/Phonemizer.h"
#include "../include/SynthData.h"
#include "../include/VoiceData.h"

int main() {
    using namespace SharpVox;

    // Build default voice (matches C# VoiceData.BaselineVoice)
    VoiceData voice = VoiceData::baseline_voice();

    // Build phonemizer with the embedded dictionary
    Phonemizer fe(
        LibraryData::dictionary,
        static_cast<size_t>(LibraryData::dictionarySize),
        [](const std::string& key, size_t& outSize) -> const uint8_t* {
            auto it = LibraryData::SymbolsTable.find(key);
            if (it == LibraryData::SymbolsTable.end()) {
                outSize = 0;
                return nullptr;
            }
            outSize = it->second.size();
            return it->second.data();
        });

    // Phonemize "hello" into sentence tokens
    auto sentences = fe.TextToSentenceTokens("hello");
    if (sentences.empty()) {
        std::fprintf(stderr, "ERROR: TextToSentenceTokens returned empty\n");
        return 1;
    }

    // Use the first sentence's tokens and end-punct
    auto& [tokens, endPunct] = sentences[0];

    // Run AudioProcessor pipeline
    AudioProcessor ap(voice);
    SynthInputDump dump = ap.Process(tokens, endPunct);

    int count = dump.PhonBuf2InIndex;
    std::printf("PhonBuf2InIndex = %d\n", count);
    std::printf("Phoneme list:\n");

    int totalFrames = 0;
    for (int i = 0; i < count; i++) {
        int16_t phonId = dump.PhonBuf2[i];
        int16_t dur    = dump.DurBuf[i];
        totalFrames += dur;

        // Look up name from PhonemeNamesTable (nullptr-safe)
        const char* name = nullptr;
        if (phonId >= 0 && phonId < 128) {
            name = AudioProcessor::PhonemeNamesTable[phonId];
        }
        std::string nameStr = (name != nullptr) ? name : "?";

        std::printf("  [%3d] phonId=%3d  %-6s  dur=%d\n",
                    i, static_cast<int>(phonId), nameStr.c_str(), static_cast<int>(dur));
    }
    std::printf("Total frames = %d  (%.3f s @ 5ms/frame)\n",
                totalFrames, totalFrames * 0.005);
    return 0;
}
