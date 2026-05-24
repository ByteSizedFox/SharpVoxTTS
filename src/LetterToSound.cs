#nullable enable
using System;
using System.Collections.Generic;
using static SharpTalk.AudioProcessor;

namespace SharpTalk {
    // English letter-to-sound rules derived from NRL Report 7948
    // (Elovitz, Johnson, McHugh, Shore - "Automatic Translation of English Text to Phonetics", 1976)
    public static class LetterToSound {

        const byte pIY = (byte)_IY_, pIH = (byte)_IH_, pEH = (byte)_EH_, pAE = (byte)_AE_, pAX = (byte)_AX_, pER = (byte)_ER_, pAH = (byte)_AH_;
        const byte pAA = (byte)_AA_, pAO = (byte)_AO_, pUH = (byte)_UH_, pUW = (byte)_UW_;
        const byte pEY = (byte)_EY_, pAY = (byte)_AY_, pOY = (byte)_OY_, pAW = (byte)_AW_, pOW = (byte)_OW_;
        const byte pM = (byte)_M_, pN = (byte)_N_, pNG = (byte)_NG_, pW = (byte)_W_, pY = (byte)_Y_, pR = (byte)_R_, pL = (byte)_L_;
        const byte pHH = (byte)_HH_, pF = (byte)_F_, pV = (byte)_V_, pTH = (byte)_TH_, pDH = (byte)_DH_, pS = (byte)_S_, pZ = (byte)_Z_, pSH = (byte)_SH_, pZH = (byte)_ZH_;
        const byte pP = (byte)_P_, pB = (byte)_B_, pT = (byte)_T_, pD = (byte)_D_, pK = (byte)_K_, pG = (byte)_G_, pCH = (byte)_CH_, pJH = (byte)_JH_;

        const byte CV = 0x01; // vowel: A E I O U Y
        const byte CF = 0x02; // front vowel: E I Y
        const byte CC = 0x04; // consonant: all non-vowels
        const byte CZ = 0x08; // voiced consonant: B D G J L M N R V W Z
        const byte CS = 0x10; // sibilant: S C G Z X J (+ CH/SH digraphs)
        const byte CU = 0x20; // @-consonant (long-U modifier): T S R D L Z N J

        static readonly byte[] CharacterFeatureTable = new byte[128];

        static LetterToSound() {
            foreach (char c in "AEIOUY") {
                CharacterFeatureTable[c] |= CV;
            }
            foreach (char c in "EIY") {
                CharacterFeatureTable[c] |= CF;
            }
            foreach (char c in "BCDFGHJKLMNPQRSTVWXZ") {
                CharacterFeatureTable[c] |= CC;
            }
            foreach (char c in "BDGJLMNRVWZ") {
                CharacterFeatureTable[c] |= CZ;
            }
            foreach (char c in "SCGZXJ") {
                CharacterFeatureTable[c] |= CS;
            }
            foreach (char c in "TSRDLZNJ") {
                CharacterFeatureTable[c] |= CU;
            }
            CompiledLetterToSoundRules = Compile(LetterToSoundRulesSource);
        }

        // Format: "LEFT[MATCH]RIGHT=OUTPUT"
        // Special symbols in LEFT / RIGHT (not between brackets):
        //   #  1+ vowels          *  1+ consonants   .  voiced consonant
        //   $  1 consonant + E/I  %  suffix           &  sibilant
        //   @  long-U consonant   ^  exactly 1 cons   +  front vowel (E I Y)
        //   :  0+ consonants      ' '  word boundary
        // OUTPUT: space-separated NRL phoneme names, or empty for silence.

