#include "../include/JapaneseParser.h"
#include "../include/AudioProcessor.h"

namespace SharpVox {

    // Vowel shorthands: JP-specific IDs for formant table lookup
    static const int16_t J_AA  = _JP_A_;
    static const int16_t J_IY  = _JP_I_;
    static const int16_t J_UH  = _JP_U_;
    static const int16_t J_EH  = _JP_E_;
    static const int16_t J_AO  = _JP_O_;

    // Consonant shorthands: English IDs
    static const uint8_t J_K   = (uint8_t)_K_;
    static const uint8_t J_G   = (uint8_t)_G_;
    static const uint8_t J_S   = (uint8_t)_S_;
    static const uint8_t J_Z   = (uint8_t)_Z_;
    static const uint8_t J_SH  = (uint8_t)_SH_;
    static const uint8_t J_JH  = (uint8_t)_JH_;
    static const uint8_t J_T   = (uint8_t)_T_;
    static const uint8_t J_D   = (uint8_t)_D_;
    static const uint8_t J_CH  = (uint8_t)_CH_;
    static const uint8_t J_N   = (uint8_t)_N_;
    static const uint8_t J_HH  = (uint8_t)_HH_;
    static const uint8_t J_F   = (uint8_t)_F_;
    static const uint8_t J_B   = (uint8_t)_B_;
    static const uint8_t J_P   = (uint8_t)_P_;
    static const uint8_t J_M   = (uint8_t)_M_;
    static const uint8_t J_Y   = (uint8_t)_Y_;
    static const uint8_t J_DX  = (uint8_t)_DX_;  // alveolar tap for Japanese /r/
    static const uint8_t J_NG  = (uint8_t)_NG_;
    static const uint8_t J_W   = (uint8_t)_W_;
    static const uint8_t J_V   = (uint8_t)_V_;

    // Sentinel values in Mora.vowel (all outside valid int16_t phoneme range when cast to uint8_t)
    static const uint8_t JP_GEMINATE   = 0xFE;
    static const uint8_t JP_SYLLABIC_N = 0xFD;
    static const uint8_t JP_INVALID    = 0xFF;
    static const uint8_t JP_ITER       = 0xF8;
    static const uint8_t JP_ITER_DAKU  = 0xF7;
    static const uint8_t JP_MIDDLEDOT  = 0xF6;

    struct Mora {
        uint8_t c1;          // first consonant, 0xFF = bare vowel
        uint8_t c2;          // second consonant (tsuu = T+S), 0xFF = none
        uint8_t vowel;       // JP_* sentinel or JP vowel ID cast to uint8_t
        bool    yoon_base;   // mora ends in /i/ and can take a yoon modifier
        bool    palatalized; // already palatalized (sh/ch/jh): yoon omits Y glide
    };

