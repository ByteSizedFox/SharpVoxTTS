using System;
using System.Collections.Generic;
using System.IO;
using SharpTalk;

namespace SharpTalk.Cli {
    class Program {
        static async Task Main(string[] args) {
            if (args.Length == 0) {
                PrintHelp();
                return;
            }

            string? text = null;
            string outputPath = "out.wav";
            string? inputPath = null;
            int rate = 160;
            int pitch = 104;
            int sampleRate = 48000;
            string voicePreset = "baseline";

            // handle arguments so it's a proper CLI citizen
            var positionalArgs = new List<string>();

            for (int i = 0; i < args.Length; i++) {
                switch (args[i]) {
                    case "-o":
                    case "--output":
                        if (++i < args.Length) {
                            outputPath = args[i];
                        }
                        break;
                    case "-i":
                    case "--input":
                        if (++i < args.Length) {
                            inputPath = args[i];
                        }
                        break;
                    case "-r":
                    case "--rate":
                        if (++i < args.Length && int.TryParse(args[i], out int r)) {
                            rate = r;
                        }
                        break;
                    case "-p":
                    case "--pitch":
                        if (++i < args.Length && int.TryParse(args[i], out int p)) {
                            pitch = p;
                        }
                        break;
                    case "-s":
                    case "--samplerate":
                        if (++i < args.Length && int.TryParse(args[i], out int sr)) {
                            sampleRate = sr;
                        }
                        break;
                    case "-v":
                    case "--voice":
                        if (++i < args.Length) {
                            voicePreset = args[i].ToLower();
                        }
                        break;
                    case "-h":
                    case "--help":
                        PrintHelp();
                        return;
                    default:
                        if (!args[i].StartsWith("-")) {
                            positionalArgs.Add(args[i]);
                        }
                        break;
                }
            }

            if (positionalArgs.Count > 0) {
                text = string.Join(" ", positionalArgs);
            } else if (inputPath != null && File.Exists(inputPath)) {
                text = File.ReadAllText(inputPath);
            } else if (Console.IsInputRedirected) {
                text = Console.In.ReadToEnd();
            }

            if (string.IsNullOrWhiteSpace(text)) {
                PrintHelp();
                return;
            }

            var voice = TryLoadVoiceJson(voicePreset)
                     ?? (voicePreset == "whisper" ? VoiceData.WhisperVoice : VoiceData.BaselineVoice);
            voice.Rate = (short)rate;
            voice.PitchHz = (short)pitch;

            TtsEngine engine;
            try {
                engine = new TtsEngine(voice, sampleRate);
            } catch (ArgumentException ex) {
                Console.Error.WriteLine($"Error: {ex.Message}");
                return;
            }

            try {
                using var writer = new WavStreamWriter(outputPath, sampleRate);
                int totalSamples = 0;
                await engine.SpeakAsync(text, chunk => {
                    writer.Write(chunk);
                    totalSamples += chunk.Length;
                    return Task.CompletedTask;
                });
                Console.WriteLine($"Generated {outputPath} ({totalSamples / (float)sampleRate:F2}s @ {sampleRate} Hz)");
            } catch (Exception ex) {
                Console.WriteLine($"Error saving WAV: {ex.Message}");
            }
        }

        static VoiceData? TryLoadVoiceJson(string name) {
            string exeDir = Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location) ?? ".";
            string[] candidates = [
                Path.Combine(exeDir, "voices", name + ".json"),
                Path.Combine("voices", name + ".json"),
            ];
            string? path = Array.Find(candidates, File.Exists);
            if (path == null) {
                return null;
            }