        static readonly string[][] LetterToSoundRulesSource =
        {
            new[]{
                "[A] =AX",
                " [ARE]=AA R",
                " [AR]O=AX R",
                "#[AR]#=EH R",
                " ^[AS]#=EY S",
                "[A]WA=AX",
                "[AW]=AO",
                " :[ANY]=EH N IY",
                "[A]^+#=EY",
                "#:[ALLY]=AX L IY",
                " [AL]#=AX L",
                "[AGAIN]=AX G EH N",
                "#:[AG]E=IH JH",
                "#[A]^+#=AE",
                " *[A]^+ =EY",
                "[A]^%=EY",
                " *[ARR]=AX R",
                "[ARR]=AE R",
                " *[AR] =AA R",
                "[AR] =ER",
                "[AR]=AA R",
                "[AIR]=EH R",
                "[AI]=EY",
                "[AY]=EY",
                "[AU]=AO",
                "#*:[AL] =AX L",
                "#*:[ALS] =AX L Z",
                "[ALK]=AO K",
                "[AL]=AO L",
                " *[ABLE]=EY B AX L",
                "[ABLE]=AX B AX L",
                "[ANG]+=EY N JH",
                "[A]=AE",
            },
            new[]{
                " [BE]^#=B IH",
                "[BEING]=B IY IH NG",
                " [BOTH] =B OW TH",
                " [BUS]#=B IH Z",
                "[BUIL]=B IH L",
                "[B]=B",
            },
            new[]{
                " [CH]=K",
                "^^E[CH]=K",
                "[CH]=CH",
                " S[CI]#=S AY",
                "[CI]A=SH",
                "[CI]O=SH",
                "[CI]EN=SH",
                "[C]+=S",
                "[CharacterFeatureTable]=K",
                ".[COM]%=K AH M",
                "[C]=K",
            },
            new[]{
                "#*:[DED] =D IH D",
                ".E[D]=D",
                "#*^E[D]=T",
                " [DE]^#=D IH",
                " [DO] =D UW",
                " [DOES]=D AH Z",
                " [DOING]=D UW IH NG",
                " [DOW]=D AW",
                "[DU]A=JH UW",
                "[D]=D",
            },
            new[]{
                "#*[E] =",
                "#*^[E] =",
                " :[E] =IY",
                "#[ED] =D",
                "#*[E]D =",
                "[EV]ER=EH V",
                "[E]^%=IY",
                "[ERI]#=IY R IY",
                "[ERI]=EH R IH",
                "#:[ER]#=ER",
                "[ER]#=EH R",
                "[ER]=ER",
                " [EVEN]=IY V EH N",
                "#:[EW]=",
                "@[EW]=UW",
                "[EW]=Y UW",
                "[EO]=IY",
                "#*&[ES] =IH Z",
                "#*[ES] =",
                "#*[ELY] =L IY",
                "#*[EMENT] =M EH N T",
                "[EFUL]=F UH L",
                "[EE]=IY",
                "[EARN]=ER N",
                " [EAR]^=ER",
                "[EAD]=EH D",
                "#*[EA] =IY AX",
                "[EA]SU=EH",
                "[EA]=IY",
                "[EIGH]=EY",
                "[EI]=IY",
                " [EYE]=AY",
                "[EY]=IY",
                "[EU]=Y UW",
                "[E]=EH",
            },
            new[]{
                "[FUL]=F UH L",
                "[F]=F",
            },
            new[]{
                " [GN]=N",
                "[GIV]=G IH V",
                " [G]I=G",
                "[GE]T=G EH",
                "SU[GGES]=G JH EH S",
                "[GG]=G",
                " B#[G]=G",
                "[G]+=JH",
                "[GREAT]=G R EY T",
                "#[GH]=",
                "[G]=G",
            },
            new[]{
                " [HAV]=HH AE V",
                " [HERE]=HH IY R",
                " [HOUR]=AW ER",
                "[HOW]=HH AW",
                "[H]#=HH",
                "[H]=",
            },
            new[]{
                " [IN]=IH N",
                " [I] =AY",
                "[IN]D=AY N",
                "[IER]=IY ER",
                "#*R[IED] =IY D",
                "[IED] =AY D",
                "[IEN]=IY EH N",
                "[IE]T=AY EH",
                " :[I]%=AY",
                "[I]%=IY",
                "[I]E=IY",
                "[I]^+#=IH",
                "[I]#=AY R",
                "[IZ]%=AY Z",
                "[IS]%=AY Z",
                "[ID]%=AY D",
                "+^[I]+=IH",
                "[I]T%=AY",
                "#*:[I]^+=IH",
                "[I]^+=AY",
                "[IR]=ER",
                "[IGH]=AY",
                "[ILD]=AY L D",
                "[IGN] =AY N",
                "[IGN]^=AY N",
                "[IGN]%=AY N",
                "[IQUE]=IY K",
                "[I]=IH",
            },
            new[]{
                "[J]=JH",
            },
            new[]{
                " [K]N=",
                "[K]=K",
            },
            new[]{
                "[LO]C#=L OW",
                "[L]L=",
                "#^:[L]%=AX L",
                "[LEAD]=L IY D",
                "[L]=L",
            },
            new[]{
                "[MOV]=M UW V",
                "[M]=M",
            },
            new[]{
                "E[NG]+=N JH",
                "[NG]R=NG G",
                "[NG]#=NG G",
                "[NGL]%=NG G AX L",
                "[NG]=NG",
                "[NK]=NG K",
                " [NOW] =N AW",
                "[N]=N",
            },
            new[]{
                "[OF] =AX V",
                "[OROUGH]=ER OW",
                "#:[OR] =ER",
                "#:[ORS] =ER Z",
                "[OR]=AO R",
                " [ONE]=W AH N",
                "[OW]=OW",
                " [OVER]=OW V ER",
                "[OV]=AH V",
                "[O]^%=OW",
                "[O]^EN=OW",
                "[O]^I#=OW",
                "[OLD]=OW L D",
                "[OUGHT]=AO T",
                "[OUGH]=AH F",
                " [OU]=AW",
                "H[OU]S#=AW",
                "[OUS]=AX S",
                "[OUR]=AO R",
                "[OULD]=UH D",
                "^^[OU]L=AH",
                "[OUP]=UW P",
                "[OU]=AW",
                "[OY]=OY",
                "[OING]=OW IH NG",
                "[OI]=OY",
                "[OOR]=AO R",
                "[OOK]=UH K",
                "[OOD]=UH D",
                "[OO]=UW",
                "[O]E=OW",
                "[O] =OW",
                "[OA]=OW",
                " [ONLY]=OW N L IY",
                " [ONCE]=W AH N S",
                "*[ON] T=OW N",
                "C[ION]=AX N",
                "[O]NG=AO",
                " ^:[ON]=AH N",
                "#:[ON]=AX N",
                "#*[ON] =AX N",
                "#^[ON]=AX N",
                "[O]ST =OW",
                "[OF]^=AO F",
                "[OTHER]=AH DH ER",
                "[OSS] =AO S",
                "#*:[OM]=AH M",
                "[O]=AA",
            },
            new[]{
                "[PH]=F",
                "[PEOP]=P IY P",
                "[POW]=P AW",
                "[PUT] =P UH T",
                "[P]=P",
            },
            new[]{
                "[QUAR]=K W AO R",
                "[QU]=K W",
                "[Q]=K",
            },
            new[]{
                " [RE]^#=R IY",
                "[R]=R",
            },
            new[]{
                "[SH]=SH",
                "#[SION]=ZH AX N",
                "[SOME]=S AH M",
                "#[SUR]#=ZH ER",
                "[SUR]#=SH ER",
                "#[SU]#=ZH UW",
                "#[SSU]#=SH UW",
                "#[SED] =Z D",
                "#[S]#=Z",
                "[SAID]=S EH D",
                "^^[SION]=SH AX N",
                "[S]S=",
                ".[S] =Z",
                "#*.E[S] =Z",
                "#*^##[S] =Z",
                "#*^#[S] =S",
                "U[S] =S",
                " :#[S] =Z",
                " [SCH]=S K",
                "[S]C+=",
                "#[SM]=Z M",
                "#[SN] =Z AX N",
                "[S]=S",
            },
            new[]{
                " [THE] =DH AX",
                "[TO] =T UW",
                "[THAT] =DH AE T",
                " [THIS] =DH IH S",
                " [THEY]=DH EY",
                " [THERE]=DH EH R",
                "[THER]=DH ER",
                "[THEIR]=DH EH R",
                " [THAN] =DH AE N",
                " [THEM] =DH EH M",
                "[THESE] =DH IY Z",
                " [THEN]=DH EH N",
                "[THROUGH]=TH R UW",
                "[THOSE]=DH OW Z",
                "[THOUGH] =DH OW",
                " [THUS]=DH AH S",
                "[TH]=TH",
                "#:[TED] =T IH D",
                "S[TI]#N=CH",
                "[TION]=SH AX N",
                "[TIO]=SH",
                "[TIA]=SH",
                "[TIEN]=SH AX N",
                "[TUR]#=CH ER",
                "[TU]A=CH UW",
                " [TWO]=T UW",
                "[T]=T",
            },
            new[]{
                " [UN]I=Y UW N",
                " [UN]=AH N",
                " [UPON]=AX P AO N",
                "@[UR]#=UH R",
                "[UR]#=Y UH R",
                "[UR]=ER",
                "[U]^ =AH",
                "[U]^^=AH",
                "[UY]=AY",
                " G[U]#=",
                "G[U]%=",
                "G[U]#=W",
                "#N[U]=Y UW",
                "@[U]=UW",
                "[U]=Y UW",
            },
            new[]{
                "[VIEW]=V Y UW",
                "[V]=V",
            },
            new[]{
                " [WERE]=W ER",
                "[WA]S=W AA",
                "[WA]T=W AA",
                "[WHERE]=WH EH R",
                "[WHOL]=HH OW L",
                "[WHO]=HH UW",
                "[WH]=WH",
                "[WAR]=W AO R",
                "[WOR]^=W ER",
                "[WR]=R",
                "[W]=W",
            },
            new[]{
                "[X]=K S",
            },
            new[]{
                "[YOUNG]=Y AH NG",
                " [YOU]=Y UW",
                " [YES]=Y EH S",
                " [Y] =AY",
                "#^:[Y] =IY",
                "#^:[Y]I=IY",
                " :[Y] =AY",
                " :[Y]#=Y",      // initial Y before vowel = glide (year, yellow, yet)
                " :[Y]^+#=IH",
                " :[Y]^#=AY",
                "[Y]=IH",
            },
            new[]{
                "[Z]=Z",
            },
        };


