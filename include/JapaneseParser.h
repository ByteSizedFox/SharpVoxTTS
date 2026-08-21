#ifndef SHARPVOX_JAPANESE_PARSER_H
#define SHARPVOX_JAPANESE_PARSER_H

#include <cstdint>
#include <string>
#include <vector>
#include "AudioProcessor.h"

namespace SharpVox {

    // Converts a UTF-8 hiragana span directly to PhonemeToken objects.
    // Vowels use JP phoneme IDs (56-60); consonants use English IDs.
    // Japanese /r/ uses DX (alveolar tap).
    class JapaneseParser {
    public:
        static std::vector<PhonemeToken> SpanToPhonemes(const std::string& text,
                                                         size_t pos, size_t len,
                                                         bool normalizeParticles = true);

        // Map a single hiragana codepoint to a sequence of phoneme IDs.
        // Supports basic moras, voiced versions, and small tsu (geminate).
        // Does NOT handle yoon (clusters) here; those should be handled by the caller or
        // by passing multiple codepoints.
        static std::vector<int16_t> GetPhonemes(uint32_t cp);

        // UTF-8 decoding helper
        static uint32_t Utf8Decode(const unsigned char* s, size_t n, size_t& i);
    };

}  // namespace SharpVox

#endif  // SHARPVOX_JAPANESE_PARSER_H
