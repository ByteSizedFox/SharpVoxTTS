#nullable enable
using System;
using System.Buffers.Binary;

namespace SharpTalk {

    public class DictReader {
        readonly byte[] _data;
        readonly int[] _index;   // absolute byte offsets into _data, one per entry
        readonly uint[] _hash;   // letter_starts[27]: word index of first entry for A-Z + sentinel
        readonly int _wordCount;

        const int HASH_ENTRIES = 27;

        public DictReader(byte[] data) {
            _data = data;

            // STDICT header layout (all little-endian):
            //   0   4 bytes  magic "STDK"
            //   4   uint16   version
            //   6   uint16   reserved
            //   8   uint32   word_count
            //  12   uint32   data_off
            //  16   uint32   index_off
            //  20   uint32   letter_starts[27]  (108 bytes)

            if (data[0] != (byte)'S' || data[1] != (byte)'T' ||
                data[2] != (byte)'D' || data[3] != (byte)'K') {
                throw new InvalidOperationException("Invalid STDICT magic");
            }

            _wordCount = (int)BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(8));
            int indexOff = (int)BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(16));

            _hash = new uint[HASH_ENTRIES];
            for (int i = 0; i < HASH_ENTRIES; i++) {
                _hash[i] = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(20 + i * 4));
            }

            _index = new int[_wordCount];
            for (int i = 0; i < _wordCount; i++) {
                _index[i] = (int)BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(indexOff + i * 4));
            }
        }

        public byte[]? Search(string word) {
            if (_wordCount == 0 || word.Length == 0) {
                return null;
            }

            int tLen = word.Length;
            char first = word[0];

            int lo, hi;
            if (first >= 'A' && first <= 'Z') {
                int letterIdx = first - 'A';
                lo = (int)_hash[letterIdx];
                hi = (int)_hash[letterIdx + 1] - 1;
            } else if (first < 'A') {
                lo = 0;
                hi = (int)_hash[0] - 1;
            } else {
                lo = (int)_hash['Z' - 'A'];
                hi = _wordCount - 1;
            }

            while (lo <= hi) {
                int mid = (lo + hi) >> 1;
                int off = _index[mid];
                int dLen = _data[off];
                int diff = 0;
                int cmp = Math.Min(tLen, dLen);
                for (int i = 0; i < cmp; i++) {
                    diff = word[i] - _data[off + 1 + i];
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
                    int phonOff = off + 1 + dLen;
                    int phonLen = _data[phonOff];
                    byte[] phons = new byte[phonLen];
                    _data.AsSpan(phonOff + 1, phonLen).CopyTo(phons);
                    return phons;
                }
            }
            return null;
        }

        public System.Collections.Generic.IEnumerable<(string word, byte[] phons)> EnumerateAll() {
            for (int i = 0; i < _wordCount; i++) {
                int off = _index[i];
                int dLen = _data[off];
                string word = System.Text.Encoding.ASCII.GetString(_data, off + 1, dLen);
                int phonOff = off + 1 + dLen;
                int phonLen = _data[phonOff];
                byte[] phons = new byte[phonLen];
                _data.AsSpan(phonOff + 1, phonLen).CopyTo(phons);
                yield return (word, phons);
            }
        }

        public int WordCount => _wordCount;
    }
}  // namespace
