#include "Morphology.h"
#include "../include/LexiconReader.h"

#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

// Phoneme IDs (mirrors AudioProcessor constants)
static constexpr int16_t _IY_ = 0;
static constexpr int16_t _IX_ = 4;  static constexpr int16_t _AX_ = 5;
static constexpr int16_t _ER_ = 6;  static constexpr int16_t _AY_ = 13;
static constexpr int16_t _M_  = 24; static constexpr int16_t _N_  = 25;
static constexpr int16_t _NG_ = 26;
static constexpr int16_t _L_  = 30; static constexpr int16_t _EL_ = 33;
static constexpr int16_t _F_  = 35;
static constexpr int16_t _TH_ = 37; static constexpr int16_t _S_  = 39;
static constexpr int16_t _Z_  = 40; static constexpr int16_t _SH_ = 41;
static constexpr int16_t _ZH_ = 42;
static constexpr int16_t _P_  = 44; static constexpr int16_t _B_  = 45;
static constexpr int16_t _T_  = 46; static constexpr int16_t _D_  = 47;
static constexpr int16_t _K_  = 48;
static constexpr int16_t _CH_ = 50; static constexpr int16_t _JH_ = 51;
static constexpr int16_t _SIL_= 23;

namespace SharpVox {

// Ordered from longest to shortest to avoid early false matches.
// Each entry: (suffix_string, stripped_length, suffix_type)
const Morph::SuffixEntry Morph::SuffixTable[] = {
    {"INESSES", Sfx::INESSES},
    {"NESSES",  Sfx::NESSES},
    {"IZINGS",  Sfx::IZINGS},
    {"IMENTS",  Sfx::IMENTS},
    {"IZERS",   Sfx::IZERS},
    {"CALLY",   Sfx::CALLY},
    {"IZING",   Sfx::IZING},
    {"INESS",   Sfx::INESS},
    {"MENTS",   Sfx::MENTS},
    {"IZED",    Sfx::IZED},
    {"IZER",    Sfx::IZER},
    {"IZES",    Sfx::IZES},
    {"ISMS",    Sfx::ISMS},
    {"IERS",    Sfx::IERS},
    {"IEST",    Sfx::IEST},
    {"INGS",    Sfx::INGS},
    {"ABLE",    Sfx::ABLE},
    {"IES",     Sfx::IES},
    {"IED",     Sfx::IED},
    {"IER",     Sfx::IER},
    {"ING",     Sfx::ING},
    {"IZE",     Sfx::IZE},
    {"ISM",     Sfx::ISM},
    {"ERS",     Sfx::ERS},
    {"EST",     Sfx::EST},
    {"BLY",     Sfx::BLY},
    {"MENT",    Sfx::MENT},
    {"ORS",     Sfx::ORS},
    {"ED",      Sfx::ED},
    {"ES",      Sfx::ES},
    {"ER",      Sfx::ER},
    {"LY",      Sfx::LY},
    {"OR",      Sfx::OR},
    {"S",       Sfx::S},
    {nullptr,   Sfx::None},
};

// Attempt to recognize a suffix, find the root in the dictionary, and
// return root phonemes + suffix allomorph. Returns empty vector if no rule fires.
// Plain trailing-S is tested first because it is the most common case and
// avoids iterating the full suffix table for every plural.
std::vector<uint8_t> Morph::TryDecompose(const std::string& upper, DictReader& dict) {
    // Try plain S first before suffix table
    if (upper.size() > 1 && upper.back() == 'S') {
        std::string stem = upper.substr(0, upper.size() - 1);
        auto root = dict.Search(stem);
        if (!root.empty()) {
            return Concat(root, SufPhons_S(root));
        }
    }

    // Try each suffix in order
    for (int i = 0; SuffixTable[i].Sfx != nullptr; i++) {
        const char* sfxStr = SuffixTable[i].Sfx;
        Sfx sfxType = SuffixTable[i].Type;
        size_t sfxLen = strlen(sfxStr);
        if (upper.size() < sfxLen) {
            continue;
        }
        if (upper.compare(upper.size() - sfxLen, sfxLen, sfxStr) != 0) {
            continue;
        }
        std::string stem = upper.substr(0, upper.size() - sfxLen);
        if (stem.size() < 1) {
            continue;
        }

        std::vector<uint8_t> result = ApplySuffix(sfxType, stem, sfxStr, dict);
        if (!result.empty()) {
            return result;
        }
    }

    return {};
}

std::vector<uint8_t> Morph::ApplySuffix(Sfx sfx, const std::string& stem,
                                          const std::string& sfxStr, DictReader& dict) {
    switch (sfx) {
        case Sfx::S: {
            auto r = dict.Search(stem);
            return !r.empty() ? Concat(r, SufPhons_S(r)) : std::vector<uint8_t>{};
        }

        case Sfx::ES: {
            // Try stem+"S" first (houseS), then stem (fish, box)
            auto r = dict.Search(stem + "S");
            if (!r.empty()) {
                return Concat(r, SufPhons_S(r));
            }
            // -sh/-ss/-x root, stem is already stripped of "ES"
            char last = stem.size() > 0 ? stem.back() : '\0';
            char prev = stem.size() > 1 ? stem[stem.size() - 2] : '\0';
            bool eshRoot = (last == 'H' && (prev == 'S' || prev == 'C'))
                        || (last == 'S' && prev == 'S')
                        || last == 'X';
            if (eshRoot) {
                r = dict.Search(stem);
                if (!r.empty()) {
                    return Concat(r, SufPhons_S(r));
                }
            }
            // stem + "E" (house, clothe, name)
            r = dict.Search(stem + "E");
            if (!r.empty()) {
                return Concat(r, SufPhons_S(r));
            }
            // stem ending in S/Z (bus, waltz)
            if (last == 'S' || last == 'Z') {
                r = dict.Search(stem);
                if (!r.empty()) {
                    return Concat(r, SufPhons_S(r));
                }
            }
            return {};
        }

        case Sfx::IES: {
            // Y-mutation, candies -> candy
            auto r = dict.Search(stem + "Y");
            if (!r.empty()) {
                return Concat(r, SufPhons_S(r));
            }
            // calorie -> calories (stem + "IE")
            r = dict.Search(stem + "IE");
            if (!r.empty()) {
                return Concat(r, SufPhons_S(r));
            }
            return {};
        }

        case Sfx::ED: {
            auto r = DecomposeE(stem, dict);
            return r.empty() ? std::vector<uint8_t>{} : Concat(r, SufPhons_ED(r));
        }

        case Sfx::ER: {
            auto r = DecomposeE(stem, dict);
            return r.empty() ? std::vector<uint8_t>{} : Append(r, _ER_);
        }

        case Sfx::ERS: {
            // Try plain S first, stem+"ERS" -> stem+"ER"+"S"
            auto r = dict.Search(stem + "ER");
            if (!r.empty()) {
                return Concat(r, SufPhons_S(r));
            }
            r = DecomposeE(stem, dict);
            if (r.empty()) {
                return {};
            }
            return Append(Append(r, _ER_), _Z_);
        }

        case Sfx::EST: {
            auto r = DecomposeE(stem, dict);
            return r.empty() ? std::vector<uint8_t>{} : Concat(r, {_IX_, _S_, _T_});
        }

        case Sfx::IED: {
            auto r = DecomposeI(stem, dict);
            return r.empty() ? std::vector<uint8_t>{} : Append(r, _D_);
        }

        case Sfx::IER: {
            auto r = DecomposeI(stem, dict);
            return r.empty() ? std::vector<uint8_t>{} : Append(r, _ER_);
        }

        case Sfx::IERS: {
            // Try plain S, stem+"IERS" -> stem+"IER"+"S"
            auto r = dict.Search(stem + "IER");
            if (!r.empty()) {
                return Concat(r, SufPhons_S(r));
            }
            r = DecomposeI(stem, dict);
            if (r.empty()) {
                return {};
            }
            return Append(Append(r, _ER_), _Z_);
        }

        case Sfx::IEST: {
            auto r = DecomposeI(stem, dict);
            if (!r.empty()) {
                return Concat(r, {_IX_, _S_, _T_});
            }
            // loneliest, stem ends in L -> remove L, look up, add LY+EST
            if (!stem.empty() && stem.back() == 'L') {
                auto r2 = dict.Search(stem.substr(0, stem.size() - 1));
                if (!r2.empty()) {
                    return Concat(r2, {_L_, _IY_, _IX_, _S_, _T_});
                }
            }
            return {};
        }

        case Sfx::ING: {
            auto r = DecomposeE(stem, dict);
            return r.empty() ? std::vector<uint8_t>{} : Append(Append(r, _IX_), _NG_);
        }

        case Sfx::INGS: {
            auto r = DecomposeE(stem, dict);
            return r.empty() ? std::vector<uint8_t>{} : Concat(r, {_IX_, _NG_, _Z_});
        }

        case Sfx::LY: {
            auto r = dict.Search(stem);
            return !r.empty() ? Append(Append(r, _L_), _IY_) : std::vector<uint8_t>{};
        }

        case Sfx::BLY: {
            // possibly -> possible+LY, stem + "BLE" ("BLY" already stripped, stem ends in "BL")
            auto r = dict.Search(stem + "BLE");
            return !r.empty() ? Append(Append(r, _L_), _IY_) : std::vector<uint8_t>{};
        }

        case Sfx::CALLY: {
            // musically -> musical + LY
            auto r = dict.Search(stem + "C");
            return !r.empty() ? Append(Append(r, _L_), _IY_) : std::vector<uint8_t>{};
        }

        case Sfx::MENT: {
            auto r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_M_, _AX_, _N_, _T_}) : std::vector<uint8_t>{};
        }

