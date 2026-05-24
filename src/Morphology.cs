#nullable enable
using System;
using System.Collections.Generic;
using static SharpTalk.AudioProcessor;

namespace SharpTalk {

    // Suffix-stripping morphology front-end for the dictionary lookup.
    //
    // When a word is not in the dictionary and letter-to-sound rules would be used,
    // TryDecompose first tries to find a known root by stripping a recognized suffix.
    // If the stripped root is in the dictionary, the suffix phonemes are appended using
    // the appropriate allomorph:
    //   /s/ /z/ /Iz/ for plural/3sg (based on last root phoneme)
    //   /t/ /d/ /Id/ for past tense
    //   ER for comparative/agent; IX NG for progressive; etc.
    //
    // The suffix table is ordered longest-first to prevent shorter suffixes from
    // matching before the correct longer one (e.g. "IZATIONS" should not match "S" first).
    static class Morph {
        enum Sfx {
            None,
            S, ES, IES, ED, ER, ERS, EST,
            IED, IER, IERS, IEST,
            ING, INGS,
            LY, BLY, CALLY,
            MENT, MENTS, IMENT, IMENTS,
            OR, ORS,
            NESS, NESSES, INESS, INESSES,
            IZE, IZED, IZES, IZER, IZERS,
            IZING, IZINGS,
            ISM, ISMS,
            ABLE,
        }

        // Ordered from longest to shortest to avoid early false matches.
        // Each entry: (suffix_string, stripped_length, suffix_type)
        static readonly (string Sfx, Sfx Type)[] SuffixTable =
        new (string Sfx, Sfx Type)[]
        {
        ("INESSES", Sfx.INESSES),
        ("NESSES",  Sfx.NESSES),
        ("IZINGS",  Sfx.IZINGS),
        ("IMENTS",  Sfx.IMENTS),
        ("IZERS",   Sfx.IZERS),
        ("CALLY",   Sfx.CALLY),
        ("IZING",   Sfx.IZING),
        ("INESS",   Sfx.INESS),
        ("MENTS",   Sfx.MENTS),
        ("IZED",    Sfx.IZED),
        ("IZER",    Sfx.IZER),
        ("IZES",    Sfx.IZES),
        ("ISMS",    Sfx.ISMS),
        ("IERS",    Sfx.IERS),
        ("IEST",    Sfx.IEST),
        ("INGS",    Sfx.INGS),
        ("ABLE",    Sfx.ABLE),
        ("IES",     Sfx.IES),
        ("IED",     Sfx.IED),
        ("IER",     Sfx.IER),
        ("ING",     Sfx.ING),
        ("IZE",     Sfx.IZE),
        ("ISM",     Sfx.ISM),
        ("ERS",     Sfx.ERS),
        ("EST",     Sfx.EST),
        ("BLY",     Sfx.BLY),
        ("MENT",    Sfx.MENT),
        ("ORS",     Sfx.ORS),
        ("ED",      Sfx.ED),
        ("ES",      Sfx.ES),
        ("ER",      Sfx.ER),
        ("LY",      Sfx.LY),
        ("OR",      Sfx.OR),
        ("S",       Sfx.S),
        };

        // Attempt to recognize a suffix, find the root in the dictionary, and
        // return root phonemes + suffix allomorph. Returns null if no rule fires.
        // Plain trailing-S is tested first because it is the most common case and
        // avoids iterating the full suffix table for every plural.
        public static byte[]? TryDecompose(string upper, DictReader dict) {
            // Try plain S first before suffix table
            if (upper.Length > 1 && upper[^1] == 'S') {
                string stem = upper[..^1];
                byte[]? root = dict.Search(stem);
                if (root != null) {
                    return Concat(root, SufPhons_S(root));
                }
            }

            // Try each suffix in order
            foreach (var (sfxStr, sfxType) in SuffixTable) {
                if (!upper.EndsWith(sfxStr, StringComparison.Ordinal)) {
                    continue;
                }
                string stem = upper[..^sfxStr.Length];
                if (stem.Length < 1) {
                    continue;
                }

                byte[]? result = ApplySuffix(sfxType, stem, sfxStr, dict);
                if (result != null) {
                    return result;
                }
            }

            return null;
        }

