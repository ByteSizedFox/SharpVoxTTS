#ifndef SHARPTALK_WAV_WRITER_H
#define SHARPTALK_WAV_WRITER_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace SharpTalk {

class WavStreamWriter {
public:
    WavStreamWriter(const std::string& path, int32_t sampleRate);
    ~WavStreamWriter();

    // Non-copyable
    WavStreamWriter(const WavStreamWriter&) = delete;
    WavStreamWriter& operator=(const WavStreamWriter&) = delete;

    void Write(const int16_t* samples, int32_t count);
    void Write(const std::vector<int16_t>& samples);

    // Finalize header (called automatically by destructor)
    void Dispose();

private:
    std::ofstream _fs;
    int32_t _sampleRate;
    int32_t _dataBytes;
    bool _disposed;

    void WriteInt16(int16_t value);
    void WriteInt32(int32_t value);
    void WriteBytes(const char* data, int32_t count);
};

class WavWriter {
public:
    static void WriteWav(const std::string& path, const std::vector<int16_t>& samples, int32_t sampleRate);
};

}  // namespace SharpTalk

#endif  // SHARPTALK_WAV_WRITER_H
