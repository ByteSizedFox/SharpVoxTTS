#ifndef SHARPTALK_SYNTH_DATA_H
#define SHARPTALK_SYNTH_DATA_H

#include <cstdint>
#include <vector>
#include <array>

namespace SharpTalk {

    struct PitchState {
        int16_t NextPitchBufTime;
        int16_t PitchBufOutIndex;
        int16_t CurPitchBufTime;
        int16_t CurPitchBufPitch;
        int16_t CurPitchBufFlags;

        int16_t PhonIndexTarg;
        int16_t PhonIndexCp;
        int16_t TimeIntoPhonTarg;
        int16_t TimeIntoPhonCp;
        int16_t CurPhonDurCc;
        int16_t CurPhonDurCp;
        int16_t PhonDurDelay;

        int16_t UvPhonPitchTarg;
        int16_t PhonPitchOffset;
        int16_t PhonPitchOffset1;

        int16_t BaseLineOffset;
        int16_t BasePitchOffset;
        int16_t PitchBoundry;
        int16_t LowGainCp;

        int16_t BaselineFallStart;
        int16_t BaselineFallEnd;
        int16_t BaselineStartOffset;
        int16_t BaselineEndOffset;

        int64_t DownRampOffset;
        int64_t DownRampStep;
        std::array<int64_t, 256> RampSteps = {};
        int16_t CurRamp;

        int64_t VpIntonation;
        int64_t VpPitchRange;
        int16_t VpBaselinePitch;

        int64_t VibratoDepth1;
        int64_t VibratoDepth2;
        int64_t VibratoFreq;
        int32_t VibratoPhase1;

        int16_t Singing;
        int16_t HzGlide;
        int16_t MusicalNoteActive;
        int64_t PortamentoAccum;
        int64_t PortamentoStep;
        int16_t NewPortaTarget;
        int16_t NewSentence;
        int16_t SpeechRate;
    };

    struct SynthInputDump {
        int32_t PhonBuf2InIndex;
        std::vector<int16_t> PhonBuf2;
        std::vector<int64_t> PhonCtrlBuf2;
        std::vector<int16_t> DurBuf;
        std::vector<int16_t> UserPitchBuf2;
        std::vector<int16_t> UserNoteBuf2;
        std::vector<uint8_t> AspirationBuf2;
        std::vector<uint8_t> TiltBuf2;
        std::vector<uint8_t> EffortBuf2;
        std::vector<uint8_t> VibDepthBuf2;
        std::vector<uint8_t> VibRateBuf2;
        std::vector<uint8_t> TremDepthBuf2;
        std::vector<uint8_t> TremRateBuf2;

        uint32_t PitchBufInIndex;
        std::vector<int16_t> PitchBufFreq;
        std::vector<int16_t> PitchBufTime;
        std::vector<int16_t> PitchBufFlags;
        std::vector<int16_t> PitchBufTiltX64;
        std::vector<int16_t> PitchBufDuration;

        PitchState Pitch;

        static SynthInputDump Create(
            int32_t phonBuf2InIndex,
            std::vector<int16_t> phonBuf2,
            std::vector<int64_t> controls,
            std::vector<int16_t> durBuf,
            std::vector<int16_t> userPitchBuf2,
            std::vector<int16_t> userNoteBuf2,
            std::vector<uint8_t> aspirationBuf2,
            std::vector<uint8_t> tiltBuf2,
            std::vector<uint8_t> effortBuf2,
            std::vector<uint8_t> vibDepthBuf2,
            std::vector<uint8_t> vibRateBuf2,
            std::vector<uint8_t> tremDepthBuf2,
            std::vector<uint8_t> tremRateBuf2,
            uint32_t pitchBufInIndex,
            std::vector<int16_t> pitchBufFreq,
            std::vector<int16_t> pitchBufTime,
            std::vector<int16_t> pitchBufFlags,
            std::vector<int16_t> pitchBufTiltX64,
            std::vector<int16_t> pitchBufDuration,
            PitchState pitch) {
            SynthInputDump d;
            d.PhonBuf2InIndex = phonBuf2InIndex;
            d.PhonBuf2 = std::move(phonBuf2);
            d.PhonCtrlBuf2 = std::move(controls);
            d.DurBuf = std::move(durBuf);
            d.UserPitchBuf2 = std::move(userPitchBuf2);
            d.UserNoteBuf2 = std::move(userNoteBuf2);
            d.AspirationBuf2 = std::move(aspirationBuf2);
            d.TiltBuf2 = std::move(tiltBuf2);
            d.EffortBuf2 = std::move(effortBuf2);
            d.VibDepthBuf2 = std::move(vibDepthBuf2);
            d.VibRateBuf2 = std::move(vibRateBuf2);
            d.TremDepthBuf2 = std::move(tremDepthBuf2);
            d.TremRateBuf2 = std::move(tremRateBuf2);
            d.PitchBufInIndex = pitchBufInIndex;
            d.PitchBufFreq = std::move(pitchBufFreq);
            d.PitchBufTime = std::move(pitchBufTime);
            d.PitchBufFlags = std::move(pitchBufFlags);
            d.PitchBufTiltX64 = std::move(pitchBufTiltX64);
            d.PitchBufDuration = std::move(pitchBufDuration);
            d.Pitch = pitch;
            return d;
        }
    };

}  // namespace SharpTalk

#endif  // SHARPTALK_SYNTH_DATA_H
