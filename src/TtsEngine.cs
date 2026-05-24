#nullable enable
using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using static SharpTalk.AudioProcessor;

namespace SharpTalk {

    public readonly struct PhonemeEvent {
        public readonly short Phoneme;
        public readonly float TimeSeconds;
        public readonly bool IsWordStart;
        public PhonemeEvent(short phoneme, float timeSeconds, bool isWordStart = false) {
            Phoneme = phoneme;
            TimeSeconds = timeSeconds;
            IsWordStart = isWordStart;
        }
    }

    // Top-level TTS API. Converts text to audio via the full pipeline:
    //   Phonemizer (text -> PhonemeToken[]) -> AudioProcessor (phoneme processing) ->
    //   SpeechRenderer (formant targets) -> KlattSynthesizer (PCM samples).
    //
    // The streaming async path pre-computes all SynthInputDumps upfront so latency to
    // first audio is bounded by the front-end processing time, not the synthesis time.
    // Each sentence or clause gets its own AudioProcessor.Process() call so pitch and
    // duration resets happen at natural boundaries.
    public sealed class TtsEngine {
        public const int DefaultSampleRate = 22050;
        public static IEnumerable<int> SupportedSampleRates => KlattSynthesizer.SupportedSampleRates;
        public int SampleRate { get; private set; }

        private readonly Phonemizer _fe;
        private VoiceData _voice;
        private AudioProcessor _be = null!;
        private SpeechRenderer _renderer = null!;
        private KlattSynthesizer _synth = null!;

#if !SANDBOX
        public TtsEngine() : this(VoiceData.BaselineVoice, DefaultSampleRate) { }

        public TtsEngine(VoiceData voice, int sampleRate = DefaultSampleRate)
        {
            _voice = voice;
            SampleRate = sampleRate;
            _fe = new Phonemizer(LibraryData.dictionary, LibraryData.SymbolsTable);
            RebuildPipeline();
        }
#endif

        public TtsEngine(byte[] dictData, System.Collections.Generic.IReadOnlyDictionary<string, byte[]> symbolsTable, int sampleRate = DefaultSampleRate)
            : this(VoiceData.BaselineVoice, dictData, symbolsTable, sampleRate) { }

        public TtsEngine(VoiceData voice, byte[] dictData, System.Collections.Generic.IReadOnlyDictionary<string, byte[]> symbolsTable, int sampleRate = DefaultSampleRate) {
            _voice = voice;
            SampleRate = sampleRate;
            _fe = new Phonemizer(dictData, symbolsTable);
            RebuildPipeline();
        }

        public VoiceData Voice {
            get => _voice;
            set { _voice = value; RebuildPipeline(); }
        }

        public (int dict, int morph, int lts) LookupStats
            => (_fe.StatDict, _fe.StatMorph, _fe.StatLts);
        public void ResetLookupStats() => _fe.ResetStats();
        public DictReader Dict => _fe.Dict;

        public string[] PhonemizeWord(string word)
        {
            var result = new List<string>();
            foreach (var (tokens, _) in _fe.TextToSentenceTokens(word))
                foreach (var tok in tokens)
                {
                    if (tok.Phon == _SIL_) continue;
                    if (PhonemeNamesTable.TryGetValue(tok.Phon, out var name))
                        result.Add(name.ToLowerInvariant());
                }
            return result.ToArray();
        }

        public void ApplyVoice() => RebuildPipeline();

        public short[] Speak(string text) {
            var samples = new List<short>();
            Speak(text, buf => samples.AddRange(buf));
            return samples.ToArray();
        }

        public readonly struct PitchFrameRecord {
            public readonly string Phoneme;
            public readonly int FrameInPhon;
            public readonly int F0;
            public readonly int TiltExcursion;
            public readonly int TiltSmooth;
            public readonly int TiltHeld;
            public readonly int TiltPhase;
            public readonly int BaselineOffset;
            public readonly int TotalOffset;
            public PitchFrameRecord(string phoneme, int frameInPhon, int f0,
                int tiltExcursion, int tiltSmooth, int tiltHeld, int tiltPhase,
                int baselineOffset, int totalOffset) {
                Phoneme = phoneme; FrameInPhon = frameInPhon; F0 = f0;
                TiltExcursion = tiltExcursion; TiltSmooth = tiltSmooth;
                TiltHeld = tiltHeld; TiltPhase = tiltPhase;
                BaselineOffset = baselineOffset; TotalOffset = totalOffset;
            }
        }

