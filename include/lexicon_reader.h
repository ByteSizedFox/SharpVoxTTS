#ifndef SHARPTALK_LEXICON_READER_H
#define SHARPTALK_LEXICON_READER_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace SharpTalk {

class DictReader {
public:
    explicit DictReader(const uint8_t* data, int32_t dataLen);

    // Returns phoneme bytes for the given word, or empty vector if not found.
    std::vector<uint8_t> Search(const std::string& word) const;

    // Calls callback(word, phons) for every entry in the dictionary.
    void EnumerateAll(std::function<void(const std::string&, const std::vector<uint8_t>&)> callback) const;

    int32_t WordCount() const { return _wordCount; }

private:
    const uint8_t* _data;
    int32_t        _dataLen;
    std::vector<int32_t>  _index;  // absolute byte offsets into _data, one per entry
    uint32_t              _hash[27]; // letter_starts[27]: word index of first entry for A-Z + sentinel

    static constexpr int HASH_ENTRIES = 27;

    int32_t _wordCount;
};

}  // namespace SharpTalk

#endif  // SHARPTALK_LEXICON_READER_H
