#nullable enable
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using System.Linq;
using static SharpTalk.AudioProcessor;

namespace SharpTalk {
    // Parser for Klattsch notation: a compact text format for phoneme-level singing and speech control.
    public sealed class KlattschParser {
        public class Token {
            public string Type = "";
            public string Text = "";
            public float Ms;
            public string Key = "";
            public float Value;
            public bool Relative;
            public bool Reset;
            public string Code = "";
            public bool Stressed;
            public float PitchDelta;
            public bool Transient;
            public bool Slurred;
        }

        private static readonly Dictionary<string, float> PauseDurationTable = new()
        {
            { ",", 100 }, { ";", 200 }, { ".", 300 }
        };

        private static readonly Dictionary<char, int> NoteSemitonesTable = new()
        {
            { 'C', 0 }, { 'D', 2 }, { 'E', 4 }, { 'F', 5 }, { 'G', 7 }, { 'A', 9 }, { 'B', 11 }
        };

        private static float NoteToHz(string name) {
            if (name.Length < 2) {
                return 0;
            }
            char letter = char.ToUpper(name[0]);
            if (!NoteSemitonesTable.ContainsKey(letter)) {
                return 0;
            }
            int i = 1;
            int semiadj = 0;
            if (i < name.Length && name[i] == '#') { semiadj = 1; i++; }
            else if (i < name.Length && name[i] == 'b') { semiadj = -1; i++; }
            if (i >= name.Length) {
                return 0;
            }
            bool neg = name[i] == '-';
            if (neg) {
                i++;
            }
            if (i >= name.Length || !char.IsDigit(name[i])) {
                return 0;
            }
            int octave = 0;
            while (i < name.Length && char.IsDigit(name[i])) {
                octave = octave * 10 + (name[i] - '0');
                i++;
            }
            if (i != name.Length) {
                return 0;
            }
            if (neg) {
                octave = -octave;
            }
            int semi = NoteSemitonesTable[letter] + semiadj;
            int midi = (octave + 1) * 12 + semi;
            return (float)(440.0 * Math.Pow(2.0, (midi - 69) / 12.0));
        }

        // Greek and Cyrillic characters that look identical to Latin letters in most fonts.
        // Notation files pasted from score editors or other systems may silently contain them.
        private static readonly Dictionary<char, char> HomoglyphMapTable = new()
        {
            {'Α','A'}, {'Β','B'}, {'Ε','E'}, {'Η','H'}, {'Ι','I'}, {'Κ','K'},
            {'Μ','M'}, {'Ν','N'}, {'Ο','O'}, {'Ρ','P'}, {'Τ','T'}, {'Υ','Y'}, {'Ζ','Z'},
            {'А','A'}, {'В','B'}, {'С','C'}, {'Е','E'}, {'Н','H'}, {'К','K'},
            {'М','M'}, {'О','O'}, {'Р','P'}, {'Т','T'},
            {'а','a'}, {'с','c'}, {'е','e'}, {'о','o'}, {'р','p'}
        };

        // NFKC-normalize, strip zero-width characters (ZWSP, ZWNJ, ZWJ, WJ, BOM),
        // then replace any remaining homoglyphs so downstream parsing sees plain ASCII.
        private static string Normalize(string input) {
            var sb = new StringBuilder();
            foreach (char c in input.Normalize(NormalizationForm.FormKC)) {
                if (c == 0x200B || c == 0x200C || c == 0x200D || c == 0x2060 || c == 0xFEFF) {
                    continue;
                }
                if (HomoglyphMapTable.TryGetValue(c, out char r)) {
                    sb.Append(r);
                } else {
                    sb.Append(c);
                }
            }
            return sb.ToString();
        }

        private static readonly Dictionary<char, string> DirectiveKeyMap = new()
        {
            {'b', "base"}, {'r', "rate"}, {'p', "pause"}, {'s', "scale"},
            {'v', "vibrato"}, {'w', "vibratoRate"},
            {'m', "tremolo"}, {'n', "tremoloRate"},
            {'h', "aspiration"}, {'t', "tilt"}, {'g', "effort"}
        };

