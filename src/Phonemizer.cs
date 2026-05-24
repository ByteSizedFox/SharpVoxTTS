#nullable enable
using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;
using static SharpTalk.AudioProcessor;
using static SharpTalk.Phonemizer.Normalizer;

namespace SharpTalk {

    public class Phonemizer {
        readonly DictReader _dict;
        readonly Func<string, byte[]?> _symbols;

        public int StatDict { get; private set; }
        public int StatMorph { get; private set; }
        public int StatLts { get; private set; }
        public void ResetStats() { StatDict = StatMorph = StatLts = 0; }
        public DictReader Dict => _dict;

        // Local aliases for AudioProcessor opcode constants
        const byte OP_STRESS1 = kOpStress1;
        const byte OP_STRESS2 = kOpStress2;
        const byte OP_EMPHSTRESS = kOpEmphStress;
        const byte OP_SYLL = kOpSyll;
        const byte OP_WORD = kOpWord;
        const byte OP_PREP = kOpPrep;
        const byte OP_VERB = kOpVerb;
        const byte OP_COMMA = kOpComma;
        const byte OP_PERIOD = kOpPeriod;
        const byte OP_QUEST = kOpQuest;
        const byte OP_EXCLAM = kOpExclam;

        // Subject pronouns: receive kPronounWord on every phoneme so the backend can
        // apply vocal-confidence emphasis (pitch accent + vowel lengthening).
        static readonly HashSet<string> PronounWordsTable = new(StringComparer.OrdinalIgnoreCase)
        {
            "i", "you", "he", "she", "it", "we", "they"
        };

        // Function words do NOT receive kContent_Word, primary dict stress is
        // suppressed so they don't drive pitch peaks in the BackEnd pitch algorithm.
        // Mirrors POS-based content/function distinction.
        static readonly HashSet<string> FunctionWordsTable = new(StringComparer.OrdinalIgnoreCase)
    {
        // articles / determiners
        "a", "an", "the",
        // prepositions
        "of", "in", "on", "at", "by", "for", "to", "up", "as", "into",
        "from", "with", "about", "over", "under", "out", "off", "than",
        // coordinating conjunctions
        "and", "or", "but", "nor", "yet", "so",
        // subordinating conjunctions
        "if", "that", "than", "when", "while", "because", "though",
        "although", "unless", "until", "since", "after", "before",
        // auxiliaries & copula
        "be", "am", "is", "are", "was", "were", "been", "being",
        "have", "has", "had", "do", "does", "did",
        "will", "would", "could", "should", "may", "might", "shall",
        "can", "must", "ought",
        // subject / object pronouns
        "i", "he", "she", "we", "they", "you", "it",
        "me", "him", "her", "us", "them",
        // possessive determiners
        "my", "your", "his", "its", "our", "their",
        // other function words
        "not", "no", "there", "here",
    };

        static readonly Regex TokenRe = new(
            @"(\d+)|([a-zA-Z]+(?:'[a-zA-Z]+)*)|([,;:])|(\.\.\.|[.!?~])|(\s+)",
            RegexOptions.Compiled);

#if !SANDBOX
        public Phonemizer() : this(LibraryData.dictionary, LibraryData.SymbolsTable) { }
#endif

        public Phonemizer(byte[] dictData, IReadOnlyDictionary<string, byte[]> symbolsTable) {
            _dict = new DictReader(dictData);
            _symbols = sym => symbolsTable.TryGetValue(sym, out var v) ? v : null;
        }

        public short LastEndPunct { get; private set; } = _Period_;

        public (PhonemeToken[] Tokens, short EndPunct)[] TextToSentenceTokens(string text) {
            var result = new List<(PhonemeToken[], short)>();
            var segments = EmbeddedCmd.ParseSegments(text);

            foreach (var seg in segments) {
                if (seg.IsCommand) {
                    continue; // handled by TtsEngine, not FrontEnd
                }

                if (seg.IsSinging) {
                    // Each singing block is its own clause  never mix with speech
                    if (seg.Singing!.Count > 0) {
                        result.Add((seg.Singing.ToArray(), 0));
                    }
                    continue;
                }

                // Split at sentence boundaries (.!?) and clause boundaries (,;:).
                // Each clause gets its own BackEnd.Process call so pitch resets cleanly.
                string plain = Normalize(seg.PlainText!);
                int start = 0;
                foreach (Match m in TokenRe.Matches(plain)) {
                    if (!m.Groups[4].Success && !m.Groups[3].Success) {
                        continue;
                    }
                    string sentence = plain[start..(m.Index + m.Length)];
                    var tokens = TextSegmentToPhonemes(sentence);
                    result.Add((tokens, LastEndPunct));
                    start = m.Index + m.Length;
                }
                if (start < plain.Length) {
                    string remaining = plain[start..];
                    if (remaining.Trim().Length > 0) {
                        var tokens = TextSegmentToPhonemes(remaining);
                        result.Add((tokens, LastEndPunct));
                    }
                }
            }

            if (result.Count == 0) {
                var tokens = TextToPhonemes(text);
                result.Add((tokens, LastEndPunct));
            }

            return result.ToArray();
        }

        // Process a pure-text span (no embedded commands) into phoneme tokens.
        private PhonemeToken[] TextSegmentToPhonemes(string text) {
            var tokens = new List<PhonemeToken>();
            LastEndPunct = _Period_;

            var matches = TokenRe.Matches(Normalize(text));

            var ctxWords = new List<string>();
            foreach (Match wm in matches) {
                if (wm.Groups[2].Success) {
                    ctxWords.Add(wm.Groups[2].Value.ToUpperInvariant());
                }
            }
            int wordIdx = 0;

            foreach (Match m in matches) {
                if (m.Groups[1].Success) {
                    if (long.TryParse(m.Groups[1].Value, out long n)) {
                        AppendWordTokens(tokens, NumberToPhonStream(n), isContent: true);
                    }
                } else if (m.Groups[2].Success) {
                    string word = m.Groups[2].Value;
                    string upper = word.ToUpperInvariant();
                    byte[]? stream = HeteronymResolver.Resolve(ctxWords, wordIdx);
                    if (stream == null && IsAllCaps(word) && _dict.Search(upper) == null) {
                        stream = SpellOutAcronym(upper);
                    }
                    stream ??= WordToPhonStream(upper);
                    AppendWordTokens(tokens, stream, !FunctionWordsTable.Contains(word), PronounWordsTable.Contains(word));
                    wordIdx++;
                } else if (m.Groups[3].Success) {
                    tokens.Add(new PhonemeToken {
                        Phon = _SIL_,
                        Ctrl = kTerm_Bound | ((long)kBND_Pause << kSilenceTypeShift),
                    });
                    LastEndPunct = _Comma_;
                } else if (m.Groups[4].Success) {
                    char p = m.Groups[4].Value[0];
                    string p4 = m.Groups[4].Value;
                    LastEndPunct = p4 == "..." ? _Ellipsis_
                                 : p4 == "?" ? _Quest_
                                 : p4 == "!" ? _Exclam_
                                 : p4 == "~" ? _Tilde_
                                 : _Period_;
                }
            }

            return tokens.ToArray();
        }

