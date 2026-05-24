#nullable enable
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Runtime.InteropServices.JavaScript;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace SharpTalk.WebUi;

public static partial class SharpTalkInterop
{
    private static TtsEngine? s_phonEngine;
    private static TtsEngine PhonEngine => s_phonEngine ??= new TtsEngine();

    private static bool s_klattschMode;
    private static VoiceData s_voice = VoiceData.BaselineVoice;
    private static List<short>? s_lastAudioBuffer;
    private static int s_lastAudioSampleRate = 48000;
    private static string? s_lastSpeakText;
    private static int s_sampleRate = 48000;
    private static float s_outputVolume = 1.0f;
    private static float s_klBaseF0 = 120f, s_klRate = 110f, s_klVibrato = 0f;
    private static float s_klVibRate = 5f, s_klAsp = 0f, s_klTilt = 0f, s_klEffort = 0.5f;
    private static readonly List<(string Code, float StartSecs, bool IsWordStart)> s_phonemeTimeline = new();
    private static readonly List<(string V1, string V2, float StartSecs)> s_lipsyncTimeline = new();
    private static CancellationTokenSource? s_cts;
    private static int s_speakGen;
    private static bool s_fastFinish = false;
    private static TaskCompletionSource? s_speakDone;


    [JSExport]
    public static void Initialize() => SyncAllParamsToUi();

    [JSExport]
    public static void SetMode(bool klattsch)
    {
        if (s_klattschMode == klattsch) return;
        s_klattschMode = klattsch;
        s_phonemeTimeline.Clear();
        s_lipsyncTimeline.Clear();
        Stop();
        JsUpdatePhonemes("[]", -1);
    }

    [JSExport]
    public static void UpdateParam(string name, string value)
    {
        try
        {
            float fv = float.Parse(value, CultureInfo.InvariantCulture);
            switch (name)
            {
                case "sampleRate":   s_sampleRate = (int)fv; break;
                case "TractScale":   s_voice.TractScale = fv; break;
                case "OutputVolume": s_outputVolume = fv; break;
                case "klBaseF0":     s_klBaseF0 = fv; break;
                case "klRate":       s_klRate = fv; break;
                case "klVibrato":    s_klVibrato = fv; break;
                case "klVibRate":    s_klVibRate = fv; break;
                case "klAsp":        s_klAsp = fv; break;
                case "klTilt":       s_klTilt = fv; break;
                case "klEffort":     s_klEffort = fv; break;
                case "Rate":         s_voice.Rate = (short)fv; break;
                case "PitchHz":      s_voice.PitchHz = (short)fv; break;
                case "VoiceType":    s_voice.VoiceType = (short)fv; break;
                case "VGain":        s_voice.VGain = (short)fv; break;
                case "AGain":        s_voice.AGain = (short)fv; break;
                case "ACycle":        s_voice.ACycle = (short)fv; break;
                case "TremoloDepth": s_voice.TremoloDepth = (short)fv; break;
                case "TremoloRate":  s_voice.TremoloRate  = (short)fv; break;
                case "Jitter":        s_voice.Jitter        = (short)fv; break;
                case "Shimmer":       s_voice.Shimmer       = (short)fv; break;
                case "Diplophonia":   s_voice.Diplophonia   = (short)fv; break;
                case "FryAmount":     s_voice.FryAmount     = (short)fv; break;
                case "SubglottalAmt": s_voice.SubglottalAmt = (short)fv; break;
                case "BreathAmt":     s_voice.BreathAmt     = (short)fv; break;
                case "OpenQuotient":  s_voice.OpenQuotient  = (short)fv; break;
                case "OQStressLink":  s_voice.OQStressLink  = (short)fv; break;
                case "OQF0Link":      s_voice.OQF0Link      = (short)fv; break;
                case "LarynxOffset":    s_voice.LarynxOffset    = (short)fv; break;
                case "PharyngealAmt":   s_voice.PharyngealAmt   = (short)fv; break;
                case "PitchOffsetHz":   s_voice.PitchOffsetHz   = (short)fv; break;
                case "LipRounding":     s_voice.LipRounding     = (short)fv; break;
                case "OnsetHardness":   s_voice.OnsetHardness   = (short)fv; break;
                case "NGain":        s_voice.NGain = (short)fv; break;
                case "F4Freq":       s_voice.F4Freq = (short)fv; break;
                case "F4BW":         s_voice.F4BW = (short)fv; break;
                case "F5Freq":       s_voice.F5Freq = (short)fv; break;
                case "F5BW":         s_voice.F5BW = (short)fv; break;
                case "F4pFreq":      s_voice.F4pFreq = (short)fv; break;
                case "F4pBW":        s_voice.F4pBW = (short)fv; break;
                case "F5pFreq":      s_voice.F5pFreq = (short)fv; break;
                case "F5pBW":        s_voice.F5pBW = (short)fv; break;
                case "F6pFreq":      s_voice.F6pFreq = (short)fv; break;
                case "F6pBW":        s_voice.F6pBW = (short)fv; break;
                case "BwGain1":      s_voice.BwGain1 = (short)fv; break;
                case "BwGain2":      s_voice.BwGain2 = (short)fv; break;
                case "BwGain3":      s_voice.BwGain3 = (short)fv; break;
                case "NasalBase":    s_voice.NasalBase = (short)fv; break;
                case "NasalTarg":    s_voice.NasalTarg = (short)fv; break;
                case "NasalBW":      s_voice.NasalBW = (short)fv; break;
                case "PitchRange":    s_voice.PitchRange    = (short)fv; break;
                case "StressGain":    s_voice.StressGain    = (short)fv; break;
                case "Intonation":    s_voice.Intonation    = (short)fv; break;
                case "RiseAmt":       s_voice.RiseAmt       = (short)fv; break;
                case "FallAmt":       s_voice.FallAmt       = (short)fv; break;
                case "BaselineFall":  s_voice.BaselineFall  = (short)fv; break;
                case "UptalkAmt":     s_voice.UptalkAmt     = (short)fv; break;
                case "StressEarly":   s_voice.StressEarly   = (short)fv; break;
                case "BreakStrength": s_voice.BreakStrength = (short)fv; break;
                case "EmphasisBoost":    s_voice.EmphasisBoost    = (short)fv; break;
                case "VocalConfidence":  s_voice.VocalConfidence  = (short)fv; break;
            }
        }
        catch { }
    }

