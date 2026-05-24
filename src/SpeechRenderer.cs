#nullable enable
using System;
using System.Collections.Generic;

namespace SharpTalk {

    // Converts phonemes into KlattSynthesizer frames using Klatt (1980) and Klatt & Klatt (1990) models.
    //
    // Parallel control blocks with HEAD and TAIL ramps ensure continuous parameter trajectory blending.
    //
    // Acoustic locus theory (Liberman 1954, Delattre 1955) used for F2 formant transitions.
    //
    // Diphthong and glide nuclei (Lehiste & Peterson 1961) modeled as 4-point formant trajectories.
    //
    // Plosive release voicing timed using Voice-Onset-Time (Lisker & Abramson 1964, Klatt 1975).
    public partial class SpeechRenderer {
        private VoiceData _voice;
        private SynthInputDump _dump = null!;

        private class ControlBlock {
            public short curP_START_Targ;
            public short curP_END_Targ;
            public short prevP_END_Targ;
            public short nextP_START_Targ;
            public int curTarget_TIME;
            public int curTarget_STEP;
            public int curTarget_OFFS;
            public int HEAD_offs;
            public int HEAD_step;
            public int TAIL_offs;
            public int TAIL_step;
            public int TAIL_START_time;
            public int onset_END_TIME;
            public short onset_VAL;
            public int ptrToTargetList; // index into _diphEntries; -1 = no list
        }

        private ControlBlock[] _cb = new ControlBlock[15];
        private short[] _controlData = new short[15];
        private short[] _diphEntries = new short[4096];
        private int _nextDiphEntryIdx;

        // Current-phoneme context
        private int _curPhon, _prevPhon, _nextPhon, _prev2Phon;
        private uint _curPhonFlags, _prevPhonFlags, _nextPhonFlags, _prev2PhonFlags;
        private int _curPhonCtrl, _prevPhonCtrl, _nextPhonCtrl, _prev2PhonCtrl;
        private int _curPhonDur;
        private int _curPhonMaxDur;
        private long _curPhonPctOfMaxDur, _curPhonPctOfMaxDur1, _curPhonPctOfMaxDur2;

        // Shared state written by InitCtrlsForNewPhon and consumed by HeadRules/TailRules
        private int _transLevel, _transTime;
        private int _curBlockIndex;

        private int _durDoneInPhon;
        private int _curPhonBufIndex;
        private bool _startingNewPhon;
        private bool _bigBang = true;

        // Klattsch parameters in 16.16 fixed point, linearly interpolated per frame.
        // Parameterization follows the KLSYN88 source model (Klatt & Klatt 1990).
        private int[] _curKlattsch = new int[7];
        private int[] _klattschStep = new int[7];
        private const int kAspIdx = 0, kTiltIdx = 1, kEffIdx = 2;
        private const int kVibDIdx = 3, kVibRIdx = 4, kTremDIdx = 5, kTremRIdx = 6;

        // Control block indices
        public const int kF1 = 0; public const int kF2 = 1; public const int kF3 = 2;
        public const int kBW1 = 3; public const int kBW2 = 4; public const int kBW3 = 5;
        public const int kFNZ = 6; public const int kAV = 7; public const int kAF = 8;
        public const int kAp2 = 9; public const int kAp3 = 10; public const int kAp4 = 11;
        public const int kAp5 = 12; public const int kAp6 = 13; public const int kAB = 14;
        private const int kNumOfBlocks = 15;

        // Block type constants (match Tables.ControlBlockTypeTable)
        public const int kFreqType = 0; public const int kBWType = 1; public const int kFNZType = 2;
        public const int kSourceAmpType = 3; public const int kResonAmpType = 4;

