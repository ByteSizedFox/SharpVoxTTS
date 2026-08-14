#include "G2PModel.h"
#include "G2PModelData.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace SharpVox {

namespace {

constexpr float kInf = 1e30f;
constexpr uint32_t kMaxToks = 512;
constexpr uint64_t kMaxPops = 5000000ull;

// SharpVox phoneme IDs (PhonemeDefs.h) for each ARPABET symbol the model
// can emit, symbols not in this table map to -1
int8_t ArpabetToSvx(const std::string& name) {
    static const struct { const char* n; int8_t id; } kMap[] = {
        {"AA", 8}, {"AE", 3}, {"AH", 7}, {"AO", 9}, {"AW", 15}, {"AY", 13},
        {"B", 45}, {"CH", 50}, {"D", 47}, {"DH", 38}, {"EH", 2}, {"ER", 6},
        {"EY", 12}, {"F", 35}, {"G", 49}, {"HH", 43}, {"IH", 1}, {"IY", 0},
        {"JH", 51}, {"K", 48}, {"L", 30}, {"M", 24}, {"N", 25}, {"NG", 26},
        {"OW", 16}, {"OY", 14}, {"P", 44}, {"R", 29}, {"S", 39}, {"SH", 41},
        {"T", 46}, {"TH", 37}, {"UH", 10}, {"UW", 11}, {"V", 36}, {"W", 27},
        {"Y", 28}, {"Z", 40}, {"ZH", 42},
    };
    for (const auto& e : kMap) {
        if (name == e.n) return e.id;
    }
    return -1;
}

float F16ToF32(uint16_t h) {
    uint32_t sign = ((uint32_t)h & 0x8000u) << 16;
    uint32_t e = (h >> 10) & 0x1f;
    uint32_t m = h & 0x3ff;
    uint32_t x;
    if (e == 0) {
        if (m == 0) {
            x = sign;
        } else {
            e = 1;
            while (!(m & 0x400)) { m <<= 1; e--; }
            m &= 0x3ff;
            x = sign | ((e + 127 - 15) << 23) | (m << 13);
        }
    } else if (e == 31) {
        x = sign | 0x7f800000u | (m << 13);
    } else {
        x = sign | ((e + 127 - 15) << 23) | (m << 13);
    }
    float f;
    std::memcpy(&f, &x, 4);
    return f;
}

uint64_t KeyOf(uint32_t pos, uint32_t state) {
    return ((uint64_t)pos << 32) | state;
}

bool ClusterMatch(const uint32_t* word, uint32_t pos,
                  const uint32_t* toks, uint32_t ntoks) {
    for (uint32_t j = 0; j < ntoks; j++)
        if (word[pos + j] != toks[j]) return false;
    return true;
}

}  // namespace

// Map (open addressing)

void G2PModel::Map::Init() {
    keys.assign(1u << 16, 0);
    vals.assign(1u << 16, 0);
    used.assign(1u << 16, 0);
    count = 0;
}

void G2PModel::Map::Clear() {
    std::fill(used.begin(), used.end(), 0);
    count = 0;
}

void G2PModel::Map::Grow() {
    std::vector<uint64_t> ok, ov;
    std::vector<uint8_t> ou;
    ok.swap(keys);
    ov.swap(vals);
    ou.swap(used);
    keys.assign(ok.size() * 2, 0);
    vals.assign(ok.size() * 2, 0);
    used.assign(ok.size() * 2, 0);
    count = 0;
    for (size_t i = 0; i < ok.size(); i++) {
        if (ou[i]) Set(ok[i], ov[i]);
    }
}

uint64_t* G2PModel::Map::Get(uint64_t key) {
    size_t j = (size_t)((key * 0x9E3779B97F4A7C15ull) & (keys.size() - 1));
    while (used[j]) {
        if (keys[j] == key) return &vals[j];
        j = (j + 1) & (keys.size() - 1);
    }
    return nullptr;
}

