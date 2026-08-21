#include "../include/MeCabReader.h"
#include "../include/IpadicDict.h"

namespace SharpVox {

    static IpadicDict s_dict;

    extern const uint8_t kIpadicData[];
    extern const size_t kIpadicDataSize;

    bool MeCabReader::Init() {
        Shutdown();
        return s_dict.Load(kIpadicData, kIpadicDataSize);
    }

    void MeCabReader::Shutdown() {
        s_dict = IpadicDict();
    }

    bool MeCabReader::IsAvailable() {
        return s_dict.IsLoaded();
    }

    std::string MeCabReader::ExtractReadings(const std::string& text) {
        if (!s_dict.IsLoaded() || text.empty()) {
            return std::string();
        }
        return s_dict.GetReadings(text);
    }

}  // namespace SharpVox