        public PhonemeToken[] TextToPhonemes(string text) {
            var tokens = new List<PhonemeToken>();
            LastEndPunct = _Period_;

            // Split into ordered segments (plain text spans interleaved with singing blocks)
            var segments = EmbeddedCmd.ParseSegments(text);

            foreach (var seg in segments) {
                if (seg.IsCommand) {
                    continue; // handled by TtsEngine, not FrontEnd
                }

                if (seg.IsSinging) {
                    tokens.AddRange(seg.Singing!);
                    continue;
                }

                var matches = TokenRe.Matches(Normalize(seg.PlainText!));

                // Pre-extract word list for heteronym context resolution.
                var ctxWords = new List<string>();
                foreach (Match wm in matches) {
                    if (wm.Groups[2].Success) {
                        ctxWords.Add(wm.Groups[2].Value.ToUpperInvariant());
                    }
                }
                int wordIdx = 0;

                foreach (Match m in matches) {
                    if (m.Groups[1].Success) {           // number
                        if (long.TryParse(m.Groups[1].Value, out long n)) {
                            AppendWordTokens(tokens, NumberToPhonStream(n), isContent: true);
                        }
                    } else if (m.Groups[2].Success) {      // word
                        string word = m.Groups[2].Value;
                        bool isContent = !FunctionWordsTable.Contains(word);
                        var stream = HeteronymResolver.Resolve(ctxWords, wordIdx)
                                     ?? WordToPhonStream(word.ToUpperInvariant());
                        AppendWordTokens(tokens, stream, isContent, PronounWordsTable.Contains(word));
                        wordIdx++;
                    } else if (m.Groups[3].Success) {      // , ;
                        tokens.Add(new PhonemeToken {
                            Phon = _SIL_,
                            Ctrl = kTerm_Bound | ((long)kBND_Pause << kSilenceTypeShift),
                        });
                        LastEndPunct = _Comma_;
                    } else if (m.Groups[4].Success) {      // ... . ! ? ~
                        string p4 = m.Groups[4].Value;
                        LastEndPunct = p4 == "..." ? _Ellipsis_
                                     : p4 == "?" ? _Quest_
                                     : p4 == "!" ? _Exclam_
                                     : p4 == "~" ? _Tilde_
                                     : _Period_;
                    }
                    // whitespace: skip
                }
            }

            return tokens.ToArray();
        }

        // Text normalization
        // Nested static class keeps normalizer state (regexes, tables) out of the
        // FrontEnd field list without a separate file.
        internal static class Normalizer {
            // Repeated-syllable words: "hahaha" -> "ha ha ha", "lolol" -> "lol ol"
            // Fires for 3+ repetitions of a 1-3 char unit. Rare in real English at that count.
            // Non-greedy {1,3}? so "iiiiiiiii" splits on "i" not "iii".
            static readonly Regex ReReduplicate = new(
                @"\b([a-zA-Z]{1,3}?)\1{2,}\b", RegexOptions.Compiled);

            static readonly Regex ReParentheses = new(
                @"\s*\(([^)]+)\)", RegexOptions.Compiled);

            // Longest dotted abbreviations first so "w.r.t." doesn't shadow a shorter prefix.
            static readonly string[] DottedAbbrevKeys = { "w.r.t.", "i.e.", "e.g.", "a.m.", "p.m.", "p.s.", "b.c.", "a.d." };

            static readonly Dictionary<string, string> DottedAbbreviationMapTable =
                new(StringComparer.OrdinalIgnoreCase) {
                    ["i.e."] = "that is",
                    ["e.g."] = "for example",
                    ["a.m."] = "ay em",
                    ["p.m."] = "pee em",
                    ["p.s."] = "postscript",
                    ["w.r.t."] = "with regard to",
                    ["b.c."] = "bee see",
                    ["a.d."] = "ay dee",
                };

            static readonly Dictionary<string, string> AbbreviationMapTable =
                new(StringComparer.OrdinalIgnoreCase) {
                    // Titles
                    ["Dr"] = "Doctor",
                    ["Mr"] = "Mister",
                    ["Mrs"] = "Missus",
                    ["Ms"] = "Miss",
                    ["Prof"] = "Professor",
                    ["Jr"] = "Junior",
                    ["Sr"] = "Senior",
                    // Common
                    ["Vs"] = "versus",
                    ["Etc"] = "etcetera",
                    ["Approx"] = "approximately",
                    ["Max"] = "maximum",
                    ["Min"] = "minimum",
                    ["Avg"] = "average",
                    ["Vol"] = "volume",
                    ["Fig"] = "figure",
                    ["Ref"] = "reference",
                    ["Est"] = "established",
                    ["Cont"] = "continued",
                    ["Abbr"] = "abbreviation",
                    ["Attr"] = "attributed",
                    ["Dist"] = "district",
                    ["Pop"] = "population",
                    ["Temp"] = "temperature",
                    ["Tech"] = "technical",
                    ["Elec"] = "electric",
                    // Addresses
                    ["St"] = "Street",
                    ["Ave"] = "Avenue",
                    ["Blvd"] = "Boulevard",
                    ["Rd"] = "Road",
                    ["Ln"] = "Lane",
                    // Military / ranks
                    ["Lt"] = "Lieutenant",
                    ["Cpt"] = "Captain",
                    ["Capt"] = "Captain",
                    ["Gen"] = "General",
                    ["Sgt"] = "Sergeant",
                    ["Pvt"] = "Private",
                    ["Col"] = "Colonel",
                    ["Maj"] = "Major",
                    ["Rev"] = "Reverend",
                    // Org
                    ["Dept"] = "Department",
                    ["Inc"] = "Incorporated",
                    ["Corp"] = "Corporation",
                    ["Govt"] = "government",
                    ["Div"] = "division",
                    ["Intl"] = "international",
                    ["Natl"] = "national",
                    ["Assoc"] = "association",
                    ["Admin"] = "administration",
                    ["Asst"] = "assistant",
                    ["Mgr"] = "manager",
                    ["Dir"] = "director",
                    // Months
                    ["Jan"] = "January",
                    ["Feb"] = "February",
                    ["Mar"] = "March",
                    ["Apr"] = "April",
                    ["Jun"] = "June",
                    ["Jul"] = "July",
                    ["Aug"] = "August",
                    ["Sep"] = "September",
                    ["Sept"] = "September",
                    ["Oct"] = "October",
                    ["Nov"] = "November",
                    ["Dec"] = "December",
                };

            static string SplitCamelCase(string text) {
                if (text.Length < 2) {
                    return text;
                }
                var sb = new StringBuilder(text.Length + 4);
                for (int i = 0; i < text.Length; i++) {
                    char c = text[i];
                    if (i > 0 && char.IsUpper(c)) {
                        char prev = text[i - 1];
                        bool nextLower = i + 1 < text.Length && char.IsLower(text[i + 1]);
                        if (char.IsLower(prev) || (char.IsUpper(prev) && nextLower)) {
                            sb.Append(' ');
                        }
                    }
                    sb.Append(c);
                }
                return sb.ToString();
            }