    [JSExport]
    public static async Task Speak(string text)
    {
        if (string.IsNullOrWhiteSpace(text)) return;
        Stop();

        string synText = text;
        if (s_klattschMode)
        {
            string defs = string.Format(CultureInfo.InvariantCulture,
                "b{0:F0} r{1:F0} v{2:F1} w{3:F1} h{4:F2} t{5:F2} g{6:F2}",
                s_klBaseF0, s_klRate, s_klVibrato, s_klVibRate, s_klAsp, s_klTilt, s_klEffort);
            synText = $"[:klattsch on] {defs} {text} [:klattsch off]";
        }

        int gen = ++s_speakGen;
        int sampleRate = s_sampleRate; // capture before any awaits - dropdown changes must not affect this run
        s_lastAudioBuffer = null;
        s_fastFinish = false;
        s_phonemeTimeline.Clear();
        s_lipsyncTimeline.Clear();
        JsUpdateStatus("processing...");
        JsUpdatePhonemes("[]", -1);

        var speakDone = new TaskCompletionSource();
        s_speakDone = speakDone;
        s_cts = new CancellationTokenSource();
        var ct = s_cts.Token;

        try
        {
            var engine = new TtsEngine(s_voice, sampleRate);
            var allSamples = new List<short>();
            s_lastAudioBuffer = allSamples;
            JsInitAudio(sampleRate);
            await JsYieldToEventLoop(); // let audio context resume before first chunk

            await engine.SpeakAsyncWithEvents(
                synText,
                async chunk =>
                {
                    short[] scaledChunk = ApplyVolume(chunk, s_outputVolume);
                    allSamples.AddRange(scaledChunk);
                    JsPlayAudioStream(SamplesToBytes(scaledChunk), sampleRate);
                    if (!s_fastFinish)
                        await JsYieldToEventLoop();
                },
                async events =>
                {
                    foreach (var e in events)
                    {
                        var (lv1, lv2) = VisemeFor(e.Phoneme);
                        s_lipsyncTimeline.Add((lv1, lv2, e.TimeSeconds));
                        if (e.Phoneme == AudioProcessor._SIL_) continue;
                        var name = AudioProcessor.PhonemeNamesTable.TryGetValue(e.Phoneme, out var n) ? n : "?";
                        s_phonemeTimeline.Add((name, e.TimeSeconds, e.IsWordStart));
                    }
                    JsUpdateStatus($"playing — {s_phonemeTimeline.Count} phonemes");
                    JsUpdatePhonemes(CodesJson(), -1);

                    if (s_phonemeTimeline.Count > 0)
                    {
                        double playAt = JsReserveStartTime(sampleRate);
                        JsStartPhonemeTracking(CodesJson(), StartSecsJson(), playAt);
                    }
                    await Task.Yield();
                },
                ct);

            s_lastAudioSampleRate = sampleRate;
            s_lastSpeakText = text;
            int durationMs = sampleRate > 0 ? allSamples.Count * 1000 / sampleRate : 0;
            JsUpdateStatus($"ready — {durationMs} ms, {s_phonemeTimeline.Count} phonemes");
        }
        catch (OperationCanceledException) { if (gen == s_speakGen) JsUpdateStatus("stopped"); }
        catch (Exception ex) { if (gen == s_speakGen) JsUpdateStatus("error: " + ex.Message); }
        finally
        {
            s_fastFinish = false;
            speakDone.TrySetResult();
        }
    }