        // Returns one record per synthesis frame (5 ms each) with pitch and tilt diagnostics.
        public List<PitchFrameRecord> DumpPitchFrames(string text) {
            var records = new List<PitchFrameRecord>();
            foreach (var seg in EmbeddedCmd.ParseSegments(text)) {
                if (seg.IsCommand) {
                    continue;
                }

                PhonemeToken[] tokens;
                short endPunct = 0;

                if (seg.IsKlattsch) {
                    tokens = KlattschParser.CompileToTokens(KlattschParser.Tokenize(seg.KlattschText!)).ToArray();
                } else if (seg.IsSinging) {
                    tokens = seg.Singing!.ToArray();
                } else {
                    // Just take the first sentence for now if multi-sentence
                    tokens = Array.Empty<PhonemeToken>();
                    foreach (var (t, ep) in _fe.TextToSentenceTokens(seg.PlainText!)) {
                        tokens = t;
                        endPunct = ep;
                        break;
                    }
                    if (tokens.Length == 0) continue;
                }

                var dump = _be.Process(tokens, endPunct);
                var pi = new PitchInterpolator(dump);

                int phonIdx = 0, frameInPhon = 0;
                int totalFrames = 0;
                for (int i = 0; i < dump.PhonBuf2InIndex; i++) {
                    totalFrames += dump.DurBuf[i];
                }

                for (int f = 0; f < totalFrames; f++) {
                    if (frameInPhon == 0) {
                        pi.DoNote(phonIdx);
                    }
                    pi.Step();
                    short p = dump.PhonBuf2[phonIdx];
                    string name = AudioProcessor.PhonemeNamesTable.TryGetValue(p, out var n) ? n : "?";
                    records.Add(new PitchFrameRecord(name, frameInPhon, pi.DbgF0,
                        pi.DbgTiltExcursion, pi.DbgTiltSmooth, pi.DbgTiltHeld, pi.DbgTiltPhase,
                        pi.DbgBaselineOffset, pi.DbgTotalOffset));
                    frameInPhon++;
                    if (frameInPhon >= dump.DurBuf[phonIdx]) {
                        phonIdx++;
                        frameInPhon = 0;
                    }
                }
            }
            return records;
        }

        public void Speak(string text, Action<short[]> onBuffer) {
            EmbeddedCmd.KlattschMode = false;
            foreach (var seg in EmbeddedCmd.ParseSegments(text)) {
                if (seg.IsCommand) {
                    ApplyCommand(seg.Cmd!.Value);
                    continue;
                }
                if (seg.IsKlattsch) {
                    ProcessKlattsch(seg.KlattschText!, onBuffer);
                    continue;
                }
                if (seg.IsSinging) {
                    ProcessSentence(seg.Singing!.ToArray(), 0, onBuffer, null, ref _dummy);
                    continue;
                }
                foreach (var (tokens, endPunct) in _fe.TextToSentenceTokens(seg.PlainText!)) {
                    ProcessSentence(tokens, endPunct, onBuffer, null, ref _dummy);
                }
            }
        }

