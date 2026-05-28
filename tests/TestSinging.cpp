// TestSinging.cpp — verifies ProcessSinging produces bit-identical output to
// Process() for singing token streams. Fails with a non-zero exit code and a
// description of any divergence.
//
// Build: g++ -std=c++11 -O2 -Iinclude tests/TestSinging.cpp \
//        src/*.o platform/cli/WavWriter.o -lm -o tests/test_singing

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../include/AudioProcessor.h"
#include "../include/LibraryData.h"
#include "../include/SynthData.h"
#include "../include/TextCommands.h"
#include "../include/VoiceData.h"

using namespace SharpVox;

static bool dumps_equal(const SynthInputDump& a, const SynthInputDump& b, const char* label) {
    bool ok = true;

    if (a.PhonBuf2InIndex != b.PhonBuf2InIndex) {
        printf("  FAIL [%s] PhonBuf2InIndex: %d vs %d\n",
               label, a.PhonBuf2InIndex, b.PhonBuf2InIndex);
        ok = false;
    }

    int32_t n = std::min(a.PhonBuf2InIndex, b.PhonBuf2InIndex);

    for (int32_t i = 0; i < n; i++) {
        if (a.PhonBuf2[i] != b.PhonBuf2[i]) {
            printf("  FAIL [%s] PhonBuf2[%d]: %d vs %d\n",
                   label, i, (int)a.PhonBuf2[i], (int)b.PhonBuf2[i]);
            ok = false;
        }
    }
    for (int32_t i = 0; i < n; i++) {
        if (a.DurBuf[i] != b.DurBuf[i]) {
            printf("  FAIL [%s] DurBuf[%d]: %d vs %d\n",
                   label, i, (int)a.DurBuf[i], (int)b.DurBuf[i]);
            ok = false;
        }
    }
    for (int32_t i = 0; i < n; i++) {
        if (a.UserNoteBuf2[i] != b.UserNoteBuf2[i]) {
            printf("  FAIL [%s] UserNoteBuf2[%d]: %d vs %d\n",
                   label, i, (int)a.UserNoteBuf2[i], (int)b.UserNoteBuf2[i]);
            ok = false;
        }
    }
    for (int32_t i = 0; i < n; i++) {
        if (a.UserPitchBuf2[i] != b.UserPitchBuf2[i]) {
            printf("  FAIL [%s] UserPitchBuf2[%d]: %d vs %d\n",
                   label, i, (int)a.UserPitchBuf2[i], (int)b.UserPitchBuf2[i]);
            ok = false;
        }
    }
    if (a.Pitch.Singing != b.Pitch.Singing) {
        printf("  FAIL [%s] Pitch.Singing: %d vs %d\n",
               label, (int)a.Pitch.Singing, (int)b.Pitch.Singing);
        ok = false;
    }
    if (a.Pitch.VpBaselinePitch != b.Pitch.VpBaselinePitch) {
        printf("  FAIL [%s] Pitch.VpBaselinePitch: %d vs %d\n",
               label, (int)a.Pitch.VpBaselinePitch, (int)b.Pitch.VpBaselinePitch);
        ok = false;
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

struct Test { const char* label; const char* text; };

static int run_test(const Test& t, AudioProcessor& be) {
    // Parse the singing segment from the text.
    auto segs = EmbeddedCmd::ParseSegments(t.text);
    std::vector<PhonemeToken> singing;
    for (const auto& seg : segs) {
        if (seg.IsSinging()) {
            singing.insert(singing.end(), seg.singing.begin(), seg.singing.end());
        }
    }
    if (singing.empty()) {
        printf("  SKIP [%s] no singing tokens found\n", t.label);
        return 0;
    }

    auto dump_old = be.Process(singing, 0);        // full pipeline
    auto dump_new = be.ProcessSinging(singing);    // streaming path

    bool ok = dumps_equal(dump_old, dump_new, t.label);
    if (ok) {
        printf("  PASS [%s]\n", t.label);
        return 0;
    }
    return 1;
}

int main() {
    VoiceData voice = VoiceData::baseline_voice();
    AudioProcessor be(voice);

    static const Test tests[] = {
        { "simple melody",      "[heh<400,A4> low<600,G4> wer<500,E4> ld<300,D4>]" },
        { "vowel notes",        "[aa<300,C4> iy<300,E4> ow<300,G4>]"               },
        { "consonant clusters", "[piy<400,C5> tiy<400,G4> diy<400,E4>]"            },
        { "nasal cluster",      "[m<300,A3> aa<500,A3> ng<300,G3>]"                },
        { "fricative onset",    "[sh<200,E4> iy<600,E4>]"                           },
        { "sil in group",       "[aa<200,C4> _<100> aa<200,E4>]"                   },
        { "two blocks",         "[heh<400,A4> low<600,G4>][wer<500,E4> ld<300,D4>]"},
        { "high note",          "[aa<500,C6>]"                                      },
        { "low note",           "[aa<500,C2>]"                                      },
        { "rapid notes",        "[piy<100,C5> tiy<100,G4> kiy<100,E4> siy<100,D4>]" },
        { "voiced stops",       "[biy<300,A4> diy<300,G4> giy<300,E4>]"             },
    };

    int failures = 0;
    for (const auto& t : tests) {
        failures += run_test(t, be);
    }

    printf("\n%s: %d/%d tests passed\n",
           failures == 0 ? "ALL PASS" : "FAILURES",
           (int)(sizeof(tests)/sizeof(tests[0])) - failures,
           (int)(sizeof(tests)/sizeof(tests[0])));
    return failures;
}