    [JSExport]
    public static void AuditionPhoneme(string code)
    {
        Stop();
        try
        {
            string seq = string.Format(CultureInfo.InvariantCulture,
                "[:klattsch on] b{0:F0} r{1:F0} {2} [:klattsch off]",
                s_klBaseF0, s_klRate, code);
            var engine = new TtsEngine(s_voice, s_sampleRate);
            var (audio, _) = engine.SpeakWithEvents(seq);
            JsInitAudio(s_sampleRate);
            JsPlayAudioStream(SamplesToBytes(ApplyVolume(audio, s_outputVolume)), s_sampleRate);
        }
        catch { }
    }

    [JSExport]
    public static void StopBtn()
    {
        Stop();
        s_lastAudioBuffer = null;
        s_lastSpeakText = null;
        JsUpdateStatus("stopped");
    }

    [JSExport]
    public static async Task DownloadWav(string text)
    {
        if (s_speakDone != null && !s_speakDone.Task.IsCompleted)
        {
            s_fastFinish = true;
            JsUpdateStatus("rendering...");
            await s_speakDone.Task;
        }
        if ((s_lastAudioBuffer == null || s_lastAudioBuffer.Count == 0) && !string.IsNullOrWhiteSpace(text))
            await RenderToBuffer(text);
        if (s_lastAudioBuffer == null || s_lastAudioBuffer.Count == 0) return;
        JsDownloadBytes(BuildWav(s_lastAudioBuffer, s_lastAudioSampleRate), "speech.wav", "audio/wav");
    }

    [JSExport]
    public static void OnPresetChange(string val)
    {
        if (val == "baseline") SetPreset(VoiceData.BaselineVoice);
        else if (val == "whisper") SetPreset(VoiceData.WhisperVoice);
        SyncAllParamsToUi();
    }

    [JSExport]
    public static void ExportPreset()
    {
        var sb = new StringBuilder("{\n");
        void K(string k, short v) { if (sb.Length > 3) sb.Append(",\n"); sb.Append("  \"").Append(k).Append("\": ").Append(v); }
        K("Rate", s_voice.Rate); K("PitchHz", s_voice.PitchHz); K("VoiceType", s_voice.VoiceType);
        if (sb.Length > 3) sb.Append(",\n"); sb.Append("  \"TractScale\": ").Append(s_voice.TractScale.ToString("G", System.Globalization.CultureInfo.InvariantCulture));
        K("VoicingGain", s_voice.VGain); K("AspirationGain", s_voice.AGain); K("AspirationCycle", s_voice.ACycle);
        K("TremoloDepth", s_voice.TremoloDepth); K("TremoloRate", s_voice.TremoloRate);
        K("Jitter", s_voice.Jitter); K("Shimmer", s_voice.Shimmer); K("Diplophonia", s_voice.Diplophonia);
        K("FryAmount", s_voice.FryAmount); K("SubglottalAmt", s_voice.SubglottalAmt); K("BreathAmt", s_voice.BreathAmt); K("OpenQuotient", s_voice.OpenQuotient); K("OQStressLink", s_voice.OQStressLink); K("OQF0Link", s_voice.OQF0Link);
        K("LarynxOffset", s_voice.LarynxOffset); K("PharyngealAmt", s_voice.PharyngealAmt); K("PitchOffsetHz", s_voice.PitchOffsetHz); K("LipRounding", s_voice.LipRounding); K("OnsetHardness", s_voice.OnsetHardness);
        K("NGain", s_voice.NGain); K("F4Freq", s_voice.F4Freq); K("F4BW", s_voice.F4BW);
        K("F5Freq", s_voice.F5Freq); K("F5BW", s_voice.F5BW);
        K("F4pFreq", s_voice.F4pFreq); K("F4pBW", s_voice.F4pBW); K("F5pFreq", s_voice.F5pFreq);
        K("F5pBW", s_voice.F5pBW); K("F6pFreq", s_voice.F6pFreq); K("F6pBW", s_voice.F6pBW);
        K("BwGain1", s_voice.BwGain1); K("BwGain2", s_voice.BwGain2); K("BwGain3", s_voice.BwGain3);
        K("NasalBase", s_voice.NasalBase); K("NasalTarg", s_voice.NasalTarg); K("NasalBW", s_voice.NasalBW);
        K("PitchRange", s_voice.PitchRange); K("StressGain", s_voice.StressGain); K("Intonation", s_voice.Intonation);
        K("RiseAmt", s_voice.RiseAmt); K("FallAmt", s_voice.FallAmt); K("BaselineFall", s_voice.BaselineFall);
        sb.Append("\n}");
        JsDownloadFile("voice.json", sb.ToString());
    }