        readonly struct CompiledRule {
            public readonly string Left;
            public readonly string Match;
            public readonly string Right;
            public readonly byte[] Out;
            public CompiledRule(string l, string m, string r, byte[] o) {
                Left = l;
                Match = m;
                Right = r;
                Out = o;
            }
        }

        static readonly CompiledRule[][] CompiledLetterToSoundRules;

        // Called once at static construction time; converts each rule string to a CompiledRule.
        static CompiledRule[][] Compile(string[][] src) {
            var result = new CompiledRule[26][];
            for (int li = 0; li < 26; li++) {
                var group = src[li];
                var compiled = new CompiledRule[group.Length];
                for (int ri = 0; ri < group.Length; ri++) {
                    compiled[ri] = ParseRule(group[ri]);
                }
                result[li] = compiled;
            }
            return result;
        }

        // Splits "LEFT[MATCH]RIGHT=OUTPUT" into its four fields.
        static CompiledRule ParseRule(string s) {
            int lbr = s.IndexOf('[');
            int rbr = s.IndexOf(']');
            int eq = s.IndexOf('=', rbr + 1);
            return new CompiledRule(
                lbr > 0 ? s[..lbr] : "",
                s[(lbr + 1)..rbr],
                s[(rbr + 1)..eq],
                ParseOutput(s[(eq + 1)..])
            );
        }

