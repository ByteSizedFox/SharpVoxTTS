#ifndef SHARPTALK_LIBRARY_DATA_H
#define SHARPTALK_LIBRARY_DATA_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace SharpTalk {

// LibraryData holds the static binary assets embedded in the library:
//   - dictionary: the STDICT phoneme dictionary blob (library_data_dictionary.cpp)
//   - SymbolsTable: number/symbol pronunciation table (library_data_symbols.cpp)
class LibraryData {
public:
    // 302096-byte STDICT binary (raw, uncompressed).
    // Consumed by DictReader; do not access directly.
    static const uint8_t dictionary[];
    static const int32_t dictionarySize;

    // Number and symbol pronunciation table.
    // Used by Phonemizer::AppendSymbol for number-to-speech conversion.
    static const std::unordered_map<std::string, std::vector<uint8_t>> SymbolsTable;
};

}  // namespace SharpTalk

#endif  // SHARPTALK_LIBRARY_DATA_H
