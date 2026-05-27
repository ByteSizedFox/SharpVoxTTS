#include "LexiconReader.h"

#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>

namespace SharpVox {

// Reads a uint32_t in little-endian byte order from the given offset.
static uint32_t ReadU32LE(const uint8_t* data, int32_t off) {
    return (uint32_t)data[off]
         | ((uint32_t)data[off + 1] << 8)
         | ((uint32_t)data[off + 2] << 16)
         | ((uint32_t)data[off + 3] << 24);
}

DictReader::DictReader(const uint8_t* data, int32_t dataLen)
    : _data(data), _dataLen(dataLen), _wordCount(0)
{
    // STDICT header layout (all little-endian):
    //   0   4 bytes  magic "STDK"
    //   4   uint16   version
    //   6   uint16   reserved
    //   8   uint32   word_count
    //  12   uint32   data_off
    //  16   uint32   index_off
    //  20   uint32   letter_starts[27]  (108 bytes)

    if (data[0] != (uint8_t)'S' || data[1] != (uint8_t)'T' ||
        data[2] != (uint8_t)'D' || data[3] != (uint8_t)'K') {
        throw std::runtime_error("Invalid STDICT magic");
    }

    _wordCount = (int32_t)ReadU32LE(data, 8);
    int32_t indexOff = (int32_t)ReadU32LE(data, 16);

    for (int i = 0; i < HASH_ENTRIES; i++) {
        _hash[i] = ReadU32LE(data, 20 + i * 4);
    }

    _index.resize(_wordCount);
    for (int i = 0; i < _wordCount; i++) {
        _index[i] = (int32_t)ReadU32LE(data, indexOff + i * 4);
    }
}

std::vector<uint8_t> DictReader::Search(const std::string& word) const {
    if (_wordCount == 0 || word.empty()) {
        return {};
    }

    int32_t tLen = (int32_t)word.size();
    char first = word[0];

    int32_t lo, hi;
    if (first >= 'A' && first <= 'Z') {
        int letterIdx = first - 'A';
        lo = (int32_t)_hash[letterIdx];
        hi = (int32_t)_hash[letterIdx + 1] - 1;
    } else if (first < 'A') {
        lo = 0;
        hi = (int32_t)_hash[0] - 1;
    } else {
        lo = (int32_t)_hash['Z' - 'A'];
        hi = _wordCount - 1;
    }

    while (lo <= hi) {
        int32_t mid = (lo + hi) >> 1;
        int32_t off = _index[mid];
        int32_t dLen = _data[off];
        int32_t diff = 0;
        int32_t cmp = std::min(tLen, dLen);
        for (int i = 0; i < cmp; i++) {
            diff = (uint8_t)word[i] - _data[off + 1 + i];
            if (diff != 0) {
                break;
            }
        }
        if (diff == 0) {
            diff = tLen - dLen;
        }

        if (diff > 0) {
            lo = mid + 1;
        } else if (diff < 0) {
            hi = mid - 1;
        } else {
            int32_t phonOff = off + 1 + dLen;
            int32_t phonLen = _data[phonOff];
            std::vector<uint8_t> phons(phonLen);
            for (int i = 0; i < phonLen; i++) {
                phons[i] = _data[phonOff + 1 + i];
            }
            return phons;
        }
    }
    return {};
}

void DictReader::EnumerateAll(
    std::function<void(const std::string&, const std::vector<uint8_t>&)> callback) const
{
    for (int i = 0; i < _wordCount; i++) {
        int32_t off = _index[i];
        int32_t dLen = _data[off];
        std::string word(reinterpret_cast<const char*>(_data + off + 1), dLen);
        int32_t phonOff = off + 1 + dLen;
        int32_t phonLen = _data[phonOff];
        std::vector<uint8_t> phons(phonLen);
        for (int j = 0; j < phonLen; j++) {
            phons[j] = _data[phonOff + 1 + j];
        }
        callback(word, phons);
    }
}

}  // namespace SharpVox
