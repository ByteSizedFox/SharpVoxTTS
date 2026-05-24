#nullable enable
using System;
using System.Threading;
using System.Threading.Tasks;

namespace SharpTalk {

    public enum VoicePreset { Baseline, Whisper, Custom }

    public sealed class SharpTalkSpeaker {
        TtsEngine _engine;

        public int SampleRate { get; set; } = 22050;

        public SharpTalkSpeaker() {
            _engine = new TtsEngine(BuildVoice(), LibraryData.dictionary, LibraryData.SymbolsTable, SampleRate);
        }

        public bool IsSpeaking { get; private set; }

        PhonemeEvent[] _phonemeEvents = Array.Empty<PhonemeEvent>();
        int _nextPhonemeIndex;
        float _pollElapsed;

        string PrepareText(string text) {
            if (!KlattschMode) {
                return text;
            }
            string defs = string.Format(System.Globalization.CultureInfo.InvariantCulture,
                "b{0:F0} r{1:F0} v{2:F1} w{3:F1} h{4:F2} t{5:F2} g{6:F2}",
                KlBaseF0, KlRate, KlVibrato, KlVibRate, KlAsp, KlTilt, KlEffort);
            KlattschMode = false;
            return $"[:klattsch on] {defs} {text} [:klattsch off]";
        }

        public short[] Speak(string text) {
            IsSpeaking = true;
            try {
                var (samples, events) = _engine.SpeakWithEvents(PrepareText(text));
                _phonemeEvents = events;
                _nextPhonemeIndex = 0;
                _pollElapsed = 0f;
                return samples;
            } finally { IsSpeaking = false; }
        }

        public (short[] Samples, PhonemeEvent[] Events) SpeakWithEvents(string text) {
            IsSpeaking = true;
            try {
                var result = _engine.SpeakWithEvents(PrepareText(text));
                _phonemeEvents = result.Item2;
                _nextPhonemeIndex = 0;
                _pollElapsed = 0f;
                return result;
            } finally { IsSpeaking = false; }
        }

        public Task<short[]> SpeakAsync(string text, CancellationToken ct = default) {
            IsSpeaking = true;
            string synText = PrepareText(text);
            return Task.Run(() => {
                try {
                    var (samples, events) = _engine.SpeakWithEvents(synText);
                    _phonemeEvents = events;
                    _nextPhonemeIndex = 0;
                    _pollElapsed = 0f;
                    return samples;
                } finally { IsSpeaking = false; }
            }, ct);
        }

        public void PollAbsolute(float absoluteSeconds) {
            if (OnPhoneme is null || _nextPhonemeIndex >= _phonemeEvents.Length) {
                return;
            }
            _pollElapsed = absoluteSeconds;
            while (_nextPhonemeIndex < _phonemeEvents.Length && _phonemeEvents[_nextPhonemeIndex].TimeSeconds <= _pollElapsed) {
                OnPhoneme.Invoke(_phonemeEvents[_nextPhonemeIndex++]);
            }
        }

        public event Action<PhonemeEvent>? OnPhoneme;
        public PhonemeEvent[] PhonemeEvents => _phonemeEvents;


        public void SetVoice(VoiceData voice) {
            voice.Rate = (short)Rate;
            voice.PitchHz = (short)PitchHz;
            _engine.Voice = voice;
        }

        public void ApplyVoice() {
            _engine = new TtsEngine(BuildVoice(), LibraryData.dictionary, LibraryData.SymbolsTable, SampleRate);
        }

        public int Rate { get; set; } = 200;
        public int PitchHz { get; set; } = 122;
        public float AudioVolume { get; set; } = 1f;

        public bool KlattschMode { get; set; } = false;
        public float KlBaseF0 { get; set; } = 120f;
        public float KlRate { get; set; } = 110f;
        public float KlVibrato { get; set; } = 0f;
        public float KlVibRate { get; set; } = 5f;
        public float KlAsp { get; set; } = 0f;
        public float KlTilt { get; set; } = 0f;
        public float KlEffort { get; set; } = 0.5f;

        bool _applyingPreset;
        VoicePreset _preset = VoicePreset.Baseline;