            static string ReplaceCurrency(string text) {
                var sb = new StringBuilder(text.Length + 16);
                int i = 0;
                while (i < text.Length) {
                    if (text[i] != '$') { sb.Append(text[i++]); continue; }
                    int start = i++;
                    while (i < text.Length && char.IsWhiteSpace(text[i])) { i++; }
                    if (i >= text.Length || !char.IsDigit(text[i])) {
                        sb.Append('$');
                        i = start + 1;
                        continue;
                    }
                    int dolStart = i;
                    while (i < text.Length && char.IsDigit(text[i])) { i++; }
                    long dollars = long.Parse(text.AsSpan(dolStart, i - dolStart));
                    sb.Append(dollars);
                    sb.Append(dollars == 1 ? " dollar" : " dollars");
                    if (i + 1 < text.Length && text[i] == '.' && char.IsDigit(text[i + 1])) {
                        i++;
                        int centsStart = i;
                        int centsLen = 0;
                        while (i < text.Length && char.IsDigit(text[i]) && centsLen < 2) { i++; centsLen++; }
                        string centsStr = text.Substring(centsStart, centsLen).PadRight(2, '0')[..2];
                        long cents = long.Parse(centsStr);
                        if (cents > 0) {
                            sb.Append(" and ");
                            sb.Append(cents);
                            sb.Append(cents == 1 ? " cent" : " cents");
                        }
                    }
                }
                return sb.ToString();
            }

            static string ReplacePercent(string text) {
                var sb = new StringBuilder(text.Length + 8);
                int i = 0;
                while (i < text.Length) {
                    if (!char.IsDigit(text[i])) { sb.Append(text[i++]); continue; }
                    int numStart = i;
                    while (i < text.Length && char.IsDigit(text[i])) { i++; }
                    int numEnd = i;
                    int ws = i;
                    while (ws < text.Length && char.IsWhiteSpace(text[ws])) { ws++; }
                    if (ws < text.Length && text[ws] == '%') {
                        sb.Append(text, numStart, numEnd - numStart);
                        sb.Append(" percent");
                        i = ws + 1;
                    } else {
                        sb.Append(text, numStart, numEnd - numStart);
                    }
                }
                return sb.ToString();
            }

            static string ReplaceOrdinals(string text) {
                var sb = new StringBuilder(text.Length);
                int i = 0;
                while (i < text.Length) {
                    if (!char.IsDigit(text[i]) ||
                            (i > 0 && (char.IsLetterOrDigit(text[i - 1]) || text[i - 1] == '_'))) {
                        sb.Append(text[i++]);
                        continue;
                    }
                    int numStart = i;
                    while (i < text.Length && char.IsDigit(text[i])) { i++; }
                    int numEnd = i;
                    int ws = i;
                    while (ws < text.Length && char.IsWhiteSpace(text[ws])) { ws++; }
                    bool isOrdinal = false;
                    int afterSuffix = ws + 2;
                    if (ws + 2 <= text.Length) {
                        char s0 = char.ToLower(text[ws]), s1 = char.ToLower(text[ws + 1]);
                        bool hasSuffix = (s0 == 's' && s1 == 't') || (s0 == 'n' && s1 == 'd') ||
                                         (s0 == 'r' && s1 == 'd') || (s0 == 't' && s1 == 'h');
                        if (hasSuffix && (afterSuffix >= text.Length || !char.IsLetterOrDigit(text[afterSuffix]))) {
                            isOrdinal = true;
                        }
                    }
                    if (isOrdinal) {
                        long n = long.Parse(text.AsSpan(numStart, numEnd - numStart));
                        sb.Append(OrdinalToWord(n));
                        i = afterSuffix;
                    } else {
                        sb.Append(text, numStart, numEnd - numStart);
                    }
                }
                return sb.ToString();
            }

            static bool IsOrdinalSuffixAt(string text, int pos) {
                if (pos + 2 > text.Length) {
                    return false;
                }
                char s0 = char.ToLower(text[pos]), s1 = char.ToLower(text[pos + 1]);
                bool isSuffix = (s0 == 's' && s1 == 't') || (s0 == 'n' && s1 == 'd') ||
                                (s0 == 'r' && s1 == 'd') || (s0 == 't' && s1 == 'h');
                return isSuffix && (pos + 2 >= text.Length || !char.IsLetterOrDigit(text[pos + 2]));
            }

            static string ReplaceYears(string text) {
                var sb = new StringBuilder(text.Length + 16);
                int i = 0;
                while (i < text.Length) {
                    if (!char.IsDigit(text[i])) { sb.Append(text[i++]); continue; }
                    int runStart = i;
                    while (i < text.Length && char.IsDigit(text[i])) { i++; }
                    int runLen = i - runStart;
                    bool asYear = false;
                    if (runLen == 4) {
                        char d0 = text[runStart], d1 = text[runStart + 1];
                        bool inRange = d0 == '1' || (d0 == '2' && d1 == '0');
                        if (inRange) {
                            bool prevOk = runStart == 0 ||
                                (text[runStart - 1] != '.' && text[runStart - 1] != '$' &&
                                 text[runStart - 1] != '€' && text[runStart - 1] != '£' &&
                                 !char.IsLetterOrDigit(text[runStart - 1]) && text[runStart - 1] != '_');
                            int la = i;
                            while (la < text.Length && char.IsWhiteSpace(text[la])) { la++; }
                            bool blocked = la < text.Length &&
                                (char.IsDigit(text[la]) || text[la] == '%' || IsOrdinalSuffixAt(text, la));
                            asYear = prevOk && !blocked;
                        }
                    }
                    if (asYear) {
                        sb.Append(YearToWords(int.Parse(text.AsSpan(runStart, 4))));
                    } else {
                        sb.Append(text, runStart, runLen);
                    }
                }
                return sb.ToString();
            }

            static string ReplaceDecimals(string text) {
                var sb = new StringBuilder(text.Length + 16);
                int i = 0;
                while (i < text.Length) {
                    if (!char.IsDigit(text[i]) ||
                            (i > 0 && (char.IsLetterOrDigit(text[i - 1]) || text[i - 1] == '_'))) {
                        sb.Append(text[i++]);
                        continue;
                    }
                    int numStart = i;
                    while (i < text.Length && char.IsDigit(text[i])) { i++; }
                    if (i + 1 < text.Length && text[i] == '.' && char.IsDigit(text[i + 1])) {
                        int fracStart = i + 1;
                        int j = fracStart;
                        while (j < text.Length && char.IsDigit(text[j])) { j++; }
                        if (j >= text.Length || !char.IsLetterOrDigit(text[j])) {
                            sb.Append(text, numStart, i - numStart);
                            sb.Append(" point");
                            for (int k = fracStart; k < j; k++) {
                                sb.Append(' ');
                                sb.Append(DigitWordsTable[text[k] - '0']);
                            }
                            i = j;
                            continue;
                        }
                    }
                    sb.Append(text, numStart, i - numStart);
                }
                return sb.ToString();
            }

