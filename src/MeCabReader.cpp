#include "../include/MeCabReader.h"
#include "../include/IpadicDict.h"
#include <cstdio>
#include <cstdlib>

namespace SharpVox {

    static IpadicDict s_dict;

    bool MeCabReader::Init(const char* dicdir) {
        Shutdown();

        std::string path;
        if (dicdir) {
            path = dicdir;
            if (!path.empty() && path.back() != '/' && path.back() != '\\') {
                path += '/';
            }
            path += "ipadic.bin";
        } else {
            path = "ipadic.bin";
        }

        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) {
            return false;
        }

        std::fseek(f, 0, SEEK_END);
        long fsize = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);

        if (fsize <= 0) {
            std::fclose(f);
            return false;
        }

        uint8_t* buf = (uint8_t*)std::malloc((size_t)fsize);
        if (!buf) {
            std::fclose(f);
            return false;
        }

        size_t rd = std::fread(buf, 1, (size_t)fsize, f);
        std::fclose(f);

        if (rd != (size_t)fsize) {
            std::free(buf);
            return false;
        }

        if (!s_dict.Load(buf, (size_t)fsize)) {
            std::free(buf);
            return false;
        }
        return true;
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
