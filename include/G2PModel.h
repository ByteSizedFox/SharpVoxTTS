#ifndef SHARPVOX_G2P_MODEL_H
#define SHARPVOX_G2P_MODEL_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include "../include/PhonemeDefs.h"

namespace SharpVox {

// interpreter for the compact G2P WFST format from export_g2p, with output
// mapped from ARPABET onto SharpVox phoneme IDs
class G2PModel {
public:
    G2PModel();

    bool Valid() const { return _valid; }

    struct Result {
        float score;              // negative log probability (tropical semiring)
        std::vector<uint8_t> phons;  // SharpVox phoneme IDs, no stress marks
    };

    // decode one word, up to `nbest` unique pronunciations lowest cost first
    int Decode(const std::string& word, int nbest, std::vector<Result>& out) const;

    // convenience, decode the single best pronunciation
    bool DecodeBest(const std::string& word, std::vector<uint8_t>& outPhons) const;

    // MITalk main stress rule on an unmarked phoneme sequence, per-position
    // level 0/1/2, may rewrite vowels in place
    static std::vector<uint8_t> AssignStress(std::vector<uint8_t>& phons);

private:
    // model structures
    struct Arc {
        uint32_t ilabel, olabel;
        float w;
        uint32_t next;
    };

    struct State {
        uint8_t is_final;
        float final_w;
        uint32_t narcs;
        uint32_t arc_off;  // offset into _arcs arena
    };

    struct Cluster {
        uint32_t ntoks;
        uint32_t toks[16];  // max cluster size is 2 in practice
    };

    struct SymEnt {
        uint64_t hash;
        int32_t id;
        uint32_t len;
    };

    // search structures
    struct Dijkstra;
    struct KBestCtx;

    struct He {
        float cost;
        uint32_t pos, state;
        int32_t parent;  // index into _entries, -1 for the root
        uint8_t ntoks;
        uint32_t toks[16];
    };

    struct Map {
        std::vector<uint64_t> keys, vals;
        std::vector<uint8_t> used;
        size_t count = 0;
        void Init();
        void Clear();
        void Grow();
        uint64_t* Get(uint64_t key);
        void Set(uint64_t key, uint64_t val);
        uint64_t Incr(uint64_t key);
    };

    struct Heap {
        std::vector<He> entries;    // append-only; stable indices for parents
        std::vector<uint32_t> heap; // indices into entries
        void Reset();
        void Push(const He& he);
        bool Pop(He& out, uint32_t& idx);
    };

    // model data
    bool _valid = false;
    uint32_t _n_isyms = 0, _n_osyms = 0, _n_states = 0, _start = 0;
    std::vector<std::string> _isyms, _osyms;
    std::vector<Cluster> _imap, _omap;
    std::vector<State> _states;
    std::vector<Arc> _arcs;          // single arena, contiguous
    std::vector<SymEnt> _symtab;     // input symbol lookup
    uint32_t _symtab_cap = 0;
    std::vector<int8_t> _tokToSvx;   // model osym id -> SharpVox id (-1 if none)

    mutable Heap _hp;
    mutable Map _mp;

    // helpers
    static uint32_t Fnv1a(const char* s, uint32_t len);
    int SymLookup(const char* s, uint32_t len) const;

    static uint32_t Tokenize(const G2PModel& m, const char* word,
                             std::vector<uint32_t>& ids);

    float BestCost(const std::vector<uint32_t>& ids) const;
    int KBest(const std::vector<uint32_t>& ids, int nbest, uint32_t beam,
              float limit, std::vector<Result>& out, int maxOut) const;

    void AppendPhons(const std::vector<uint32_t>& toks,
                     std::vector<uint8_t>& phons) const;

    // MITalk stress helpers
    static bool IsVowelPhon(uint8_t p);
    static bool IsLongVowelPhon(uint8_t p);
    static uint8_t ShortenVowel(uint8_t p);
    static uint8_t ReduceVowel(uint8_t p);
    struct StressSyl { int idx; bool isLong; int coda; };
    static void BuildStressSyls(const std::vector<uint8_t>& phons, std::vector<StressSyl>& out);
    static int MainStressSyl(const std::vector<StressSyl>& syls);
};

}  // namespace SharpVox

#endif  // SHARPVOX_G2P_MODEL_H