        // Persistent state for Klattsch mode
        private static float _curF0 = 120;
        private static float _curRate = 110;
        private static float _curScale = 1.0f;
        private static float _curVibDepth = 0;
        private static float _curVibRate = 5;
        private static float _curTremDepth = 0;
        private static float _curTremRate = 5;
        private static float _curAsp = 0;
        private static float _curTilt = 0;
        private static float _curEffort = 0.5f;

        public static void Reset() {
            _curF0 = 120;
            _curRate = 110;
            _curScale = 1.0f;
            _curVibDepth = 0;
            _curVibRate = 5;
            _curTremDepth = 0;
            _curTremRate = 5;
            _curAsp = 0;
            _curTilt = 0;
            _curEffort = 0.5f;
        }

        // Classify one whitespace-delimited token from the Klattsch source.
        // Tries each shape in priority order: punctuation, bracket directives [KEY=N],
        // note names (bC4), compact directives (b120 r+10), phoneme codes (AE IY ...).
        // Returns null if the token is syntactically valid but produces no output (e.g. bare "p").
        private static Token? ClassifyPart(string part) {
            if (part == "(") {
                return new Token { Type = "syllable_open" };
            }
            if (part == ")") {
                return new Token { Type = "syllable_close" };
            }
            if (PauseDurationTable.TryGetValue(part, out float ms)) {
                return new Token { Type = "pause", Ms = ms };
            }
            if (part == "!" || part == "'") {
                return new Token { Type = "stress_mark" };
            }

            // [KEY=value] bracket directive
            if (part.Length >= 5 && part[0] == '[' && part[part.Length - 1] == ']') {
                int eq = part.IndexOf('=', 1, part.Length - 2);
                if (eq > 1) {
                    bool keyOk = true;
                    for (int ki = 1; ki < eq && keyOk; ki++) {
                        if (!char.IsLetterOrDigit(part[ki]) && part[ki] != '_') {
                            keyOk = false;
                        }
                    }
                    if (keyOk && float.TryParse(part.AsSpan(eq + 1, part.Length - eq - 2),
                            NumberStyles.Float, CultureInfo.InvariantCulture, out float bval)) {
                        return new Token { Type = "directive", Key = part.Substring(1, eq - 1), Value = bval, Relative = false };
                    }
                }
            }

            // note name directive: b[=]NoteOctave (e.g. bC4, b=A#3)
            if (part.Length >= 3 && (part[0] == 'b' || part[0] == 'B')) {
                int npos = 1;
                if (part[npos] == '=') {
                    npos++;
                }
                float hz = NoteToHz(part.Substring(npos));
                if (hz > 0) {
                    return new Token { Type = "directive", Key = "base", Value = hz, Relative = false };
                }
            }

            // compact directive: single lowercase key letter + optional [=][+-]number
            if (part[0] >= 'a' && part[0] <= 'z' && DirectiveKeyMap.TryGetValue(part[0], out string? key)) {
                if (part.Length == 1) {
                    if (key != "pause") {
                        return new Token { Type = "directive", Key = key, Reset = true };
                    }
                    return null;
                }
                int pos = 1;
                bool hasEq = part[pos] == '=';
                if (hasEq) {
                    pos++;
                }
                int numStart = pos;
                bool hasSign = pos < part.Length && (part[pos] == '+' || part[pos] == '-');
                if (hasSign) {
                    pos++;
                }
                if (pos < part.Length && char.IsDigit(part[pos])) {
                    while (pos < part.Length && char.IsDigit(part[pos])) {
                        pos++;
                    }
                    if (pos < part.Length && part[pos] == '.' && pos + 1 < part.Length && char.IsDigit(part[pos + 1])) {
                        pos++;
                        while (pos < part.Length && char.IsDigit(part[pos])) {
                            pos++;
                        }
                    }
                    if (pos == part.Length && float.TryParse(part.AsSpan(numStart),
                            NumberStyles.Float, CultureInfo.InvariantCulture, out float value)) {
                        return new Token { Type = "directive", Key = key, Value = value, Relative = !hasEq && hasSign };
                    }
                }
            }

            // phoneme token: optional [-^] slur, uppercase letters (code), optional ['!] stress,
            // optional pitch delta as (+-N) transient or +-N sticky
            {
                int pos = 0;
                bool slurred = pos < part.Length && (part[pos] == '-' || part[pos] == '^');
                if (slurred) {
                    pos++;
                }
                int codeStart = pos;
                while (pos < part.Length && char.IsUpper(part[pos])) {
                    pos++;
                }
                if (pos > codeStart) {
                    string code = part.Substring(codeStart, pos - codeStart);
                    if (KlattschToSharpTalkPhonemeTable.ContainsKey(code)) {
                        bool stressed = pos < part.Length && (part[pos] == '\'' || part[pos] == '!');
                        if (stressed) {
                            pos++;
                        }
                        float transientDelta = float.NaN;
                        float stickyDelta = float.NaN;
                        if (pos < part.Length && part[pos] == '(') {
                            pos++;
                            int numStart = pos;
                            if (pos < part.Length && (part[pos] == '+' || part[pos] == '-')) {
                                pos++;
                            }
                            while (pos < part.Length && (char.IsDigit(part[pos]) || part[pos] == '.')) {
                                pos++;
                            }
                            if (pos < part.Length && part[pos] == ')' &&
                                    float.TryParse(part.AsSpan(numStart, pos - numStart),
                                        NumberStyles.Float, CultureInfo.InvariantCulture, out float tv)) {
                                transientDelta = tv;
                                pos++;
                            }
                        } else if (pos < part.Length && (part[pos] == '+' || part[pos] == '-')) {
                            int numStart = pos++;
                            while (pos < part.Length && (char.IsDigit(part[pos]) || part[pos] == '.')) {
                                pos++;
                            }
                            if (pos == part.Length &&
                                    float.TryParse(part.AsSpan(numStart),
                                        NumberStyles.Float, CultureInfo.InvariantCulture, out float sv)) {
                                stickyDelta = sv;
                            }
                        }
                        if (pos == part.Length) {
                            return new Token {
                                Type = "phoneme",
                                Code = code,
                                Slurred = slurred,
                                Stressed = stressed,
                                PitchDelta = !float.IsNaN(transientDelta) ? transientDelta : (!float.IsNaN(stickyDelta) ? stickyDelta : 0),
                                Transient = !float.IsNaN(transientDelta)
                            };
                        }
                    }
                }
            }

            return new Token { Type = "unknown", Text = part };
        }

