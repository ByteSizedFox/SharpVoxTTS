#nullable enable
using System;
using System.Collections.Generic;
using static SharpTalk.AudioProcessor;

namespace SharpTalk {

    public static class EmbeddedCmd {
        // ASCII note name (C5, A#4, Bb3) -> Hz using equal temperament (A4 = 440 Hz)
        static int NoteNameToHz(string name) {
            if (name.Length < 2) {
                return 0;
            }
            int semitone = char.ToUpperInvariant(name[0]) switch {
                'C' => 0,
                'D' => 2,
                'E' => 4,
                'F' => 5,
                'G' => 7,
                'A' => 9,
                'B' => 11,
                _ => -1,
            };
            if (semitone < 0) {
                return 0;
            }
            int pos = 1;
            if (pos < name.Length && name[pos] == '#') {
                semitone++;
                pos++;
            } else if (pos < name.Length && name[pos] == 'b') {
                semitone--;
                pos++;
            }
            if (pos >= name.Length || !int.TryParse(name[pos..], out int octave)) {
                return 0;
            }
            int midi = 12 * (octave + 1) + semitone;
            return (int)Math.Round(440.0 * Math.Pow(2.0, (midi - 69) / 12.0));
        }

        static short MapPhoneme(string p) => p switch {
            // Vowels
            "iy" => _IY_,
            "ih" => _IH_,
            "eh" => _EH_,
            "ae" => _AE_,
            "aa" => _AA_,
            "ah" => _AH_,
            "ao" => _AO_,
            "uh" => _UH_,
            "ax" => _AX_,
            "er" => _ER_,
            "ey" => _EY_,
            "ay" => _AY_,
            "oy" => _OY_,
            "aw" => _AW_,
            "ow" => _OW_,
            "uw" => _UW_,
            "ix" => _IX_,
            // Single-char vowel shortcuts, "i"->IH, "e"->EH, "a"->AE, "o"->AO, "u"->UW
            // Allows compact notation like "KIT" instead of "KIHT".
            "i" => _IH_,
            "e" => _EH_,
            "a" => _AE_,
            "o" => _AO_,
            "u" => _UW_,
            // Sonorants
            "w" => _W_,
            "y" => _Y_,
            "r" => _R_,
            "l" => _L_,
            // Nasals
            "m" => _M_,
            "n" => _N_,
            "ng" => _NG_,
            // Fricatives
            "hh" => _HH_,
            "f" => _F_,
            "v" => _V_,
            "th" => _TH_,
            "dh" => _DH_,
            "s" => _S_,
            "z" => _Z_,
            "sh" => _SH_,
            "zh" => _ZH_,
            // Stops
            "p" => _P_,
            "b" => _B_,
            "t" => _T_,
            "d" => _D_,
            "dx" => _DX_,
            "k" => _K_,
            "g" => _G_,
            // Affricates
            "ch" => _CH_,
            "jh" => _JH_,
            // Japanese vowels
            "jp_iy" => _JP_I_,
            "jp_eh" => _JP_E_,
            "jp_aa" => _JP_A_,
            "jp_ow" => _JP_O_,
            "jp_uw" => _JP_U_,
            // Silence / rest
            "_" => _SIL_,
            _ => -1,
        };

        public readonly struct VoiceCommand {
            public enum Kind { Rate, Pitch, Volume }
            public readonly Kind Type;
            public readonly int Value;
            public VoiceCommand(Kind type, int value) { Type = type; Value = value; }
        }

        public static bool KlattschMode = false;

        public readonly struct Segment {
            public readonly string? PlainText;
            public readonly List<PhonemeToken>? Singing;
            public readonly VoiceCommand? Cmd;
            public readonly string? KlattschText;
            public bool IsSinging => Singing != null;
            public bool IsCommand => Cmd != null;
            public bool IsKlattsch => KlattschText != null;
            public Segment(string text) { PlainText = text; Singing = null; Cmd = null; KlattschText = null; }
            public Segment(List<PhonemeToken> s) { PlainText = null; Singing = s; Cmd = null; KlattschText = null; }
            public Segment(VoiceCommand cmd) { PlainText = null; Singing = null; Cmd = cmd; KlattschText = null; }
            public static Segment Klattsch(string text) => new Segment(null, null, null, text);
            private Segment(string? p, List<PhonemeToken>? s, VoiceCommand? c, string? k) { PlainText = p; Singing = s; Cmd = c; KlattschText = k; }
        }