        static readonly Dictionary<string, byte> PhonemeMapTable = new()
        {
            {"IY",pIY},{"IH",pIH},{"EH",pEH},{"AE",pAE},{"AA",pAA},{"AH",pAH},
            {"AO",pAO},{"UH",pUH},{"AX",pAX},{"ER",pER},{"EY",pEY},{"AY",pAY},
            {"OY",pOY},{"AW",pAW},{"OW",pOW},{"UW",pUW},
            {"W",pW},{"Y",pY},{"R",pR},{"L",pL},{"HH",pHH},{"M",pM},{"N",pN},
            {"NG",pNG},{"F",pF},{"V",pV},{"TH",pTH},{"DH",pDH},{"S",pS},{"Z",pZ},
            {"SH",pSH},{"ZH",pZH},{"P",pP},{"B",pB},{"T",pT},{"D",pD},
            {"K",pK},{"G",pG},{"CH",pCH},{"JH",pJH},{"WH",pW},
        };

        static byte[] ParseOutput(string s) {
            if (string.IsNullOrWhiteSpace(s)) {
                return Array.Empty<byte>();
            }
            var parts = s.Trim().Split(' ', StringSplitOptions.RemoveEmptyEntries);
            var buf = new byte[parts.Length];
            for (int i = 0; i < parts.Length; i++) {
                buf[i] = PhonemeMapTable.TryGetValue(parts[i], out byte p) ? p : pAX;
            }
            return buf;
        }