        static byte[]? ApplySuffix(Sfx sfx, string stem, string sfxStr, DictReader dict) {
            switch (sfx) {
                case Sfx.S: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Concat(r, SufPhons_S(r));
                    }

                case Sfx.ES: {
                        // Try stem+"S" first (houseS), then stem (fish, box)
                        byte[]? r = dict.Search(stem + "S");
                        if (r != null) {
                            return Concat(r, SufPhons_S(r));
                        }
                        // -sh/-ss/-x root, stem is already stripped of "ES"
                        char last = stem.Length > 0 ? stem[^1] : '\0';
                        char prev = stem.Length > 1 ? stem[^2] : '\0';
                        bool eshRoot = (last == 'H' && (prev == 'S' || prev == 'C'))
                                    || (last == 'S' && prev == 'S')
                                    || last == 'X';
                        if (eshRoot) {
                            r = dict.Search(stem);
                            if (r != null) {
                                return Concat(r, SufPhons_S(r));
                            }
                        }
                        // stem + "E" (house, clothe, name)
                        r = dict.Search(stem + "E");
                        if (r != null) {
                            return Concat(r, SufPhons_S(r));
                        }
                        // stem ending in S/Z (bus, waltz)
                        if (last == 'S' || last == 'Z') {
                            r = dict.Search(stem);
                            if (r != null) {
                                return Concat(r, SufPhons_S(r));
                            }
                        }
                        return null;
                    }

                case Sfx.IES: {
                        // Y-mutation, candies -> candy
                        byte[]? r = dict.Search(stem + "Y");
                        if (r != null) {
                            return Concat(r, SufPhons_S(r));
                        }
                        // calorie -> calories (stem + "IE")
                        r = dict.Search(stem + "IE");
                        if (r != null) {
                            return Concat(r, SufPhons_S(r));
                        }
                        return null;
                    }

                case Sfx.ED: {
                        byte[]? r = DecomposeE(stem, dict);
                        return r == null ? null : Concat(r, SufPhons_ED(r));
                    }

                case Sfx.ER: {
                        byte[]? r = DecomposeE(stem, dict);
                        return r == null ? null : Append(r, _ER_);
                    }

                case Sfx.ERS: {
                        // Try plain S first, stem+"ERS" -> stem+"ER"+"S"
                        byte[]? r = dict.Search(stem + "ER");
                        if (r != null) {
                            return Concat(r, SufPhons_S(r));
                        }
                        r = DecomposeE(stem, dict);
                        return r == null ? null : Append(Append(r, _ER_), _Z_);
                    }

                case Sfx.EST: {
                        byte[]? r = DecomposeE(stem, dict);
                        return r == null ? null : Concat(r, new byte[] { (byte)_IX_, (byte)_S_, (byte)_T_ });
                    }

                case Sfx.IED: {
                        byte[]? r = DecomposeI(stem, dict);
                        return r == null ? null : Append(r, _D_);
                    }

                case Sfx.IER: {
                        byte[]? r = DecomposeI(stem, dict);
                        return r == null ? null : Append(r, _ER_);
                    }

                case Sfx.IERS: {
                        // Try plain S, stem+"IERS" -> stem+"IER"+"S"
                        byte[]? r = dict.Search(stem + "IER");
                        if (r != null) {
                            return Concat(r, SufPhons_S(r));
                        }
                        r = DecomposeI(stem, dict);
                        return r == null ? null : Append(Append(r, _ER_), _Z_);
                    }

                case Sfx.IEST: {
                        byte[]? r = DecomposeI(stem, dict);
                        if (r != null) {
                            return Concat(r, new byte[] { (byte)_IX_, (byte)_S_, (byte)_T_ });
                        }
                        // loneliest, stem ends in L -> remove L, look up, add LY+EST
                        if (stem.Length > 0 && stem[^1] == 'L') {
                            r = dict.Search(stem[..^1]);
                            if (r != null) {
                                return Concat(r, new byte[] { (byte)_L_, (byte)_IY_, (byte)_IX_, (byte)_S_, (byte)_T_ });
                            }
                        }
                        return null;
                    }

                case Sfx.ING: {
                        byte[]? r = DecomposeE(stem, dict);
                        return r == null ? null : Append(Append(r, _IX_), _NG_);
                    }

                case Sfx.INGS: {
                        byte[]? r = DecomposeE(stem, dict);
                        return r == null ? null : Concat(r, new byte[] { (byte)_IX_, (byte)_NG_, (byte)_Z_ });
                    }

                case Sfx.LY: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Append(Append(r, _L_), _IY_);
                    }