        // Phoneme numbers, aliased from AudioProcessor for unqualified access within this file
        private const int _IY_ = AudioProcessor._IY_; private const int _ER_ = AudioProcessor._ER_;
        private const int _AY_ = AudioProcessor._AY_; private const int _OY_ = AudioProcessor._OY_;
        private const int _UW_ = AudioProcessor._UW_; private const int _YU_ = AudioProcessor._YU_;
        private const int _SIL_ = AudioProcessor._SIL_; private const int _LX_ = AudioProcessor._LX_;
        private const int _EL_ = AudioProcessor._EL_; private const int _EN_ = AudioProcessor._EN_;
        private const int _W_ = AudioProcessor._W_; private const int _Y_ = AudioProcessor._Y_;
        private const int _R_ = AudioProcessor._R_; private const int _L_ = AudioProcessor._L_;
        private const int _HH_ = AudioProcessor._HH_; private const int _M_ = AudioProcessor._M_;
        private const int _N_ = AudioProcessor._N_; private const int _NG_ = AudioProcessor._NG_;
        private const int _F_ = AudioProcessor._F_; private const int _V_ = AudioProcessor._V_;
        private const int _TH_ = AudioProcessor._TH_; private const int _DH_ = AudioProcessor._DH_;
        private const int _S_ = AudioProcessor._S_; private const int _Z_ = AudioProcessor._Z_;
        private const int _SH_ = AudioProcessor._SH_; private const int _ZH_ = AudioProcessor._ZH_;
        private const int _P_ = AudioProcessor._P_; private const int _B_ = AudioProcessor._B_;
        private const int _T_ = AudioProcessor._T_; private const int _D_ = AudioProcessor._D_;
        private const int _K_ = AudioProcessor._K_; private const int _G_ = AudioProcessor._G_;
        private const int _CH_ = AudioProcessor._CH_; private const int _JH_ = AudioProcessor._JH_;
        private const int _TX_ = AudioProcessor._TX_; private const int _DX_ = AudioProcessor._DX_;
        private const int _QX_ = AudioProcessor._QX_; private const int _DD_ = AudioProcessor._DD_;

        // Phoneme feature flags
        private const uint kVowelF = 1 << 0; private const uint kConsonantF = 1 << 1;
        private const uint kVoicedF = 1 << 2; private const uint kVowel1F = 1 << 3;
        private const uint kSonorantF = 1 << 4; private const uint kSonorant1F = 1 << 5;
        private const uint kNasalF = 1 << 6; private const uint kLiqGlideF = 1 << 7;
        private const uint kSonorConsonF = 1 << 8; private const uint kPlosiveF = 1 << 9;
        private const uint kPlosFricF = 1 << 10; private const uint kObstF = 1 << 11;
        private const uint kStopF = 1 << 12; private const uint kAlveolarF = 1 << 13;
        private const uint kVelar = 1 << 14; private const uint kLabialF = 1 << 15;
        private const uint kDentalF = 1 << 16; private const uint kPalatalF = 1 << 17;
        private const uint kYGlideStartF = 1 << 18; private const uint kYGlideEndF = 1 << 19;
        private const uint kGStopF = 1 << 20; private const uint kFrontF = 1 << 21;
        private const uint kDiphthongF = 1 << 22; private const uint kAffricateF = 1 << 24;
        private const uint kLiqGlide2F = 1 << 25; private const uint kVocLiq = 1 << 26;
        private const uint kFric = 1 << 27;

        // Phoneme control field masks
        private const int kPlosive_Release = 0x4000;
        private const int kPrimOrEmphStress = 0x1400;
        private const int kStressField = 0x1C00;
        private const int kSyllableTypeField = 0x000F;

        private const int kNoValue = -1;
        private const int kMaxBandWidth = 1000;
        private const int C_V_type = 0;
        private const int V_C_type = 1;
        private const int kFrontR = 0; private const int kMiddleR = 1;
        private const int kBackR = 2; private const int kRoundR = 3;
        private const int kConsonantR = 4;
        private const int kStepSizeRes = 3;
        private const int k1pct = 655;
        private const int kFrameTime = 5;
        private const int ReciprocalTableSize = 100;
        private const int kOneHalf = 0x8000;

        // Voice-dependent tables and parameters
        private short[] _envelopeListTable;
        private short[] _locusTable;
        private short[] _voiceAmplitudeVoicingTable;
        private short[] _voiceNoiseAmplitudeTable;
        private int _nasalTargFreq, _nasalBaseFreq, _locusOffset;
        private int _voiceBWgain1, _voiceBWgain2, _voiceBWgain3;
        private float _tractScale;
        private byte _voiceTremDepth;
        private byte _voiceTremRate;
        private int _voiceOnsetHardness;
        private readonly bool _male;

        public SpeechRenderer(VoiceData voice) {
            _voice = voice;
            for (int i = 0; i < _cb.Length; i++) {
                _cb[i] = new ControlBlock();
            }
            _male = voice.VoiceType == 0;
            _envelopeListTable = _male ? Tables.MaleEnvelopeTable : Tables.FemaleEnvelopeTable;
            _locusTable = _male ? Tables.MaleLocusTable : Tables.FemaleLocusTable;
            _voiceAmplitudeVoicingTable = _male ? Tables.MaleAmplitudeVoicingVolumeTable : Tables.FemaleAmplitudeVoicingVolumeTable;
            _voiceNoiseAmplitudeTable = _male ? Tables.MaleNoiseAmplitudeTable : Tables.FemaleNoiseAmplitudeTable;
            _nasalTargFreq = voice.NasalTarg;
            _nasalBaseFreq = voice.NasalBase;
            _locusOffset = voice.Locus;
            _voiceBWgain1 = (voice.BwGain1 << 16) / 100;
            _voiceBWgain2 = (voice.BwGain2 << 16) / 100;
            _voiceBWgain3 = (voice.BwGain3 << 16) / 100;
            _tractScale = voice.TractScale > 0 ? voice.TractScale : 1.0f;
            _voiceTremDepth = (byte)Math.Clamp((int)voice.TremoloDepth, 0, 100);
            _voiceTremRate = (byte)Math.Clamp((int)voice.TremoloRate, 0, 200);
            _voiceOnsetHardness = Math.Clamp((int)voice.OnsetHardness, 0, 100);
            // Seed tremolo state so it is active from the very first frame
            _curKlattsch[kTremDIdx] = _voiceTremDepth << 16;
            _curKlattsch[kTremRIdx] = _voiceTremRate << 16;
        }