    [JSExport]
    public static void HandleImport(string json)
    {
        if (string.IsNullOrEmpty(json)) return;
        try
        {
            using var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;
            int G(string k, int def)
            {
                if (!root.TryGetProperty(k, out var el)) return def;
                return el.ValueKind == JsonValueKind.True ? 1
                     : el.ValueKind == JsonValueKind.False ? 0
                     : el.TryGetInt32(out var i) ? i
                     : el.TryGetDouble(out var d) ? (int)d : def;
            }
            s_voice.Rate = (short)G("Rate", s_voice.Rate); s_voice.PitchHz = (short)G("PitchHz", s_voice.PitchHz);
            if (root.TryGetProperty("TractScale", out var tsEl) && tsEl.TryGetDouble(out var tsD)) s_voice.TractScale = (float)tsD;
            s_voice.VoiceType = (short)G("VoiceType", s_voice.VoiceType);
            s_voice.VGain = (short)G("VoicingGain", s_voice.VGain); s_voice.AGain = (short)G("AspirationGain", s_voice.AGain);
            s_voice.ACycle = (short)G("AspirationCycle", s_voice.ACycle);
            s_voice.TremoloDepth  = (short)G("TremoloDepth",  0);
            s_voice.TremoloRate   = (short)G("TremoloRate",   0);
            s_voice.Jitter        = (short)G("Jitter",        0);
            s_voice.Shimmer       = (short)G("Shimmer",       0);
            s_voice.Diplophonia   = (short)G("Diplophonia",   0);
            s_voice.FryAmount     = (short)G("FryAmount",     0);
            s_voice.SubglottalAmt = (short)G("SubglottalAmt", 0);
            s_voice.BreathAmt     = (short)G("BreathAmt",     0);
            s_voice.OpenQuotient  = (short)G("OpenQuotient",  50);
            s_voice.OQStressLink  = (short)G("OQStressLink",   0);
            s_voice.OQF0Link      = (short)G("OQF0Link",        0);
            s_voice.LarynxOffset  = (short)G("LarynxOffset",  0);
            s_voice.PharyngealAmt = (short)G("PharyngealAmt", 0);
            s_voice.PitchOffsetHz = (short)G("PitchOffsetHz", 0);
            s_voice.LipRounding   = (short)G("LipRounding",   0);
            s_voice.OnsetHardness = (short)G("OnsetHardness", 50);
            s_voice.NGain = (short)G("NGain", s_voice.NGain);
            s_voice.F4Freq = (short)G("F4Freq", s_voice.F4Freq); s_voice.F4BW = (short)G("F4BW", s_voice.F4BW);
            s_voice.F5Freq = (short)G("F5Freq", s_voice.F5Freq); s_voice.F5BW = (short)G("F5BW", s_voice.F5BW);
            s_voice.F4pFreq = (short)G("F4pFreq", s_voice.F4pFreq); s_voice.F4pBW = (short)G("F4pBW", s_voice.F4pBW);
            s_voice.F5pFreq = (short)G("F5pFreq", s_voice.F5pFreq); s_voice.F5pBW = (short)G("F5pBW", s_voice.F5pBW);
            s_voice.F6pFreq = (short)G("F6pFreq", s_voice.F6pFreq); s_voice.F6pBW = (short)G("F6pBW", s_voice.F6pBW);
            s_voice.BwGain1 = (short)G("BwGain1", s_voice.BwGain1); s_voice.BwGain2 = (short)G("BwGain2", s_voice.BwGain2);
            s_voice.BwGain3 = (short)G("BwGain3", s_voice.BwGain3);
            s_voice.NasalBase = (short)G("NasalBase", s_voice.NasalBase); s_voice.NasalTarg = (short)G("NasalTarg", s_voice.NasalTarg);
            s_voice.NasalBW = (short)G("NasalBW", s_voice.NasalBW);
            s_voice.PitchRange = (short)G("PitchRange", s_voice.PitchRange); s_voice.StressGain = (short)G("StressGain", s_voice.StressGain);
            s_voice.Intonation = (short)G("Intonation", s_voice.Intonation);
            s_voice.RiseAmt = (short)G("RiseAmt", s_voice.RiseAmt); s_voice.FallAmt = (short)G("FallAmt", s_voice.FallAmt);
            s_voice.BaselineFall = (short)G("BaselineFall", s_voice.BaselineFall);
            SyncAllParamsToUi();
            JsUpdateStatus("preset imported");
        }
        catch (Exception ex) { JsUpdateStatus("import error: " + ex.Message); }
    }

    [JSExport]
    public static string ConvertUst(string text, string language, int offset, string? bank)
    {
        try
        {
            var r = UstConverter.Convert(text, language, offset, bank, PhonEngine.PhonemizeWord);
            return "{\"klattsch\":" + JsonStr(r.Klattsch) + ",\"diagnostics\":" + JsonStr(r.Diagnostics) + "}";
        }
        catch (Exception ex)
        {
            return "{\"klattsch\":\"\",\"diagnostics\":" + JsonStr("Conversion error: " + ex.Message) + "}";
        }
    }


