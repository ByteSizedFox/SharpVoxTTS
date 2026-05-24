#nullable enable
using System;
using System.Runtime.InteropServices;
using OpenTK.Audio.OpenAL;
using SharpTalk;

namespace SharpTalkGui;

sealed class AudioPlayer : IDisposable {
    readonly ALDevice _device;
    readonly ALContext _context;
    readonly int _source;
    int _buffer;

    public AudioPlayer() {
        _device = ALC.OpenDevice(null);
        _context = ALC.CreateContext(_device, (int[]?)null);
        ALC.MakeContextCurrent(_context);

        _source = AL.GenSource();
        _buffer = 0;
    }

    public void Play(short[] samples, int sampleRate = TtsEngine.DefaultSampleRate) {
        // Stop and detach any current buffer
        AL.SourceStop(_source);
        AL.Source(_source, ALSourcei.Buffer, 0);

        // Delete the old buffer if one exists
        if (_buffer != 0) {
            AL.DeleteBuffer(_buffer);
            _buffer = 0;
        }

        // Upload new buffer
        _buffer = AL.GenBuffer();
        var bytes = MemoryMarshal.AsBytes(samples.AsSpan());
        AL.BufferData(_buffer, ALFormat.Mono16, bytes, sampleRate);

        // Attach and play
        AL.Source(_source, ALSourcei.Buffer, _buffer);
        AL.SourcePlay(_source);
    }

    public void Stop() {
        AL.SourceStop(_source);
    }

    public bool IsPlaying {
        get {
            AL.GetSource(_source, ALGetSourcei.SourceState, out int state);
            return state == (int)ALSourceState.Playing;
        }
    }

    public float ElapsedSeconds {
        get {
            AL.GetSource(_source, ALSourcef.SecOffset, out float offset);
            return offset;
        }
    }

    public void Dispose() {
        AL.SourceStop(_source);
        AL.Source(_source, ALSourcei.Buffer, 0);
        AL.DeleteSource(_source);
        if (_buffer != 0) {
            AL.DeleteBuffer(_buffer);
        }
        ALC.MakeContextCurrent(ALContext.Null);
        ALC.DestroyContext(_context);
        ALC.CloseDevice(_device);
    }
}
