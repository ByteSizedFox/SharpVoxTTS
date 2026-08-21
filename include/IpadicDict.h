#ifndef SHARPVOX_IPADIC_DICT_H
#define SHARPVOX_IPADIC_DICT_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace SharpVox {

class IpadicDict {
public:
    bool Load(const uint8_t* data, size_t size);

    std::string Lookup(const std::string& surface) const;

    std::string GetReadings(const std::string& text) const;

    bool IsLoaded() const { return mEntryCount > 0; }

private:
    bool FindEntry(const uint8_t* surface, uint16_t surfaceLen,
                   uint32_t& outOffset, uint16_t& outReadingLen) const;

    const uint8_t* mData = nullptr;
    size_t         mDataSize = 0;
    uint32_t       mEntryCount = 0;
    uint32_t       mDataOffset = 0;  // offset to data blob within the file
};

}  // namespace SharpVox

#endif  // SHARPVOX_IPADIC_DICT_H