        // Scan Klattsch source text into a flat Token list.
        // Handles: # line comments, /* block comments (including mid-token),
        // syllable-group parens ( ), pause punctuation , ; . and stress marks ! '.
        // Stress marks retroactively mark the most recent phoneme token as stressed.
        public static List<Token> Tokenize(string rawInput) {
            string source = Normalize(rawInput);
            int len = source.Length;
            var tokens = new List<Token>();
            int i = 0;

            while (i < len) {
                char c = source[i];
                if (char.IsWhiteSpace(c)) {
                    i++;
                    continue;
                }
                if (c == '#' && (i == 0 || char.IsWhiteSpace(source[i - 1]))) {
                    while (i < len && source[i] != '\n') {
                        i++;
                    }
                    continue;
                }
                if (c == '/' && i + 1 < len && source[i + 1] == '*') {
                    i = source.IndexOf("*/", i + 2);
                    if (i == -1) {
                        i = len;
                    } else {
                        i += 2;
                    }
                    continue;
                }

                int start = i;
                var part = new StringBuilder();
                while (i < len && !char.IsWhiteSpace(source[i])) {
                    char cur = source[i];
                    if (cur == '/' && i + 1 < len && source[i + 1] == '*') {
                        int end = source.IndexOf("*/", i + 2);
                        if (end == -1) {
                            i = len;
                        } else {
                            i = end + 2;
                        }
                        continue;
                    }
                    // ) is a syllable-close token unless it ends a pitch expression (AE(+15)),
                    // in which case it always follows a digit. Split it off when it follows anything else.
                    if (cur == ')' && part.Length > 0 && !char.IsDigit(part[part.Length - 1])) {
                        break;
                    }
                    // , ; . are pause tokens, split them off when adjacent to phoneme content.
                    // Exception: . inside decimal notation always follows a digit (AE+1.5).
                    if ((cur == ',' || cur == ';') && part.Length > 0) {
                        break;
                    }
                    if (cur == '.' && part.Length > 0 && !char.IsDigit(part[part.Length - 1])) {
                        break;
                    }
                    part.Append(cur);
                    i++;
                }
                if (part.Length == 0) {
                    continue;
                }

                var tok = ClassifyPart(part.ToString());
                if (tok == null) {
                    continue;
                }

                if (tok.Type == "stress_mark") {
                    for (int j = tokens.Count - 1; j >= 0; j--) {
                        if (tokens[j].Type == "phoneme") {
                            tokens[j].Stressed = true;
                            break;
                        }
                    }
                    continue;
                }
                tokens.Add(tok);
            }
            return tokens;
        }