    [JSExport]
    public static async Task ExportVideo(string text)
    {
        if (s_speakDone != null && !s_speakDone.Task.IsCompleted)
        {
            s_fastFinish = true;
            JsUpdateStatus("rendering...");
            await s_speakDone.Task;
        }
        if ((s_lastAudioBuffer == null || s_lastAudioBuffer.Count == 0 || s_lastSpeakText != text) && !string.IsNullOrWhiteSpace(text))
            await RenderToBuffer(text);
        if (s_lastAudioBuffer == null || s_lastAudioBuffer.Count == 0) return;

        var sbCodes = new StringBuilder("[");
        var sbTimes = new StringBuilder("[");
        var sbWordTimes = new StringBuilder("[");
        bool firstWord = true;
        foreach (var (code, time, isWordStart) in s_phonemeTimeline)
        {
            if (sbCodes.Length > 1) { sbCodes.Append(','); sbTimes.Append(','); }
            sbCodes.Append(JsonStr(code));
            sbTimes.Append(time.ToString(CultureInfo.InvariantCulture));
            if (isWordStart)
            {
                if (!firstWord) sbWordTimes.Append(',');
                sbWordTimes.Append(time.ToString(CultureInfo.InvariantCulture));
                firstWord = false;
            }
        }
        sbCodes.Append(']');
        sbTimes.Append(']');
        sbWordTimes.Append(']');

        var sbLsTimes = new StringBuilder("[");
        var sbLsV1    = new StringBuilder("[");
        var sbLsV2    = new StringBuilder("[");
        foreach (var (v1, v2, t) in s_lipsyncTimeline)
        {
            if (sbLsTimes.Length > 1) { sbLsTimes.Append(','); sbLsV1.Append(','); sbLsV2.Append(','); }
            sbLsTimes.Append(t.ToString(CultureInfo.InvariantCulture));
            sbLsV1.Append(JsonStr(v1));
            sbLsV2.Append(JsonStr(v2));
        }
        sbLsTimes.Append(']'); sbLsV1.Append(']'); sbLsV2.Append(']');

        float totalDuration = (float)s_lastAudioBuffer.Count / s_lastAudioSampleRate;
        JsStartVideoExport(SamplesToBytes(s_lastAudioBuffer.ToArray()), s_lastAudioSampleRate,
            sbCodes.ToString(), sbTimes.ToString(), sbWordTimes.ToString(), totalDuration, text,
            sbLsTimes.ToString(), sbLsV1.ToString(), sbLsV2.ToString());
    }

    [JSImport("globalThis.ui.startVideoExport")]
    static partial void JsStartVideoExport(byte[] pcmBytes, int sampleRate, string eventsJson, string timesJson, string wordTimesJson, float duration, string sourceText, string lipsyncTimesJson, string lipsyncV1Json, string lipsyncV2Json);

    [JSImport("globalThis.initAudio")]
    static partial void JsInitAudio(int sampleRate);

    [JSImport("globalThis.playAudioStream")]
    static partial void JsPlayAudioStream(byte[] pcmBytes, int sampleRate);

    [JSImport("globalThis.stopAudio")]
    static partial void JsStopAudio();

    [JSImport("globalThis.stopPhonemeTracking")]
    static partial void JsStopPhonemeTracking();

    [JSImport("globalThis.reserveStartTime")]
    private static partial double JsReserveStartTime(int sampleRate);

    [JSImport("globalThis.startPhonemeTracking")]
    static partial void JsStartPhonemeTracking(string codesJson, string startSecsJson, double playAt);

    [JSImport("globalThis.yieldToEventLoop")]
    private static partial Task JsYieldToEventLoop();

    [JSImport("globalThis.downloadBytes")]
    static partial void JsDownloadBytes(byte[] data, string filename, string mimeType);

    [JSImport("globalThis.downloadFile")]
    static partial void JsDownloadFile(string filename, string content);

    [JSImport("globalThis.ui.updateStatus")]
    static partial void JsUpdateStatus(string text);

    [JSImport("globalThis.ui.updatePhonemes")]
    static partial void JsUpdatePhonemes(string codesJson, int activeIdx);

    [JSImport("globalThis.ui.showDownloadBtn")]
    static partial void JsShowDownloadBtn(bool show);

    [JSImport("globalThis.ui.updateAllParams")]
    static partial void JsUpdateAllParams(string json);

    [JSImport("globalThis.ui.setMode")]
    static partial void JsSetMode(string mode);