                case Sfx.BLY: {
                        // possibly -> possible+LY, stem + "BLE" ("BLY" already stripped, stem ends in "BL")
                        byte[]? r = dict.Search(stem + "BLE");
                        return r == null ? null : Append(Append(r, _L_), _IY_);
                    }

                case Sfx.CALLY: {
                        // musically -> musical + LY
                        byte[]? r = dict.Search(stem + "C");
                        return r == null ? null : Append(Append(r, _L_), _IY_);
                    }

                case Sfx.MENT: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_M_, (byte)_AX_, (byte)_N_, (byte)_T_ });
                    }

                case Sfx.MENTS: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_M_, (byte)_AX_, (byte)_N_, (byte)_T_, (byte)_S_ });
                    }

                case Sfx.IMENT: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_M_, (byte)_AX_, (byte)_N_, (byte)_T_ });
                    }

                case Sfx.IMENTS: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_M_, (byte)_AX_, (byte)_N_, (byte)_T_, (byte)_S_ });
                    }

                case Sfx.OR: {
                        byte[]? r = dict.Search(stem + "E");
                        if (r != null) {
                            return Append(r, _ER_);
                        }
                        r = dict.Search(stem);
                        return r == null ? null : Append(r, _ER_);
                    }

                case Sfx.ORS: {
                        byte[]? r = dict.Search(stem + "E");
                        if (r != null) {
                            return Concat(r, new byte[] { (byte)_ER_, (byte)_Z_ });
                        }
                        r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_ER_, (byte)_Z_ });
                    }

                case Sfx.NESS: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_N_, (byte)_IX_, (byte)_S_ });
                    }

                case Sfx.NESSES: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_N_, (byte)_IX_, (byte)_S_, (byte)_IX_, (byte)_Z_ });
                    }

                case Sfx.INESS: {
                        // Y-mutation, sexiness -> sexy
                        byte[]? r = dict.Search(stem + "Y");
                        if (r != null) {
                            return Concat(r, new byte[] { (byte)_N_, (byte)_IX_, (byte)_S_ });
                        }
                        // loneliness, stem ends in L -> remove L, add LY+NESS
                        if (stem.Length > 0 && stem[^1] == 'L') {
                            r = dict.Search(stem[..^1]);
                            if (r != null) {
                                return Concat(r, new byte[] { (byte)_L_, (byte)_IY_, (byte)_N_, (byte)_IX_, (byte)_S_ });
                            }
                        }
                        return null;
                    }

                case Sfx.INESSES: {
                        byte[]? r = dict.Search(stem + "Y");
                        if (r != null) {
                            return Concat(r, new byte[] { (byte)_N_, (byte)_IX_, (byte)_S_, (byte)_IX_, (byte)_Z_ });
                        }
                        if (stem.Length > 0 && stem[^1] == 'L') {
                            r = dict.Search(stem[..^1]);
                            if (r != null) {
                                return Concat(r, new byte[] { (byte)_L_, (byte)_IY_, (byte)_N_, (byte)_IX_, (byte)_S_, (byte)_IX_, (byte)_Z_ });
                            }
                        }
                        return null;
                    }

                case Sfx.IZE: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_AY_, (byte)_Z_ });
                    }

                case Sfx.IZED: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_AY_, (byte)_Z_, (byte)_D_ });
                    }

                case Sfx.IZES: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_AY_, (byte)_Z_, (byte)_IX_, (byte)_Z_ });
                    }

                case Sfx.IZER: {
                        // Try Decompose_E first ("organizer" with ING fallback)
                        byte[]? r = DecomposeE(stem, dict);
                        if (r != null) {
                            return Append(r, _ER_);
                        }
                        r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_AY_, (byte)_Z_, (byte)_ER_ });
                    }

                case Sfx.IZERS: {
                        byte[]? r = DecomposeE(stem, dict);
                        if (r != null) {
                            return Concat(r, new byte[] { (byte)_ER_, (byte)_Z_ });
                        }
                        r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_AY_, (byte)_Z_, (byte)_ER_, (byte)_Z_ });
                    }

                case Sfx.IZING: {
                        // Try E-decompose first, "timing" ->"time"
                        byte[]? r = DecomposeE(stem, dict);
                        if (r != null) {
                            return Concat(r, new byte[] { (byte)_IX_, (byte)_NG_ });
                        }
                        r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_AY_, (byte)_Z_, (byte)_IX_, (byte)_NG_ });
                    }

                case Sfx.IZINGS: {
                        byte[]? r = DecomposeE(stem, dict);
                        if (r != null) {
                            return Concat(r, new byte[] { (byte)_IX_, (byte)_NG_, (byte)_Z_ });
                        }
                        r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_AY_, (byte)_Z_, (byte)_IX_, (byte)_NG_, (byte)_Z_ });
                    }

                case Sfx.ISM: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_IX_, (byte)_Z_, (byte)_AX_, (byte)_M_ });
                    }

                case Sfx.ISMS: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_IX_, (byte)_Z_, (byte)_AX_, (byte)_M_, (byte)_Z_ });
                    }

                case Sfx.ABLE: {
                        byte[]? r = dict.Search(stem);
                        return r == null ? null : Concat(r, new byte[] { (byte)_AX_, (byte)_B_, (byte)_EL_ });
                    }

                default:
                    return null;
            }
        }

        // Root recovery

        // Recovers root for -ED/-ER/-ERS/-EST/-ING etc.
        // Tries: stem+"E" (timed->time), then consonant-doubling removal (napped->nap).
        static byte[]? DecomposeE(string stem, DictReader dict) {
            byte[]? r = dict.Search(stem + "E");
            if (r != null) {
                return r;
            }
            string undoubled = RemoveDoubling(stem);
            if (undoubled.Length != stem.Length) {
                r = dict.Search(undoubled);
            }
            return r;
        }

        // Recovers root for -IED/-IER/-IERS/-IEST (Y-mutation, steadiest->steady).
        static byte[]? DecomposeI(string stem, DictReader dict)
            => dict.Search(stem + "Y");

        // Remove consonant doubling, "canned" stem "cann" -> "can", "slurring" stem "slurr" -> "slur".
        // Vowels and S/L/F are not doubled in roots (they stand alone).
        static string RemoveDoubling(string s) {
            if (s.Length < 2) {
                return s;
            }
            char last = s[^1];
            if ("AEIOUSLF".Contains(last)) {
                return s;
            }
            if (s[^2] == last) {
                return s[..^1];
            }
            return s;
        }

        // Suffix phoneme helpers

        // /s/ or /z/ or /Iz/ depending on last root phoneme.
        static byte[] SufPhons_S(byte[] root) {
            byte last = LastPhon(root);
            // After sibilants, /Iz/
            if (last == _S_ || last == _Z_ || last == _SH_ || last == _ZH_ || last == _CH_ || last == _JH_) {
                return new byte[] { (byte)_IX_, (byte)_Z_ };
            }
            // After unvoiced consonants, /s/
            if (IsUnvoicedConsonant(last)) {
                return new byte[] { (byte)_S_ };
            }
            return new byte[] { (byte)_Z_ };
        }

        // /t/ or /d/ or /Id/ depending on last root phoneme.
        static byte[] SufPhons_ED(byte[] root) {
            byte last = LastPhon(root);
            if (last == _T_ || last == _D_) {
                return new byte[] { (byte)_IX_, (byte)_D_ };
            }
            if (IsUnvoicedConsonant(last)) {
                return new byte[] { (byte)_T_ };
            }
            return new byte[] { (byte)_D_ };
        }

        static byte LastPhon(byte[] phons) {
            for (int i = phons.Length - 1; i >= 0; i--) {
                if (phons[i] <= 55) {
                    return phons[i];
                }
            }
            return (byte)_SIL_;
        }

        // Unvoiced obstruents, /p t k f th s sh tsh/
        static bool IsUnvoicedConsonant(byte p) =>
            p == _P_ || p == _T_ || p == _K_ || p == _F_ ||
            p == _TH_ || p == _S_ || p == _SH_ || p == _CH_;

        // Array helpers

        static byte[] Append(byte[] a, short phon) {
            var r = new byte[a.Length + 1];
            a.CopyTo(r, 0);
            r[^1] = (byte)phon;
            return r;
        }

        static byte[] Concat(byte[] a, byte[] b) {
            var r = new byte[a.Length + b.Length];
            a.CopyTo(r, 0);
            b.CopyTo(r, a.Length);
            return r;
        }

        static byte[] Concat(byte[] a, short[] b) {
            var r = new byte[a.Length + b.Length];
            a.CopyTo(r, 0);
            for (int i = 0; i < b.Length; i++) {
                r[a.Length + i] = (byte)b[i];
            }
            return r;
        }
    }
}  // namespace
