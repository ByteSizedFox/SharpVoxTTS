#ifndef SHARPVOX_LIBRARY_DATA_H
#define SHARPVOX_LIBRARY_DATA_H

#include <cstdint>
#include <cstddef>
#include <string>

namespace SharpVox {

// LibraryData holds the static binary assets embedded in the library:
//   - dictionary: the STDICT phoneme dictionary blob (library_data_dictionary.cpp)
//   - FindSymbol: number/symbol pronunciation lookup (library_data_symbols.cpp)
class LibraryData {
public:
    // 302096-byte STDICT binary (raw, uncompressed).
    // Consumed by DictReader; do not access directly.
    static const uint8_t dictionary[];
    static const int32_t dictionarySize;

    // Number and symbol pronunciation lookup.
    // Returns pointer to flash-resident phoneme data and sets sz, or nullptr if not found.
    static const uint8_t* FindSymbol(const char* key, size_t& sz);
};

}  // namespace SharpVox

#endif  // SHARPVOX_LIBRARY_DATA_H
