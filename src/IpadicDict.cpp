#include "../include/IpadicDict.h"
#include <cstring>

namespace SharpVox {

static const uint8_t kMagic[] = {'I','P','A','D'};
static const uint32_t kIndexEntrySize = 6;  // u32 + u16

static uint16_t read_u16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool IpadicDict::Load(const uint8_t* data, size_t size) {
    mData = nullptr;
    mDataSize = 0;
    mEntryCount = 0;

    if (size < 16) {
        return false;
    }
    if (std::memcmp(data, kMagic, 4) != 0) {
        return false;
    }

    uint16_t version = read_u16(data + 4);
    if (version != 1) {
        return false;
    }

    mEntryCount = read_u32(data + 6);
    mDataOffset = read_u32(data + 10);

    if (mDataOffset >= size) {
        return false;
    }

    if (mDataOffset + (size_t)mEntryCount * kIndexEntrySize > size) {
        return false;
    }

    mData = data;
    mDataSize = size;
    return true;
}

bool IpadicDict::FindEntry(const uint8_t* surface, uint16_t surfaceLen,  uint32_t& outOffset, uint16_t& outReadingLen) const {
    const uint8_t* indexBase = mData + 16;
    uint32_t lo = 0, hi = mEntryCount;

    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        const uint8_t* entry = indexBase + (size_t)mid * kIndexEntrySize;
        uint32_t dataOff = read_u32(entry);
        uint16_t sLen = read_u16(entry + 4);

        const uint8_t* sData = mData + mDataOffset + dataOff + 2;
        int cmp;
        uint16_t minLen = surfaceLen < sLen ? surfaceLen : sLen;
        cmp = std::memcmp(surface, sData, minLen);
        if (cmp == 0) {
            if (surfaceLen < sLen) {
                cmp = -1;
            } else if (surfaceLen > sLen) {
                cmp = 1;
            }
        }

        if (cmp < 0) {
            hi = mid;
        } else if (cmp > 0) {
            lo = mid + 1;
        } else {
            outOffset = dataOff;
            outReadingLen = read_u16(mData + mDataOffset + dataOff + 2 + sLen);
            return true;
        }
    }
    return false;
}

static uint32_t decode_utf8(const uint8_t*& p, const uint8_t* end) {
    if (p >= end) {
        return 0;
    }

    uint32_t cp;
    uint8_t b0 = *p;
    if (b0 < 0x80) {
        cp = b0; p++;
    } else if ((b0 & 0xE0) == 0xC0) {
        if (p + 1 >= end) {
            p++;
            return 0xFFFD;
        }
        cp = (uint32_t)(b0 & 0x1F) << 6 | (p[1] & 0x3F); p += 2;
    } else if ((b0 & 0xF0) == 0xE0) {
        if (p + 2 >= end) {
            p += 1;
            return 0xFFFD;
        }
        cp = (uint32_t)(b0 & 0x0F) << 12 | (uint32_t)(p[1] & 0x3F) << 6 |  (p[2] & 0x3F); p += 3;
    } else if ((b0 & 0xF8) == 0xF0) {
        if (p + 3 >= end) {
            p += 1;
            return 0xFFFD;
        }
        cp = (uint32_t)(b0 & 0x07) << 18 | (uint32_t)(p[1] & 0x3F) << 12 | (uint32_t)(p[2] & 0x3F) << 6 | (p[3] & 0x3F); p += 4;
    } else {
        p++;
        return 0xFFFD;
    }
    return cp;
}

static int utf8_char_len(uint8_t b0) {
    if (b0 < 0x80) return 1;
    if ((b0 & 0xE0) == 0xC0) return 2;
    if ((b0 & 0xF0) == 0xE0) return 3;
    if ((b0 & 0xF8) == 0xF0) return 4;
    return 1;
}

static bool is_kanji(uint32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0xF900 && cp <= 0xFAFF);
}

std::string IpadicDict::Lookup(const std::string& surface) const {
    if (!IsLoaded() || surface.empty()) {
        return std::string();
    }

    uint32_t dataOff;
    uint16_t readLen;
    if (!FindEntry(reinterpret_cast<const uint8_t*>(surface.data()),(uint16_t)surface.size(), dataOff, readLen)) {
        return std::string();
    }

    const uint8_t* readPtr = mData + mDataOffset + dataOff + 2 +
                             surface.size() + 2;
    return std::string(reinterpret_cast<const char*>(readPtr), readLen);
}

std::string IpadicDict::GetReadings(const std::string& text) const {
    if (!IsLoaded() || text.empty()) {
        return text;
    }

    std::string result;
    result.reserve(text.size());

    const uint8_t* tStart = reinterpret_cast<const uint8_t*>(text.data());
    const uint8_t* tEnd = tStart + text.size();
    const uint8_t* pos = tStart;

    while (pos < tEnd) {
        const uint8_t* scanTmp = pos;
        uint32_t curCp = decode_utf8(scanTmp, tEnd);

        if (!is_kanji(curCp)) {
            int clen = utf8_char_len(*pos);
            result.append(reinterpret_cast<const char*>(pos), clen);
            pos += clen;
            continue;
        }

        const uint8_t* boundaries[17];
        boundaries[0] = pos;
        int nChars = 1;
        const uint8_t* tmp = pos;
        while (tmp < tEnd && nChars < 17) {
            int clen = utf8_char_len(*tmp);
            tmp += clen;
            boundaries[nChars++] = tmp;
        }

        const uint8_t* bestEnd = nullptr;
        uint32_t bestDataOff = 0;
        uint16_t bestReadLen = 0;

        for (int len = nChars - 1; len >= 1; len--) {
            uint16_t sLen = (uint16_t)(boundaries[len] - boundaries[0]);
            uint32_t dataOff;
            uint16_t readLen;
            if (FindEntry(boundaries[0], sLen, dataOff, readLen)) {
                bestEnd = boundaries[len];
                bestDataOff = dataOff;
                bestReadLen = readLen;
                break;
            }
        }

        if (bestEnd) {
            const uint8_t* readPtr = mData + mDataOffset + bestDataOff + 2 + (bestEnd - boundaries[0]) + 2;
            result.append(reinterpret_cast<const char*>(readPtr), bestReadLen);
            pos = bestEnd;
        } else {
            int clen = utf8_char_len(*pos);
            result.append(reinterpret_cast<const char*>(pos), clen);
            pos += clen;
        }
    }

    return result;
}

}  // namespace SharpVox
