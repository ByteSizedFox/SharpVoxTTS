#include <cstdio>
#include <string>
#include <vector>
#include <cassert>

#include "../include/TtsEngine.h"
#include "../include/LibraryData.h"
#include "../include/VoiceData.h"
#include "../include/VoicePresets.h"

using namespace SharpVox;

int main() {
    TtsEngine engine(
        LibraryData::dictionary,
        static_cast<size_t>(LibraryData::dictionarySize),
        [](const std::string& key, size_t& outSize) -> const uint8_t* {
            return LibraryData::FindSymbol(key.c_str(), outSize);
        },
        48000);

    VoiceData baseline = engine.GetVoice();
    printf("Baseline Pitch: %d\n", baseline.PitchHz);

    // Test [:voice beth]
    engine.Speak("[:voice beth]", [](const int16_t*, int32_t, void*){}, nullptr);
    VoiceData beth = engine.GetVoice();
    printf("Beth Pitch: %d\n", beth.PitchHz);
    assert(beth.PitchHz != baseline.PitchHz);
    
    // Check if it matches Beth preset
    VoiceData bethPreset;
    VoicePresets::TryGet("beth", bethPreset);
    assert(beth.PitchHz == bethPreset.PitchHz);
    printf("Beth Pitch matches preset: %d\n", beth.PitchHz);

    // Test [:voice chris]
    engine.Speak("[:voice chris]", [](const int16_t*, int32_t, void*){}, nullptr);
    VoiceData chris = engine.GetVoice();
    printf("Chris Pitch: %d\n", chris.PitchHz);
    assert(chris.PitchHz != beth.PitchHz);

    // Test in Klattsch mode
    engine.Speak("[:klattsch on] IY [:voice beth] AE [:klattsch off]", [](const int16_t*, int32_t, void*){}, nullptr);
    VoiceData finalVoice = engine.GetVoice();
    printf("Final Voice (Beth) Pitch: %d\n", finalVoice.PitchHz);
    assert(finalVoice.PitchHz == bethPreset.PitchHz);

    printf("SUCCESS: [:voice] command works in both TTS and Klattsch modes.\n");
    return 0;
}
