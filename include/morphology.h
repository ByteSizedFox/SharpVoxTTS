#ifndef SHARPTALK_MORPHOLOGY_H
#define SHARPTALK_MORPHOLOGY_H

#include <cstdint>
#include <string>
#include <vector>

namespace SharpTalk {

    class DictReader;

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
    class Morph {
    public:
        // Attempt to recognize a suffix, find the root in the dictionary, and
        // return root phonemes + suffix allomorph. Returns empty vector if no rule fires.
        // Plain trailing-S is tested first because it is the most common case and
        // avoids iterating the full suffix table for every plural.
        static std::vector<uint8_t> TryDecompose(const std::string& upper, DictReader& dict);

    private:
        enum class Sfx {
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
        };

        struct SuffixEntry {
            const char* Sfx;
            enum Sfx Type;
        };

        // Ordered from longest to shortest to avoid early false matches.
        // Each entry: (suffix_string, suffix_type)
        static const SuffixEntry SuffixTable[];

        static std::vector<uint8_t> ApplySuffix(Sfx sfx, const std::string& stem,
                                                  const std::string& sfxStr, DictReader& dict);

        // Root recovery

        // Recovers root for -ED/-ER/-ERS/-EST/-ING etc.
        // Tries: stem+"E" (timed->time), then consonant-doubling removal (napped->nap).
        static std::vector<uint8_t> DecomposeE(const std::string& stem, DictReader& dict);

        // Recovers root for -IED/-IER/-IERS/-IEST (Y-mutation, steadiest->steady).
        static std::vector<uint8_t> DecomposeI(const std::string& stem, DictReader& dict);

        // Remove consonant doubling, "canned" stem "cann" -> "can", "slurring" stem "slurr" -> "slur".
        // Vowels and S/L/F are not doubled in roots (they stand alone).
        static std::string RemoveDoubling(const std::string& s);

        // Suffix phoneme helpers

        // /s/ or /z/ or /Iz/ depending on last root phoneme.
        static std::vector<uint8_t> SufPhons_S(const std::vector<uint8_t>& root);

        // /t/ or /d/ or /Id/ depending on last root phoneme.
        static std::vector<uint8_t> SufPhons_ED(const std::vector<uint8_t>& root);

        static uint8_t LastPhon(const std::vector<uint8_t>& phons);

        // Unvoiced obstruents, /p t k f th s sh tsh/
        static bool IsUnvoicedConsonant(uint8_t p);

        // Array helpers

        static std::vector<uint8_t> Append(const std::vector<uint8_t>& a, int16_t phon);
        static std::vector<uint8_t> Concat(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);
        static std::vector<uint8_t> Concat(const std::vector<uint8_t>& a, std::initializer_list<int16_t> b);
    };

}  // namespace SharpTalk

#endif  // SHARPTALK_MORPHOLOGY_H