        public IEnumerable<Frame> RenderStreaming(SynthInputDump dump) {
            _dump = dump;
            _curPhonBufIndex = 0;
            _durDoneInPhon = 0;
            _startingNewPhon = true;

            // Seed curP_END_Targ from the first phoneme so envelope ramps have a prior target
            if (_bigBang) {
                _bigBang = false;
                SetPhonContext(0);
                for (_curBlockIndex = 0; _curBlockIndex < kNumOfBlocks; _curBlockIndex++) {
                    _cb[_curBlockIndex].curP_END_Targ = (short)GetFirstTarget(0);
                }
            }

            var pitchInterp = new PitchInterpolator(dump);
            int totalFrames = 0;
            for (int i = 0; i < dump.PhonBuf2InIndex; i++) {
                totalFrames += dump.DurBuf[i];
            }

            for (int i = 0; i < totalFrames; i++) {
                if (_durDoneInPhon >= _dump.DurBuf[_curPhonBufIndex]) {
                    _curPhonBufIndex++;
                    _durDoneInPhon = 0;
                    _startingNewPhon = true;
                }
                if (_startingNewPhon) {
                    InitCtrlsForNewPhon();
                    pitchInterp.DoNote(_curPhonBufIndex);
                    _startingNewPhon = false;
                }

                short f0 = pitchInterp.Step();
                InterpolateControls();
                yield return SaveFrame(f0, (byte)_dump.PhonCtrlBuf2[_curPhonBufIndex]);

                _durDoneInPhon++;
            }
        }

        public Frame[] Render(SynthInputDump dump) {
            return new List<Frame>(RenderStreaming(dump)).ToArray();
        }

        private void SetPhonContext(int index) {
            _curPhon = GP(index); _curPhonFlags = PF(_curPhon); _curPhonCtrl = PC(index);
            _nextPhon = GP(index + 1); _nextPhonFlags = PF(_nextPhon); _nextPhonCtrl = PC(index + 1);
            _prevPhon = GP(index - 1); _prevPhonFlags = PF(_prevPhon); _prevPhonCtrl = PC(index - 1);
            _prev2Phon = GP(index - 2); _prev2PhonFlags = PF(_prev2Phon); _prev2PhonCtrl = PC(index - 2);
            _curPhonDur = (index >= 0 && index < _dump.DurBuf.Length) ? _dump.DurBuf[index] : 0;
        }

        // Safe phoneme buffer access; returns SIL for out-of-range indices
        private int GP(int i) {
            if (i >= 0 && i < _dump.PhonBuf2.Length) {
                return _dump.PhonBuf2[i];
            }
            return _SIL_;
        }

        // Phoneme feature flags; returns 0 for out-of-range phoneme indices
        private uint PF(int p) {
            if (p >= 0 && p < Tables.PhonemeFeatureFlags.Length) {
                return Tables.GetFeatureFlags(p);
            }
            return 0;
        }

        // Phoneme control field; returns 0 for out-of-range indices
        private int PC(int i) {
            if (i >= 0 && i < _dump.PhonCtrlBuf2.Length) {
                return (int)_dump.PhonCtrlBuf2[i];
            }
            return 0;
        }

        // One-over-x from reciprocal table for small x, or computed directly for large x
        private int OvX(int x) {
            if (x <= 0) {
                return 0;
            }
            if (x < ReciprocalTableSize) {
                return (int)Tables.ReciprocalTable[x];
            }
            return (int)(65536L / x);
        }

        // Converts 6-bit log-domain amplitude (Klatt 1980) to a linear multiplier via lookup.
        public static short LogToLin(short v) {
            if (v > 63) {
                v = 63;
            }
            if (v < 0) {
                return 0;
            }
            return Tables.LogarithmicToLinearTable[v >> 1];
        }
    }

}  // namespace