void G2PModel::Map::Set(uint64_t key, uint64_t val) {
    if (count * 10 >= keys.size() * 6) Grow();
    size_t j = (size_t)((key * 0x9E3779B97F4A7C15ull) & (keys.size() - 1));
    while (used[j]) {
        if (keys[j] == key) { vals[j] = val; return; }
        j = (j + 1) & (keys.size() - 1);
    }
    used[j] = 1;
    keys[j] = key;
    vals[j] = val;
    count++;
}

uint64_t G2PModel::Map::Incr(uint64_t key) {
    uint64_t* v = Get(key);
    if (!v) { Set(key, 1); return 1; }
    return ++(*v);
}

// Heap (binary heap over entries, cost order, ties FIFO by index)

void G2PModel::Heap::Reset() {
    entries.clear();
    heap.clear();
}

void G2PModel::Heap::Push(const He& he) {
    entries.push_back(he);
    uint32_t idx = (uint32_t)(entries.size() - 1);
    heap.push_back(idx);
    size_t i = heap.size() - 1;
    while (i > 0) {
        size_t p = (i - 1) / 2;
        uint32_t a = heap[p];
        if (entries[a].cost < he.cost ||
            (entries[a].cost == he.cost && a < idx))
            break;
        heap[i] = a;
        i = p;
    }
    heap[i] = idx;
}

bool G2PModel::Heap::Pop(He& out, uint32_t& idx) {
    if (heap.empty()) return false;
    uint32_t top = heap[0];
    out = entries[top];
    idx = top;
    uint32_t last = heap.back();
    heap.pop_back();
    if (heap.empty()) return true;
    size_t i = 0;
    for (;;) {
        size_t l = 2 * i + 1, r = 2 * i + 2, m = i;
        if (l < heap.size()) m = l;
        if (r < heap.size()) {
            uint32_t a = heap[r], b = heap[m];
            if (entries[a].cost < entries[b].cost ||
                (entries[a].cost == entries[b].cost && a < b))
                m = r;
        }
        if (m == i) break;
        uint32_t a = heap[m];
        if (!(entries[a].cost < entries[last].cost ||
              (entries[a].cost == entries[last].cost && a < last)))
            break;
        heap[i] = heap[m];
        i = m;
    }
    heap[i] = last;
    return true;
}

// Construction