            static string ReplaceDottedAbbrevs(string text) {
                var sb = new StringBuilder(text.Length + 16);
                int i = 0;
                while (i < text.Length) {
                    bool prevIsWord = i > 0 && (char.IsLetterOrDigit(text[i - 1]) || text[i - 1] == '_');
                    if (!prevIsWord) {
                        bool found = false;
                        foreach (string abbr in DottedAbbrevKeys) {
                            if (i + abbr.Length <= text.Length &&
                                    string.Compare(text, i, abbr, 0, abbr.Length, StringComparison.OrdinalIgnoreCase) == 0) {
                                int end = i + abbr.Length;
                                if (end >= text.Length || !(char.IsLetterOrDigit(text[end]) || text[end] == '_')) {
                                    sb.Append(DottedAbbreviationMapTable[abbr]);
                                    i = end;
                                    found = true;
                                    break;
                                }
                            }
                        }
                        if (found) { continue; }
                    }
                    sb.Append(text[i++]);
                }
                return sb.ToString();
            }

            static string ReplaceAbbrevs(string text) {
                var sb = new StringBuilder(text.Length + 32);
                int i = 0;
                while (i < text.Length) {
                    bool prevIsWord = i > 0 && (char.IsLetterOrDigit(text[i - 1]) || text[i - 1] == '_');
                    if (!prevIsWord && char.IsLetter(text[i])) {
                        int wordStart = i;
                        while (i < text.Length && char.IsLetter(text[i])) { i++; }
                        if (i < text.Length && text[i] == '.' &&
                                AbbreviationMapTable.TryGetValue(text.Substring(wordStart, i - wordStart), out var expansion)) {
                            sb.Append(expansion);
                            i++;
                            continue;
                        }
                        sb.Append(text, wordStart, i - wordStart);
                        continue;
                    }
                    sb.Append(text[i++]);
                }
                return sb.ToString();
            }

            static readonly string[] DigitWordsTable =
                new string[] { "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine" };
            static readonly string[] TeenWordsTable =
                new string[] { "ten","eleven","twelve","thirteen","fourteen","fifteen",
                               "sixteen","seventeen","eighteen","nineteen" };

            static string SmallCardinal(int n) {
                if (n == 0) {
                    return "zero";
                }
                if (n < 10) {
                    return DigitWordsTable[n];
                }
                if (n < 20) {
                    return TeenWordsTable[n - 10];
                }
                int t = n / 10, o = n % 10;
                return TensWordsTable[t] + (o > 0 ? " " + DigitWordsTable[o] : "");
            }

            static string YearToWords(int y) {
                int hi = y / 100;
                int lo = y % 100;
                if (y == 2000) {
                    return "two thousand";
                }
                if (y > 2000 && y < 2010) {
                    return "two thousand " + SmallCardinal(lo);
                }
                string hiPart = SmallCardinal(hi);
                if (lo == 0) {
                    return hiPart + " hundred";
                }
                if (lo < 10) {
                    return hiPart + " oh " + SmallCardinal(lo);
                }
                return hiPart + " " + SmallCardinal(lo);
            }

            static readonly string[] OnesOrdinalTable = new string[]
            {
            "zeroth","first","second","third","fourth","fifth","sixth","seventh",
            "eighth","ninth","tenth","eleventh","twelfth","thirteenth","fourteenth",
            "fifteenth","sixteenth","seventeenth","eighteenth","nineteenth",
            };
            static readonly string[] TensOrdinalTable = new string[]
                {"","","twentieth","thirtieth","fortieth","fiftieth",
             "sixtieth","seventieth","eightieth","ninetieth"};
            static readonly string[] TensWordsTable = new string[]
                {"","","twenty","thirty","forty","fifty","sixty","seventy","eighty","ninety"};

            static string OrdinalToWord(long n) {
                if (n < 0) {
                    return n.ToString();
                }
                if (n < 20) {
                    return OnesOrdinalTable[n];
                }
                if (n < 100) {
                    int t = (int)(n / 10), o = (int)(n % 10);
                    return o == 0 ? TensOrdinalTable[t] : TensWordsTable[t] + " " + OnesOrdinalTable[o];
                }
                return n.ToString(); // cardinal fallback for 100+ (rare as ordinal)
            }

            public static string Normalize(string text) {
                // 0. Split CamelCase/PascalCase so "SharpTalk" -> "Sharp Talk"
                text = SplitCamelCase(text);

                // 10. Parentheses -> comma-separated pauses
                text = ReParentheses.Replace(text, ", $1, ");

                // 1. Currency - before decimal so $3.99 isn't split at the dot
                text = ReplaceCurrency(text);

                // 2. Percentages
                text = ReplacePercent(text);

                // 3. Ordinals - before decimals to avoid "1.5th" oddities
                text = ReplaceOrdinals(text);

                // 4. Years - 4-digit numbers read as pairs ("nineteen eighty-four")
                text = ReplaceYears(text);

                // 5. Decimal numbers - spell each digit after the point individually
                text = ReplaceDecimals(text);

                // 6. Dotted abbreviations (i.e., e.g., a.m. ...) - must run before step 7
                //    so their embedded periods don't trigger sentence splitting.
                text = ReplaceDottedAbbrevs(text);

                // 7. Single-dot abbreviations
                text = ReplaceAbbrevs(text);

                // 8. Em-dash, en-dash, double-hyphen -> sentence break; plain hyphens -> space
                text = text.Replace("—", ". ").Replace("–", ". ").Replace("--", ". ");
                text = text.Replace('-', ' ');

                // 9. Expressive reduplication: "hahaha" -> "ha ha ha"
                text = ReReduplicate.Replace(text, m => {
                    string unit = m.Groups[1].Value;
                    int count = m.Value.Length / unit.Length;
                    return string.Join(" ", System.Linq.Enumerable.Repeat(unit, count));
                });

                return text;
            }
        }

        // Word -> raw phoneme stream

        // Hardcoded letter pronunciations - A-Z indexed by (char - 'A').
        // Stress opcode placed immediately before the stressed vowel.
        // Never routed through dict or LTS so missing entries can't break them.
        static readonly byte[][] LetterPhonemesTable =
        {
            new byte[]{ 0x38, 0x0C },                            // A  -> EY
            new byte[]{ 0x2D, 0x38, 0x00 },                     // B  -> B IY
            new byte[]{ 0x27, 0x38, 0x00 },                     // C  -> S IY
            new byte[]{ 0x2F, 0x38, 0x00 },                     // D  -> D IY
            new byte[]{ 0x38, 0x00 },                            // E  -> IY
            new byte[]{ 0x38, 0x02, 0x23 },                     // F  -> EH F
            new byte[]{ 0x33, 0x38, 0x00 },                     // G  -> JH IY
            new byte[]{ 0x38, 0x0C, 0x32 },                     // H  -> EY CH  (aitch)
            new byte[]{ 0x38, 0x0D },                            // I  -> AY
            new byte[]{ 0x33, 0x38, 0x0C },                     // J  -> JH EY
            new byte[]{ 0x30, 0x38, 0x0C },                     // K  -> K EY
            new byte[]{ 0x38, 0x02, 0x1E },                     // L  -> EH L
            new byte[]{ 0x38, 0x02, 0x18 },                     // M  -> EH M
            new byte[]{ 0x38, 0x02, 0x19 },                     // N  -> EH N
            new byte[]{ 0x38, 0x10 },                            // O  -> OW
            new byte[]{ 0x2C, 0x38, 0x00 },                     // P  -> P IY
            new byte[]{ 0x30, 0x1C, 0x38, 0x0B },               // Q  -> K Y UW  (cue)
            new byte[]{ 0x38, 0x08, 0x1D },                     // R  -> AA R
            new byte[]{ 0x38, 0x02, 0x27 },                     // S  -> EH S
            new byte[]{ 0x2E, 0x38, 0x00 },                     // T  -> T IY
            new byte[]{ 0x1C, 0x38, 0x0B },                     // U  -> Y UW
            new byte[]{ 0x24, 0x38, 0x00 },                     // V  -> V IY
            new byte[]{ 0x2F, 0x38, 0x07, 0x2D, 0x05, 0x1E, 0x1C, 0x38, 0x0B }, // W  -> D AH B AX L Y UW  (double-you)
            new byte[]{ 0x38, 0x02, 0x30, 0x27 },               // X  -> EH K S
            new byte[]{ 0x1B, 0x38, 0x0D },                     // Y  -> W AY
            new byte[]{ 0x28, 0x38, 0x00 },                     // Z  -> Z IY
        };

