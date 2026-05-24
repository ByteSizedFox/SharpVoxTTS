#nullable enable
namespace SharpTalk {

    public sealed class SynthInputDump {
        public int PhonBuf2InIndex { get; }
        public short[] PhonBuf2 { get; }
        public long[] PhonCtrlBuf2 { get; }
        public short[] DurBuf { get; }
        public short[] UserPitchBuf2 { get; }
        public short[] UserNoteBuf2 { get; }
        public byte[] AspirationBuf2 { get; }
        public byte[] TiltBuf2 { get; }
        public byte[] EffortBuf2 { get; }
        public byte[] VibDepthBuf2 { get; }
        public byte[] VibRateBuf2 { get; }
        public byte[] TremDepthBuf2 { get; }
        public byte[] TremRateBuf2 { get; }

        public uint PitchBufInIndex { get; }
        public short[] PitchBufFreq { get; }
        public short[] PitchBufTime { get; }
        public short[] PitchBufFlags { get; }
        public short[] PitchBufTiltX64 { get; }
        public short[] PitchBufDuration { get; }

        public PitchState Pitch { get; }

        private SynthInputDump(
            int phonBuf2InIndex,
            short[] phonBuf2,
            long[] controls,
            short[] durBuf,
            short[] userPitchBuf2,
            short[] userNoteBuf2,
            byte[] aspirationBuf2,
            byte[] tiltBuf2,
            byte[] effortBuf2,
            byte[] vibDepthBuf2,
            byte[] vibRateBuf2,
            byte[] tremDepthBuf2,
            byte[] tremRateBuf2,
            uint pitchBufInIndex,
            short[] pitchBufFreq,
            short[] pitchBufTime,
            short[] pitchBufFlags,
            short[] pitchBufTiltX64,
            short[] pitchBufDuration,
            PitchState pitch) {
            PhonBuf2InIndex = phonBuf2InIndex;
            PhonBuf2 = phonBuf2;
            PhonCtrlBuf2 = controls;
            DurBuf = durBuf;
            UserPitchBuf2 = userPitchBuf2;
            UserNoteBuf2 = userNoteBuf2;
            AspirationBuf2 = aspirationBuf2;
            TiltBuf2 = tiltBuf2;
            EffortBuf2 = effortBuf2;
            VibDepthBuf2 = vibDepthBuf2;
            VibRateBuf2 = vibRateBuf2;
            TremDepthBuf2 = tremDepthBuf2;
            TremRateBuf2 = tremRateBuf2;
            PitchBufInIndex = pitchBufInIndex;
            PitchBufFreq = pitchBufFreq;
            PitchBufTime = pitchBufTime;
            PitchBufFlags = pitchBufFlags;
            PitchBufTiltX64 = pitchBufTiltX64;
            PitchBufDuration = pitchBufDuration;
            Pitch = pitch;
        }

        internal static SynthInputDump Create(
            int phonBuf2InIndex,
            short[] phonBuf2,
            long[] controls,
            short[] durBuf,
            short[] userPitchBuf2,
            short[] userNoteBuf2,
            byte[] aspirationBuf2,
            byte[] tiltBuf2,
            byte[] effortBuf2,
            byte[] vibDepthBuf2,
            byte[] vibRateBuf2,
            byte[] tremDepthBuf2,
            byte[] tremRateBuf2,
            uint pitchBufInIndex,
            short[] pitchBufFreq,
            short[] pitchBufTime,
            short[] pitchBufFlags,
            short[] pitchBufTiltX64,
            short[] pitchBufDuration,
            PitchState pitch)
        => new SynthInputDump(
            phonBuf2InIndex, phonBuf2, controls, durBuf,
            userPitchBuf2, userNoteBuf2,
            aspirationBuf2, tiltBuf2, effortBuf2,
            vibDepthBuf2, vibRateBuf2, tremDepthBuf2, tremRateBuf2,
            pitchBufInIndex, pitchBufFreq, pitchBufTime, pitchBufFlags,
            pitchBufTiltX64, pitchBufDuration,
            pitch);
    }

    public sealed class PitchState {
        public short NextPitchBufTime { get; set; }
        public short PitchBufOutIndex { get; set; }
        public short CurPitchBufTime { get; set; }
        public short CurPitchBufPitch { get; set; }
        public short CurPitchBufFlags { get; set; }

        public short PhonIndexTarg { get; set; }
        public short PhonIndexCp { get; set; }
        public short TimeIntoPhonTarg { get; set; }
        public short TimeIntoPhonCp { get; set; }
        public short CurPhonDurCc { get; set; }
        public short CurPhonDurCp { get; set; }
        public short PhonDurDelay { get; set; }

        public short UvPhonPitchTarg { get; set; }
        public short PhonPitchOffset { get; set; }
        public short PhonPitchOffset1 { get; set; }

        public short BaseLineOffset { get; set; }
        public short BasePitchOffset { get; set; }
        public short PitchBoundry { get; set; }
        public short LowGainCp { get; set; }

        public short BaselineFallStart { get; set; }
        public short BaselineFallEnd { get; set; }
        public short BaselineStartOffset { get; set; }
        public short BaselineEndOffset { get; set; }

        public long DownRampOffset { get; set; }
        public long DownRampStep { get; set; }
        public long[] RampSteps { get; set; } = new long[256];
        public short CurRamp { get; set; }

        public long VpIntonation { get; set; }
        public long VpPitchRange { get; set; }
        public short VpBaselinePitch { get; set; }

        public long VibratoDepth1 { get; set; }
        public long VibratoDepth2 { get; set; }
        public long VibratoFreq { get; set; }
        public int VibratoPhase1 { get; set; }

        public short Singing { get; set; }
        public short HzGlide { get; set; }
        public short MusicalNoteActive { get; set; }
        public long PortamentoAccum { get; set; }
        public long PortamentoStep { get; set; }
        public short NewPortaTarget { get; set; }
        public short NewSentence { get; set; }
        public short SpeechRate { get; set; }
    }
}  // namespace