G2PModel::G2PModel()
    : _hp(), _mp() {
    const uint8_t* data = kG2PModelData;
    const size_t size = kG2PModelSize;
    if (!data || size < 28 || std::memcmp(data, "G2PM", 4) != 0) {
        throw std::invalid_argument("G2PModel: embedded data is not a G2PM model");
    }
    const uint8_t* p = data + 4;
    auto u32 = [&p]() -> uint32_t {
        uint32_t v;
        std::memcpy(&v, p, 4);
        p += 4;
        return v;
    };
    auto f32 = [&p]() -> float {
        float v;
        std::memcpy(&v, p, 4);
        p += 4;
        return v;
    };
    auto u16 = [&p]() -> uint16_t {
        uint16_t v = (uint16_t)(p[0] | (p[1] << 8));
        p += 2;
        return v;
    };
    auto u24 = [&p]() -> uint32_t {
        uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16);
        p += 3;
        return v;
    };

    uint32_t version = u32();
    if (version < 1 || version > 3) {
        throw std::invalid_argument("G2PModel: unsupported model version");
    }
    _n_isyms = u32();
    _n_osyms = u32();
    _n_states = u32();
    _start = u32();
    u32();  // imax
    u32();  // omax

    // v3 weight decode table: u8 -> log-quantized float.
    float qlut[256];
    if (version >= 2) {
        float wmax = f32();
        if (wmax <= 0.0f) wmax = 1.0f;
        if (version == 3) {
            float lw = log1pf(wmax);
            for (int i = 0; i < 256; i++) qlut[i] = expm1f((float)i * lw / 255.0f);
        }
    }

    auto readStr = [&p, &u32]() -> std::string {
        uint32_t len = u32();
        std::string s((const char*)p, len);
        p += len;
        return s;
    };

    _isyms.resize(_n_isyms);
    _osyms.resize(_n_osyms);
    for (uint32_t i = 0; i < _n_isyms; i++) _isyms[i] = readStr();
    for (uint32_t i = 0; i < _n_osyms; i++) _osyms[i] = readStr();

    _imap.resize(_n_isyms);
    _omap.resize(_n_osyms);
    auto readClusters = [&p, &u32](std::vector<Cluster>& map, uint32_t n) {
        for (uint32_t i = 0; i < n; i++) {
            uint32_t nt = u32();
            if (nt > 16) nt = 16;
            map[i].ntoks = nt;
            for (uint32_t j = 0; j < nt; j++) map[i].toks[j] = u32();
        }
    };
    readClusters(_imap, _n_isyms);
    readClusters(_omap, _n_osyms);

    // Parse states and arcs into a contiguous arena.
    _states.resize(_n_states);
    _arcs.reserve(_n_states ? _n_states * 2 : 0);
    for (uint32_t s = 0; s < _n_states; s++) {
        State& st = _states[s];
        if (version == 1) {
            st.is_final = *p != 0;
            p += 1;
            st.final_w = f32();
            uint32_t narcs = u32();
            st.narcs = narcs;
            st.arc_off = (uint32_t)_arcs.size();
            for (uint32_t a = 0; a < narcs; a++) {
                Arc ar;
                ar.ilabel = u32();
                ar.olabel = u32();
                ar.w = f32();
                ar.next = u32();
                _arcs.push_back(ar);
            }
        } else {
            uint8_t flags = *p++;
            st.is_final = (flags & 1) != 0;
            st.final_w = 0.0f;
            if (st.is_final) {
                if (version == 2) st.final_w = F16ToF32(u16());
                else st.final_w = qlut[*p++];
            }
            uint32_t narcs = u16();
            st.narcs = narcs;
            st.arc_off = (uint32_t)_arcs.size();
            for (uint32_t a = 0; a < narcs; a++) {
                Arc ar;
                ar.ilabel = u16();
                ar.olabel = u16();
                if (version == 2) ar.w = F16ToF32(u16());
                else ar.w = qlut[*p++];
                ar.next = u24();
                _arcs.push_back(ar);
            }
        }
    }

    // Input-symbol hash table.
    _symtab_cap = 1;
    while (_symtab_cap < _n_isyms * 2) _symtab_cap *= 2;
    _symtab.assign(_symtab_cap, SymEnt());
    for (uint32_t i = 0; i < _symtab_cap; i++) _symtab[i].id = -1;
    for (uint32_t i = 0; i < _n_isyms; i++) {
        const std::string& s = _isyms[i];
        uint64_t h = Fnv1a(s.c_str(), (uint32_t)s.size());
        size_t j = (size_t)(h & (_symtab_cap - 1));
        while (_symtab[j].id >= 0) j = (j + 1) & (_symtab_cap - 1);
        _symtab[j].hash = h;
        _symtab[j].id = (int32_t)i;
        _symtab[j].len = (uint32_t)s.size();
    }

    // Output symbol -> SharpVox phoneme id table.
    _tokToSvx.assign(_n_osyms, -1);
    for (uint32_t i = 0; i < _n_osyms; i++) {
        _tokToSvx[i] = ArpabetToSvx(_osyms[i]);
    }

    _mp.Init();
    _valid = true;
}

uint32_t G2PModel::Fnv1a(const char* s, uint32_t len) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint32_t i = 0; i < len; i++) {
        h ^= (uint8_t)s[i];
        h *= 0x100000001b3ull;
    }
    return (uint32_t)h;
}

