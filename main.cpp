#include <cstdio>
#include "platform/lib/sharptalk_speaker.h"

using SharpTalk::SharpTalkSpeaker;

int main() {
    SharpTalkSpeaker speaker;

    FILE* f = fopen("hello.wav", "wb");
    auto w32 = [f](int v)   { fwrite(&v, 4, 1, f); };
    auto w16 = [f](short v) { fwrite(&v, 2, 1, f); };
    int sr = speaker.SampleRate;

    // Write header with placeholder sizes, patched after synthesis.
    fwrite("RIFF", 4, 1, f); w32(0);
    fwrite("WAVEfmt ", 8, 1, f); w32(16); w16(1); w16(1);
    w32(sr); w32(sr * 2); w16(2); w16(16);
    fwrite("data", 4, 1, f); w32(0);

    int total = 0;
    speaker.Speak("hello world", [&](const short* buf, int len) {
        fwrite(buf, 2, len, f);
        total += len;
    });

    int data = total * 2;
    fseek(f, 4,  SEEK_SET); w32(36 + data);
    fseek(f, 40, SEEK_SET); w32(data);
    fclose(f);
}