    static Mora LookupMora(uint32_t cp) {
        Mora m = { 0xFF, 0xFF, JP_INVALID, false, false };
        uint8_t va = (uint8_t)J_AA, vi = (uint8_t)J_IY, vu = (uint8_t)J_UH,
                ve = (uint8_t)J_EH, vo = (uint8_t)J_AO;
        switch (cp) {
            // -- bare vowels --
            case 0x3041: case 0x3042: m.vowel = va; break;  // a  a
            case 0x3043: case 0x3044: m.vowel = vi; break;  // i  i
            case 0x3045: case 0x3046: m.vowel = vu; break;  // u  u
            case 0x3047: case 0x3048: m.vowel = ve; break;  // e  e
            case 0x3049: case 0x304A: m.vowel = vo; break;  // o  o
            // -- K-row --
            case 0x304B: m.c1 = J_K; m.vowel = va; break;
            case 0x304C: m.c1 = J_G; m.vowel = va; break;
            case 0x304D: m.c1 = J_K; m.vowel = vi; m.yoon_base = true; break;
            case 0x304E: m.c1 = J_G; m.vowel = vi; m.yoon_base = true; break;
            case 0x304F: m.c1 = J_K; m.vowel = vu; break;
            case 0x3050: m.c1 = J_G; m.vowel = vu; break;
            case 0x3051: m.c1 = J_K; m.vowel = ve; break;
            case 0x3052: m.c1 = J_G; m.vowel = ve; break;
            case 0x3053: m.c1 = J_K; m.vowel = vo; break;
            case 0x3054: m.c1 = J_G; m.vowel = vo; break;
            // -- S-row --
            case 0x3055: m.c1 = J_S;  m.vowel = va; break;
            case 0x3056: m.c1 = J_Z;  m.vowel = va; break;
            case 0x3057: m.c1 = J_SH; m.vowel = vi; m.yoon_base = true; m.palatalized = true; break;
            case 0x3058: m.c1 = J_JH; m.vowel = vi; m.yoon_base = true; m.palatalized = true; break;
            case 0x3059: m.c1 = J_S;  m.vowel = vu; break;
            case 0x305A: m.c1 = J_Z;  m.vowel = vu; break;
            case 0x305B: m.c1 = J_S;  m.vowel = ve; break;
            case 0x305C: m.c1 = J_Z;  m.vowel = ve; break;
            case 0x305D: m.c1 = J_S;  m.vowel = vo; break;
            case 0x305E: m.c1 = J_Z;  m.vowel = vo; break;
            // -- T-row --
            case 0x305F: m.c1 = J_T;  m.vowel = va; break;
            case 0x3060: m.c1 = J_D;  m.vowel = va; break;
            case 0x3061: m.c1 = J_CH; m.vowel = vi; m.yoon_base = true; m.palatalized = true; break;
            case 0x3062: m.c1 = J_JH; m.vowel = vi; m.yoon_base = true; m.palatalized = true; break;
            case 0x3063: m.vowel = JP_GEMINATE; break;
            case 0x3064: m.c1 = J_T; m.c2 = J_S; m.vowel = vu; break;  // T+S+u
            case 0x3065: m.c1 = J_Z; m.vowel = vu; break;
            case 0x3066: m.c1 = J_T; m.vowel = ve; break;
            case 0x3067: m.c1 = J_D; m.vowel = ve; break;
            case 0x3068: m.c1 = J_T; m.vowel = vo; break;
            case 0x3069: m.c1 = J_D; m.vowel = vo; break;
            // -- N-row --
            case 0x306A: m.c1 = J_N; m.vowel = va; break;
            case 0x306B: m.c1 = J_N; m.vowel = vi; m.yoon_base = true; break;
            case 0x306C: m.c1 = J_N; m.vowel = vu; break;
            case 0x306D: m.c1 = J_N; m.vowel = ve; break;
            case 0x306E: m.c1 = J_N; m.vowel = vo; break;
            // -- H/B/P-row --
            case 0x306F: m.c1 = J_HH; m.vowel = va; break;
            case 0x3070: m.c1 = J_B;  m.vowel = va; break;
            case 0x3071: m.c1 = J_P;  m.vowel = va; break;
            case 0x3072: m.c1 = J_HH; m.vowel = vi; m.yoon_base = true; break;
            case 0x3073: m.c1 = J_B;  m.vowel = vi; m.yoon_base = true; break;
            case 0x3074: m.c1 = J_P;  m.vowel = vi; m.yoon_base = true; break;
            case 0x3075: m.c1 = J_F;  m.vowel = vu; break;
            case 0x3076: m.c1 = J_B;  m.vowel = vu; break;
            case 0x3077: m.c1 = J_P;  m.vowel = vu; break;
            case 0x3078: m.c1 = J_HH; m.vowel = ve; break;
            case 0x3079: m.c1 = J_B;  m.vowel = ve; break;
            case 0x307A: m.c1 = J_P;  m.vowel = ve; break;
            case 0x307B: m.c1 = J_HH; m.vowel = vo; break;
            case 0x307C: m.c1 = J_B;  m.vowel = vo; break;
            case 0x307D: m.c1 = J_P;  m.vowel = vo; break;
            // -- M-row --
            case 0x307E: m.c1 = J_M; m.vowel = va; break;
            case 0x307F: m.c1 = J_M; m.vowel = vi; m.yoon_base = true; break;
            case 0x3080: m.c1 = J_M; m.vowel = vu; break;
            case 0x3081: m.c1 = J_M; m.vowel = ve; break;
            case 0x3082: m.c1 = J_M; m.vowel = vo; break;
            // -- Y-row --
            case 0x3083: case 0x3084: m.c1 = J_Y; m.vowel = va; break;
            case 0x3085: case 0x3086: m.c1 = J_Y; m.vowel = vu; break;
            case 0x3087: case 0x3088: m.c1 = J_Y; m.vowel = vo; break;
            // -- R-row: alveolar tap --
            case 0x3089: m.c1 = J_DX; m.vowel = va; break;
            case 0x308A: m.c1 = J_DX; m.vowel = vi; m.yoon_base = true; break;
            case 0x308B: m.c1 = J_DX; m.vowel = vu; break;
            case 0x308C: m.c1 = J_DX; m.vowel = ve; break;
            case 0x308D: m.c1 = J_DX; m.vowel = vo; break;
            // -- W-row + particles --
            case 0x308E: case 0x308F: m.c1 = J_W; m.vowel = va; break;
            case 0x3090: m.c1 = J_W; m.vowel = vi; break;
            case 0x3091: m.c1 = J_W; m.vowel = ve; break;
            case 0x3092: m.vowel = vo; break;   // particle wo
            case 0x3093: m.vowel = JP_SYLLABIC_N; break;
            case 0x3094: m.c1 = J_V; m.vowel = vu; break;
            case 0x3095: m.c1 = J_K; m.vowel = va; break;
            case 0x3096: m.c1 = J_K; m.vowel = ve; break;
            // iteration marks, repeat previous mora (handled in main loop)
            case 0x309D: m.vowel = JP_ITER;      break;
            case 0x309E: m.vowel = JP_ITER_DAKU; break;
            // middle dot, word separator
            case 0x30FB: m.vowel = JP_MIDDLEDOT; break;
        }
        return m;
    }

