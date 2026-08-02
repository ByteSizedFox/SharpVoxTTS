// MidiTimingProbe.cpp
// Dumps phoneme callback times for a Klattsch/SharpVox text input so external
// tools can compare MIDI absolute note times against TTS event timing.

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../include/LibraryData.h"
#include "../platform/lib/SharpVox.h"

static std::string ReadAllFromStdin() {
    std::string data;
    char buf[4096];
    while (true) {
        size_t n = std::fread(buf, 1, sizeof(buf), stdin);
        if (n == 0) break;
        data.append(buf, buf + n);
    }
    return data;
}

static std::string ReadFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
    std::string text;
    int32_t sampleRate = 0;
    const char* inputPath = nullptr;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            inputPath = argv[++i];
        } else if ((arg == "-s" || arg == "--samplerate") && i + 1 < argc) {
            sampleRate = static_cast<int32_t>(std::strtol(argv[++i], nullptr, 10));
        } else if (text.empty()) {
            text = arg;
        }
    }
    if (inputPath) {
        text = ReadFile(inputPath);
    }
    if (text.empty()) {
        text = ReadAllFromStdin();
    }

    if (text.empty()) {
        std::fprintf(stderr, "usage: %s [--input file] [-s samplerate] <text>\n", argv[0]);
        return 1;
    }

    SharpVox::SharpVoxSpeaker speaker;
    if (sampleRate > 0) {
        speaker.SampleRate = sampleRate;
        speaker.ApplyVoice();
    }
    std::vector<SharpVox::PhonemeEvent> events;
    try {
        speaker.SpeakWithEvents(
            text,
            [](SharpVox::SharpVoxSpeaker*, const int16_t*, int32_t,
               const SharpVox::PhonemeEvent* ev, int32_t count,
               const SharpVox::FormantEvent*, int32_t, void* ud) {
                auto* out = static_cast<std::vector<SharpVox::PhonemeEvent>*>(ud);
                out->insert(out->end(), ev, ev + count);
            },
            &events
        );
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "error: %s\n", ex.what());
        return 2;
    }

    for (const auto& ev : events) {
        const char* name = nullptr;
        if (ev.Phoneme == SharpVox::_SIL_) {
            name = "SIL";
        } else if (ev.Phoneme >= 0 && ev.Phoneme < 56) {
            name = SharpVox::AudioProcessor::PhonemeNamesTable[ev.Phoneme];
        }
        std::printf("%s,%.6f,%d\n",
                    name ? name : "?",
                    ev.TimeSeconds,
                    ev.IsWordStart ? 1 : 0);
    }
    return 0;
}
