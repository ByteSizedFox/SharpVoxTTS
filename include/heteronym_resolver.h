#ifndef SHARPTALK_HETERONYM_RESOLVER_H
#define SHARPTALK_HETERONYM_RESOLVER_H

#include <cstdint>
#include <string>
#include <vector>

namespace SharpTalk {

    // Contextual heteronym disambiguation.
    // For each ambiguous word we store a default pronunciation and a list of
    // context rules, the first matching rule wins.  CompiledLetterToSoundRules fire when a word in
    // the before-set appears immediately before OR a word in the after-set
    // appears immediately after the target.
    class HeteronymResolver {
    public:
        // Returns the full phoneme stream (OP_WORD + phonemes) if a rule matches,
        // or null to fall through to normal dictionary/LTS lookup.
        // Returns empty vector to indicate no match (caller checks .empty()).
        static std::vector<uint8_t> Resolve(const std::vector<std::string>& words, int32_t index);
    };

}  // namespace SharpTalk

#endif  // SHARPTALK_HETERONYM_RESOLVER_H