        // yep, it's a preset
        public VoicePreset Preset {
            get => _preset;
            set {
                _preset = value;
                if (value == VoicePreset.Custom) {
                    return;
                }
                _applyingPreset = true;
                var v = value == VoicePreset.Whisper ? VoiceData.WhisperVoice : VoiceData.BaselineVoice;
                Female = v.VoiceType == 1;
                TractScale = v.TractScale;
                F5Freq = v.F5Freq;
                F5BW = v.F5BW;
                VoicingGain = v.VGain;
                AspirationGain = v.AGain;
                AspirationCycle = v.ACycle;
                TremoloDepth = v.TremoloDepth;
                TremoloRate = v.TremoloRate;
                Jitter = v.Jitter;
                Shimmer = v.Shimmer;
                Diplophonia = v.Diplophonia;
                FryAmount = v.FryAmount;
                SubglottalAmt = v.SubglottalAmt;
                BreathAmt = v.BreathAmt;
                OpenQuotient = v.OpenQuotient;
                OQStressLink = v.OQStressLink;
                OQF0Link = v.OQF0Link;
                OnsetHardness = v.OnsetHardness;
                PitchOffsetHz = v.PitchOffsetHz;
                LarynxOffset = v.LarynxOffset;
                PharyngealAmt = v.PharyngealAmt;
                LipRounding = v.LipRounding;
                F4Freq = v.F4Freq;
                F4BW = v.F4BW;
                F4pFreq = v.F4pFreq;
                F4pBW = v.F4pBW;
                F5pFreq = v.F5pFreq;
                F5pBW = v.F5pBW;
                F6pFreq = v.F6pFreq;
                F6pBW = v.F6pBW;
                BwGain1 = v.BwGain1;
                BwGain2 = v.BwGain2;
                BwGain3 = v.BwGain3;
                NasalBase = v.NasalBase;
                NasalTarg = v.NasalTarg;
                NasalBW = v.NasalBW;
                NGain = v.NGain;
                PitchRange = v.PitchRange;
                StressGain = v.StressGain;
                Intonation = v.Intonation;
                RiseAmt = v.RiseAmt;
                FallAmt = v.FallAmt;
                BaselineFall = v.BaselineFall;
                UptalkAmt = v.UptalkAmt;
                StressEarly = v.StressEarly;
                BreakStrength = v.BreakStrength;
                EmphasisBoost = v.EmphasisBoost;
                VocalConfidence = v.VocalConfidence;
                _applyingPreset = false;
            }
        }

        void MarkCustom() {
            if (!_applyingPreset) {
                _preset = VoicePreset.Custom;
            }
        }

        // Voice Definition
        bool _female;
        public bool Female { get => _female; set { _female = value; MarkCustom(); } }
        int _voicingGain = 100;
        public int VoicingGain { get => _voicingGain; set { _voicingGain = value; MarkCustom(); } }
        int _aspirationGain = 0;
        public int AspirationGain { get => _aspirationGain; set { _aspirationGain = value; MarkCustom(); } }
        int _aspirationCycle = 192;
        public int AspirationCycle { get => _aspirationCycle; set { _aspirationCycle = value; MarkCustom(); } }
        int _tremoloDepth = 0;
        public int TremoloDepth { get => _tremoloDepth; set { _tremoloDepth = value; MarkCustom(); } }
        int _tremoloRate = 0;
        public int TremoloRate { get => _tremoloRate; set { _tremoloRate = value; MarkCustom(); } }

