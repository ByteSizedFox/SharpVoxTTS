#include "wav_writer.h"

#include <cstring>
#include <stdexcept>

namespace SharpTalk {

WavStreamWriter::WavStreamWriter(const std::string& path, int32_t sampleRate)
    : _sampleRate(sampleRate), _dataBytes(0), _disposed(false) {
    _fs.open(path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!_fs.is_open()) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    // RIFF header
    WriteBytes("RIFF", 4);
    WriteInt32(0); // Placeholder for file size - 8
    WriteBytes("WAVE", 4);

    // fmt chunk
    WriteBytes("fmt ", 4);
    WriteInt32(16); // Chunk size
    WriteInt16(1);  // Audio format (PCM)
    WriteInt16(1);  // Mono
    WriteInt32(sampleRate);
    WriteInt32(sampleRate * 2); // Byte rate
    WriteInt16(2);  // Block align
    WriteInt16(16); // Bits per sample

    // data chunk
    WriteBytes("data", 4);
    WriteInt32(0); // Placeholder for data size
}

WavStreamWriter::~WavStreamWriter() {
    Dispose();
}

void WavStreamWriter::Write(const int16_t* samples, int32_t count) {
    if (count == 0) {
        return;
    }

    _fs.write(reinterpret_cast<const char*>(samples), count * sizeof(int16_t));
    _dataBytes += count * 2;
}

void WavStreamWriter::Write(const std::vector<int16_t>& samples) {
    Write(samples.data(), static_cast<int32_t>(samples.size()));
}

void WavStreamWriter::Dispose() {
    // Finalize header
    if (_disposed) {
        return;
    }
    _disposed = true;

    if (_fs.is_open()) {
        _fs.seekp(4, std::ios::beg);
        WriteInt32(36 + _dataBytes);
        _fs.seekp(40, std::ios::beg);
        WriteInt32(_dataBytes);
        _fs.close();
    }
}

void WavStreamWriter::WriteInt16(int16_t value) {
    _fs.write(reinterpret_cast<const char*>(&value), sizeof(int16_t));
}

void WavStreamWriter::WriteInt32(int32_t value) {
    _fs.write(reinterpret_cast<const char*>(&value), sizeof(int32_t));
}

void WavStreamWriter::WriteBytes(const char* data, int32_t count) {
    _fs.write(data, count);
}

void WavWriter::WriteWav(const std::string& path, const std::vector<int16_t>& samples, int32_t sampleRate) {
    WavStreamWriter writer(path, sampleRate);
    writer.Write(samples);
}

}  // namespace SharpTalk
