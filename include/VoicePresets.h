#ifndef SHARPVOX_VOICE_PRESETS_H
#define SHARPVOX_VOICE_PRESETS_H

#include "VoiceData.h"
#include <string>

namespace SharpVox {

class VoicePresets {
public:
    static bool TryGet(const std::string& name, VoiceData& outVoice);
    static bool SetParam(VoiceData& voice, const std::string& name, float value);
};

} // namespace SharpVox

#endif // SHARPVOX_VOICE_PRESETS_H