int G2PModel::SymLookup(const char* s, uint32_t len) const {
    uint64_t h = Fnv1a(s, len);
    size_t j = (size_t)(h & (_symtab_cap - 1));
    while (_symtab[j].id >= 0) {
        if (_symtab[j].hash == h && _symtab[j].len == len &&
            std::memcmp(_isyms[_symtab[j].id].c_str(), s, len) == 0)
            return _symtab[j].id;
        j = (j + 1) & (_symtab_cap - 1);
    }
    return -1;
}

// Decoding

uint32_t G2PModel::Tokenize(const G2PModel& m, const char* word,
                            std::vector<uint32_t>& ids) {
    const unsigned char* p = (const unsigned char*)word;
    ids.clear();
    while (*p) {
        const unsigned char* start = p;
        uint32_t len;
        unsigned char c = *p;
        if (c < 0x80) { len = 1; }
        else if ((c >> 5) == 0x6) { len = 2; }
        else if ((c >> 4) == 0xE) { len = 3; }
        else if ((c >> 3) == 0x1E) { len = 4; }
        else { len = 1; }
        p += len;
        int id = m.SymLookup((const char*)start, len);
        if (id >= 0) ids.push_back((uint32_t)id);
    }
    return (uint32_t)ids.size();
}

// search helpers (members so they can reach the private model structures)

struct G2PModel::Dijkstra {
    Heap* hp;
    Map* mp;
    float best;
    const std::vector<uint32_t>* ids;
    const std::vector<Arc>* arcs;
    const std::vector<State>* states;
    const std::vector<Cluster>* imap;
    uint32_t n;

    void Expand(uint32_t pos, uint32_t s, float cost) {
        const State& st = (*states)[s];
        const Arc* a0 = arcs->data() + st.arc_off;
        for (uint32_t a = 0; a < st.narcs; a++) {
            const Arc& ar = a0[a];
            uint32_t np, ns;
            if (ar.ilabel == 0) {
                np = pos;
                ns = ar.next;
            } else {
                if (ar.ilabel == 1 || ar.ilabel >= imap->size()) continue;
                const Cluster& cl = (*imap)[ar.ilabel];
                if (pos + cl.ntoks > n) continue;
                if (!ClusterMatch(ids->data(), pos, cl.toks, cl.ntoks)) continue;
                np = pos + cl.ntoks;
                ns = ar.next;
            }
            float nc = cost + ar.w;
            if (nc >= best) continue;
            uint64_t key = KeyOf(np, ns);
            uint64_t* v = mp->Get(key);
            if (v) {
                float cur;
                std::memcpy(&cur, v, 4);
                if (nc >= cur) continue;
            }
            float tmp = nc;
            mp->Set(key, 0);
            std::memcpy(mp->Get(key), &tmp, 4);
            He he;
            he.cost = nc;
            he.pos = np;
            he.state = ns;
            he.parent = -1;
            he.ntoks = 0;
            hp->Push(he);
        }
    }
};

struct G2PModel::KBestCtx {
    Heap* hp;
    Map* mp;
    const std::vector<uint32_t>* ids;
    const std::vector<Arc>* arcs;
    const std::vector<State>* states;
    const std::vector<Cluster>* imap;
    const std::vector<Cluster>* omap;
    uint32_t n;
    float limit;
    int32_t parent_idx;
    uint64_t max_pops;
    uint64_t pops;
    bool truncated;

    void Push(uint32_t pos, uint32_t s, float cost, const Arc& ar) {
        // Fold final weight into goal costs so complete paths pop in
        // total-cost order.
        if (pos == n && (*states)[s].is_final) cost += (*states)[s].final_w;
        if (cost > limit) return;
        He he;
        he.cost = cost;
        he.pos = pos;
        he.state = s;
        he.parent = parent_idx;
        he.ntoks = 0;
        if (ar.olabel < omap->size()) {
            const Cluster& cl = (*omap)[ar.olabel];
            for (uint32_t j = 0; j < cl.ntoks && he.ntoks < 16; j++) {
                uint32_t t = cl.toks[j];
                if (t >= 3) he.toks[he.ntoks++] = t;  // drop eps, tie, skip
            }
        }
        hp->Push(he);
    }
};

