#ifndef SHARPVOX_VOICE_PRESETS_H
#define SHARPVOX_VOICE_PRESETS_H

#include "VoiceData.h"
#include <string>

namespace SharpVox {

class VoicePresets {
public:
    static bool TryGet(const std::string& name, VoiceData& outVoice);
};

} // namespace SharpVox

#endif // SHARPVOX_VOICE_PRESETS_H