        // Phoneme sequences for every word the normalizer can produce.
        // Checked before dict + LTS so dictionary swaps never affect normalizer output.
        static readonly Dictionary<string, byte[]> NormalizationWordsTable = new() {
            //    Digits
            ["ZERO"] = new byte[] { 0x28, 0x38, 0x01, 0x1D, 0x10 },
            ["ONE"] = new byte[] { 0x1B, 0x38, 0x07, 0x19 },
            ["TWO"] = new byte[] { 0x2E, 0x38, 0x0B },
            ["THREE"] = new byte[] { 0x25, 0x1D, 0x38, 0x00 },
            ["FOUR"] = new byte[] { 0x23, 0x38, 0x09, 0x1D },
            ["FIVE"] = new byte[] { 0x23, 0x38, 0x0D, 0x24 },
            ["SIX"] = new byte[] { 0x27, 0x38, 0x01, 0x30, 0x27 },
            ["SEVEN"] = new byte[] { 0x27, 0x38, 0x02, 0x24, 0x05, 0x19 },
            ["EIGHT"] = new byte[] { 0x38, 0x0C, 0x2E },
            ["NINE"] = new byte[] { 0x19, 0x38, 0x0D, 0x19 },
            //    Teens
            ["TEN"] = new byte[] { 0x2E, 0x38, 0x02, 0x19 },
            ["ELEVEN"] = new byte[] { 0x04, 0x1E, 0x38, 0x02, 0x24, 0x05, 0x19 },
            ["TWELVE"] = new byte[] { 0x2E, 0x1B, 0x38, 0x02, 0x1E, 0x24 },
            ["THIRTEEN"] = new byte[] { 0x25, 0x38, 0x06, 0x2E, 0x38, 0x00, 0x19 },
            ["FOURTEEN"] = new byte[] { 0x23, 0x38, 0x09, 0x1D, 0x2E, 0x38, 0x00, 0x19 },
            ["FIFTEEN"] = new byte[] { 0x23, 0x04, 0x23, 0x2E, 0x38, 0x00, 0x19 },
            ["SIXTEEN"] = new byte[] { 0x27, 0x04, 0x30, 0x27, 0x2E, 0x38, 0x00, 0x19 },
            ["SEVENTEEN"] = new byte[] { 0x27, 0x38, 0x02, 0x24, 0x05, 0x19, 0x2E, 0x38, 0x00, 0x19 },
            ["EIGHTEEN"] = new byte[] { 0x0C, 0x2E, 0x38, 0x00, 0x19 },
            ["NINETEEN"] = new byte[] { 0x19, 0x38, 0x0D, 0x19, 0x2E, 0x38, 0x00, 0x19 },
            //    Tens
            ["TWENTY"] = new byte[] { 0x2E, 0x1B, 0x38, 0x02, 0x19, 0x2E, 0x00 },
            ["THIRTY"] = new byte[] { 0x25, 0x38, 0x06, 0x2F, 0x39, 0x00 },
            ["FORTY"] = new byte[] { 0x23, 0x38, 0x09, 0x1D, 0x2E, 0x00 },
            ["FIFTY"] = new byte[] { 0x23, 0x38, 0x01, 0x23, 0x2E, 0x00 },
            ["SIXTY"] = new byte[] { 0x27, 0x38, 0x01, 0x30, 0x27, 0x2E, 0x00 },
            ["SEVENTY"] = new byte[] { 0x27, 0x38, 0x02, 0x24, 0x05, 0x19, 0x2E, 0x00 },
            ["EIGHTY"] = new byte[] { 0x38, 0x0C, 0x2E, 0x00 },
            ["NINETY"] = new byte[] { 0x19, 0x38, 0x0D, 0x19, 0x2E, 0x00 },
            //    Large / misc number
            ["HUNDRED"] = new byte[] { 0x2B, 0x38, 0x07, 0x19, 0x2F, 0x1D, 0x05, 0x2F },
            ["THOUSAND"] = new byte[] { 0x25, 0x38, 0x0F, 0x28, 0x05, 0x19, 0x2F },
            ["MILLION"] = new byte[] { 0x18, 0x38, 0x01, 0x1E, 0x1C, 0x05, 0x19 },
            ["BILLION"] = new byte[] { 0x2D, 0x38, 0x01, 0x1E, 0x1C, 0x05, 0x19 },
            ["OH"] = new byte[] { 0x38, 0x10 },
            ["POINT"] = new byte[] { 0x2C, 0x38, 0x0E, 0x19, 0x2E },
            ["AND"] = new byte[] { 0x05, 0x19, 0x2F },
            //    Currency / percent
            ["DOLLAR"] = new byte[] { 0x2F, 0x38, 0x08, 0x1E, 0x06 },
            ["DOLLARS"] = new byte[] { 0x2F, 0x38, 0x08, 0x1E, 0x06, 0x28 },
            ["CENT"] = new byte[] { 0x27, 0x38, 0x02, 0x19, 0x2E },
            ["CENTS"] = new byte[] { 0x27, 0x38, 0x02, 0x19, 0x2E, 0x27 },
            ["PERCENT"] = new byte[] { 0x2C, 0x06, 0x27, 0x38, 0x02, 0x19, 0x2E },
            //    Ordinals
            ["ZEROTH"] = new byte[] { 0x28, 0x38, 0x00, 0x1D, 0x10, 0x25 },
            ["FIRST"] = new byte[] { 0x23, 0x38, 0x06, 0x27, 0x2E },
            ["SECOND"] = new byte[] { 0x27, 0x38, 0x02, 0x30, 0x05, 0x19, 0x2F },
            ["THIRD"] = new byte[] { 0x25, 0x38, 0x06, 0x2F },
            ["FOURTH"] = new byte[] { 0x23, 0x38, 0x09, 0x1D, 0x25 },
            ["FIFTH"] = new byte[] { 0x23, 0x38, 0x01, 0x23, 0x25 },
            ["SIXTH"] = new byte[] { 0x27, 0x38, 0x01, 0x30, 0x27, 0x25 },
            ["SEVENTH"] = new byte[] { 0x27, 0x38, 0x02, 0x24, 0x05, 0x19, 0x25 },
            ["EIGHTH"] = new byte[] { 0x38, 0x0C, 0x2E, 0x25 },
            ["NINTH"] = new byte[] { 0x19, 0x38, 0x0D, 0x19, 0x25 },
            ["TENTH"] = new byte[] { 0x2E, 0x38, 0x02, 0x19, 0x25 },
            ["ELEVENTH"] = new byte[] { 0x04, 0x1E, 0x38, 0x02, 0x24, 0x05, 0x19, 0x25 },
            ["TWELFTH"] = new byte[] { 0x2E, 0x1B, 0x38, 0x02, 0x1E, 0x23, 0x25 },
            ["THIRTEENTH"] = new byte[] { 0x25, 0x38, 0x06, 0x2E, 0x38, 0x00, 0x19, 0x25 },
            ["FOURTEENTH"] = new byte[] { 0x23, 0x38, 0x09, 0x1D, 0x2E, 0x38, 0x00, 0x19, 0x25 },
            ["FIFTEENTH"] = new byte[] { 0x23, 0x04, 0x23, 0x2E, 0x38, 0x00, 0x19, 0x25 },
            ["SIXTEENTH"] = new byte[] { 0x27, 0x04, 0x30, 0x27, 0x2E, 0x38, 0x00, 0x19, 0x25 },
            ["SEVENTEENTH"] = new byte[] { 0x27, 0x38, 0x02, 0x24, 0x05, 0x19, 0x2E, 0x38, 0x00, 0x19, 0x25 },
            ["EIGHTEENTH"] = new byte[] { 0x0C, 0x2E, 0x38, 0x00, 0x19, 0x25 },
            ["NINETEENTH"] = new byte[] { 0x19, 0x38, 0x0D, 0x19, 0x2E, 0x38, 0x00, 0x19, 0x25 },
            ["TWENTIETH"] = new byte[] { 0x2E, 0x1B, 0x38, 0x02, 0x19, 0x2E, 0x00, 0x05, 0x25 },
            ["THIRTIETH"] = new byte[] { 0x25, 0x38, 0x06, 0x2E, 0x00, 0x05, 0x25 },
            ["FORTIETH"] = new byte[] { 0x23, 0x38, 0x09, 0x1D, 0x2E, 0x00, 0x04, 0x25 },
            ["FIFTIETH"] = new byte[] { 0x23, 0x38, 0x01, 0x23, 0x2E, 0x00, 0x04, 0x25 },
            ["SIXTIETH"] = new byte[] { 0x27, 0x38, 0x01, 0x30, 0x27, 0x2E, 0x00, 0x04, 0x25 },
            ["SEVENTIETH"] = new byte[] { 0x27, 0x38, 0x02, 0x24, 0x05, 0x19, 0x2E, 0x00, 0x04, 0x25 },
            ["EIGHTIETH"] = new byte[] { 0x38, 0x0C, 0x2E, 0x00, 0x04, 0x25 },
            ["NINETIETH"] = new byte[] { 0x19, 0x38, 0x0D, 0x19, 0x2E, 0x00, 0x04, 0x25 },
            //    Letter names (used by dotted abbreviation expansions)
            ["AY"] = new byte[] { 0x38, 0x0C },
            ["BEE"] = new byte[] { 0x2D, 0x38, 0x00 },
            ["SEE"] = new byte[] { 0x27, 0x38, 0x00 },
            ["DEE"] = new byte[] { 0x2F, 0x38, 0x00 },
            ["EF"] = new byte[] { 0x38, 0x02, 0x23 },
            ["EM"] = new byte[] { 0x38, 0x02, 0x18 },
            ["PEE"] = new byte[] { 0x2C, 0x38, 0x00 },
            //    Dotted abbreviation expansions
            ["THAT"] = new byte[] { 0x26, 0x38, 0x03, 0x2E },
            ["IS"] = new byte[] { 0x38, 0x01, 0x28 },
            ["FOR"] = new byte[] { 0x23, 0x38, 0x09, 0x1D },
            ["EXAMPLE"] = new byte[] { 0x04, 0x31, 0x28, 0x38, 0x03, 0x18, 0x2C, 0x05, 0x1E },
            ["POSTSCRIPT"] = new byte[] { 0x2C, 0x38, 0x10, 0x27, 0x30, 0x1D, 0x39, 0x01, 0x2C, 0x2E },
            ["WITH"] = new byte[] { 0x1B, 0x38, 0x01, 0x26 },
            ["REGARD"] = new byte[] { 0x1D, 0x04, 0x31, 0x38, 0x08, 0x1D, 0x2F },
            ["TO"] = new byte[] { 0x2E, 0x38, 0x0B },
            //    Titles
            ["DOCTOR"] = new byte[] { 0x2F, 0x38, 0x08, 0x30, 0x2E, 0x06 },
            ["MISTER"] = new byte[] { 0x18, 0x38, 0x01, 0x27, 0x2E, 0x06 },
            ["MISSUS"] = new byte[] { 0x18, 0x38, 0x01, 0x27, 0x04, 0x28 },
            ["MISS"] = new byte[] { 0x18, 0x38, 0x01, 0x27 },
            ["PROFESSOR"] = new byte[] { 0x2C, 0x1D, 0x05, 0x23, 0x38, 0x02, 0x27, 0x06 },
            ["JUNIOR"] = new byte[] { 0x33, 0x38, 0x0B, 0x19, 0x1C, 0x06 },
            ["SENIOR"] = new byte[] { 0x27, 0x38, 0x00, 0x19, 0x1C, 0x06 },
            //    Common abbreviation expansions
            ["VERSUS"] = new byte[] { 0x24, 0x38, 0x06, 0x27, 0x05, 0x27 },
            ["ETCETERA"] = new byte[] { 0x38, 0x02, 0x2E, 0x27, 0x38, 0x02, 0x2E, 0x06, 0x05 },
            ["APPROXIMATELY"] = new byte[] { 0x05, 0x2C, 0x1D, 0x38, 0x08, 0x30, 0x27, 0x05, 0x18, 0x05, 0x2E, 0x1E, 0x00 },
            ["MAXIMUM"] = new byte[] { 0x18, 0x38, 0x03, 0x30, 0x27, 0x05, 0x18, 0x05, 0x18 },
            ["MINIMUM"] = new byte[] { 0x18, 0x38, 0x01, 0x19, 0x05, 0x18, 0x05, 0x18 },
            ["AVERAGE"] = new byte[] { 0x38, 0x03, 0x24, 0x06, 0x04, 0x33 },
            ["VOLUME"] = new byte[] { 0x24, 0x38, 0x08, 0x1E, 0x1C, 0x0B, 0x18 },
            ["FIGURE"] = new byte[] { 0x23, 0x38, 0x01, 0x31, 0x1C, 0x06 },
            ["REFERENCE"] = new byte[] { 0x1D, 0x38, 0x02, 0x23, 0x06, 0x05, 0x19, 0x27 },
            ["ESTABLISHED"] = new byte[] { 0x04, 0x27, 0x2E, 0x38, 0x03, 0x2D, 0x1E, 0x04, 0x29, 0x2E },
            ["CONTINUED"] = new byte[] { 0x30, 0x05, 0x19, 0x2E, 0x38, 0x01, 0x19, 0x1C, 0x0B, 0x2F },
            ["ABBREVIATION"] = new byte[] { 0x05, 0x2D, 0x1D, 0x39, 0x00, 0x24, 0x00, 0x38, 0x0C, 0x29, 0x05, 0x19 },
            ["ATTRIBUTED"] = new byte[] { 0x05, 0x2E, 0x1D, 0x38, 0x01, 0x2D, 0x1C, 0x05, 0x2E, 0x04, 0x2F },
            ["DISTRICT"] = new byte[] { 0x2F, 0x38, 0x01, 0x27, 0x2E, 0x1D, 0x04, 0x30, 0x2E },
            ["POPULATION"] = new byte[] { 0x2C, 0x39, 0x08, 0x2C, 0x1C, 0x05, 0x1E, 0x38, 0x0C, 0x29, 0x05, 0x19 },
            ["TEMPERATURE"] = new byte[] { 0x2E, 0x38, 0x02, 0x18, 0x2C, 0x1D, 0x05, 0x32, 0x06 },
            ["TECHNICAL"] = new byte[] { 0x2E, 0x38, 0x02, 0x30, 0x19, 0x04, 0x30, 0x05, 0x1E },
            ["ELECTRIC"] = new byte[] { 0x04, 0x1E, 0x38, 0x02, 0x30, 0x2E, 0x1D, 0x04, 0x30 },
            //    Address
            ["STREET"] = new byte[] { 0x27, 0x2E, 0x1D, 0x38, 0x00, 0x2E },
            ["AVENUE"] = new byte[] { 0x38, 0x03, 0x24, 0x05, 0x19, 0x39, 0x0B },
            ["BOULEVARD"] = new byte[] { 0x2D, 0x38, 0x0A, 0x1E, 0x05, 0x24, 0x39, 0x08, 0x1D, 0x2F },
            ["ROAD"] = new byte[] { 0x1D, 0x38, 0x10, 0x2F },
            ["LANE"] = new byte[] { 0x1E, 0x38, 0x0C, 0x19 },
            //    Military
            ["LIEUTENANT"] = new byte[] { 0x1E, 0x0B, 0x2E, 0x38, 0x02, 0x19, 0x05, 0x19, 0x2E },
            ["CAPTAIN"] = new byte[] { 0x30, 0x38, 0x03, 0x2C, 0x2E, 0x05, 0x19 },
            ["GENERAL"] = new byte[] { 0x33, 0x38, 0x02, 0x19, 0x06, 0x05, 0x1E },
            ["SERGEANT"] = new byte[] { 0x27, 0x38, 0x08, 0x1D, 0x33, 0x05, 0x19, 0x2E },
            ["PRIVATE"] = new byte[] { 0x2C, 0x1D, 0x38, 0x0D, 0x24, 0x05, 0x2E },
            ["COLONEL"] = new byte[] { 0x30, 0x38, 0x06, 0x19, 0x05, 0x1E },
            ["MAJOR"] = new byte[] { 0x18, 0x38, 0x0C, 0x33, 0x06 },
            ["REVEREND"] = new byte[] { 0x1D, 0x38, 0x02, 0x24, 0x06, 0x05, 0x19, 0x2F },
            //    Org
            ["DEPARTMENT"] = new byte[] { 0x2F, 0x04, 0x2C, 0x38, 0x08, 0x1D, 0x2E, 0x18, 0x05, 0x19, 0x2E },
            ["INCORPORATED"] = new byte[] { 0x39, 0x01, 0x19, 0x30, 0x38, 0x09, 0x1D, 0x2C, 0x06, 0x39, 0x0C, 0x2E, 0x04, 0x2F },
            ["CORPORATION"] = new byte[] { 0x30, 0x39, 0x09, 0x1D, 0x2C, 0x06, 0x38, 0x0C, 0x29, 0x05, 0x19 },
            ["GOVERNMENT"] = new byte[] { 0x31, 0x38, 0x07, 0x24, 0x06, 0x18, 0x05, 0x19, 0x2E },
            ["DIVISION"] = new byte[] { 0x2F, 0x04, 0x24, 0x38, 0x01, 0x2A, 0x05, 0x19 },
            ["INTERNATIONAL"] = new byte[] { 0x39, 0x01, 0x19, 0x2E, 0x06, 0x19, 0x38, 0x03, 0x29, 0x05, 0x19, 0x05, 0x1E },
            ["NATIONAL"] = new byte[] { 0x19, 0x38, 0x03, 0x29, 0x05, 0x19, 0x05, 0x1E },
            ["ASSOCIATION"] = new byte[] { 0x05, 0x27, 0x39, 0x10, 0x27, 0x00, 0x38, 0x0C, 0x29, 0x05, 0x19 },
            ["ADMINISTRATION"] = new byte[] { 0x03, 0x2F, 0x18, 0x39, 0x01, 0x19, 0x04, 0x27, 0x2E, 0x1D, 0x38, 0x0C, 0x29, 0x05, 0x19 },
            ["ASSISTANT"] = new byte[] { 0x05, 0x27, 0x38, 0x01, 0x27, 0x2E, 0x05, 0x19, 0x2E },
            ["MANAGER"] = new byte[] { 0x18, 0x38, 0x03, 0x19, 0x05, 0x33, 0x06 },
            ["DIRECTOR"] = new byte[] { 0x2F, 0x06, 0x38, 0x02, 0x30, 0x2E, 0x06 },
            //    Months
            ["JANUARY"] = new byte[] { 0x33, 0x38, 0x03, 0x19, 0x1C, 0x0B, 0x39, 0x02, 0x1D, 0x00 },
            ["FEBRUARY"] = new byte[] { 0x23, 0x38, 0x02, 0x2D, 0x1C, 0x05, 0x1B, 0x39, 0x02, 0x1D, 0x00 },
            ["MARCH"] = new byte[] { 0x18, 0x38, 0x08, 0x1D, 0x32 },
            ["APRIL"] = new byte[] { 0x38, 0x0C, 0x2C, 0x1D, 0x05, 0x1E },
            ["JUNE"] = new byte[] { 0x33, 0x38, 0x0B, 0x19 },
            ["JULY"] = new byte[] { 0x33, 0x39, 0x0B, 0x1E, 0x38, 0x0D },
            ["AUGUST"] = new byte[] { 0x38, 0x08, 0x31, 0x05, 0x27, 0x2E },
            ["SEPTEMBER"] = new byte[] { 0x27, 0x02, 0x2C, 0x2E, 0x38, 0x02, 0x18, 0x2D, 0x06 },
            ["OCTOBER"] = new byte[] { 0x08, 0x30, 0x2E, 0x38, 0x10, 0x2D, 0x06 },
            ["NOVEMBER"] = new byte[] { 0x19, 0x10, 0x24, 0x38, 0x02, 0x18, 0x2D, 0x06 },
            ["DECEMBER"] = new byte[] { 0x2F, 0x04, 0x27, 0x38, 0x02, 0x18, 0x2D, 0x06 },
        };

