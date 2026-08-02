#include "sharpvox_wrapper.h"
#include "TtsEngine.h"
#include "LibraryData.h"
#include "VoicePresets.h"
#include <memory>

static std::unique_ptr<SharpVox::TtsEngine> s_engine;

extern "C" {

void sharpvox_init(void) {
    if (s_engine) return;

    auto symbolsTable = [](const std::string& key, size_t& sz) -> const uint8_t* {
        return SharpVox::LibraryData::FindSymbol(key.c_str(), sz);
    };

    s_engine = std::make_unique<SharpVox::TtsEngine>(
        SharpVox::LibraryData::dictionary,
        (size_t)SharpVox::LibraryData::dictionarySize,
        symbolsTable,
        22050
    );
}

namespace {
    struct SpeakCtx {
        sharpvox_buffer_cb cb;
        void* userdata;
        int phon;
        int32_t sample_pos;
    };
}

void sharpvox_speak(const char* text, sharpvox_buffer_cb on_buffer, void* userdata) {
    if (!s_engine) sharpvox_init();

    SpeakCtx ctx;
    ctx.cb = on_buffer;
    ctx.userdata = userdata;
    ctx.phon = -1;
    ctx.sample_pos = 0;

    s_engine->SpeakWithEvents(
        text,
        [](const int16_t* buf, int32_t len,
           const SharpVox::PhonemeEvent* ev, int32_t n,
           const SharpVox::FormantEvent*, int32_t, void* ud) {
            auto* c = static_cast<SpeakCtx*>(ud);
            // report the phoneme sounding at the start of this chunk
            float t0 = (float)c->sample_pos / 22050.0f;
            int phon = c->phon;
            for (int32_t i = 0; i < n && ev[i].TimeSeconds <= t0; i++) phon = ev[i].Phoneme;
            c->cb(buf, len, phon, c->userdata);
            if (n > 0) c->phon = ev[n - 1].Phoneme;
            c->sample_pos += len;
        },
        &ctx
    );
}

void sharpvox_terminate(void) {
    s_engine.reset();
}

}