    // Silent synthesis - fills s_lastAudioBuffer and s_phonemeTimeline without touching the audio context.
    // Used by DownloadWav and ExportVideo so they don't behave like Speak.
    private static async Task RenderToBuffer(string text)
    {
        Stop();

        string synText = text;
        if (s_klattschMode)
        {
            string defs = string.Format(CultureInfo.InvariantCulture,
                "b{0:F0} r{1:F0} v{2:F1} w{3:F1} h{4:F2} t{5:F2} g{6:F2}",
                s_klBaseF0, s_klRate, s_klVibrato, s_klVibRate, s_klAsp, s_klTilt, s_klEffort);
            synText = $"[:klattsch on] {defs} {text} [:klattsch off]";
        }

        int sampleRate = s_sampleRate;
        s_lastAudioBuffer = null;
        s_phonemeTimeline.Clear();
        s_lipsyncTimeline.Clear();
        JsUpdateStatus("rendering...");

        var speakDone = new TaskCompletionSource();
        s_speakDone = speakDone;
        s_cts = new CancellationTokenSource();
        var ct = s_cts.Token;

        try
        {
            var engine = new TtsEngine(s_voice, sampleRate);
            var allSamples = new List<short>();
            s_lastAudioBuffer = allSamples;

            await engine.SpeakAsyncWithEvents(
                synText,
                chunk =>
                {
                    allSamples.AddRange(ApplyVolume(chunk, s_outputVolume));
                    return Task.CompletedTask;
                },
                events =>
                {
                    foreach (var e in events)
                    {
                        var (lv1, lv2) = VisemeFor(e.Phoneme);
                        s_lipsyncTimeline.Add((lv1, lv2, e.TimeSeconds));
                        if (e.Phoneme == AudioProcessor._SIL_) continue;
                        var name = AudioProcessor.PhonemeNamesTable.TryGetValue(e.Phoneme, out var n) ? n : "?";
                        s_phonemeTimeline.Add((name, e.TimeSeconds, e.IsWordStart));
                    }
                    return Task.CompletedTask;
                },
                ct);

            s_lastAudioSampleRate = sampleRate;
            s_lastSpeakText = text;
        }
        catch (OperationCanceledException) { JsUpdateStatus("stopped"); }
        catch (Exception ex) { JsUpdateStatus("error: " + ex.Message); }
        finally { speakDone.TrySetResult(); }
    }

    private static void Stop()
    {
        s_cts?.Cancel();
        s_cts?.Dispose();
        s_cts = null;
        JsStopPhonemeTracking();
        JsStopAudio();
    }

    private static short[] ApplyVolume(short[] samples, float volume)
    {
        if (Math.Abs(volume - 1.0f) < 0.001f) return samples;
        short[] result = new short[samples.Length];
        for (int i = 0; i < samples.Length; i++)
        {
            result[i] = (short)Math.Clamp(MathF.Round(samples[i] * volume), short.MinValue, short.MaxValue);
        }
        return result;
    }

    private static void SetPreset(VoiceData preset)
    {
        s_voice.Rate = preset.Rate; s_voice.PitchHz = preset.PitchHz; s_voice.TractScale = preset.TractScale; s_voice.VoiceType = preset.VoiceType;
        s_voice.VGain = preset.VGain; s_voice.AGain = preset.AGain; s_voice.ACycle = preset.ACycle;
        s_voice.TremoloDepth = preset.TremoloDepth; s_voice.TremoloRate = preset.TremoloRate;
        s_voice.Jitter = preset.Jitter; s_voice.Shimmer = preset.Shimmer; s_voice.Diplophonia = preset.Diplophonia;
        s_voice.FryAmount = preset.FryAmount; s_voice.SubglottalAmt = preset.SubglottalAmt; s_voice.BreathAmt = preset.BreathAmt; s_voice.OpenQuotient = preset.OpenQuotient; s_voice.OQStressLink = preset.OQStressLink; s_voice.OQF0Link = preset.OQF0Link;
        s_voice.LarynxOffset = preset.LarynxOffset; s_voice.PharyngealAmt = preset.PharyngealAmt; s_voice.PitchOffsetHz = preset.PitchOffsetHz; s_voice.LipRounding = preset.LipRounding; s_voice.OnsetHardness = preset.OnsetHardness;
        s_voice.NGain = preset.NGain;
        s_voice.F4Freq = preset.F4Freq; s_voice.F4BW = preset.F4BW;
        s_voice.F5Freq = preset.F5Freq; s_voice.F5BW = preset.F5BW;
        s_voice.F4pFreq = preset.F4pFreq; s_voice.F4pBW = preset.F4pBW;
        s_voice.F5pFreq = preset.F5pFreq; s_voice.F5pBW = preset.F5pBW; s_voice.F6pFreq = preset.F6pFreq; s_voice.F6pBW = preset.F6pBW;
        s_voice.BwGain1 = preset.BwGain1; s_voice.BwGain2 = preset.BwGain2; s_voice.BwGain3 = preset.BwGain3;
        s_voice.NasalBase = preset.NasalBase; s_voice.NasalTarg = preset.NasalTarg; s_voice.NasalBW = preset.NasalBW;
        s_voice.PitchRange = preset.PitchRange; s_voice.StressGain = preset.StressGain; s_voice.Intonation = preset.Intonation;
        s_voice.RiseAmt = preset.RiseAmt; s_voice.FallAmt = preset.FallAmt; s_voice.BaselineFall = preset.BaselineFall;
    }