        // For all-caps words absent from the dict, inject letter phonemes directly
        // no dict lookup, no LTS. Each letter becomes its own word-boundary token.
        byte[] SpellOutAcronym(string upper) {
            var buf = new System.Collections.Generic.List<byte>(upper.Length * 4);
            foreach (char c in upper) {
                if (c < 'A' || c > 'Z') {
                    continue;
                }
                buf.Add(OP_WORD);
                buf.AddRange(LetterPhonemesTable[c - 'A']);
            }
            return buf.ToArray();
        }

        static bool IsAllCaps(string word) {
            if (word.Length < 2) {
                return false;
            }
            foreach (char c in word) {
                if (c < 'A' || c > 'Z') {
                    return false;
                }
            }
            return true;
        }

        byte[] WordToPhonStream(string upperWord) {
            // Contractions are stored in the dict without apostrophes ("ISN'T" -> "ISNT").
            string lookupWord = upperWord.Contains('\'')
                ? upperWord.Replace("'", "") : upperWord;

            // 0. Normalizer word table - bypasses dict entirely
            if (NormalizationWordsTable.TryGetValue(upperWord, out var normPhons)) {
                var nb = new byte[normPhons.Length + 1];
                nb[0] = OP_WORD; normPhons.CopyTo(nb, 1);
                return nb;
            }

            // 1. Try dictionary directly
            byte[]? phons = _dict.Search(lookupWord);
            if (phons != null) {
                StatDict++;
            }

            // 2. Try morphological decomposition (suffix stripping + root lookup)
            if (phons == null) {
                phons = Morph.TryDecompose(lookupWord, _dict);
                if (phons != null) {
                    StatMorph++;
                }
            }

            // 3. Fall back to letter-to-sound rules
            if (phons == null) {
                phons = LetterToSound.Convert(upperWord);
                StatLts++;
            }

            // Prepend OP_WORD marker
            var buf = new byte[phons.Length + 1];
            buf[0] = OP_WORD;
            phons.CopyTo(buf, 1);
            return buf;
        }

