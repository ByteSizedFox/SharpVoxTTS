#ifndef SHARPTALK_PHONEMIZER_H
#define SHARPTALK_PHONEMIZER_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <utility>
#include "../include/audio_processor.h"
#include "../include/synth_data.h"
#include "../include/lexicon_reader.h"

namespace SharpTalk {

    class Phonemizer {
    public:
        int32_t StatDict;
        int32_t StatMorph;
        int32_t StatLts;

        void ResetStats() { StatDict = StatMorph = StatLts = 0; }
        DictReader& Dict() { return _dict; }

        Phonemizer(const uint8_t* dictData, size_t dictSize,
                   std::function<const uint8_t*(const std::string&, size_t&)> symbolsTable);

        int16_t LastEndPunct;

        // Split text into sentence/clause chunks, each yielding a token array + end-punct code.
        std::vector<std::pair<std::vector<PhonemeToken>, int16_t>> TextToSentenceTokens(const std::string& text);

        // Process a pure-text span (no embedded commands) into phoneme tokens.
        std::vector<PhonemeToken> TextSegmentToPhonemes(const std::string& text);

        std::vector<PhonemeToken> TextToPhonemes(const std::string& text);

        // Text normalization
        // Nested static class keeps normalizer state (regexes, tables) out of the
        // FrontEnd field list without a separate file.
        class Normalizer {
        public:
            static std::string Normalize(const std::string& text);

        private:
            static std::string SplitCamelCase(const std::string& text);
            static std::string ReplaceCurrency(const std::string& text);
            static std::string ReplacePercent(const std::string& text);
            static std::string ReplaceOrdinals(const std::string& text);
            static bool IsOrdinalSuffixAt(const std::string& text, int32_t pos);
            static std::string ReplaceYears(const std::string& text);
            static std::string ReplaceDecimals(const std::string& text);
            static std::string ReplaceDottedAbbrevs(const std::string& text);
            static std::string ReplaceAbbrevs(const std::string& text);
            static std::string SmallCardinal(int32_t n);
            static std::string YearToWords(int32_t y);
            static std::string OrdinalToWord(int64_t n);
        };

    private:
        DictReader _dict;
        std::function<const uint8_t*(const std::string&, size_t&)> _symbols;

        // For all-caps words absent from the dict, inject letter phonemes directly
        // no dict lookup, no LTS. Each letter becomes its own word-boundary token.
        std::vector<uint8_t> SpellOutAcronym(const std::string& upper);

        static bool IsAllCaps(const std::string& word);

        std::vector<uint8_t> WordToPhonStream(const std::string& upperWord);

        // Number -> raw phoneme stream
        std::vector<uint8_t> NumberToPhonStream(int64_t n);
        void BuildNumberPhons(std::vector<uint8_t>& buf, int64_t n);

        void AppendDigit(std::vector<uint8_t>& buf, int32_t d);
        void AppendTeen(std::vector<uint8_t>& buf, int32_t n);
        void AppendTens(std::vector<uint8_t>& buf, int32_t t);
        void AppendSymbol(std::vector<uint8_t>& buf, const std::string& sym);

        // Stream -> PhonemeToken list
        void AppendWordTokens(std::vector<PhonemeToken>& tokens, const std::vector<uint8_t>& stream,
                              bool isContent, bool isPronoun = false);
    };

}  // namespace SharpTalk

#endif  // SHARPTALK_PHONEMIZER_H
