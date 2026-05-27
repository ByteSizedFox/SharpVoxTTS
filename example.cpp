#include <cstdio>
#include <vector>
#include "platform/lib/sharpvox.h"

using SharpVox::SharpVoxSpeaker;
using SharpVox::PhonemeEvent;

static void write_wav_streaming(const char* path, SharpVoxSpeaker& speaker, const char* text) {
    FILE* f = fopen(path, "wb");
    auto w32 = [f](int v)   { fwrite(&v, 4, 1, f); };
    auto w16 = [f](short v) { fwrite(&v, 2, 1, f); };
    int sr = speaker.SampleRate;

    fwrite("RIFF", 4, 1, f); w32(0);
    fwrite("WAVEfmt ", 8, 1, f); w32(16); w16(1); w16(1);
    w32(sr); w32(sr * 2); w16(2); w16(16);
    fwrite("data", 4, 1, f); w32(0);

    int total = 0;
    speaker.Speak(text, [&](const short* buf, int len) {
        fwrite(buf, 2, len, f);
        total += len;
    });

    int data = total * 2;
    fseek(f, 4,  SEEK_SET); w32(36 + data);
    fseek(f, 40, SEEK_SET); w32(data);
    fclose(f);
}

static void speak_with_phonemes(SharpVoxSpeaker& speaker, const char* text) {
    std::vector<short> samples;

    speaker.SpeakWithEvents(text,
        [&](const short* buf, int len) {
            samples.insert(samples.end(), buf, buf + len);
        },
        [](const std::vector<PhonemeEvent>& events) {
            for (const auto& e : events) {
                printf("  phoneme %d at %.3fs\n", e.Phoneme, e.TimeSeconds);
            }
        });

    printf("total samples: %d\n", (int)samples.size());
}

int main() {
    SharpVoxSpeaker speaker;

    write_wav_streaming("hello.wav", speaker, "hello world");

    speak_with_phonemes(speaker, "hello world");
}