        // Number -> raw phoneme stream

        byte[] NumberToPhonStream(long n) {
            var buf = new List<byte>();
            BuildNumberPhons(buf, n);
            return buf.ToArray();
        }

        void BuildNumberPhons(List<byte> buf, long n) {
            // "minus" via billion slot TODO: add MINUS to symbols
            if (n < 0) {
                AppendSymbol(buf, "1E3");
                BuildNumberPhons(buf, -n);
                return;
            }
            if (n == 0) {
                AppendSymbol(buf, "0");
                return;
            }

            if (n >= 1_000_000_000) {
                BuildNumberPhons(buf, n / 1_000_000_000);
                AppendSymbol(buf, "1E3");  // billion
                n %= 1_000_000_000;
            }
            if (n >= 1_000_000) {
                BuildNumberPhons(buf, n / 1_000_000);
                AppendSymbol(buf, "1E2");  // million
                n %= 1_000_000;
            }
            if (n >= 1_000) {
                BuildNumberPhons(buf, n / 1_000);
                AppendSymbol(buf, "1E1");  // thousand
                n %= 1_000;
            }
            if (n >= 100) {
                AppendDigit(buf, (int)(n / 100));
                AppendSymbol(buf, "100");  // hundred
                n %= 100;
            }
            if (n >= 20) {
                AppendTens(buf, (int)(n / 10));
                n %= 10;
                if (n > 0) {
                    AppendDigit(buf, (int)n);
                }
            } else if (n >= 10) {
                AppendTeen(buf, (int)n);
            } else if (n > 0) {
                AppendDigit(buf, (int)n);
            }
        }