        /// Like Speak, but also returns a timeline of phoneme events with start times
        /// in seconds relative to the start of the returned audio.
        public (short[] audio, PhonemeEvent[] events) SpeakWithEvents(string text) {
            var samples = new List<short>();
            var events = new List<PhonemeEvent>();
            int sampleOffset = 0;
            EmbeddedCmd.KlattschMode = false;
            foreach (var seg in EmbeddedCmd.ParseSegments(text)) {
                if (seg.IsCommand) {
                    ApplyCommand(seg.Cmd!.Value);
                    continue;
                }
                if (seg.IsKlattsch) {
                    var klattTokens = KlattschParser.CompileToTokens(KlattschParser.Tokenize(seg.KlattschText!));
                    if (klattTokens.Count > 0) {
                        var dump = _be.Process(klattTokens.ToArray(), 0);
                        int frameOff = 0;
                        for (int i = 0; i < dump.PhonBuf2InIndex; i++) {
                            short phon = dump.PhonBuf2[i];
                            bool emitSil = phon == _SIL_ && (i == 0 || dump.PhonBuf2[i - 1] != _SIL_);
                            if (phon != _SIL_ || emitSil) {
                                events.Add(new PhonemeEvent(phon,
                                    (float)(sampleOffset + frameOff * _synth.SampFrameLen) / SampleRate));
                            }
                            frameOff += dump.DurBuf[i];
                        }
                        var audio = ProcessSentenceToBuffer(klattTokens.ToArray(), 0);
                        samples.AddRange(audio);
                        sampleOffset += audio.Length;
                    }
                    continue;
                }
                if (seg.IsSinging) {
                    ProcessSentence(seg.Singing!.ToArray(), 0, buf => samples.AddRange(buf), events, ref sampleOffset);
                    continue;
                }
                foreach (var (tokens, endPunct) in _fe.TextToSentenceTokens(seg.PlainText!)) {
                    ProcessSentence(tokens, endPunct, buf => samples.AddRange(buf), events, ref sampleOffset);
                }
            }
            return (samples.ToArray(), events.ToArray());
        }

        // Internal helpers

        static int _dummy;

        public async Task SpeakAsync(string text, Func<short[], Task> onBuffer, System.Threading.CancellationToken ct = default) {
            EmbeddedCmd.KlattschMode = false;
            foreach (var seg in EmbeddedCmd.ParseSegments(text)) {
                ct.ThrowIfCancellationRequested();
                if (seg.IsCommand) {
                    ApplyCommand(seg.Cmd!.Value);
                    continue;
                }
                if (seg.IsKlattsch) {
                    var tokens = KlattschParser.CompileToTokens(KlattschParser.Tokenize(seg.KlattschText!));
                    if (tokens.Count > 0) {
                        await ProcessSentenceStreaming(tokens.ToArray(), 0, onBuffer, ct);
                    }
                    continue;
                }
                if (seg.IsSinging) {
                    await ProcessSentenceStreaming(seg.Singing!.ToArray(), 0, onBuffer, ct);
                    continue;
                }
                foreach (var (tokens, endPunct) in _fe.TextToSentenceTokens(seg.PlainText!)) {
                    ct.ThrowIfCancellationRequested();
                    await ProcessSentenceStreaming(tokens, endPunct, onBuffer, ct);
                }
            }
        }

        private void ProcessKlattsch(string text, Action<short[]> onBuffer) {
            var tokens = KlattschParser.CompileToTokens(KlattschParser.Tokenize(text));
            if (tokens.Count > 0) {
                ProcessSentence(tokens.ToArray(), 0, onBuffer, null, ref _dummy);
            }
        }

        private async Task ProcessSentenceStreaming(PhonemeToken[] tokens, short endPunct, Func<short[], Task> onBuffer, System.Threading.CancellationToken ct) {
            var dump = _be.Process(tokens, endPunct);
            await ProcessSentenceStreamingFromDump(dump, onBuffer, ct);
        }

        private async Task ProcessSentenceStreamingFromDump(SynthInputDump dump, Func<short[], Task> onBuffer, System.Threading.CancellationToken ct) {
            const int framesPerChunk = 10;
            var audioChunk = new short[framesPerChunk * _synth.SampFrameLen];
            int frameInChunk = 0;

            foreach (var frame in _renderer.RenderStreaming(dump)) {
                ct.ThrowIfCancellationRequested();

                _synth.SynthesizeFrame(frame, audioChunk, frameInChunk * _synth.SampFrameLen);
                frameInChunk++;

                if (frameInChunk >= framesPerChunk) {
                    await onBuffer(audioChunk);
                    audioChunk = new short[framesPerChunk * _synth.SampFrameLen];
                    frameInChunk = 0;
                    await Task.Yield();
                }
            }

            if (frameInChunk > 0) {
                ct.ThrowIfCancellationRequested();
                var finalChunk = new short[frameInChunk * _synth.SampFrameLen];
                Array.Copy(audioChunk, finalChunk, finalChunk.Length);
                await onBuffer(finalChunk);
            }
        }

