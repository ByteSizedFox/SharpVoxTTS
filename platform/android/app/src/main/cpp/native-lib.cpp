#include <jni.h>
#include <string>
#include <cstring>

#include "SharpVox.h"
#include "VoicePresets.h"

#define MAX_BUFFER (10 * 60) * 22050
static short samples[MAX_BUFFER];
static int total_size = 0;
static int halting = 0;
static int callback_enabled = 0;

static SharpVox::SharpVoxSpeaker* g_speaker = nullptr;

struct CallbackInfo {
    JavaVM* jvm = nullptr;
    jobject callbackObject = nullptr;
    jclass callbackClass = nullptr;
    jmethodID callbackMethod = nullptr;
};
static CallbackInfo g_callbackInfo;

static void onBuffer(SharpVox::SharpVoxSpeaker* /*speaker*/, const int16_t* buf, int32_t len, void* /*userdata*/) {
    if (halting) return;

    if (total_size + len <= MAX_BUFFER) {
        std::memcpy(samples + total_size, buf, len * sizeof(short));
        total_size += len;
    }

    if (callback_enabled && g_callbackInfo.jvm && g_callbackInfo.callbackObject) {
        JNIEnv* env = nullptr;
        bool attached = false;
        int status = g_callbackInfo.jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (status == JNI_EDETACHED) {
            if (g_callbackInfo.jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
            attached = true;
        } else if (status != JNI_OK) {
            return;
        }

        jshortArray jarr = env->NewShortArray(len);
        env->SetShortArrayRegion(jarr, 0, len, buf);
        env->CallVoidMethod(g_callbackInfo.callbackObject, g_callbackInfo.callbackMethod, jarr, (jint)len);

        if (attached) {
            g_callbackInfo.jvm->DetachCurrentThread();
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeInit(JNIEnv* /*env*/, jobject /*obj*/) {
    if (!g_speaker) {
        g_speaker = new SharpVox::SharpVoxSpeaker();
    }
    total_size = 0;
    halting = 0;
    callback_enabled = 0;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeReset(JNIEnv* /*env*/, jobject /*obj*/) {
    total_size = 0;
    halting = 0;
}

static int pitch_hz = 0;

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeSetRate(JNIEnv* /*env*/, jobject /*obj*/, jint rate) {
    if (g_speaker) g_speaker->Rate = rate;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeSetPitch(JNIEnv* /*env*/, jobject /*obj*/, jint pitchHz) {
    pitch_hz = pitchHz;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeSetVolume(JNIEnv* /*env*/, jobject /*obj*/, jfloat volume) {
    if (g_speaker) g_speaker->AudioVolume = volume;
}

static std::string pending_voice;
static std::string current_voice = "john";

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeSetVoice(JNIEnv* env, jobject /*obj*/, jstring preset) {
    const char* str = env->GetStringUTFChars(preset, nullptr);
    pending_voice = str;
    current_voice = str;
    env->ReleaseStringUTFChars(preset, str);
}

extern "C" JNIEXPORT jint JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeGetVoicePitchHz(JNIEnv* /*env*/, jobject /*obj*/) {
    SharpVox::VoiceData voice;
    if (SharpVox::VoicePresets::TryGet(current_voice, voice)) {
        return voice.PitchHz;
    }
    return SharpVox::VoiceData::baseline_voice().PitchHz;
}

static void applyPendingVoice() {
    if (!g_speaker || pending_voice.empty()) return;

    SharpVox::VoiceData voice;
    if (SharpVox::VoicePresets::TryGet(pending_voice, voice)) {
        voice.Rate = g_speaker->Rate;
    } else {
        voice = SharpVox::VoiceData::baseline_voice();
        voice.Rate = g_speaker->Rate;
    }
    if (pitch_hz > 0) {
        int32_t hz = pitch_hz;
        if (hz < 40) hz = 40;
        if (hz > 1000) hz = 1000;
        voice.PitchHz = (int16_t)hz;
    }
    g_speaker->ApplyVoiceData(voice);
    pending_voice.clear();
}

extern "C" JNIEXPORT jshortArray JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeSpeak(JNIEnv* env, jobject /*obj*/, jstring text, jboolean enableCallback) {
    if (!g_speaker) return env->NewShortArray(0);

    const char* textStr = env->GetStringUTFChars(text, nullptr);
    std::string text_cpp(textStr);
    env->ReleaseStringUTFChars(text, textStr);

    total_size = 0;
    callback_enabled = enableCallback;

    applyPendingVoice();
    g_speaker->Speak(text_cpp, onBuffer, nullptr);

    jsize length = total_size;
    jshortArray result = env->NewShortArray(length);
    env->SetShortArrayRegion(result, 0, length, samples);
    return result;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeSync(JNIEnv* /*env*/, jobject /*obj*/, jint /*what*/) {
    halting = 1;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_tts_TtsService_SetCallback(JNIEnv* env, jobject obj) {
    env->GetJavaVM(&g_callbackInfo.jvm);
    jclass localClass = env->GetObjectClass(obj);
    g_callbackInfo.callbackClass = (jclass)env->NewGlobalRef(localClass);
    g_callbackInfo.callbackObject = env->NewGlobalRef(obj);
    g_callbackInfo.callbackMethod = env->GetMethodID(
        g_callbackInfo.callbackClass,
        "javaCallback", "([SI)V");
}