    static bool IsHiragana(uint32_t cp) {
        return (cp >= 0x3041 && cp <= 0x3096) || cp == 0x30FC
            || cp == 0x309D || cp == 0x309E    // iteration marks
            || cp == 0x30FB;                    // middle dot
    }

    // Remap は/へ to their spoken form, mark particle boundaries so
    // vowel-length rules skip them. へ skips if followed by ん, not a particle.
    static void NormalizeParticles(std::vector<uint32_t>& cps, std::vector<bool>& boundary) {
        bool hadKana = false;
        for (size_t i = 0; i < cps.size(); i++) {
            uint32_t cp = cps[i];
            if (cp == 0x306F && hadKana) { cps[i] = 0x308F; boundary[i] = true; }
            else if (cp == 0x3078 && hadKana) {
                bool followedByN = (i + 1 < cps.size() && cps[i + 1] == 0x3093);
                if (!followedByN) { cps[i] = 0x3048; boundary[i] = true; }
            }
            else if (cp == 0x3092) { boundary[i] = true; }  // を particle
            if (IsHiragana(cp)) hadKana = true;
        }
    }

    uint32_t JapaneseParser::Utf8Decode(const unsigned char* s, size_t n, size_t& i) {
        if (i >= n) return 0xFFFFFFFF;
        uint8_t b0 = s[i];
        if (b0 < 0x80) { i++; return b0; }
        if ((b0 & 0xE0) == 0xC0) {
            if (i + 1 >= n) { i++; return 0xFFFFFFFF; }
            uint32_t cp = ((b0 & 0x1F) << 6) | (s[i+1] & 0x3F);
            i += 2; return cp;
        }
        if ((b0 & 0xF0) == 0xE0) {
            if (i + 2 >= n) { i++; return 0xFFFFFFFF; }
            uint32_t cp = ((b0 & 0x0F) << 12) | ((s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F);
            i += 3; return cp;
        }
        if ((b0 & 0xF8) == 0xF0) {
            if (i + 3 >= n) { i++; return 0xFFFFFFFF; }
            uint32_t cp = ((b0 & 0x07) << 18) | ((s[i+1] & 0x3F) << 12) |
                          ((s[i+2] & 0x3F) << 6)  | (s[i+3] & 0x3F);
            i += 4; return cp;
        }
        i++; return 0xFFFFFFFF;
    }

    static bool IsSmallYoon(uint32_t cp) {
        return cp == 0x3083 || cp == 0x3085 || cp == 0x3087;
    }

    static int16_t SmallYoonVowel(uint32_t cp) {
        if (cp == 0x3083) return J_AA;
        if (cp == 0x3085) return J_UH;
        return J_AO; // 0x3087
    }

    // Small vowel modifiers (loanword combos: fa, ti, di, wi, tsa, etc.)
    // These are the small forms: small-a/i/u/e/o (0x3041/3043/3045/3047/3049).
    static bool IsSmallVowelMod(uint32_t cp) {
        return cp == 0x3041 || cp == 0x3043 || cp == 0x3045 || cp == 0x3047 || cp == 0x3049;
    }

    static int16_t SmallVowelModVowel(uint32_t cp) {
        if (cp == 0x3041) return J_AA;
        if (cp == 0x3043) return J_IY;
        if (cp == 0x3045) return J_UH;
        if (cp == 0x3047) return J_EH;
        return J_AO; // 0x3049
    }

    // voiced counterpart of a voiceless onset, dakuten voicing for iteration marks.
    static uint8_t DakutenOf(uint8_t phon) {
        if (phon == J_K)  return J_G;
        if (phon == J_S)  return J_Z;
        if (phon == J_SH) return J_JH;
        if (phon == J_T)  return J_D;
        if (phon == J_CH) return J_JH;
        if (phon == J_HH) return J_B;
        if (phon == J_F)  return J_B;
        return phon;
    }

    // Returns the vowel quality ('a','i','u','e','o') at the end of the mora at cps[i].
    // peek is cps[i+1] — needed to detect yoon compounds.
    static char MoraEndVowelQuality(uint32_t cp, uint32_t peek) {
        if (IsSmallYoon(peek)) {
            Mora m = LookupMora(cp);
            if (m.yoon_base) {
                if (peek == 0x3083) return 'a';
                if (peek == 0x3085) return 'u';
                return 'o';  // 0x3087
            }
        }
        Mora m = LookupMora(cp);
        if (m.vowel == JP_INVALID || m.vowel == JP_GEMINATE || m.vowel == JP_SYLLABIC_N) return 0;
        uint8_t v = m.vowel;
        if (v == (uint8_t)J_AA) return 'a';
        if (v == (uint8_t)J_IY) return 'i';
        if (v == (uint8_t)J_UH) return 'u';
        if (v == (uint8_t)J_EH) return 'e';
        if (v == (uint8_t)J_AO) return 'o';
        return 0;
    }

    // True when cp is a bare hiragana vowel that lengthens a mora of quality prevQ.
    // Rules: same vowel repeats; ou -> o:; ei -> e:
    // Small forms (0x3041/3043/3045/3047/3049) are excluded: they are loanword modifiers,
    // not lengthening vowels, and matching them here corrupts e.g. ti (te+small-i) to te:.
    static bool IsLengtheningVowel(char prevQ, uint32_t cp) {
        if (prevQ == 0) return false;
        char q = 0;
        if (cp == 0x3042) q = 'a';
        else if (cp == 0x3044) q = 'i';
        else if (cp == 0x3046) q = 'u';
        else if (cp == 0x3048) q = 'e';
        else if (cp == 0x304A) q = 'o';
        else return false;
        return (prevQ == q) || (prevQ == 'o' && q == 'u') || (prevQ == 'e' && q == 'i');
    }

    // Replaces length-marking vowels (ou, ei, aa, ii, uu, ee, oo) with the long
    // vowel mark so the main loop treats them as duration extensions.
    // Skips positions marked in boundary (particle-remapped codepoints).
    static void NormalizeVowelLength(std::vector<uint32_t>& cps,
                                     const std::vector<bool>& boundary) {
        for (size_t i = 0; i + 1 < cps.size(); i++) {
            if (boundary[i]) continue;
            uint32_t peek = cps[i + 1];
            char q = MoraEndVowelQuality(cps[i], peek);
            if (q == 0) continue;
            bool yoonConsumed = IsSmallYoon(peek) && LookupMora(cps[i]).yoon_base;
            size_t nextIdx = yoonConsumed ? i + 2 : i + 1;
            if (nextIdx < cps.size() && !boundary[nextIdx] && IsLengtheningVowel(q, cps[nextIdx])) {
                cps[nextIdx] = 0x30FC;
            }
        }
    }

    std::vector<PhonemeToken> JapaneseParser::SpanToPhonemes(
            const std::string& text, size_t pos, size_t len) {
        std::vector<PhonemeToken> out;

        const unsigned char* s = (const unsigned char*)text.c_str() + pos;
        size_t n = len;

        // Decode entire span to codepoints, then normalize particles.
        std::vector<uint32_t> cps;
        {
            size_t i = 0;
            while (i < n) {
                uint32_t cp = Utf8Decode(s, n, i);
                if (cp != 0xFFFFFFFF) cps.push_back(cp);
            }
        }
        // Katakana (U+30A1-U+30F6) maps 1:1 to hiragana via a fixed offset.
        for (auto& cp : cps) {
            if (cp >= 0x30A1 && cp <= 0x30F6) cp -= 0x60;
        }
        std::vector<bool> particleBoundary(cps.size(), false);
        NormalizeParticles(cps, particleBoundary);
        NormalizeVowelLength(cps, particleBoundary);

        bool geminate  = false;
        bool wordStart = true;
        int16_t lastVowel = -1;
        size_t firstSecondaryIdx = (size_t)-1;
        int32_t consAccum = 0;
        bool    gemClosure = false;
        uint8_t lastCons = 0xFF;
        int16_t lastVowelPhon = -1;

        // Intrinsic consonant duration (ms) within a 120ms mora budget.
        auto ConsDurMs = [](uint8_t phon) -> int16_t {
            if (phon == J_DX)                             return 25;
            if (phon == J_Y)                              return 15;
            if (phon == J_W)                              return 20;
            if (phon == J_N || phon == J_M || phon == J_NG) return 45;
            if (phon == J_HH)                             return 50;
            if (phon == J_Z)                              return 50;
            if (phon == J_SH || phon == J_S || phon == J_F) return 55;
            if (phon == J_JH || phon == J_CH)             return 65;
            return 45;  // stops: K G T D B P V
        };

        auto EmitCons = [&](uint8_t phon) {
            PhonemeToken tok;
            tok.Phon = (int16_t)phon;
            tok.Ctrl = kContent_Word | kJapaneseMora;
            if (wordStart) { tok.Ctrl |= kWord_Start; wordStart = false; }
            int16_t d = gemClosure ? (int16_t)120 : ConsDurMs(phon);
            tok.UserDur = d;
            consAccum += d;
            lastCons = phon;
            out.push_back(tok);
        };

        auto EmitVowel = [&](int16_t phon) {
            PhonemeToken tok;
            tok.Phon = phon;
            tok.Ctrl = kContent_Word | kSecondaryStress | kJapaneseMora;
            if (wordStart) { tok.Ctrl |= kWord_Start; wordStart = false; }
            tok.UserDur = (int16_t)std::max((int32_t)30, (int32_t)120 - consAccum);
            consAccum = 0;
            if (firstSecondaryIdx == (size_t)-1) firstSecondaryIdx = out.size();
            out.push_back(tok);
            lastVowel = phon;
            lastVowelPhon = phon;
        };

        for (size_t ci = 0; ci < cps.size(); ci++) {
            uint32_t cp = cps[ci];
            if (cp == 0xFFFFFFFF) break;

            // Particle boundary, mark mora break for acoustic model.
            if (particleBoundary[ci]) wordStart = true;

            // Space or ideographic space, word boundary.
            if (cp == 0x0020 || cp == 0x3000) wordStart = true;

            // Long vowel mark: repeat previous vowel
            if (cp == 0x30FC) {
                if (lastVowel >= 0) EmitVowel(lastVowel);
                continue;
            }

            // middle dot, treat as word separator
            if (cp == 0x30FB) { wordStart = true; continue; }

            // iteration marks, repeat previous mora, dakuten variant voices the consonant
            if (cp == 0x309D || cp == 0x309E) {
                bool daku = (cp == 0x309E);
                if (lastVowelPhon >= 0) {
                    if (lastCons != 0xFF) {
                        EmitCons(daku ? DakutenOf(lastCons) : lastCons);
                    }
                    EmitVowel(lastVowelPhon);
                }
                continue;
            }

            Mora m = LookupMora(cp);
            if (m.vowel == JP_INVALID) continue;

            if (m.vowel == JP_GEMINATE) { geminate = true; continue; }

            // Syllabic N (ん): context-sensitive nasal assimilation
            if (m.vowel == JP_SYLLABIC_N) {
                if (geminate) {
                    gemClosure = true; EmitCons(J_N); gemClosure = false;
                    consAccum = 0;
                    geminate = false;
                }
                uint8_t nasal = J_N;
                if (ci + 1 < cps.size()) {
                    Mora nm = LookupMora(cps[ci + 1]);
                    if (nm.c1 == J_B || nm.c1 == J_P || nm.c1 == J_M) nasal = J_M;
                    else if (nm.c1 == J_K || nm.c1 == J_G) nasal = J_NG;
                }
                EmitVowel((int16_t)nasal);
                continue;
            }

            // Yoon / small-vowel-modifier lookahead
            if (ci + 1 < cps.size()) {
                uint32_t next_cp = cps[ci + 1];
                if (m.yoon_base && IsSmallYoon(next_cp)) {
                    int16_t yv = SmallYoonVowel(next_cp); ci++;
                    if (geminate && m.c1 != 0xFF) {
                        gemClosure = true; EmitCons(m.c1); gemClosure = false;
                        consAccum = 0; geminate = false;
                    }
                    if (m.c1 != 0xFF) EmitCons(m.c1);
                    if (!m.palatalized) EmitCons(J_Y);
                    EmitVowel(yv);
                    continue;
                }
                if (IsSmallVowelMod(next_cp)) {
                    int16_t sv = SmallVowelModVowel(next_cp); ci++;
                    if (geminate && m.c1 != 0xFF) {
                        gemClosure = true; EmitCons(m.c1); gemClosure = false;
                        consAccum = 0; geminate = false;
                    }
                    // bare u (0x3046) + small i/e/o -> w + vowel (wi, we, wo)
                    if (cp == 0x3046 && sv != J_UH) {
                        EmitCons(J_W); EmitVowel(sv); continue;
                    }
                    // bare i (0x3044) + small e -> y + e (ye)
                    if (cp == 0x3044 && sv == J_EH) {
                        EmitCons(J_Y); EmitVowel(J_EH); continue;
                    }
                    if (m.c1 != 0xFF) {
                        EmitCons(m.c1);
                        if (m.c2 != 0xFF) EmitCons(m.c2);
                        // yoon_base + small vowel mod: insert Y glide unless palatalized
                        if (m.yoon_base && !m.palatalized) EmitCons(J_Y);
                        EmitVowel(sv);
                        continue;
                    }
                    // fallback: bare vowel + same-quality small mod -> emit base
                    EmitVowel((int16_t)m.vowel);
                    continue;
                }
            }

            // Normal mora
            if (geminate && m.c1 != 0xFF) {
                gemClosure = true; EmitCons(m.c1); gemClosure = false;
                consAccum = 0;
                geminate = false;
            }
            if (m.c1 != 0xFF) EmitCons(m.c1);
            if (m.c2 != 0xFF) EmitCons(m.c2);
            EmitVowel((int16_t)m.vowel);
        }

        // Vowel devoicing, /u/ and /i/ after voiceless onsets or phrase-finally.
        // /u/ reduces more than /i/, consecutive devoiced moras suppressed.
        // Spirant exception chains, s->s/sh, f->f/h, h->f/h resist. (Maekawa/CSJ)
        {
            const int64_t kVowelBit = kSecondaryStress | kPrimaryStress;

            auto isVoiceless = [](uint8_t c) -> bool {
                return c == J_K || c == J_S || c == J_SH || c == J_T || c == J_CH
                    || c == J_HH || c == J_F || c == J_P;
            };
            auto isException = [](uint8_t onset, uint8_t next) -> bool {
                if (onset == J_S  && (next == J_S  || next == J_SH)) return true;
                if (onset == J_F  && (next == J_F  || next == J_HH)) return true;
                if (onset == J_HH && (next == J_HH || next == J_F )) return true;
                return false;
            };

            // Graduated reduction, phrase-final is strongest, /u/ devoices more than /i/.
            // Values in ms within a ~120ms mora budget.
            auto devoiceDur = [](int16_t phon, bool phraseFinal) -> int16_t {
                if (phon == J_UH) {
                    return phraseFinal ? 10 : 22;  // /u/, strong reduction, weak rounding
                } else {
                    return phraseFinal ? 18 : 38;  // /i/, more active tongue protects voicing
                }
            };

            bool prevDevoiced = false;
            for (size_t j = 0; j < out.size(); j++) {
                bool isVowel = (out[j].Ctrl & kVowelBit) != 0;
                if (!isVowel) continue;

                int16_t phon = out[j].Phon;
                if (phon != J_IY && phon != J_UH) { prevDevoiced = false; continue; }

                // Find immediate consonant before this vowel
                uint8_t onset = 0xFF;
                for (size_t jj = j; jj-- > 0;) {
                    if (out[jj].Ctrl & kVowelBit) break;
                    onset = (uint8_t)out[jj].Phon;
                    break;
                }

                if (onset == 0xFF || !isVoiceless(onset)) { prevDevoiced = false; continue; }

                // Find first consonant of next mora
                uint8_t nextCons = 0xFF;
                for (size_t jj = j + 1; jj < out.size(); jj++) {
                    if (out[jj].Ctrl & kVowelBit) break;
                    nextCons = (uint8_t)out[jj].Phon;
                    break;
                }

                bool phraseFinal = (nextCons == 0xFF);
                // /u/ devoices with just a voiceless onset (weak rounding bleeds through).
                // /i/ requires voiceless on both sides (tongue/jaw work protects voicing).
                bool needsBothSides = (phon == J_IY);
                if (needsBothSides && !phraseFinal && !isVoiceless(nextCons)) { prevDevoiced = false; continue; }
                if (!phraseFinal && isException(onset, nextCons)) { prevDevoiced = false; continue; }
                if (prevDevoiced)                                { prevDevoiced = false; continue; }

                out[j].UserDur = devoiceDur(phon, phraseFinal);
                prevDevoiced = true;
            }
        }

        // Promote first secondary to primary (no function-word demotion for Japanese)
        if (firstSecondaryIdx != (size_t)-1) {
            out[firstSecondaryIdx].Ctrl =
                (out[firstSecondaryIdx].Ctrl & ~kSecondaryStress) |
                kPrimaryStress;
        }

        return out;
    }

    std::vector<int16_t> JapaneseParser::GetPhonemes(uint32_t cp) {
        std::vector<int16_t> res;
        Mora m = LookupMora(cp);
        if (m.vowel == JP_INVALID) return res;

        if (m.vowel == JP_GEMINATE) {
            // In a singing context, small tsu is often treated as a glottal stop or silence.
            // But here we'll just return SIL to let the caller handle it.
            res.push_back((int16_t)_SIL_);
            return res;
        }
        if (m.vowel == JP_SYLLABIC_N) {
            res.push_back((int16_t)_N_);
            return res;
        }

        if (m.c1 != 0xFF) res.push_back((int16_t)m.c1);
        if (m.c2 != 0xFF) res.push_back((int16_t)m.c2);
        if (m.vowel != 0xFF) res.push_back((int16_t)m.vowel);

        return res;
    }

}  // namespace SharpVox