        // Glottal source
        int _jitter = 0; public int Jitter { get => _jitter; set { _jitter = value; MarkCustom(); } }
        int _shimmer = 0; public int Shimmer { get => _shimmer; set { _shimmer = value; MarkCustom(); } }
        int _diplophonia = 0; public int Diplophonia { get => _diplophonia; set { _diplophonia = value; MarkCustom(); } }
        int _fryAmount = 0; public int FryAmount { get => _fryAmount; set { _fryAmount = value; MarkCustom(); } }
        int _subglottalAmt = 0; public int SubglottalAmt { get => _subglottalAmt; set { _subglottalAmt = value; MarkCustom(); } }
        int _breathAmt = 0; public int BreathAmt { get => _breathAmt; set { _breathAmt = value; MarkCustom(); } }
        int _openQuotient = 50; public int OpenQuotient { get => _openQuotient; set { _openQuotient = value; MarkCustom(); } }
        int _oqStressLink = 0; public int OQStressLink { get => _oqStressLink; set { _oqStressLink = value; MarkCustom(); } }
        int _oqF0Link = 0; public int OQF0Link { get => _oqF0Link; set { _oqF0Link = value; MarkCustom(); } }
        int _onsetHardness = 50; public int OnsetHardness { get => _onsetHardness; set { _onsetHardness = value; MarkCustom(); } }

        // Tract articulation
        int _pitchOffsetHz = 0; public int PitchOffsetHz { get => _pitchOffsetHz; set { _pitchOffsetHz = value; MarkCustom(); } }
        int _larynxOffset = 0; public int LarynxOffset { get => _larynxOffset; set { _larynxOffset = value; MarkCustom(); } }
        int _pharyngealAmt = 0; public int PharyngealAmt { get => _pharyngealAmt; set { _pharyngealAmt = value; MarkCustom(); } }
        int _lipRounding = 0; public int LipRounding { get => _lipRounding; set { _lipRounding = value; MarkCustom(); } }

        float _tractScale = 1.0f;
        public float TractScale { get => _tractScale; set { _tractScale = value; MarkCustom(); } }

        // Formants
        int _f5Freq = 4500; public int F5Freq { get => _f5Freq; set { _f5Freq = value; MarkCustom(); } }
        int _f5BW = 250; public int F5BW { get => _f5BW; set { _f5BW = value; MarkCustom(); } }
        int _f4Freq = 3000; public int F4Freq { get => _f4Freq; set { _f4Freq = value; MarkCustom(); } }
        int _f4BW = 200; public int F4BW { get => _f4BW; set { _f4BW = value; MarkCustom(); } }
        int _f4pFreq = 3600; public int F4pFreq { get => _f4pFreq; set { _f4pFreq = value; MarkCustom(); } }
        int _f4pBW = 150; public int F4pBW { get => _f4pBW; set { _f4pBW = value; MarkCustom(); } }
        int _f5pFreq = 3750; public int F5pFreq { get => _f5pFreq; set { _f5pFreq = value; MarkCustom(); } }
        int _f5pBW = 100; public int F5pBW { get => _f5pBW; set { _f5pBW = value; MarkCustom(); } }
        int _f6pFreq = 4500; public int F6pFreq { get => _f6pFreq; set { _f6pFreq = value; MarkCustom(); } }
        int _f6pBW = 150; public int F6pBW { get => _f6pBW; set { _f6pBW = value; MarkCustom(); } }
        int _bwGain1 = 150; public int BwGain1 { get => _bwGain1; set { _bwGain1 = value; MarkCustom(); } }
        int _bwGain2 = 100; public int BwGain2 { get => _bwGain2; set { _bwGain2 = value; MarkCustom(); } }
        int _bwGain3 = 100; public int BwGain3 { get => _bwGain3; set { _bwGain3 = value; MarkCustom(); } }

        // Nasal
        int _nasalBase = 330; public int NasalBase { get => _nasalBase; set { _nasalBase = value; MarkCustom(); } }
        int _nasalTarg = 400; public int NasalTarg { get => _nasalTarg; set { _nasalTarg = value; MarkCustom(); } }
        int _nasalBW = 60; public int NasalBW { get => _nasalBW; set { _nasalBW = value; MarkCustom(); } }
        int _nGain = 100; public int NGain { get => _nGain; set { _nGain = value; MarkCustom(); } }