        // Phase 1, fast (_be.Process per sentence) -> collect events + dumps.
        // Calls onEventsReady before any audio is rendered so the UI can set up
        // tracking while the first frame hasn't been synthesized yet.
        // Phase 2, stream formant frames from pre-computed dumps.
        public async Task SpeakAsyncWithEvents(
            string text,
            Func<short[], Task> onBuffer,
            Func<List<PhonemeEvent>, Task> onEventsReady,
            System.Threading.CancellationToken ct = default) {
            var events = new List<PhonemeEvent>();
            var workItems = new List<(SynthInputDump? dump, PhonemeToken[]? klattTokens)>();
            int sampleOffset = 0;
            EmbeddedCmd.KlattschMode = false;

            foreach (var seg in EmbeddedCmd.ParseSegments(text)) {
                ct.ThrowIfCancellationRequested();
                if (seg.IsCommand) {
                    ApplyCommand(seg.Cmd!.Value);
                    continue;
                }

                if (seg.IsKlattsch) {
                    var tokens = KlattschParser.CompileToTokens(KlattschParser.Tokenize(seg.KlattschText!));
                    if (tokens.Count == 0) {
                        continue;
                    }
                    var dump = _be.Process(tokens.ToArray(), 0);
                    int frameOff = 0;
                    for (int i = 0; i < dump.PhonBuf2InIndex; i++) {
                        short phon = dump.PhonBuf2[i];
                        bool emitSil = phon == _SIL_ && (i == 0 || dump.PhonBuf2[i - 1] != _SIL_);
                        if (phon != _SIL_ || emitSil) {
                            events.Add(new PhonemeEvent(phon,
                                (float)(sampleOffset + frameOff * _synth.SampFrameLen) / SampleRate));
                        }
                        frameOff += dump.DurBuf[i];
                    }
                    sampleOffset += frameOff * _synth.SampFrameLen;
                    workItems.Add((dump, null));
                    continue;
                }

                if (seg.IsSinging) {
                    var dump = _be.Process(seg.Singing!.ToArray(), 0);
                    int frameOff = 0;
                    for (int i = 0; i < dump.PhonBuf2InIndex; i++) {
                        short phon = dump.PhonBuf2[i];
                        bool emitSil = phon == _SIL_ && (i == 0 || dump.PhonBuf2[i - 1] != _SIL_);
                        if (phon != _SIL_ || emitSil) {
                            events.Add(new PhonemeEvent(phon,
                                (float)(sampleOffset + frameOff * _synth.SampFrameLen) / SampleRate));
                        }
                        frameOff += dump.DurBuf[i];
                    }
                    sampleOffset += frameOff * _synth.SampFrameLen;
                    workItems.Add((dump, null));
                    continue;
                }

                foreach (var (tokens, endPunct) in _fe.TextToSentenceTokens(seg.PlainText!)) {
                    var dump = _be.Process(tokens, endPunct);
                    int frameOff = 0;
                    for (int i = 0; i < dump.PhonBuf2InIndex; i++) {
                        short phon = dump.PhonBuf2[i];
                        bool emitSil = phon == _SIL_ && (i == 0 || dump.PhonBuf2[i - 1] != _SIL_);
                        if (phon != _SIL_ || emitSil) {
                            events.Add(new PhonemeEvent(phon,
                                (float)(sampleOffset + frameOff * _synth.SampFrameLen) / SampleRate,
                                phon != _SIL_ && (dump.PhonCtrlBuf2[i] & kWord_Start) != 0));
                        }
                        frameOff += dump.DurBuf[i];
                    }
                    sampleOffset += frameOff * _synth.SampFrameLen;
                    workItems.Add((dump, null));
                }
            }

            await onEventsReady(events);

            foreach (var (dump, _) in workItems) {
                ct.ThrowIfCancellationRequested();
                await ProcessSentenceStreamingFromDump(dump!, onBuffer, ct);
            }
        }

