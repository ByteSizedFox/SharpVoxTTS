#ifndef SHARPVOX_MECAB_READER_H
#define SHARPVOX_MECAB_READER_H

#include <string>

namespace SharpVox {

    class MeCabReader {
    public:
        static bool Init(const char* dicdir = nullptr);

        static void Shutdown();
        static bool IsAvailable();

        static std::string ExtractReadings(const std::string& text);
    };

}  // namespace SharpVox

#endif  // SHARPVOX_MECAB_READER_H