        private static readonly HashSet<string> StopPhonemeCodesTable = new()
        {
            "P", "B", "T", "D", "K", "G", "CH", "JH"
        };

        public static readonly IReadOnlyDictionary<short, string> PhonemeNamesTable =
            new Dictionary<short, string>
            {
                { _IY_, "IY" }, { _IH_, "IH" }, { _EH_, "EH" }, { _AE_, "AE" },
                { _AA_, "AA" }, { _AO_, "AO" }, { _AH_, "AH" }, { _UH_, "UH" },
                { _UW_, "UW" }, { _ER_, "ER" }, { _AY_, "AY" }, { _AW_, "AW" },
                { _EY_, "EY" }, { _OW_, "OW" }, { _OY_, "OY" },
                { _W_,  "W"  }, { _Y_,  "Y"  }, { _R_,  "R"  }, { _L_,  "L"  },
                { _M_,  "M"  }, { _N_,  "N"  }, { _NG_, "NG" }, { _HH_, "HH" },
                { _F_,  "F"  }, { _TH_, "TH" }, { _S_,  "S"  }, { _SH_, "SH" },
                { _V_,  "V"  }, { _DH_, "DH" }, { _Z_,  "Z"  }, { _ZH_, "ZH" },
                { _P_,  "P"  }, { _B_,  "B"  }, { _T_,  "T"  }, { _D_,  "D"  },
                { _K_,  "K"  }, { _G_,  "G"  }, { _CH_, "CH" }, { _JH_, "JH" },
                { _AX_, "AX" }, { _IX_, "IX" }, { _YU_, "YU" },
                { _RX_, "RX" }, { _LX_, "LX" }, { _EL_, "EL" }, { _EN_, "EN" },
                { _DX_, "DX" }, { _TX_, "TX" },
                { _JP_A_, "JP_A" }, { _JP_I_, "JP_I" }, { _JP_U_, "JP_U" },
                { _JP_E_, "JP_E" }, { _JP_O_, "JP_O" },
            };