    private static void SyncAllParamsToUi()
    {
        var sb = new StringBuilder("{");
        void KS(string k, short v) { if (sb.Length > 1) sb.Append(','); sb.Append('"').Append(k).Append("\":").Append(v); }
        void KF(string k, float v) { if (sb.Length > 1) sb.Append(','); sb.Append('"').Append(k).Append("\":").Append(v.ToString("G", CultureInfo.InvariantCulture)); }
        void KI(string k, int v)   { if (sb.Length > 1) sb.Append(','); sb.Append('"').Append(k).Append("\":").Append(v); }
        KS("Rate", s_voice.Rate); KS("PitchHz", s_voice.PitchHz); KF("TractScale", s_voice.TractScale); KS("VoiceType", s_voice.VoiceType);
        KS("VGain", s_voice.VGain); KS("AGain", s_voice.AGain); KS("ACycle", s_voice.ACycle);
        KS("TremoloDepth", s_voice.TremoloDepth); KS("TremoloRate", s_voice.TremoloRate);
        KS("Jitter", s_voice.Jitter); KS("Shimmer", s_voice.Shimmer); KS("Diplophonia", s_voice.Diplophonia);
        KS("FryAmount", s_voice.FryAmount); KS("SubglottalAmt", s_voice.SubglottalAmt); KS("BreathAmt", s_voice.BreathAmt); KS("OpenQuotient", s_voice.OpenQuotient); KS("OQStressLink", s_voice.OQStressLink); KS("OQF0Link", s_voice.OQF0Link);
        KS("LarynxOffset", s_voice.LarynxOffset); KS("PharyngealAmt", s_voice.PharyngealAmt); KS("PitchOffsetHz", s_voice.PitchOffsetHz); KS("LipRounding", s_voice.LipRounding); KS("OnsetHardness", s_voice.OnsetHardness);
        KS("NGain", s_voice.NGain); KS("F4Freq", s_voice.F4Freq); KS("F4BW", s_voice.F4BW);
        KS("F5Freq", s_voice.F5Freq); KS("F5BW", s_voice.F5BW);
        KS("F4pFreq", s_voice.F4pFreq); KS("F4pBW", s_voice.F4pBW); KS("F5pFreq", s_voice.F5pFreq);
        KS("F5pBW", s_voice.F5pBW); KS("F6pFreq", s_voice.F6pFreq); KS("F6pBW", s_voice.F6pBW);
        KS("BwGain1", s_voice.BwGain1); KS("BwGain2", s_voice.BwGain2); KS("BwGain3", s_voice.BwGain3);
        KS("NasalBase", s_voice.NasalBase); KS("NasalTarg", s_voice.NasalTarg); KS("NasalBW", s_voice.NasalBW);
        KS("PitchRange", s_voice.PitchRange); KS("StressGain", s_voice.StressGain); KS("Intonation", s_voice.Intonation);
        KS("RiseAmt", s_voice.RiseAmt); KS("FallAmt", s_voice.FallAmt); KS("BaselineFall", s_voice.BaselineFall);
        KF("klBaseF0", s_klBaseF0); KF("klRate", s_klRate); KF("klVibrato", s_klVibrato);
        KF("klVibRate", s_klVibRate); KF("klAsp", s_klAsp); KF("klTilt", s_klTilt); KF("klEffort", s_klEffort);
        KF("OutputVolume", s_outputVolume);
        KI("sampleRate", s_sampleRate);
        sb.Append('}');
        JsUpdateAllParams(sb.ToString());
    }

    private static string CodesJson()
    {
        if (s_phonemeTimeline.Count == 0) return "[]";
        var sb = new StringBuilder("[");
        foreach (var (code, _, _) in s_phonemeTimeline)
        {
            if (sb.Length > 1) sb.Append(',');
            sb.Append(JsonStr(code));
        }
        return sb.Append(']').ToString();
    }

    private static string StartSecsJson()
    {
        if (s_phonemeTimeline.Count == 0) return "[]";
        var sb = new StringBuilder("[");
        foreach (var (_, startSecs, _) in s_phonemeTimeline)
        {
            if (sb.Length > 1) sb.Append(',');
            sb.Append(((double)startSecs).ToString("G", CultureInfo.InvariantCulture));
        }
        return sb.Append(']').ToString();
    }