        // Apply LTS rules to a single word and return its phoneme sequence.
        // CompiledLetterToSoundRules for each initial letter are tried in order; the first match wins.
        // The word is padded with a leading space (word boundary) so left-context patterns
        // that begin with ' ' (space) can fire at the word onset.
        public static byte[] Convert(string word) {
            if (string.IsNullOrEmpty(word)) {
                return Array.Empty<byte>();
            }

            // Pad: leading space (word boundary) + word uppercase + two trailing spaces
            var inp = new char[word.Length + 3];
            inp[0] = ' ';
            for (int i = 0; i < word.Length; i++) {
                inp[i + 1] = char.ToUpperInvariant(word[i]);
            }
            inp[word.Length + 1] = ' ';
            inp[word.Length + 2] = ' ';

            var phons = new List<byte>(word.Length * 2);
            int pos = 1;

            while (inp[pos] != ' ') {
                char c = inp[pos];
                if (c == '\'' || c == '.') {
                    pos++;
                    continue;
                }
                int li = c - 'A';
                if (li < 0 || li >= 26) {
                    pos++;
                    continue;
                }

                bool matched = false;
                foreach (var rule in CompiledLetterToSoundRules[li]) {
                    if (!MatchMid(inp, pos, rule.Match, out int endPos)) {
                        continue;
                    }
                    if (!MatchCtx(inp, pos - 1, rule.Left, rule.Left.Length - 1, -1)) {
                        continue;
                    }
                    if (!MatchCtx(inp, endPos, rule.Right, 0, +1)) {
                        continue;
                    }

                    foreach (byte ph in rule.Out) {
                        phons.Add(ph);
                    }
                    pos = endPos;
                    matched = true;
                    break;
                }
                if (!matched) {
                    pos++;
                }
            }

            return phons.ToArray();
        }

        static bool MatchMid(char[] inp, int pos, string match, out int end) {
            end = pos;
            foreach (char m in match) {
                if (end >= inp.Length || inp[end] != m) {
                    return false;
                }
                end++;
            }
            return true;
        }

        // Recursive context matcher with backtracking for #, *, :
        // dir=+1: left-to-right (right context), ci advances forward
        // dir=-1: right-to-left (left context), ci retreats toward -1
        static bool MatchCtx(char[] inp, int pos, string ctx, int ci, int dir) {
            int cEnd = dir == 1 ? ctx.Length : -1;
            if (ci == cEnd) {
                return true;
            }

            char sym = ctx[ci];
            int nci = ci + dir;

            switch (sym) {
                case '#': // one or more vowels
                    if (!IsVowel(inp, pos)) {
                        return false;
                    }
                    pos += dir;
                    while (true) {
                        if (MatchCtx(inp, pos, ctx, nci, dir)) {
                            return true;
                        }
                        if (!IsVowel(inp, pos)) {
                            return false;
                        }
                        pos += dir;
                    }

                case '*': // one or more consonants
                    if (!IsConsonant(inp, pos)) {
                        return false;
                    }
                    pos += dir;
                    while (true) {
                        if (MatchCtx(inp, pos, ctx, nci, dir)) {
                            return true;
                        }
                        if (!IsConsonant(inp, pos)) {
                            return false;
                        }
                        pos += dir;
                    }

                case ':': // zero or more consonants
                    if (MatchCtx(inp, pos, ctx, nci, dir)) {
                        return true;
                    }
                    while (IsConsonant(inp, pos)) {
                        pos += dir;
                        if (MatchCtx(inp, pos, ctx, nci, dir)) {
                            return true;
                        }
                    }
                    return false;

                case '^': // exactly one consonant
                    if (!IsConsonant(inp, pos)) {
                        return false;
                    }
                    return MatchCtx(inp, pos + dir, ctx, nci, dir);

                case '+': // one front vowel (E I Y)
                    if (pos < 0 || pos >= inp.Length || (CharacterFeatureTable[inp[pos]] & CF) == 0) {
                        return false;
                    }
                    return MatchCtx(inp, pos + dir, ctx, nci, dir);

                case '.': // one voiced consonant
                    if (!IsVoiced(inp, pos)) {
                        return false;
                    }
                    return MatchCtx(inp, pos + dir, ctx, nci, dir);

                case '&': // one sibilant
                {
                        int p2 = pos;
                        if (!MatchSibilant(inp, ref p2, dir)) {
                            return false;
                        }
                        return MatchCtx(inp, p2, ctx, nci, dir);
                    }

                case '@': // one long-U consonant
                    if (!IsUMod(inp, pos)) {
                        return false;
                    }
                    return MatchCtx(inp, pos + dir, ctx, nci, dir);

                case '%': // suffix (right context only): ER, E, ES, ED, ING, ELY
                    if (!MatchSuffix(inp, pos, out int sfxEnd)) {
                        return false;
                    }
                    return MatchCtx(inp, sfxEnd, ctx, nci, dir);

                case '$': // one consonant followed by E or I
                    if (!IsConsonant(inp, pos)) {
                        return false;
                    }
                    pos += dir;
                    if (pos < 0 || pos >= inp.Length || (CharacterFeatureTable[inp[pos]] & CF) == 0) {
                        return false;
                    }
                    return MatchCtx(inp, pos + dir, ctx, nci, dir);

                case ' ': // word boundary
                    if (pos < 0 || pos >= inp.Length || inp[pos] != ' ') {
                        return false;
                    }
                    return MatchCtx(inp, pos + dir, ctx, nci, dir);

                default: // literal character
                    if (pos < 0 || pos >= inp.Length || inp[pos] != sym) {
                        return false;
                    }
                    return MatchCtx(inp, pos + dir, ctx, nci, dir);
            }
        }