        // Intonation
        int _pitchRange = 100; public int PitchRange { get => _pitchRange; set { _pitchRange = value; MarkCustom(); } }
        int _stressGain = 60; public int StressGain { get => _stressGain; set { _stressGain = value; MarkCustom(); } }
        int _intonation = 100; public int Intonation { get => _intonation; set { _intonation = value; MarkCustom(); } }
        int _riseAmt = 29; public int RiseAmt { get => _riseAmt; set { _riseAmt = value; MarkCustom(); } }
        int _fallAmt = -29; public int FallAmt { get => _fallAmt; set { _fallAmt = value; MarkCustom(); } }
        int _baselineFall = 51; public int BaselineFall { get => _baselineFall; set { _baselineFall = value; MarkCustom(); } }
        int _uptalkAmt = 0; public int UptalkAmt { get => _uptalkAmt; set { _uptalkAmt = value; MarkCustom(); } }
        int _stressEarly = 0; public int StressEarly { get => _stressEarly; set { _stressEarly = value; MarkCustom(); } }
        int _breakStrength = 50; public int BreakStrength { get => _breakStrength; set { _breakStrength = value; MarkCustom(); } }
        int _emphasisBoost = 0; public int EmphasisBoost { get => _emphasisBoost; set { _emphasisBoost = value; MarkCustom(); } }
        int _vocalConfidence = 0; public int VocalConfidence { get => _vocalConfidence; set { _vocalConfidence = value; MarkCustom(); } }

        public float OutputVolume { get; set; } = 1.0f;

        // yep, make a new one
        VoiceData BuildVoice() {
            VoiceData v = _preset switch {
                VoicePreset.Whisper => VoiceData.WhisperVoice,
                VoicePreset.Custom => new VoiceData {
                    VGain = (short)VoicingGain,
                    AGain = (short)AspirationGain,
                    ACycle = (short)AspirationCycle,
                    TremoloDepth = (short)TremoloDepth,
                    TremoloRate = (short)TremoloRate,
                    Jitter = (short)Jitter,
                    Shimmer = (short)Shimmer,
                    Diplophonia = (short)Diplophonia,
                    FryAmount = (short)FryAmount,
                    SubglottalAmt = (short)SubglottalAmt,
                    BreathAmt = (short)BreathAmt,
                    OpenQuotient = (short)OpenQuotient,
                    OQStressLink = (short)OQStressLink,
                    OQF0Link = (short)OQF0Link,
                    OnsetHardness = (short)OnsetHardness,
                    PitchOffsetHz = (short)PitchOffsetHz,
                    LarynxOffset = (short)LarynxOffset,
                    PharyngealAmt = (short)PharyngealAmt,
                    LipRounding = (short)LipRounding,
                    TractScale = TractScale,
                    F5Freq = (short)F5Freq,
                    F5BW = (short)F5BW,
                    F4Freq = (short)F4Freq,
                    F4BW = (short)F4BW,
                    F4pFreq = (short)F4pFreq,
                    F4pBW = (short)F4pBW,
                    F5pFreq = (short)F5pFreq,
                    F5pBW = (short)F5pBW,
                    F6pFreq = (short)F6pFreq,
                    F6pBW = (short)F6pBW,
                    BwGain1 = (short)BwGain1,
                    BwGain2 = (short)BwGain2,
                    BwGain3 = (short)BwGain3,
                    NasalBase = (short)NasalBase,
                    NasalTarg = (short)NasalTarg,
                    NasalBW = (short)NasalBW,
                    NGain = (short)NGain,
                    PitchRange = (short)PitchRange,
                    StressGain = (short)StressGain,
                    Intonation = (short)Intonation,
                    RiseAmt = (short)RiseAmt,
                    FallAmt = (short)FallAmt,
                    BaselineFall = (short)BaselineFall,
                    UptalkAmt = (short)UptalkAmt,
                    StressEarly = (short)StressEarly,
                    BreakStrength = (short)BreakStrength,
                    EmphasisBoost = (short)EmphasisBoost,
                    VocalConfidence = (short)VocalConfidence,
                },
                _ => VoiceData.BaselineVoice,
            };
            v.Rate = (short)Rate;
            v.PitchHz = (short)PitchHz;
            v.TractScale = TractScale;
            v.VoiceType = (short)(Female ? 1 : 0);
            return v;
        }

        public void ApplyVoiceData(VoiceData v) {
            _engine = new TtsEngine(v, LibraryData.dictionary, LibraryData.SymbolsTable, SampleRate);
        }
    }

}  // namespace