    private static (string v1, string v2) VisemeFor(short ph)
    {
        if (ph == AudioProcessor._SIL_ || ph == AudioProcessor._QX_) return ("", "");
        if (ph == AudioProcessor._IY_ || ph == AudioProcessor._IH_ || ph == AudioProcessor._AX_ || ph == AudioProcessor._IX_) return ("vrc.v_ih", "");
        if (ph == AudioProcessor._EH_ || ph == AudioProcessor._AE_) return ("vrc.v_e", "");
        if (ph == AudioProcessor._AH_ || ph == AudioProcessor._AA_) return ("vrc.v_aa", "");
        if (ph == AudioProcessor._AO_) return ("vrc.v_oh", "");
        if (ph == AudioProcessor._UH_ || ph == AudioProcessor._UW_) return ("vrc.v_ou", "");
        if (ph == AudioProcessor._ER_ || ph == AudioProcessor._XR_ || ph == AudioProcessor._RX_) return ("vrc.v_nn", "");
        if (ph == AudioProcessor._EY_) return ("vrc.v_e", "");
        if (ph == AudioProcessor._AY_) return ("vrc.v_aa", "vrc.v_ih");
        if (ph == AudioProcessor._OY_) return ("vrc.v_oh", "vrc.v_ih");
        if (ph == AudioProcessor._AW_) return ("vrc.v_aa", "vrc.v_ou");
        if (ph == AudioProcessor._OW_) return ("vrc.v_oh", "vrc.v_ou");
        if (ph == AudioProcessor._YU_) return ("vrc.v_nn", "vrc.v_ou");
        if (ph == AudioProcessor._IR_) return ("vrc.v_ih", "vrc.v_nn");
        if (ph == AudioProcessor._AR_) return ("vrc.v_aa", "vrc.v_nn");
        if (ph == AudioProcessor._OR_) return ("vrc.v_oh", "vrc.v_nn");
        if (ph == AudioProcessor._UR_) return ("vrc.v_ou", "vrc.v_nn");
        if (ph == AudioProcessor._M_ || ph == AudioProcessor._P_ || ph == AudioProcessor._B_) return ("vrc.v_pp", "");
        if (ph == AudioProcessor._F_ || ph == AudioProcessor._V_) return ("vrc.v_ff", "");
        if (ph == AudioProcessor._TH_ || ph == AudioProcessor._DH_) return ("vrc.v_th", "");
        if (ph == AudioProcessor._S_ || ph == AudioProcessor._Z_ || ph == AudioProcessor._T_ || ph == AudioProcessor._D_ ||
            ph == AudioProcessor._TX_ || ph == AudioProcessor._DX_ || ph == AudioProcessor._DD_) return ("vrc.v_dd", "");
        if (ph == AudioProcessor._SH_ || ph == AudioProcessor._ZH_ || ph == AudioProcessor._CH_ || ph == AudioProcessor._JH_) return ("vrc.v_ch", "");
        if (ph == AudioProcessor._N_ || ph == AudioProcessor._NG_ || ph == AudioProcessor._K_ || ph == AudioProcessor._G_ ||
            ph == AudioProcessor._Y_ || ph == AudioProcessor._R_ || ph == AudioProcessor._HH_ || ph == AudioProcessor._EN_) return ("vrc.v_nn", "");
        if (ph == AudioProcessor._L_ || ph == AudioProcessor._LX_ || ph == AudioProcessor._EL_) return ("vrc.v_dd", "");
        if (ph == AudioProcessor._W_) return ("vrc.v_ou", "");
        return ("", "");
    }

    private static string JsonStr(string s) =>
        "\"" + s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", "\\n").Replace("\r", "\\r").Replace("\t", "\\t") + "\"";

    private static byte[] SamplesToBytes(short[] samples)
    {
        byte[] buf = new byte[samples.Length * 2];
        Buffer.BlockCopy(samples, 0, buf, 0, buf.Length);
        return buf;
    }

    private static byte[] BuildWav(List<short> samples, int sampleRate)
    {
        int dataBytes = samples.Count * 2;
        byte[] buf = new byte[44 + dataBytes];
        void Ascii(int off, string s) { for (int k = 0; k < s.Length; k++) buf[off + k] = (byte)s[k]; }
        void U16(int off, uint v) { buf[off] = (byte)v; buf[off + 1] = (byte)(v >> 8); }
        void U32(int off, uint v) { buf[off] = (byte)v; buf[off + 1] = (byte)(v >> 8); buf[off + 2] = (byte)(v >> 16); buf[off + 3] = (byte)(v >> 24); }
        Ascii(0, "RIFF"); U32(4, (uint)(36 + dataBytes));
        Ascii(8, "WAVE"); Ascii(12, "fmt ");
        U32(16, 16); U16(20, 1); U16(22, 1);
        U32(24, (uint)sampleRate); U32(28, (uint)(sampleRate * 2));
        U16(32, 2); U16(34, 16);
        Ascii(36, "data"); U32(40, (uint)dataBytes);
        short[] arr = samples.ToArray();
        Buffer.BlockCopy(arr, 0, buf, 44, dataBytes);
        return buf;
    }
}