        static bool IsVowel(char[] inp, int p)
            => p >= 0 && p < inp.Length && (CharacterFeatureTable[inp[p]] & CV) != 0;

        static bool IsConsonant(char[] inp, int p) {
            if (p < 0 || p >= inp.Length) {
                return false;
            }
            char c = inp[p];
            if ((CharacterFeatureTable[c] & CC) != 0) {
                return true;
            }
            if ((c == 'Q' || c == 'G') && p + 1 < inp.Length && inp[p + 1] == 'U') {
                return true;
            }
            return false;
        }

        static bool IsVoiced(char[] inp, int p)
            => p >= 0 && p < inp.Length && (CharacterFeatureTable[inp[p]] & CZ) != 0;

        static bool IsUMod(char[] inp, int p)
            => p >= 0 && p < inp.Length && (CharacterFeatureTable[inp[p]] & CU) != 0;

        static bool MatchSibilant(char[] inp, ref int pos, int dir) {
            if (pos < 0 || pos >= inp.Length) {
                return false;
            }
            char c = inp[pos];
            if ((CharacterFeatureTable[c] & CS) != 0) {
                pos += dir;
                return true;
            }
            if (dir == 1 && (c == 'C' || c == 'S') && pos + 1 < inp.Length && inp[pos + 1] == 'H') {
                pos += 2;
                return true;
            }
            if (dir == -1 && c == 'H' && pos > 0 && (inp[pos - 1] == 'C' || inp[pos - 1] == 'S')) {
                pos -= 2;
                return true;
            }
            return false;
        }

        static bool MatchSuffix(char[] inp, int pos, out int end) {
            end = pos;
            if (pos < 0 || pos >= inp.Length) {
                return false;
            }
            char c = inp[pos];
            if (c == 'E') {
                if (pos + 2 < inp.Length && inp[pos + 1] == 'R') {
                    end = pos + 2;
                    return true;
                }
                if (pos + 2 < inp.Length && inp[pos + 1] == 'D') {
                    end = pos + 2;
                    return true;
                }
                if (pos + 2 < inp.Length && inp[pos + 1] == 'S') {
                    end = pos + 2;
                    return true;
                }
                if (pos + 3 < inp.Length && inp[pos + 1] == 'L' && inp[pos + 2] == 'Y') {
                    end = pos + 3;
                    return true;
                }
                if (pos + 1 < inp.Length && inp[pos + 1] == ' ') {
                    end = pos + 1;
                    return true;
                }
                return false;
            }
            if (c == 'I' && pos + 3 < inp.Length && inp[pos + 1] == 'N' && inp[pos + 2] == 'G') {
                end = pos + 3;
                return true;
            }
            return false;
        }
    }
}
