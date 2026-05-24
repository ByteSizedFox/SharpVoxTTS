#nullable enable
using System.Collections.Generic;
using static SharpTalk.AudioProcessor;

namespace SharpTalk {
    // Contextual heteronym disambiguation.
    // For each ambiguous word we store a default pronunciation and a list of
    // context rules, the first matching rule wins.  CompiledLetterToSoundRules fire when a word in
    // the before-set appears immediately before OR a word in the after-set
    // appears immediately after the target.
    internal static class HeteronymResolver {
        const short S1 = 56; // OP_STRESS1
        const byte OP_WORD = 64; // word boundary marker (same as Phonemizer.OP_WORD)

        static byte[] Ph(params short[] p) {
            var buf = new byte[p.Length + 1];
            buf[0] = OP_WORD;
            for (int i = 0; i < p.Length; i++) {
                buf[i + 1] = (byte)p[i];
            }
            return buf;
        }

        readonly struct Rule {
            public readonly HashSet<string>? Before;
            public readonly HashSet<string>? After;
            public readonly byte[] Phonemes;
            public Rule(string[]? before, string[]? after, byte[] ph) {
                Before = before is null ? null : new HashSet<string>(before);
                After = after is null ? null : new HashSet<string>(after);
                Phonemes = ph;
            }
            public bool Matches(string? prev, string? next) =>
                (Before != null && prev != null && Before.Contains(prev)) ||
                (After != null && next != null && After.Contains(next));
        }

        readonly struct Entry {
            public readonly Rule[] CompiledLetterToSoundRules;
            public readonly byte[] Default;
            public Entry(byte[] def, params Rule[] rules) { Default = def; CompiledLetterToSoundRules = rules; }
        }

        // LIVE
        static readonly byte[] LiveVerb = Ph(_L_, S1, _IH_, _V_);         // /lIv/
        static readonly byte[] LiveAdj = Ph(_L_, S1, _AY_, _V_);         // /laIv/
        // READ
        static readonly byte[] ReadPres = Ph(_R_, S1, _IY_, _D_);         // /ri:d/
        static readonly byte[] ReadPast = Ph(_R_, S1, _EH_, _D_);         // /rEd/
        // LEAD
        static readonly byte[] LeadVerb = Ph(_L_, S1, _IY_, _D_);         // /li:d/
        static readonly byte[] LeadMet = Ph(_L_, S1, _EH_, _D_);         // /lEd/
        // WIND
        static readonly byte[] WindNoun = Ph(_W_, S1, _IH_, _N_, _D_);    // /wInd/
        static readonly byte[] WindVerb = Ph(_W_, S1, _AY_, _N_, _D_);    // /waInd/
        // WOUND
        static readonly byte[] WoundInj = Ph(_W_, S1, _UW_, _N_, _D_);    // /wu:nd/ injury
        static readonly byte[] WoundPst = Ph(_W_, S1, _AW_, _N_, _D_);    // /waUnd/ past-of-wind
        // TEAR
        static readonly byte[] TearRip = Ph(_T_, S1, _EH_, _R_);         // /tEr/ rip
        static readonly byte[] TearEye = Ph(_T_, S1, _IH_, _R_);         // /tIr/ cry
        // BOW
        static readonly byte[] BowWeap = Ph(_B_, S1, _OW_);              // /boU/ weapon/ribbon
        static readonly byte[] BowGest = Ph(_B_, S1, _AW_);              // /baU/ gesture
        // CLOSE
        static readonly byte[] CloseVrb = Ph(_K_, _L_, S1, _OW_, _Z_);    // /kloUz/ verb
        static readonly byte[] CloseAdj = Ph(_K_, _L_, S1, _OW_, _S_);    // /kloUs/ near


        static readonly Dictionary<string, Entry> Table = new() {
            ["LIVE"] = new Entry(LiveVerb,
                new Rule(
                    before: new[] { "GO", "GOES", "WENT", "STREAM", "STREAMED", "BROADCAST", "AIRED" },
                    after: new[] { "MUSIC", "CONCERT", "SHOW", "PERFORMANCE", "WIRE", "AMMUNITION",
                                    "AMMO", "BAIT", "ROUND", "FIRE", "BROADCAST", "EVENT", "GAME", "GAMES" },
                    LiveAdj)),

            ["READ"] = new Entry(ReadPres,
                new Rule(
                    before: new[] { "HAD", "HAVE", "HAS", "ALREADY", "JUST", "NEVER",
                                    "I'VE", "YOU'VE", "WE'VE", "THEY'VE", "WHO'VE" },
                    after: null,
                    ReadPast)),

            ["LEAD"] = new Entry(LeadVerb,
                new Rule(
                    before: null,
                    after: new[] { "PIPE", "PIPES", "PAINT", "PENCIL", "POISONING",
                                    "BULLET", "BULLETS", "SHOT", "WEIGHT", "WEIGHTS", "FREE" },
                    LeadMet)),

            ["WIND"] = new Entry(WindNoun,
                new Rule(
                    before: new[] { "TO", "WILL", "CAN", "COULD", "WOULD", "LET", "LETS" },
                    after: new[] { "UP", "DOWN", "BACK", "THROUGH", "AROUND" },
                    WindVerb)),

            ["WOUND"] = new Entry(WoundInj,
                new Rule(
                    before: null,
                    after: new[] { "UP", "DOWN", "BACK", "AROUND", "THROUGH" },
                    WoundPst)),

            ["TEAR"] = new Entry(TearRip,
                new Rule(
                    before: new[] { "A", "THE", "ONE", "MY", "HER", "HIS", "YOUR",
                                    "EACH", "EVERY", "SINGLE" },
                    after: new[] { "DUCT", "DUCTS", "DROP", "DROPS", "GAS", "JERKER", "STAINED" },
                    TearEye)),

            ["BOW"] = new Entry(BowWeap,
                new Rule(
                    before: new[] { "TAKE", "TAKES", "TOOK", "MAKE", "MADE", "GIVE",
                                    "GIVES", "GAVE", "DEEP" },
                    after: new[] { "DOWN", "TO", "BEFORE", "OUT" },
                    BowGest)),

            ["CLOSE"] = new Entry(CloseVrb,
                new Rule(
                    before: new[] { "VERY", "TOO", "SO", "QUITE", "FAIRLY", "PRETTY",
                                    "REALLY", "THAT", "THIS", "COME", "STAY", "REMAIN",
                                    "REMAINS", "GET", "GETS", "CAME", "GETTING", "STAYING" },
                    after: new[] { "TO", "BY", "ENOUGH", "CALL", "FRIEND", "FRIENDS",
                                    "CONTACT", "SHAVE", "QUARTERS", "TOGETHER" },
                    CloseAdj)),
        };

        // Returns the full phoneme stream (OP_WORD + phonemes) if a rule matches,
        // or null to fall through to normal dictionary/LTS lookup.
        public static byte[]? Resolve(IReadOnlyList<string> words, int index) {
            string word = words[index];
            if (!Table.TryGetValue(word, out var entry)) {
                return null;
            }

            string? prev = index > 0 ? words[index - 1] : null;
            string? next = index < words.Count - 1 ? words[index + 1] : null;

            foreach (var rule in entry.CompiledLetterToSoundRules) {
                if (rule.Matches(prev, next)) {
                    return rule.Phonemes;
                }
            }

            return entry.Default;
        }
    }
}