float G2PModel::BestCost(const std::vector<uint32_t>& ids) const {
    _mp.Clear();
    _hp.Reset();

    He root;
    root.cost = 0.0f;
    root.pos = 0;
    root.state = _start;
    root.parent = -1;
    root.ntoks = 0;
    float z = 0.0f;
    _mp.Set(KeyOf(0, _start), 0);
    std::memcpy(_mp.Get(KeyOf(0, _start)), &z, 4);
    _hp.Push(root);

    Dijkstra d;
    d.hp = &_hp;
    d.mp = &_mp;
    d.best = kInf;
    d.ids = &ids;
    d.arcs = &_arcs;
    d.states = &_states;
    d.imap = &_imap;
    d.n = (uint32_t)ids.size();

    float best = kInf;
    He cur;
    uint32_t idx;
    while (_hp.Pop(cur, idx)) {
        (void)idx;
        uint64_t* v = _mp.Get(KeyOf(cur.pos, cur.state));
        float dist;
        if (!v) continue;
        std::memcpy(&dist, v, 4);
        if (cur.cost > dist) continue;
        if (cur.pos == d.n && _states[cur.state].is_final) {
            float g = cur.cost + _states[cur.state].final_w;
            if (g < best) best = g;
            continue;
        }
        if (cur.cost >= best) continue;
        d.best = best;
        d.Expand(cur.pos, cur.state, cur.cost);
    }
    return best;
}

int G2PModel::KBest(const std::vector<uint32_t>& ids, int nbest,
                    uint32_t beam, float limit,
                    std::vector<Result>& out, int maxOut) const {
    KBestCtx kb;
    kb.hp = &_hp;
    kb.mp = &_mp;
    kb.ids = &ids;
    kb.arcs = &_arcs;
    kb.states = &_states;
    kb.imap = &_imap;
    kb.omap = &_omap;
    kb.n = (uint32_t)ids.size();
    kb.limit = limit;
    kb.parent_idx = -1;
    kb.max_pops = kMaxPops;
    kb.pops = 0;
    kb.truncated = false;

    _mp.Clear();
    _hp.Reset();

    He root;
    root.cost = 0.0f;
    root.pos = 0;
    root.state = _start;
    root.parent = -1;
    root.ntoks = 0;
    _hp.Push(root);

    // Raw model token sequences seen so far, for unique-output dedup.
    std::vector<std::vector<uint32_t>> seen;

    int nres = 0;
    He cur;
    uint32_t idx;
    while (_hp.Pop(cur, idx)) {
        if (++kb.pops > kb.max_pops) {
            kb.truncated = true;
            break;
        }
        if (kb.mp->Incr(KeyOf(cur.pos, cur.state)) > beam) continue;

        if (cur.pos == kb.n && _states[cur.state].is_final) {
            // parent chain runs goal->root, emit entries in reverse order
            // while keeping each entry's tokens in their original order
            uint32_t chain[kMaxToks];
            uint32_t nents = 0;
            for (int32_t i = (int32_t)idx; i >= 0 && nents < kMaxToks;
                 i = _hp.entries[i].parent)
                chain[nents++] = (uint32_t)i;
            std::vector<uint32_t> seq;
            seq.reserve(32);
            for (int32_t e = (int32_t)nents - 1; e >= 0; e--) {
                const He& he = _hp.entries[chain[e]];
                for (uint32_t j = 0; j < he.ntoks; j++) seq.push_back(he.toks[j]);
            }
            bool dup = false;
            for (const auto& s : seen) {
                if (s == seq) { dup = true; break; }
            }
            if (!dup) {
                seen.push_back(seq);
                Result r;
                r.score = cur.cost;
                AppendPhons(seq, r.phons);
                out.push_back(std::move(r));
                nres++;
                if (nres >= nbest || nres >= maxOut) break;
            }
            continue;
        }

        kb.parent_idx = (int32_t)idx;
        const State& st = _states[cur.state];
        const Arc* a0 = _arcs.data() + st.arc_off;
        for (uint32_t a = 0; a < st.narcs; a++) {
            const Arc& ar = a0[a];
            uint32_t np, ns;
            if (ar.ilabel == 0) {
                np = cur.pos;
                ns = ar.next;
            } else {
                if (ar.ilabel == 1 || ar.ilabel >= _imap.size()) continue;
                const Cluster& cl = _imap[ar.ilabel];
                if (cur.pos + cl.ntoks > kb.n) continue;
                if (!ClusterMatch(ids.data(), cur.pos, cl.toks, cl.ntoks)) continue;
                np = cur.pos + cl.ntoks;
                ns = ar.next;
            }
            kb.Push(np, ns, cur.cost + ar.w, ar);
        }
    }
    return nres;
}