        case Sfx::MENTS: {
            auto r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_M_, _AX_, _N_, _T_, _S_}) : std::vector<uint8_t>{};
        }

        case Sfx::IMENT: {
            auto r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_M_, _AX_, _N_, _T_}) : std::vector<uint8_t>{};
        }

        case Sfx::IMENTS: {
            auto r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_M_, _AX_, _N_, _T_, _S_}) : std::vector<uint8_t>{};
        }

        case Sfx::OR: {
            auto r = dict.Search(stem + "E");
            if (!r.empty()) {
                return Append(r, _ER_);
            }
            r = dict.Search(stem);
            return !r.empty() ? Append(r, _ER_) : std::vector<uint8_t>{};
        }

        case Sfx::ORS: {
            auto r = dict.Search(stem + "E");
            if (!r.empty()) {
                return Concat(r, {_ER_, _Z_});
            }
            r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_ER_, _Z_}) : std::vector<uint8_t>{};
        }

        case Sfx::NESS: {
            auto r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_N_, _IX_, _S_}) : std::vector<uint8_t>{};
        }

        case Sfx::NESSES: {
            auto r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_N_, _IX_, _S_, _IX_, _Z_}) : std::vector<uint8_t>{};
        }

        case Sfx::INESS: {
            // Y-mutation, sexiness -> sexy
            auto r = dict.Search(stem + "Y");
            if (!r.empty()) {
                return Concat(r, {_N_, _IX_, _S_});
            }
            // loneliness, stem ends in L -> remove L, add LY+NESS
            if (!stem.empty() && stem.back() == 'L') {
                r = dict.Search(stem.substr(0, stem.size() - 1));
                if (!r.empty()) {
                    return Concat(r, {_L_, _IY_, _N_, _IX_, _S_});
                }
            }
            return {};
        }

        case Sfx::INESSES: {
            auto r = dict.Search(stem + "Y");
            if (!r.empty()) {
                return Concat(r, {_N_, _IX_, _S_, _IX_, _Z_});
            }
            if (!stem.empty() && stem.back() == 'L') {
                r = dict.Search(stem.substr(0, stem.size() - 1));
                if (!r.empty()) {
                    return Concat(r, {_L_, _IY_, _N_, _IX_, _S_, _IX_, _Z_});
                }
            }
            return {};
        }

        case Sfx::IZE: {
            auto r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_AY_, _Z_}) : std::vector<uint8_t>{};
        }

        case Sfx::IZED: {
            auto r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_AY_, _Z_, _D_}) : std::vector<uint8_t>{};
        }

        case Sfx::IZES: {
            auto r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_AY_, _Z_, _IX_, _Z_}) : std::vector<uint8_t>{};
        }

        case Sfx::IZER: {
            // Try Decompose_E first ("organizer" with ING fallback)
            auto r = DecomposeE(stem, dict);
            if (!r.empty()) {
                return Append(r, _ER_);
            }
            r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_AY_, _Z_, _ER_}) : std::vector<uint8_t>{};
        }

        case Sfx::IZERS: {
            auto r = DecomposeE(stem, dict);
            if (!r.empty()) {
                return Concat(r, {_ER_, _Z_});
            }
            r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_AY_, _Z_, _ER_, _Z_}) : std::vector<uint8_t>{};
        }

        case Sfx::IZING: {
            // Try E-decompose first, "timing" ->"time"
            auto r = DecomposeE(stem, dict);
            if (!r.empty()) {
                return Concat(r, {_IX_, _NG_});
            }
            r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_AY_, _Z_, _IX_, _NG_}) : std::vector<uint8_t>{};
        }

        case Sfx::IZINGS: {
            auto r = DecomposeE(stem, dict);
            if (!r.empty()) {
                return Concat(r, {_IX_, _NG_, _Z_});
            }
            r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_AY_, _Z_, _IX_, _NG_, _Z_}) : std::vector<uint8_t>{};
        }

        case Sfx::ISM: {
            auto r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_IX_, _Z_, _AX_, _M_}) : std::vector<uint8_t>{};
        }

        case Sfx::ISMS: {
            auto r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_IX_, _Z_, _AX_, _M_, _Z_}) : std::vector<uint8_t>{};
        }

        case Sfx::ABLE: {
            auto r = dict.Search(stem);
            return !r.empty() ? Concat(r, {_AX_, _B_, _EL_}) : std::vector<uint8_t>{};
        }

        default:
            return {};
    }
}