            try {
                using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(path));
                var root = doc.RootElement;
                var v = new VoiceData();
                int G(string k, int d) => root.TryGetProperty(k, out var el) && el.TryGetInt32(out var i) ? i : d;
                float F(string k, float d) => root.TryGetProperty(k, out var el) && el.TryGetSingle(out var f) ? f : d;
                v.Rate = (short)G("Rate", v.Rate); v.PitchHz = (short)G("PitchHz", v.PitchHz);
                v.VoiceType = (short)G("VoiceType", v.VoiceType); v.TractScale = F("TractScale", v.TractScale);
                v.VGain = (short)G("VoicingGain", v.VGain); v.AGain = (short)G("AspirationGain", v.AGain);
                v.ACycle = (short)G("AspirationCycle", v.ACycle); v.NGain = (short)G("NGain", v.NGain);
                v.F4Freq = (short)G("F4Freq", v.F4Freq); v.F4BW = (short)G("F4BW", v.F4BW);
                v.F5Freq = (short)G("F5Freq", v.F5Freq); v.F5BW = (short)G("F5BW", v.F5BW);
                v.F4pFreq = (short)G("F4pFreq", v.F4pFreq); v.F4pBW = (short)G("F4pBW", v.F4pBW);
                v.F5pFreq = (short)G("F5pFreq", v.F5pFreq); v.F5pBW = (short)G("F5pBW", v.F5pBW);
                v.F6pFreq = (short)G("F6pFreq", v.F6pFreq); v.F6pBW = (short)G("F6pBW", v.F6pBW);
                v.BwGain1 = (short)G("BwGain1", v.BwGain1); v.BwGain2 = (short)G("BwGain2", v.BwGain2);
                v.BwGain3 = (short)G("BwGain3", v.BwGain3);
                v.NasalBase = (short)G("NasalBase", v.NasalBase); v.NasalTarg = (short)G("NasalTarg", v.NasalTarg);
                v.NasalBW = (short)G("NasalBW", v.NasalBW);
                v.PitchRange = (short)G("PitchRange", v.PitchRange); v.StressGain = (short)G("StressGain", v.StressGain);
                v.Intonation = (short)G("Intonation", v.Intonation);
                v.RiseAmt = (short)G("RiseAmt", v.RiseAmt); v.FallAmt = (short)G("FallAmt", v.FallAmt);
                v.BaselineFall = (short)G("BaselineFall", v.BaselineFall);
                v.Jitter = (short)G("Jitter", v.Jitter); v.Shimmer = (short)G("Shimmer", v.Shimmer);
                v.Diplophonia = (short)G("Diplophonia", v.Diplophonia);
                v.FryAmount = (short)G("FryAmount", v.FryAmount);
                v.SubglottalAmt = (short)G("SubglottalAmt", v.SubglottalAmt);
                v.BreathAmt = (short)G("BreathAmt", v.BreathAmt);
                v.OpenQuotient = (short)G("OpenQuotient", v.OpenQuotient);
                v.OQStressLink = (short)G("OQStressLink", v.OQStressLink);
                v.OQF0Link = (short)G("OQF0Link", v.OQF0Link);
                v.LarynxOffset = (short)G("LarynxOffset", v.LarynxOffset);
                v.PharyngealAmt = (short)G("PharyngealAmt", v.PharyngealAmt);
                v.PitchOffsetHz = (short)G("PitchOffsetHz", v.PitchOffsetHz);
                v.LipRounding = (short)G("LipRounding", v.LipRounding);
                v.OnsetHardness = (short)G("OnsetHardness", v.OnsetHardness);
                return v;
            } catch { return null; }
        }

        // in case it wasn't obvious enough
        static void PrintHelp() {
            Console.WriteLine("SharpTalk TTS");
            Console.WriteLine("Usage: sharptalk [options] [\"text\"]");
            Console.WriteLine();
            Console.WriteLine("Options:");
            Console.WriteLine("  -o, --output <file>    Output WAV file (default: out.wav)");
            Console.WriteLine("  -i, --input <file>     Input text file (if text not provided as argument)");
            Console.WriteLine("  -r, --rate <value>     Speech rate (default: 160)");
            Console.WriteLine("  -p, --pitch <value>    Base pitch in Hz (default: 104)");
            Console.WriteLine("  -s, --samplerate <hz>  Output sample rate (default: 48000)");
            Console.WriteLine("  -v, --voice <name>     Voice preset name — loads voices/<name>.json, fallback to baseline/whisper builtins");
            Console.WriteLine("  -h, --help             Show this help message");
            Console.WriteLine();
        }
    }
}