void G2PModel::AppendPhons(const std::vector<uint32_t>& toks,
                           std::vector<uint8_t>& phons) const {
    phons.reserve(phons.size() + toks.size());
    for (uint32_t t : toks) {
        if (t < _tokToSvx.size() && _tokToSvx[t] >= 0)
            phons.push_back((uint8_t)_tokToSvx[t]);
    }
}

int G2PModel::Decode(const std::string& word, int nbest,
                     std::vector<Result>& out) const {
    if (!_valid || word.empty() || nbest <= 0) return 0;
    out.clear();
    std::vector<uint32_t> ids;
    uint32_t n = Tokenize(*this, word.c_str(), ids);
    if (n == 0) return 0;

    float d0 = BestCost(ids);
    if (d0 >= kInf / 2) return 0;
    return KBest(ids, nbest, 10000, d0 + 99.0f, out, nbest);
}

bool G2PModel::DecodeBest(const std::string& word,
                          std::vector<uint8_t>& outPhons) const {
    std::vector<Result> res;
    if (Decode(word, 1, res) == 0) return false;
    outPhons = std::move(res[0].phons);
    return true;
}

// MITalk lexical stress (moved here with the G2P model, see header)

bool G2PModel::IsVowelPhon(uint8_t p) {
    // _IY_.._UR_ (0..22) are vocalic nuclei; _SIL_ = 23 and up are not.
    return p <= 22;
}

bool G2PModel::IsLongVowelPhon(uint8_t p) {
    // Tense/long vowels per MITalk 6.3: ey iy ay ow uw oy aw yu plus the
    // r-colored vowels; everything else (ih eh ae ix ax ah aa ao uh) is short.
    switch (p) {
        case _IY_: case _EY_: case _AY_: case _OY_: case _AW_:
        case _OW_: case _UW_: case _YU_:
        case _ER_: case _IR_: case _XR_: case _AR_: case _OR_: case _UR_:
            return true;
        default:
            return false;
    }
}

uint8_t G2PModel::ShortenVowel(uint8_t p) {
    // 6.3.5 tenseness reduction table. oy and aw are left long per MITalk.
    switch (p) {
        case _EY_: return _AE_;
        case _IY_: return _EH_;
        case _AY_: return _IH_;
        case _OW_: return _AA_;
        case _UW_: return _UH_;
        default:   return p;
    }
}

uint8_t G2PModel::ReduceVowel(uint8_t p) {
    // Only short vowels reduce; long/tense/r-colored and already-reduced ix/ax pass.
    if (IsLongVowelPhon(p)) {
        return p;
    }
    switch (p) {
        case _EH_: case _IH_: return _IX_;
        case _IX_: case _AX_: return p;
        default:              return _AX_;
    }
}