// Root recovery

// Recovers root for -ED/-ER/-ERS/-EST/-ING etc.
// Tries: stem+"E" (timed->time), then consonant-doubling removal (napped->nap).
std::vector<uint8_t> Morph::DecomposeE(const std::string& stem, DictReader& dict) {
    auto r = dict.Search(stem + "E");
    if (!r.empty()) {
        return r;
    }
    std::string undoubled = RemoveDoubling(stem);
    if (undoubled.size() != stem.size()) {
        r = dict.Search(undoubled);
        if (!r.empty()) {
            return r;
        }
    }
    return {};
}

// Recovers root for -IED/-IER/-IERS/-IEST (Y-mutation, steadiest->steady).
std::vector<uint8_t> Morph::DecomposeI(const std::string& stem, DictReader& dict) {
    auto r = dict.Search(stem + "Y");
    return !r.empty() ? r : std::vector<uint8_t>{};
}

// Remove consonant doubling, "canned" stem "cann" -> "can", "slurring" stem "slurr" -> "slur".
// Vowels and S/L/F are not doubled in roots (they stand alone).
std::string Morph::RemoveDoubling(const std::string& s) {
    if (s.size() < 2) {
        return s;
    }
    char last = s.back();
    if (std::string("AEIOUSLF").find(last) != std::string::npos) {
        return s;
    }
    if (s[s.size() - 2] == last) {
        return s.substr(0, s.size() - 1);
    }
    return s;
}