        private short[] ProcessSentenceToBuffer(PhonemeToken[] tokens, short endPunct) {
            var dump = _be.Process(tokens, endPunct);
            var audio = new List<short>();
            foreach (var frame in _renderer.RenderStreaming(dump)) {
                var frameAudio = new short[_synth.SampFrameLen];
                _synth.SynthesizeFrame(frame, frameAudio, 0);
                audio.AddRange(frameAudio);
            }
            return audio.ToArray();
        }

        void ProcessSentence(PhonemeToken[] tokens, short endPunct, Action<short[]> onBuffer,
                             List<PhonemeEvent>? events, ref int sampleOffset) {
            var dump = _be.Process(tokens, endPunct);

            if (events != null) {
                int frameOffset = 0;
                for (int i = 0; i < dump.PhonBuf2InIndex; i++) {
                    short phon = dump.PhonBuf2[i];
                    bool emitSil = phon == _SIL_ && (i == 0 || dump.PhonBuf2[i - 1] != _SIL_);
                    if (phon != _SIL_ || emitSil) {
                        float t = (float)(sampleOffset + frameOffset * _synth.SampFrameLen) / SampleRate;
                        events.Add(new PhonemeEvent(phon, t,
                            phon != _SIL_ && (dump.PhonCtrlBuf2[i] & kWord_Start) != 0));
                    }
                    frameOffset += dump.DurBuf[i];
                }
            }

            var audio = ProcessSentenceToBuffer(tokens, endPunct);
            onBuffer(audio);
            sampleOffset += audio.Length;
        }

        void ApplyCommand(EmbeddedCmd.VoiceCommand cmd) {
            switch (cmd.Type) {
                case EmbeddedCmd.VoiceCommand.Kind.Rate:
                    _voice.Rate = (short)Math.Clamp(cmd.Value, 40, 600);
                    _be = new AudioProcessor(_voice);
                    break;
                case EmbeddedCmd.VoiceCommand.Kind.Pitch:
                    _voice.PitchHz = (short)Math.Clamp(cmd.Value, 40, 500);
                    _be = new AudioProcessor(_voice);
                    break;
                case EmbeddedCmd.VoiceCommand.Kind.Volume:
                    _voice.VGain = (short)Math.Clamp(cmd.Value, 0, 100);
                    _synth.InvDFT(_voice.VWave, _voice.VWave1, (short)_voice.VGain);
                    break;
            }
        }

        void RebuildPipeline() {
            _be = new AudioProcessor(_voice);
            _renderer = new SpeechRenderer(_voice);
            _synth = new KlattSynthesizer(SampleRate);
            short lo = _voice.LarynxOffset;
            _synth.SetVoice(_voice.NGain, true,
                (short)Math.Clamp(_voice.F4Freq + lo, 100, 8000), _voice.F4BW,
                (short)Math.Clamp(_voice.F5Freq + lo, 100, 8000), _voice.F5BW,
                (short)Math.Clamp(_voice.F4pFreq + lo, 100, 8000), _voice.F4pBW,
                (short)Math.Clamp(_voice.F5pFreq + lo, 100, 8000), _voice.F5pBW,
                (short)Math.Clamp(_voice.F6pFreq + lo, 100, 8000), _voice.F6pBW,
                _voice.NasalBase, _voice.NasalBW,
                _voice.AGain, _voice.ACycle);
            _synth.Jitter = _voice.Jitter;
            _synth.Shimmer = _voice.Shimmer;
            _synth.Diplophonia = _voice.Diplophonia;
            _synth.FryAmount = _voice.FryAmount;
            _synth.SubglottalAmt = _voice.SubglottalAmt;
            _synth.BreathAmt = _voice.BreathAmt;
            _synth.OpenQuotient = _voice.OpenQuotient;
            _synth.OQStressLink = _voice.OQStressLink;
            _synth.OQF0Link = _voice.OQF0Link;
            _synth.BasePitchHz = _voice.PitchHz;
            _synth.LarynxOffset = lo;
            _synth.PharyngealAmt = _voice.PharyngealAmt;
            _synth.PitchOffsetHz = _voice.PitchOffsetHz;
            _synth.LipRounding = _voice.LipRounding;
            _synth.InvDFT(_voice.VWave, _voice.VWave1, (short)_voice.VGain);
        }
    }
}  // namespace