void G2PModel::BuildStressSyls(const std::vector<uint8_t>& phons, std::vector<StressSyl>& out) {
    // A syllable is a vowel nucleus plus the consonants that follow it up to
    // (not including) the next vowel. coda = that consonant count.
    for (int i = 0; i < (int)phons.size(); i++) {
        if (!IsVowelPhon(phons[i])) {
            continue;
        }
        StressSyl s;
        s.idx = i;
        s.isLong = IsLongVowelPhon(phons[i]);
        s.coda = 0;
        for (int j = i + 1; j < (int)phons.size() && !IsVowelPhon(phons[j]); j++) {
            s.coda++;
        }
        out.push_back(s);
    }
}

int G2PModel::MainStressSyl(const std::vector<StressSyl>& syls) {
    int n = (int)syls.size();
    if (n == 0) {
        return -1;
    }
    if (n == 1) {
        return 0;  // Rule 3 on a monosyllable: its only vowel.
    }

    const StressSyl& ult = syls[n - 1];
    const StressSyl& pen = syls[n - 2];
    // The ultima is light when short (any coda) or an open final syllable.
    bool ultLight = !ult.isLong || ult.coda == 0;
    // The penult qualifies for antepenult stress when weak (short, coda <= 1)
    // or open.
    bool penWeakOrOpen = pen.coda == 0 || (!pen.isLong && pen.coda <= 1);

    // Rule 1: antepenult stress (difficult, oregano, secretariat, oratorio).
    if (n >= 3 && penWeakOrOpen && ultLight) {
        return n - 3;
    }
    // Rule 2: penult stress (edit, agenda, bitumen).
    if (ultLight) {
        return n - 2;
    }
    // Rule 3: last-syllable stress (parole, cascade, stand).
    return n - 1;
}

std::vector<uint8_t> G2PModel::AssignStress(std::vector<uint8_t>& phons) {
    std::vector<uint8_t> marks(phons.size(), 0);
    std::vector<StressSyl> syls;
    BuildStressSyls(phons, syls);
    int n = (int)syls.size();
    int primary = MainStressSyl(syls);
    if (primary < 0) {
        return marks;
    }
    // 6.3.2 -ic (ih k) attracts primary to the penult: symBOLic, teleGRAPHic.
    if (n >= 2) {
        const StressSyl& last = syls[n - 1];
        if (phons[last.idx] == _IH_ && last.coda == 1 && phons[last.idx + 1] == _K_) {
            primary = n - 2;
        }
    }
    marks[syls[primary].idx] = 1;

    // 6.3.5 Destressing Rule 1: shorten an unmarked long vowel one consonant
    // before the stress (instrumental: uw -> uh).
    for (int k = 1; k + 1 < n; k++) {
        if (marks[syls[k].idx] != 0) {
            continue;
        }
        if (syls[k].coda == 1 && marks[syls[k + 1].idx] != 0) {
            phons[syls[k].idx] = ShortenVowel(phons[syls[k].idx]);
        }
    }

    // 6.3.7 Strong First Syllable: secondary on a non-primary first syllable
    // when heavy (long vowel, coda >= 2) or the primary sits on syllable 3+.
    if (n >= 2 && primary != 0) {
        const StressSyl& first = syls[0];
        if (first.isLong || first.coda >= 2 || primary >= 2) {
            marks[first.idx] = 2;
        }
    }

    // 6.3.9 Vowel Reduction: unstressed short vowels go to schwa (eh/ih -> ix,
    // else -> ax), last so it sees the final marks.
    for (int i = 0; i < (int)phons.size(); i++) {
        if (marks[i] == 0 && IsVowelPhon(phons[i])) {
            phons[i] = ReduceVowel(phons[i]);
        }
    }
    return marks;
}

}  // namespace SharpVox