        private static readonly Dictionary<string, short> KlattschToSharpTalkPhonemeTable = new()
        {
            { "IY", _IY_ }, { "IH", _IH_ }, { "EH", _EH_ }, { "AE", _AE_ },
            { "AA", _AA_ }, { "AO", _AO_ }, { "AH", _AH_ }, { "UH", _UH_ },
            { "UW", _UW_ }, { "ER", _ER_ }, { "AY", _AY_ }, { "AW", _AW_ },
            { "EY", _EY_ }, { "OW", _OW_ }, { "OY", _OY_ },
            { "W", _W_ }, { "Y", _Y_ }, { "R", _R_ }, { "L", _L_ },
            { "M", _M_ }, { "N", _N_ }, { "NG", _NG_ },
            { "F", _F_ }, { "TH", _TH_ }, { "S", _S_ }, { "SH", _SH_ },
            { "V", _V_ }, { "DH", _DH_ }, { "Z", _Z_ }, { "ZH", _ZH_ },
            { "HH", _HH_ },
            { "P", _P_ }, { "B", _B_ }, { "T", _T_ }, { "D", _D_ },
            { "K", _K_ }, { "G", _G_ }, { "CH", _CH_ }, { "JH", _JH_ },
            { "AX", _AX_ }, { "IX", _IX_ }, { "YU", _YU_ },
            { "RX", _RX_ }, { "LX", _LX_ }, { "EL", _EL_ }, { "EN", _EN_ },
            { "DX", _DX_ }, { "TX", _TX_ },
            { "_", _SIL_ },
            { "A", _JP_A_ }, { "I", _JP_I_ }, { "U", _JP_U_ }, { "E", _JP_E_ }, { "O", _JP_O_ },
        };