        public static List<Segment> ParseSegments(string text) {
            var segments = new List<Segment>();

            if (KlattschMode) {
                // In Klattsch mode, we still look for [:klattsch off]
                int offIdx = text.IndexOf("[:klattsch off]", StringComparison.OrdinalIgnoreCase);
                if (offIdx >= 0) {
                    string before = text[..offIdx];
                    if (before.Length > 0) {
                        segments.Add(Segment.Klattsch(before));
                    }
                    KlattschMode = false;
                    string after = text[(offIdx + "[:klattsch off]".Length)..];
                    if (after.Length > 0) {
                        segments.AddRange(ParseSegments(after));
                    }
                    return segments;
                }
                segments.Add(Segment.Klattsch(text));
                return segments;
            }

            if (!text.Contains('[')) {
                if (text.Length > 0) {
                    segments.Add(new Segment(text));
                }
                return segments;
            }

            var plain = new System.Text.StringBuilder();
            bool inSingMode = false;
            int i = 0;

            void FlushPlain() {
                if (plain.Length > 0) {
                    segments.Add(new Segment(plain.ToString()));
                    plain.Clear();
                }
            }

            while (i < text.Length) {
                if (text[i] != '[') {
                    plain.Append(text[i++]);
                    continue;
                }

                i++; // consume '['
                if (i >= text.Length) {
                    break;
                }

                if (text[i] == ':') {
                    i++; // Skip ':'
                    int cmdStart = i;
                    while (i < text.Length && !char.IsWhiteSpace(text[i]) && text[i] != ']') {
                        i++;
                    }
                    string cmd = text[cmdStart..i].ToLowerInvariant();
                    while (i < text.Length && char.IsWhiteSpace(text[i])) {
                        i++;
                    }
                    int argStart = i;
                    while (i < text.Length && text[i] != ']') {
                        i++;
                    }
                    string argStr = text[argStart..i].Trim().ToLowerInvariant();
                    if (i < text.Length) {
                        i++; // consume ']'
                    }

                    if (cmd == "klattsch") {
                        if (argStr == "on") {
                            FlushPlain();
                            KlattschMode = true;
                            KlattschParser.Reset();
                            string rest = text[i..];
                            segments.AddRange(ParseSegments(rest));
                            return segments;
                        } else if (argStr == "off") {
                            KlattschMode = false;
                        }
                    } else if (cmd == "sing") {
                        inSingMode = true;
                    } else if (cmd == "talk" || cmd == "stop") {
                        inSingMode = false;
                    } else if (int.TryParse(argStr, out int argVal)) {
                        VoiceCommand.Kind? kind = cmd switch { "rate" => VoiceCommand.Kind.Rate, "pitch" => VoiceCommand.Kind.Pitch, "volume" => VoiceCommand.Kind.Volume, _ => null };
                        if (kind is { } k) {
                            FlushPlain();
                            segments.Add(new Segment(new VoiceCommand(k, argVal)));
                        }
                    }
                    continue;
                }

                // Phoneme block [phoneme<dur,note> ...]
                var blockSing = new List<PhonemeToken>();
                bool firstPhon = true;
                bool firstInBlock = true; // Track first note in the [...] block
                short lastPitch = 0; // inherited by trailing consonants with no <note>

                while (i < text.Length && text[i] != ']') {
                    while (i < text.Length && text[i] == ' ') {
                        i++;
                    }
                    if (i >= text.Length || text[i] == ']') {
                        break;
                    }

                    if (text[i] == '_' || char.IsLetter(text[i])) {
                        // Collect all phonemes up to '<', ']', or ' '
                        // "dey<600,24>" -> [d, ey] with dur=600 note=24
                        var group = new List<short>();
                        while (i < text.Length && text[i] != '<' && text[i] != ']' && text[i] != ' ') {
                            if ((text[i] == 'J' || text[i] == 'j') && i + 3 < text.Length
                                && (text[i + 1] == 'P' || text[i + 1] == 'p') && text[i + 2] == '_') {
                                bool matchedJp = false;
                                if (i + 4 < text.Length && char.IsLetter(text[i + 3]) && char.IsLetter(text[i + 4])) {
                                    string code5 = ("jp_" + text[i + 3] + text[i + 4]).ToLowerInvariant();
                                    short p5 = MapPhoneme(code5);
                                    if (p5 >= 0) {
                                        group.Add(p5);
                                        i += 5;
                                        matchedJp = true;
                                    }
                                }
                                if (!matchedJp && char.IsLetter(text[i + 3])) {
                                    string code4 = ("jp_" + text[i + 3]).ToLowerInvariant();
                                    short p4 = MapPhoneme(code4);
                                    if (p4 >= 0) {
                                        group.Add(p4);
                                        i += 4;
                                        matchedJp = true;
                                    }
                                }
                                if (matchedJp) {
                                    continue;
                                }
                            }
                            if (text[i] == '_') {
                                group.Add(_SIL_);
                                i++;
                                continue;
                            }
                            bool matched2 = false;
                            if (i + 1 < text.Length && char.IsLetter(text[i + 1])) {
                                string two = string.Concat(text[i], text[i + 1]).ToLowerInvariant();
                                short op2 = MapPhoneme(two);
                                if (op2 >= 0) {
                                    group.Add(op2);
                                    i += 2;
                                    matched2 = true;
                                }
                            }
                            if (!matched2) {
                                string one = text[i].ToString().ToLowerInvariant();
                                short op1 = MapPhoneme(one);
                                group.Add(op1 >= 0 ? op1 : _SIL_);
                                i++;
                            }
                        }

                        int dur = 0, note = 0;
                        bool hasNote = false, noteIsNamed = false;
                        if (i < text.Length && text[i] == '<') {
                            hasNote = true;
                            i++;
                            while (i < text.Length && char.IsDigit(text[i])) {
                                dur = dur * 10 + (text[i++] - '0');
                            }
                            if (i < text.Length && text[i] == ',') {
                                i++;
                                while (i < text.Length && text[i] == ' ') {
                                    i++;
                                }
                                if (i < text.Length && char.IsLetter(text[i])) {
                                    int nameStart = i;
                                    while (i < text.Length && text[i] != '>' && text[i] != ']') {
                                        i++;
                                    }
                                    note = NoteNameToHz(text[nameStart..i].Trim());
                                    noteIsNamed = true;
                                } else {
                                    while (i < text.Length && char.IsDigit(text[i])) {
                                        note = note * 10 + (text[i++] - '0');
                                    }
                                }
                            }
                            while (i < text.Length && text[i] != '>' && text[i] != ']') {
                                i++;
                            }
                            if (i < text.Length && text[i] == '>') {
                                i++;
                            }
                        }

                        if (!hasNote && !inSingMode && blockSing.Count == 0) {
                            continue;
                        }

                        short pitch = hasNote
                            ? (noteIsNamed ? (short)note : (short)-note)
                            : lastPitch;
                        if (hasNote) {
                            lastPitch = pitch;
                        }

                        int durIdx = group.Count - 1;

                        // Subtract every other phoneme's minimum duration from the
                        // user-specified duration so the whole cluster fits the beat.
                        // We account for the 5ms initial silence and backend frame rounding.
                        int overhead = firstInBlock ? 5 : 0;
                        firstInBlock = false;
                        for (int gi2 = 0; gi2 < group.Count; gi2++) {
                            if (gi2 == durIdx) {
                                continue;
                            }
                            short p = group[gi2];
                            int m = (p == _SIL_) ? 5 : Tables.GetMinimumDuration(p);
                            overhead += (m / 5) * 5;
                        }
                        int adjustedDur = Math.Max(5, dur - overhead);

                        for (int gi = 0; gi < group.Count; gi++) {
                            long ctrl = kWord_Start | kContent_Word;
                            if (pitch != 0) {
                                ctrl |= kSingingPhon;
                            }
                            if (hasNote && gi == durIdx) {
                                ctrl |= kSingingDuration;
                            }
                            if (firstPhon) {
                                firstPhon = false;
                            } else {
                                ctrl &= ~(kWord_Start | kContent_Word);
                            }
                            blockSing.Add(new PhonemeToken {
                                Phon = group[gi],
                                Ctrl = ctrl,
                                UserDur = hasNote && gi == durIdx ? (short)adjustedDur : (short)0,
                                UserNote = (hasNote && gi == durIdx) ? pitch : (short)0,
                            });
                        }
                    } else {
                        i++;
                    }
                }
                if (i < text.Length && text[i] == ']') {
                    i++;
                }

                if (blockSing.Count > 0) {
                    FlushPlain();
                    segments.Add(new Segment(blockSing));
                }
            }

            FlushPlain();
            return segments;
        }

        public static string Parse(string text, out List<PhonemeToken>? singingTokens) {
            singingTokens = null;
            var segments = ParseSegments(text);

            var plain = new System.Text.StringBuilder();
            List<PhonemeToken>? sing = null;

            foreach (var seg in segments) {
                if (seg.IsSinging) {
                    sing ??= new List<PhonemeToken>();
                    sing.AddRange(seg.Singing!);
                } else if (!seg.IsCommand) {
                    plain.Append(seg.PlainText);
                }
            }

            singingTokens = sing;
            return plain.ToString();
        }

        public static string StripCommands(string text) {
            var result = Parse(text, out _);
            return result;
        }
    }
}  // namespace
