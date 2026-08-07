#ifndef SHARPVOX_SYNTH_DATA_H
#define SHARPVOX_SYNTH_DATA_H

#include <cstdint>
#include <vector>
#include <array>

namespace SharpVox {

    // High-rate intelligibility. Janse et al. (2003) found that at extreme
    // speech rates uniform (linear) time-scaling is more intelligible than
    // the natural non-linear pattern, so these helpers blend the duration model
    // toward linear as the rate climbs. Frac is 0 at/below kStart and 1.0 (65536)
    // at/above kFull; between, the natural incompressible timing is kept.
    namespace RateLin {
        // kStart/kFull overridable via -D at build time; defaults are the shipped values.
#ifndef SVX_LIN_START
#define SVX_LIN_START 150
#endif
#ifndef SVX_LIN_FULL
#define SVX_LIN_FULL 350
#endif
        static constexpr int32_t kNormalRate = 180;  // matches AudioProcessor baseline
        static constexpr int32_t kStart      = SVX_LIN_START;  // wpm: below this, natural timing
        static constexpr int32_t kFull       = SVX_LIN_FULL;  // wpm: at/above this, fully linear

        // Q16 blend fraction in [0, 65536].
        static inline int32_t FracQ16(int32_t rate) {
            if (rate <= kStart) return 0;
            if (rate >= kFull)  return 65536;
            return (int32_t)(((int64_t)(rate - kStart) << 16) / (kFull - kStart));
        }

        // Q16 multiplier for fixed-length events (formant transition ramps, stop
        // bursts) so they shrink toward the linear tempo ratio kNormalRate/rate at
        // high rates instead of keeping their absolute textbook length. 1.0 when
        // the blend is inactive.
        static inline int32_t EventScaleQ16(int32_t rate) {
            int32_t frac = FracQ16(rate);
            if (frac == 0 || rate <= 0) return 65536;
            int64_t ratioQ16 = ((int64_t)kNormalRate << 16) / rate;
            return (int32_t)(65536 + (((ratioQ16 - 65536) * frac) >> 16));
        }

        // Q16 cascade-bandwidth scaling by rate (TGSpeechBox reference). Above normal
        // rate the BW widens: wider BW = shorter resonator time constant = formant
        // identity reached sooner in short frames. Below normal rate the symmetric
        // opposite, the BW narrows, since long frames leave time for a sharper, more
        // defined resonance. Identity exactly at kNormalRate.
#ifndef SVX_MAX_BW_WIDEN_PCT
#define SVX_MAX_BW_WIDEN_PCT 30
#endif
        // Slow-rate narrowing: max percent, and the rate at/below which it is full.
#ifndef SVX_MAX_BW_NARROW_PCT
#define SVX_MAX_BW_NARROW_PCT 20
#endif
#ifndef SVX_BW_NARROW_FLOOR_RATE
#define SVX_BW_NARROW_FLOOR_RATE 90
#endif
        static inline int32_t BwWidenQ16(int32_t rate) {
            if (rate == kNormalRate) return 65536;
            if (rate > kNormalRate) {
                int32_t frac = rate >= kFull ? 65536 :
                    (int32_t)(((int64_t)(rate - kNormalRate) << 16) / (kFull - kNormalRate));
                int64_t maxAddQ16 = ((int64_t)SVX_MAX_BW_WIDEN_PCT << 16) / 100;
                return (int32_t)(65536 + ((maxAddQ16 * frac) >> 16));
            }
            // rate < kNormalRate: narrow toward (1 - maxNarrow/100) at the floor rate.
            int32_t floorRate = SVX_BW_NARROW_FLOOR_RATE;
            int32_t frac = rate <= floorRate ? 65536 :
                (int32_t)(((int64_t)(kNormalRate - rate) << 16) / (kNormalRate - floorRate));
            int64_t maxSubQ16 = ((int64_t)SVX_MAX_BW_NARROW_PCT << 16) / 100;
            return (int32_t)(65536 - ((maxSubQ16 * frac) >> 16));
        }
    }

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

