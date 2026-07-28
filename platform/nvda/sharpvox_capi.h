#ifndef SHARPVOX_CAPI_H
#define SHARPVOX_CAPI_H

#include <stdint.h>

#ifdef SHARPVOX_CAPI_EXPORTS
#define SHARPVOX_API __declspec(dllexport)
#else
#define SHARPVOX_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* SharpVoxHandle;

typedef void (*SharpVoxAudioCallback)(const int16_t* samples, int32_t count, void* userdata);

SHARPVOX_API SharpVoxHandle sharpvox_create(void);
SHARPVOX_API void sharpvox_destroy(SharpVoxHandle h);

SHARPVOX_API void sharpvox_speak(SharpVoxHandle h, const char* utf8,
                                  SharpVoxAudioCallback on_audio, void* userdata);
SHARPVOX_API void sharpvox_stop(SharpVoxHandle h);
SHARPVOX_API int  sharpvox_is_speaking(SharpVoxHandle h);

SHARPVOX_API void sharpvox_set_rate(SharpVoxHandle h, int32_t rate);
SHARPVOX_API int32_t sharpvox_get_rate(SharpVoxHandle h);

SHARPVOX_API void sharpvox_set_pitch(SharpVoxHandle h, int32_t hz);
SHARPVOX_API int32_t sharpvox_get_pitch(SharpVoxHandle h);

SHARPVOX_API void sharpvox_set_volume(SharpVoxHandle h, float vol);
SHARPVOX_API float sharpvox_get_volume(SharpVoxHandle h);

SHARPVOX_API int32_t sharpvox_get_sample_rate(SharpVoxHandle h);

#ifdef __cplusplus
}
#endif

#endif
