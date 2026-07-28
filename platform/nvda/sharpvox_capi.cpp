#include "sharpvox_capi.h"

#include <cstring>
#include <new>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <mutex>
#include <atomic>
#endif

#include "../lib/SharpVox.h"

struct SharpVoxCtx {
    SharpVox::SharpVoxSpeaker speaker;

    SharpVoxAudioCallback cb = nullptr;
    void* cb_userdata = nullptr;

    // values to hold pending settings changes
    int32_t pend_rate = 200;
    int32_t pend_pitch = 122;
    float   pend_volume = 1.0f;
    bool    params_dirty = false;

#ifdef _WIN32
    CRITICAL_SECTION cs;
    volatile LONG speaking_flag = 0;

    SharpVoxCtx() { InitializeCriticalSection(&cs); }
    ~SharpVoxCtx() { DeleteCriticalSection(&cs); }
    void lock() { EnterCriticalSection(&cs); }
    void unlock() { LeaveCriticalSection(&cs); }
    void set_speaking(bool v) { InterlockedExchange(&speaking_flag, v ? 1 : 0); }
    bool is_speaking() const { return speaking_flag != 0; }
#else
    std::mutex mtx;
    std::atomic<bool> speaking_flag{false};

    void lock() { mtx.lock(); }
    void unlock() { mtx.unlock(); }
    void set_speaking(bool v) { speaking_flag.store(v); }
    bool is_speaking() const { return speaking_flag.load(); }
#endif

    // Push deferred params into the speaker
    void flushParamsLocked() {
        speaker.Rate = pend_rate;
        speaker.PitchHz = pend_pitch;
        speaker.AudioVolume = pend_volume;
        speaker.ApplyVoice();
        params_dirty = false;
    }
};

extern "C" {

SharpVoxHandle sharpvox_create(void) {
    try {
        return new SharpVoxCtx;
    } catch (...) {
        return nullptr;
    }
}

void sharpvox_destroy(SharpVoxHandle h) {
    delete static_cast<SharpVoxCtx*>(h);
}

static void speak_adapter(SharpVox::SharpVoxSpeaker* /*sp*/, const int16_t* buf,
                           int32_t len, void* ud) {
    auto* ctx = static_cast<SharpVoxCtx*>(ud);
    if (ctx->cb) {
        ctx->cb(buf, len, ctx->cb_userdata);
    }
}

void sharpvox_speak(SharpVoxHandle h, const char* utf8,
                     SharpVoxAudioCallback on_audio, void* userdata) {
    if (!h || !utf8 || !on_audio) return;
    auto* ctx = static_cast<SharpVoxCtx*>(h);
    ctx->lock();
    if (ctx->params_dirty) {
        ctx->flushParamsLocked();
    }
    ctx->cb = on_audio;
    ctx->cb_userdata = userdata;
    ctx->set_speaking(true);
    ctx->unlock();

    ctx->speaker.Speak(std::string(utf8), speak_adapter, ctx);

    ctx->lock();
    ctx->cb = nullptr;
    ctx->cb_userdata = nullptr;
    ctx->set_speaking(false);
    ctx->unlock();
}

void sharpvox_stop(SharpVoxHandle h) {
    if (!h) return;
    auto* ctx = static_cast<SharpVoxCtx*>(h);
    ctx->lock();
    ctx->set_speaking(false);
    ctx->unlock();
}

int sharpvox_is_speaking(SharpVoxHandle h) {
    if (!h) return 0;
    return static_cast<SharpVoxCtx*>(h)->is_speaking() ? 1 : 0;
}

void sharpvox_set_rate(SharpVoxHandle h, int32_t rate) {
    if (!h) return;
    auto* ctx = static_cast<SharpVoxCtx*>(h);
    ctx->lock();
    ctx->pend_rate = rate;
    if (ctx->is_speaking()) {
        ctx->params_dirty = true;
    } else {
        ctx->speaker.Rate = rate;
        ctx->speaker.ApplyVoice();
    }
    ctx->unlock();
}

int32_t sharpvox_get_rate(SharpVoxHandle h) {
    if (!h) return 200;
    return static_cast<SharpVoxCtx*>(h)->speaker.Rate;
}

void sharpvox_set_pitch(SharpVoxHandle h, int32_t hz) {
    if (!h) return;
    auto* ctx = static_cast<SharpVoxCtx*>(h);
    ctx->lock();
    ctx->pend_pitch = hz;
    if (ctx->is_speaking()) {
        ctx->params_dirty = true;
    } else {
        ctx->speaker.PitchHz = hz;
        ctx->speaker.ApplyVoice();
    }
    ctx->unlock();
}

int32_t sharpvox_get_pitch(SharpVoxHandle h) {
    if (!h) return 122;
    return static_cast<SharpVoxCtx*>(h)->speaker.PitchHz;
}

void sharpvox_set_volume(SharpVoxHandle h, float vol) {
    if (!h) return;
    auto* ctx = static_cast<SharpVoxCtx*>(h);
    ctx->lock();
    ctx->pend_volume = vol;
    if (ctx->is_speaking()) {
        ctx->params_dirty = true;
    } else {
        ctx->speaker.AudioVolume = vol;
        ctx->speaker.ApplyVoice();
    }
    ctx->unlock();
}

float sharpvox_get_volume(SharpVoxHandle h) {
    if (!h) return 1.0f;
    return static_cast<SharpVoxCtx*>(h)->speaker.AudioVolume;
}

int32_t sharpvox_get_sample_rate(SharpVoxHandle h) {
    if (!h) return 22050;
    return static_cast<SharpVoxCtx*>(h)->speaker.SampleRate;
}

}  // extern "C"