        // Convert a Klattsch token list to PhonemeTokens ready for AudioProcessor.
        //
        // Directives update persistent state (_curF0, _curRate, etc.) as they are encountered.
        // Phonemes outside syllable groups get full beat duration (_curRate ms, x1.5 if stressed).
        // Phonemes inside ( ) groups share the beat: stops get a short burst, others split the rest.
        // Pauses and p-directives emit SIL tokens with exact millisecond durations.
        // A trailing SIL with kTerm_End is always appended so AudioProcessor sees a clean clause end.
        public static List<PhonemeToken> CompileToTokens(List<Token> tokens) {
            Reset();
            var result = new List<PhonemeToken>();
            bool inSyllable = false;
            var syllableQueue = new List<Token>();

            void EmitPhoneme(Token t, float durationMs, bool isStartOfBeat, bool isEndOfBeat) {
                if (!KlattschToSharpTalkPhonemeTable.TryGetValue(t.Code, out short phonId)) {
                    return;
                }

                float startF0 = t.Stressed ? _curF0 + 8 : _curF0;
                float endF0 = startF0 + t.PitchDelta;

                // Singing flags. We treat each beat as a word for coarticulation purposes.
                long ctrl = kSingingPhon | kSingingDuration | kContent_Word;
                if (isStartOfBeat && !t.Slurred) {
                    ctrl |= kWord_Start | kSyllable_Start;
                }
                if (isEndOfBeat) {
                    ctrl |= kWord_End;
                }

                // Positive note (IIR settle) for stable-pitch phonemes, snaps to target fast,
                // avoids linear portamento glide from stale TTS pitch at block start.
                // Negative note (portamento) for pitch-delta phonemes, linear glide to endF0.
                short note = (t.PitchDelta != 0) ? (short)-endF0 : (short)startF0;

                result.Add(new PhonemeToken {
                    Phon = phonId,
                    Ctrl = ctrl,
                    UserNote = note,
                    UserDur = (short)Math.Max(5, durationMs),
                    Aspiration = (byte)Math.Clamp(_curAsp * 100, 0, 100),
                    Tilt = (byte)Math.Clamp(_curTilt * 100, 0, 100),
                    Effort = (byte)Math.Clamp(_curEffort * 100, 0, 100),
                    VibDepth = (byte)Math.Clamp(_curVibDepth, 0, 255),
                    VibRate = (byte)Math.Clamp(_curVibRate * 10, 0, 255),
                    TremDepth = (byte)Math.Clamp(_curTremDepth * 100, 0, 100),
                    TremRate = (byte)Math.Clamp(_curTremRate * 10, 0, 255)
                });

                if (!t.Transient) {
                    _curF0 += t.PitchDelta;
                }
            }

            void FlushSyllable() {
                if (syllableQueue.Count == 0) {
                    inSyllable = false;
                    return;
                }

                // Stops/affricates get a short burst slot; the saved time flows to non-stops.
                // Mirrors Klattsch JS, burstMs = min(stopBurstMs, equalSlot * 0.3).
                float equalSlot = _curRate / syllableQueue.Count;
                float stopBurst = Math.Min(25f, equalSlot * 0.3f);
                int nStops = syllableQueue.Count(t => StopPhonemeCodesTable.Contains(t.Code));
                int nOther = syllableQueue.Count - nStops;
                float otherDur = nOther > 0 ? (_curRate - nStops * stopBurst) / nOther : equalSlot;

                for (int i = 0; i < syllableQueue.Count; i++) {
                    float dur = StopPhonemeCodesTable.Contains(syllableQueue[i].Code) ? stopBurst : otherDur;
                    EmitPhoneme(syllableQueue[i], dur, i == 0, i == syllableQueue.Count - 1);
                }
                syllableQueue.Clear();
                inSyllable = false;
            }

            foreach (var t in tokens) {
                if (t.Type == "syllable_open") {
                    inSyllable = true;
                    continue;
                }
                if (t.Type == "syllable_close") {
                    FlushSyllable();
                    continue;
                }
                if (t.Type == "pause") {
                    FlushSyllable();
                    result.Add(new PhonemeToken {
                        Phon = _SIL_,
                        Ctrl = kSingingPhon | kSingingDuration | kWord_End,
                        UserDur = (short)t.Ms,
                        UserNote = (short)-_curF0
                    });
                    continue;
                }
                if (t.Type == "directive") {
                    switch (t.Key) {
                        case "base": if (t.Reset) _curF0 = 120; else if (t.Relative) _curF0 += t.Value; else _curF0 = t.Value; break;
                        case "rate": if (t.Reset) _curRate = 110; else if (t.Relative) _curRate += t.Value; else _curRate = t.Value; break;
                        case "scale": if (t.Reset) _curScale = 1.0f; else if (t.Relative) _curScale += t.Value; else _curScale = t.Value; break;
                        case "vibrato": if (t.Reset) _curVibDepth = 0; else if (t.Relative) _curVibDepth += t.Value; else _curVibDepth = t.Value; break;
                        case "vibratoRate": if (t.Reset) _curVibRate = 5; else if (t.Relative) _curVibRate += t.Value; else _curVibRate = t.Value; break;
                        case "tremolo": if (t.Reset) _curTremDepth = 0; else if (t.Relative) _curTremDepth += t.Value; else _curTremDepth = t.Value; break;
                        case "tremoloRate": if (t.Reset) _curTremRate = 5; else if (t.Relative) _curTremRate += t.Value; else _curTremRate = t.Value; break;
                        case "aspiration": if (t.Reset) _curAsp = 0; else if (t.Relative) _curAsp += t.Value; else _curAsp = t.Value; break;
                        case "tilt": if (t.Reset) _curTilt = 0; else if (t.Relative) _curTilt += t.Value; else _curTilt = t.Value; break;
                        case "effort": if (t.Reset) _curEffort = 0.5f; else if (t.Relative) _curEffort += t.Value; else _curEffort = t.Value; break;
                        case "pause":
                            result.Add(new PhonemeToken {
                                Phon = _SIL_,
                                Ctrl = kSingingPhon | kSingingDuration | kWord_End,
                                UserDur = (short)Math.Abs(t.Value),
                                UserNote = (short)-_curF0
                            });
                            break;
                    }
                    continue;
                }
                if (t.Type == "phoneme") {
                    if (inSyllable) {
                        syllableQueue.Add(t);
                    } else {
                        float phoneDur = t.Stressed ? _curRate * 1.5f : _curRate;
                        EmitPhoneme(t, phoneDur, true, true);
                    }
                }
            }
            if (inSyllable) {
                FlushSyllable();
            }

            result.Add(new PhonemeToken {
                Phon = _SIL_,
                Ctrl = kSingingPhon | kSingingDuration | kTerm_End | kWord_End,
                UserDur = 150,
                UserNote = (short)-_curF0
            });

            return result;
        }
    }
}