        static readonly string[] DigitNames = new string[] { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
        static readonly string[] TeenNames = new string[] { "10", "11", "12", "13", "14", "15", "16", "17", "18", "19" };
        static readonly string[] TensNames = new string[] { "", "", "20", "30", "40", "50", "60", "70", "80", "90" };

        void AppendDigit(List<byte> buf, int d) => AppendSymbol(buf, DigitNames[d]);
        void AppendTeen(List<byte> buf, int n) => AppendSymbol(buf, TeenNames[n - 10]);
        void AppendTens(List<byte> buf, int t) => AppendSymbol(buf, TensNames[t]);

        void AppendSymbol(List<byte> buf, string sym) {
            if (buf.Count == 0) {
                buf.Add(OP_WORD);
            }
            byte[]? phons = _symbols(sym);
            if (phons == null) {
                return;
            }
            buf.AddRange(phons);
        }

        // Stream -> PhonemeToken list

        void AppendWordTokens(List<PhonemeToken> tokens, byte[] stream, bool isContent, bool isPronoun = false) {
            long pending = 0;
            long persistent = isPronoun ? kPronounWord : 0L;
            int startIdx = tokens.Count;
            bool hadPrimary = false;

            foreach (byte b in stream) {
                switch (b) {
                    case OP_WORD:
                        pending |= kWord_Start;
                        if (isContent) {
                            pending |= kContent_Word;
                        }
                        break;
                    case OP_STRESS1:
                        // Function words: demote dict primary stress to secondary so they
                        // don't trigger pitch peaks in the BackEnd pitch algorithm.
                        if (isContent) {
                            pending |= kPrimaryStress;
                            hadPrimary = true;
                        } else {
                            pending |= kSecondaryStress;
                        }
                        break;
                    case OP_STRESS2: pending |= kSecondaryStress; break;
                    case OP_EMPHSTRESS: pending |= kEmphaticStress; break;
                    case OP_SYLL: pending |= kSyllable_Start; break;
                    case OP_PREP: pending |= kPrep_Start; break;
                    case OP_VERB: pending |= kVerb_Start; break;
                    case OP_COMMA:
                    case OP_PERIOD:
                    case OP_QUEST:
                    case OP_EXCLAM:
                        tokens.Add(new PhonemeToken { Phon = (short)b, Ctrl = kTerm_Bound });
                        pending = 0;
                        break;
                    default:
                        if (b <= 55) {
                            tokens.Add(new PhonemeToken { Phon = (short)b, Ctrl = pending | persistent });
                            pending = 0;
                        }
                        break;
                }
            }

            // Content word with only secondary stress: promote to primary so the pitch
            // algorithm has a peak to work with on words like "how".
            if (isContent && !hadPrimary) {
                for (int i = startIdx; i < tokens.Count; i++) {
                    if ((tokens[i].Ctrl & kSecondaryStress) != 0) {
                        tokens[i] = new PhonemeToken {
                            Phon = tokens[i].Phon,
                            Ctrl = (tokens[i].Ctrl & ~kSecondaryStress) | kPrimaryStress,
                            UserPitch = tokens[i].UserPitch,
                            UserDur = tokens[i].UserDur,
                            UserNote = tokens[i].UserNote,
                            UserRate = tokens[i].UserRate,
                        };
                        break;
                    }
                }
            }
        }

    }
}  // namespace
