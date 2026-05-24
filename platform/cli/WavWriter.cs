using System;
using System.IO;
using System.Runtime.InteropServices;

namespace SharpTalk.Cli;

public sealed class WavStreamWriter : IDisposable {
    private readonly FileStream _fs;
    private readonly BinaryWriter _w;
    private readonly int _sampleRate;
    private int _dataBytes;

    public WavStreamWriter(string path, int sampleRate) {
        _fs = File.Create(path);
        _w = new BinaryWriter(_fs);
        _sampleRate = sampleRate;

        // RIFF header
        _w.Write("RIFF"u8);
        _w.Write(0); // Placeholder for file size - 8
        _w.Write("WAVE"u8);

        // fmt chunk
        _w.Write("fmt "u8);
        _w.Write(16); // Chunk size
        _w.Write((short)1); // Audio format (PCM)
        _w.Write((short)1); // Mono
        _w.Write(sampleRate);
        _w.Write(sampleRate * 2); // Byte rate
        _w.Write((short)2); // Block align
        _w.Write((short)16); // Bits per sample

        // data chunk
        _w.Write("data"u8);
        _w.Write(0); // Placeholder for data size
    }

    public void Write(short[] samples) {
        if (samples.Length == 0) return;
        
        _w.Write(MemoryMarshal.Cast<short, byte>(samples.AsSpan()));
        _dataBytes += samples.Length * 2;
    }

    public void Dispose() {
        // Finalize header
        try {
            if (_fs.CanSeek) {
                _fs.Seek(4, SeekOrigin.Begin);
                _w.Write(36 + _dataBytes);
                _fs.Seek(40, SeekOrigin.Begin);
                _w.Write(_dataBytes);
            }
        } finally {
            _w.Dispose();
            _fs.Dispose();
        }
    }
}

public static class WavWriter {
    public static void WriteWav(string path, short[] samples, int sampleRate) {
        using var writer = new WavStreamWriter(path, sampleRate);
        writer.Write(samples);
    }
}