// Suffix phoneme helpers

// /s/ or /z/ or /Iz/ depending on last root phoneme.
std::vector<uint8_t> Morph::SufPhons_S(const std::vector<uint8_t>& root) {
    uint8_t last = LastPhon(root);
    // After sibilants, /Iz/
    if (last == _S_ || last == _Z_ || last == _SH_ || last == _ZH_ || last == _CH_ || last == _JH_) {
        return {(uint8_t)_IX_, (uint8_t)_Z_};
    }
    // After unvoiced consonants, /s/
    if (IsUnvoicedConsonant(last)) {
        return {(uint8_t)_S_};
    }
    return {(uint8_t)_Z_};
}

// /t/ or /d/ or /Id/ depending on last root phoneme.
std::vector<uint8_t> Morph::SufPhons_ED(const std::vector<uint8_t>& root) {
    uint8_t last = LastPhon(root);
    if (last == _T_ || last == _D_) {
        return {(uint8_t)_IX_, (uint8_t)_D_};
    }
    if (IsUnvoicedConsonant(last)) {
        return {(uint8_t)_T_};
    }
    return {(uint8_t)_D_};
}

uint8_t Morph::LastPhon(const std::vector<uint8_t>& phons) {
    for (int i = (int)phons.size() - 1; i >= 0; i--) {
        if (phons[i] <= 55) {
            return phons[i];
        }
    }
    return (uint8_t)_SIL_;
}

// Unvoiced obstruents, /p t k f th s sh tsh/
bool Morph::IsUnvoicedConsonant(uint8_t p) {
    return p == _P_ || p == _T_ || p == _K_ || p == _F_ ||
           p == _TH_ || p == _S_ || p == _SH_ || p == _CH_;
}

// Array helpers

std::vector<uint8_t> Morph::Append(const std::vector<uint8_t>& a, int16_t phon) {
    std::vector<uint8_t> r = a;
    r.push_back((uint8_t)phon);
    return r;
}

std::vector<uint8_t> Morph::Concat(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    std::vector<uint8_t> r = a;
    r.insert(r.end(), b.begin(), b.end());
    return r;
}

std::vector<uint8_t> Morph::Concat(const std::vector<uint8_t>& a, std::initializer_list<int16_t> b) {
    std::vector<uint8_t> r = a;
    for (int16_t v : b) {
        r.push_back((uint8_t)v);
    }
    return r;
}

}  // namespace SharpVox