        // Fujisaki intonation model knobs (Q16 neper amplitudes / percent scales)
        int64_t FujisakiPhraseAmpQ16;
        int64_t FujisakiPrimaryAccentAmpQ16;
        int64_t FujisakiHeadAccentAmpQ16;
        int64_t FujisakiSecondaryAccentAmpQ16;
        int64_t FujisakiEmphaticAccentAmpQ16;
        int32_t FujisakiPitchCurveExpPct;
        int32_t FujisakiMonoAccentDurScalePct;
        int64_t FujisakiCompoundStepQ16;
        int32_t FujisakiFinalDropPct;

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

    struct ClausePlan {
        int32_t PhonBufInIndex;
        std::vector<int16_t> PhonBuf;
        std::vector<int64_t> PhonCtrlBuf;
        std::vector<int16_t> DurBuf;
        std::vector<int16_t> UserPitchBuf;
        std::vector<int16_t> UserNoteBuf;
        std::vector<uint8_t> AspirationBuf;
        std::vector<uint8_t> TiltBuf;
        std::vector<uint8_t> EffortBuf;
        std::vector<uint8_t> VibDepthBuf;
        std::vector<uint8_t> VibRateBuf;
        std::vector<uint8_t> TremDepthBuf;
        std::vector<uint8_t> TremRateBuf;

        uint32_t PitchBufInIndex;
        std::vector<int16_t> PitchBufFreq;
        std::vector<int16_t> PitchBufTime;
        std::vector<int16_t> PitchBufFlags;
        std::vector<int16_t> PitchBufTiltX64;
        std::vector<int16_t> PitchBufDuration;

        // Clause-terminating punctuation (_Period_, _Quest_, ...; 0 = none)
        int16_t EndPunctuation = 0;

        PitchState Pitch;

        static ClausePlan Create(
            int32_t phonBufInIndex,
            std::vector<int16_t> phonBuf,
            std::vector<int64_t> controls,
            std::vector<int16_t> durBuf,
            std::vector<int16_t> userPitchBuf,
            std::vector<int16_t> userNoteBuf,
            std::vector<uint8_t> aspirationBuf,
            std::vector<uint8_t> tiltBuf,
            std::vector<uint8_t> effortBuf,
            std::vector<uint8_t> vibDepthBuf,
            std::vector<uint8_t> vibRateBuf,
            std::vector<uint8_t> tremDepthBuf,
            std::vector<uint8_t> tremRateBuf,
            uint32_t pitchBufInIndex,
            std::vector<int16_t> pitchBufFreq,
            std::vector<int16_t> pitchBufTime,
            std::vector<int16_t> pitchBufFlags,
            std::vector<int16_t> pitchBufTiltX64,
            std::vector<int16_t> pitchBufDuration,
            PitchState pitch,
            int16_t endPunctuation) {
            ClausePlan d;
            d.PhonBufInIndex = phonBufInIndex;
            d.PhonBuf = std::move(phonBuf);
            d.PhonCtrlBuf = std::move(controls);
            d.DurBuf = std::move(durBuf);
            d.UserPitchBuf = std::move(userPitchBuf);
            d.UserNoteBuf = std::move(userNoteBuf);
            d.AspirationBuf = std::move(aspirationBuf);
            d.TiltBuf = std::move(tiltBuf);
            d.EffortBuf = std::move(effortBuf);
            d.VibDepthBuf = std::move(vibDepthBuf);
            d.VibRateBuf = std::move(vibRateBuf);
            d.TremDepthBuf = std::move(tremDepthBuf);
            d.TremRateBuf = std::move(tremRateBuf);
            d.PitchBufInIndex = pitchBufInIndex;
            d.PitchBufFreq = std::move(pitchBufFreq);
            d.PitchBufTime = std::move(pitchBufTime);
            d.PitchBufFlags = std::move(pitchBufFlags);
            d.PitchBufTiltX64 = std::move(pitchBufTiltX64);
            d.PitchBufDuration = std::move(pitchBufDuration);
            d.EndPunctuation = endPunctuation;
            d.Pitch = pitch;
            return d;
        }
    };

}  // namespace SharpVox

#endif  // SHARPVOX_SYNTH_DATA_H
