#ifndef SHARPVOX_LETTER_TO_SOUND_H
#define SHARPVOX_LETTER_TO_SOUND_H

#include <cstdint>
#include <string>
#include <vector>

namespace SharpVox {

    // English letter-to-sound rules derived from NRL Report 7948
    // (Elovitz, Johnson, McHugh, Shore - "Automatic Translation of English Text to Phonetics", 1976)
    class LetterToSound {
    public:
        // Apply LTS rules to a single word and return its phoneme sequence.
        // CompiledLetterToSoundRules for each initial letter are tried in order; the first match wins.
        // The word is padded with a leading space (word boundary) so left-context patterns
        // that begin with ' ' (space) can fire at the word onset.
        static std::vector<uint8_t> Convert(const std::string& word);

    private:
        // Feature flag bits
        static constexpr uint8_t CV = 0x01; // vowel: A E I O U Y
        static constexpr uint8_t CF = 0x02; // front vowel: E I Y
        static constexpr uint8_t CC = 0x04; // consonant: all non-vowels
        static constexpr uint8_t CZ = 0x08; // voiced consonant: B D G J L M N R V W Z
        static constexpr uint8_t CS = 0x10; // sibilant: S C G Z X J (+ CH/SH digraphs)
        static constexpr uint8_t CU = 0x20; // @-consonant (long-U modifier): T S R D L Z N J

        struct CompiledRule {
            std::string Left;
            std::string Match;
            std::string Right;
            std::vector<uint8_t> Out;
            CompiledRule(std::string l, std::string m, std::string r, std::vector<uint8_t> o)
                : Left(std::move(l)), Match(std::move(m)), Right(std::move(r)), Out(std::move(o)) {}
        };

        static uint8_t CharacterFeatureTable[128];
        static std::vector<CompiledRule> CompiledLetterToSoundRules[26];
        static bool s_initialized;

        static void Initialize();

        // Format: "LEFT[MATCH]RIGHT=OUTPUT"
        // Special symbols in LEFT / RIGHT (not between brackets):
        //   #  1+ vowels          *  1+ consonants   .  voiced consonant
        //   $  1 consonant + E/I  %  suffix           &  sibilant
        //   @  long-U consonant   ^  exactly 1 cons   +  front vowel (E I Y)
        //   :  0+ consonants      ' '  word boundary
        // OUTPUT: space-separated NRL phoneme names, or empty for silence.

        // Called once at static construction time; converts each rule string to a CompiledRule.
        static void Compile(const char* const src[][64], const int groupSizes[26]);

        // Splits "LEFT[MATCH]RIGHT=OUTPUT" into its four fields.
        static CompiledRule ParseRule(const char* s);

        static std::vector<uint8_t> ParseOutput(const std::string& s);

        static bool MatchMid(const std::vector<char>& inp, int pos, const std::string& match, int& end);

        // Recursive context matcher with backtracking for #, *, :
        // dir=+1: left-to-right (right context), ci advances forward
        // dir=-1: right-to-left (left context), ci retreats toward -1
        static bool MatchCtx(const std::vector<char>& inp, int pos, const std::string& ctx, int ci, int dir);

        static bool IsVowel(const std::vector<char>& inp, int p);
        static bool IsConsonant(const std::vector<char>& inp, int p);
        static bool IsVoiced(const std::vector<char>& inp, int p);
        static bool IsUMod(const std::vector<char>& inp, int p);
        static bool MatchSibilant(const std::vector<char>& inp, int& pos, int dir);
        static bool MatchSuffix(const std::vector<char>& inp, int pos, int& end);
    };

}  // namespace SharpVox

#endif  // SHARPVOX_LETTER_TO_SOUND_H
