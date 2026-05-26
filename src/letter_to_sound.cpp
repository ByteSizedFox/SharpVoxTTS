#include "letter_to_sound.h"

#include <cctype>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

// Phoneme IDs (mirrors AudioProcessor constants)
static constexpr int16_t _IY_ = 0;  static constexpr int16_t _IH_ = 1;
static constexpr int16_t _EH_ = 2;  static constexpr int16_t _AE_ = 3;
static constexpr int16_t _IX_ = 4;  static constexpr int16_t _AX_ = 5;
static constexpr int16_t _ER_ = 6;  static constexpr int16_t _AH_ = 7;
static constexpr int16_t _AA_ = 8;  static constexpr int16_t _AO_ = 9;
static constexpr int16_t _UH_ = 10; static constexpr int16_t _UW_ = 11;
static constexpr int16_t _EY_ = 12; static constexpr int16_t _AY_ = 13;
static constexpr int16_t _OY_ = 14; static constexpr int16_t _AW_ = 15;
static constexpr int16_t _OW_ = 16;
static constexpr int16_t _M_  = 24; static constexpr int16_t _N_  = 25;
static constexpr int16_t _NG_ = 26; static constexpr int16_t _W_  = 27;
static constexpr int16_t _Y_  = 28; static constexpr int16_t _R_  = 29;
static constexpr int16_t _L_  = 30;
static constexpr int16_t _HH_ = 43;
static constexpr int16_t _F_  = 35; static constexpr int16_t _V_  = 36;
static constexpr int16_t _TH_ = 37; static constexpr int16_t _DH_ = 38;
static constexpr int16_t _S_  = 39; static constexpr int16_t _Z_  = 40;
static constexpr int16_t _SH_ = 41; static constexpr int16_t _ZH_ = 42;
static constexpr int16_t _P_  = 44; static constexpr int16_t _B_  = 45;
static constexpr int16_t _T_  = 46; static constexpr int16_t _D_  = 47;
static constexpr int16_t _K_  = 48; static constexpr int16_t _G_  = 49;
static constexpr int16_t _CH_ = 50; static constexpr int16_t _JH_ = 51;

namespace SharpTalk {

// Static member definitions
uint8_t LetterToSound::CharacterFeatureTable[128] = {};
std::vector<LetterToSound::CompiledRule> LetterToSound::CompiledLetterToSoundRules[26];
bool LetterToSound::s_initialized = false;

// Format: "LEFT[MATCH]RIGHT=OUTPUT"
// Special symbols in LEFT / RIGHT (not between brackets):
//   #  1+ vowels          *  1+ consonants   .  voiced consonant
//   $  1 consonant + E/I  %  suffix           &  sibilant
//   @  long-U consonant   ^  exactly 1 cons   +  front vowel (E I Y)
//   :  0+ consonants      ' '  word boundary
// OUTPUT: space-separated NRL phoneme names, or empty for silence.

static const char* const LTS_A[] = {
    "[A] =AX",
    " [ARE]=AA R",
    " [AR]O=AX R",
    "#[AR]#=EH R",
    " ^[AS]#=EY S",
    "[A]WA=AX",
    "[AW]=AO",
    " :[ANY]=EH N IY",
    "[A]^+#=EY",
    "#:[ALLY]=AX L IY",
    " [AL]#=AX L",
    "[AGAIN]=AX G EH N",
    "#:[AG]E=IH JH",
    "#[A]^+#=AE",
    " *[A]^+ =EY",
    "[A]^%=EY",
    " *[ARR]=AX R",
    "[ARR]=AE R",
    " *[AR] =AA R",
    "[AR] =ER",
    "[AR]=AA R",
    "[AIR]=EH R",
    "[AI]=EY",
    "[AY]=EY",
    "[AU]=AO",
    "#*:[AL] =AX L",
    "#*:[ALS] =AX L Z",
    "[ALK]=AO K",
    "[AL]=AO L",
    " *[ABLE]=EY B AX L",
    "[ABLE]=AX B AX L",
    "[ANG]+=EY N JH",
    "[A]=AE",
    nullptr,
};
static const char* const LTS_B[] = {
    " [BE]^#=B IH",
    "[BEING]=B IY IH NG",
    " [BOTH] =B OW TH",
    " [BUS]#=B IH Z",
    "[BUIL]=B IH L",
    "[B]=B",
    nullptr,
};
static const char* const LTS_C[] = {
    " [CH]=K",
    "^^E[CH]=K",
    "[CH]=CH",
    " S[CI]#=S AY",
    "[CI]A=SH",
    "[CI]O=SH",
    "[CI]EN=SH",
    "[C]+=S",
    "[CharacterFeatureTable]=K",
    ".[COM]%=K AH M",
    "[C]=K",
    nullptr,
};
static const char* const LTS_D[] = {
    "#*:[DED] =D IH D",
    ".E[D]=D",
    "#*^E[D]=T",
    " [DE]^#=D IH",
    " [DO] =D UW",
    " [DOES]=D AH Z",
    " [DOING]=D UW IH NG",
    " [DOW]=D AW",
    "[DU]A=JH UW",
    "[D]=D",
    nullptr,
};
static const char* const LTS_E[] = {
    "#*[E] =",
    "#*^[E] =",
    " :[E] =IY",
    "#[ED] =D",
    "#*[E]D =",
    "[EV]ER=EH V",
    "[E]^%=IY",
    "[ERI]#=IY R IY",
    "[ERI]=EH R IH",
    "#:[ER]#=ER",
    "[ER]#=EH R",
    "[ER]=ER",
    " [EVEN]=IY V EH N",
    "#:[EW]=",
    "@[EW]=UW",
    "[EW]=Y UW",
    "[EO]=IY",
    "#*&[ES] =IH Z",
    "#*[ES] =",
    "#*[ELY] =L IY",
    "#*[EMENT] =M EH N T",
    "[EFUL]=F UH L",
    "[EE]=IY",
    "[EARN]=ER N",
    " [EAR]^=ER",
    "[EAD]=EH D",
    "#*[EA] =IY AX",
    "[EA]SU=EH",
    "[EA]=IY",
    "[EIGH]=EY",
    "[EI]=IY",
    " [EYE]=AY",
    "[EY]=IY",
    "[EU]=Y UW",
    "[E]=EH",
    nullptr,
};
static const char* const LTS_F[] = {
    "[FUL]=F UH L",
    "[F]=F",
    nullptr,
};
static const char* const LTS_G[] = {
    " [GN]=N",
    "[GIV]=G IH V",
    " [G]I=G",
    "[GE]T=G EH",
    "SU[GGES]=G JH EH S",
    "[GG]=G",
    " B#[G]=G",
    "[G]+=JH",
    "[GREAT]=G R EY T",
    "#[GH]=",
    "[G]=G",
    nullptr,
};
static const char* const LTS_H[] = {
    " [HAV]=HH AE V",
    " [HERE]=HH IY R",
    " [HOUR]=AW ER",
    "[HOW]=HH AW",
    "[H]#=HH",
    "[H]=",
    nullptr,
};
static const char* const LTS_I[] = {
    " [IN]=IH N",
    " [I] =AY",
    "[IN]D=AY N",
    "[IER]=IY ER",
    "#*R[IED] =IY D",
    "[IED] =AY D",
    "[IEN]=IY EH N",
    "[IE]T=AY EH",
    " :[I]%=AY",
    "[I]%=IY",
    "[I]E=IY",
    "[I]^+#=IH",
    "[I]#=AY R",
    "[IZ]%=AY Z",
    "[IS]%=AY Z",
    "[ID]%=AY D",
    "+^[I]+=IH",
    "[I]T%=AY",
    "#*:[I]^+=IH",
    "[I]^+=AY",
    "[IR]=ER",
    "[IGH]=AY",
    "[ILD]=AY L D",
    "[IGN] =AY N",
    "[IGN]^=AY N",
    "[IGN]%=AY N",
    "[IQUE]=IY K",
    "[I]=IH",
    nullptr,
};
static const char* const LTS_J[] = {
    "[J]=JH",
    nullptr,
};
static const char* const LTS_K[] = {
    " [K]N=",
    "[K]=K",
    nullptr,
};
static const char* const LTS_L[] = {
    "[LO]C#=L OW",
    "[L]L=",
    "#^:[L]%=AX L",
    "[LEAD]=L IY D",
    "[L]=L",
    nullptr,
};
static const char* const LTS_M[] = {
    "[MOV]=M UW V",
    "[M]=M",
    nullptr,
};
static const char* const LTS_N[] = {
    "E[NG]+=N JH",
    "[NG]R=NG G",
    "[NG]#=NG G",
    "[NGL]%=NG G AX L",
    "[NG]=NG",
    "[NK]=NG K",
    " [NOW] =N AW",
    "[N]=N",
    nullptr,
};
static const char* const LTS_O[] = {
    "[OF] =AX V",
    "[OROUGH]=ER OW",
    "#:[OR] =ER",
    "#:[ORS] =ER Z",
    "[OR]=AO R",
    " [ONE]=W AH N",
    "[OW]=OW",
    " [OVER]=OW V ER",
    "[OV]=AH V",
    "[O]^%=OW",
    "[O]^EN=OW",
    "[O]^I#=OW",
    "[OLD]=OW L D",
    "[OUGHT]=AO T",
    "[OUGH]=AH F",
    " [OU]=AW",
    "H[OU]S#=AW",
    "[OUS]=AX S",
    "[OUR]=AO R",
    "[OULD]=UH D",
    "^^[OU]L=AH",
    "[OUP]=UW P",
    "[OU]=AW",
    "[OY]=OY",
    "[OING]=OW IH NG",
    "[OI]=OY",
    "[OOR]=AO R",
    "[OOK]=UH K",
    "[OOD]=UH D",
    "[OO]=UW",
    "[O]E=OW",
    "[O] =OW",
    "[OA]=OW",
    " [ONLY]=OW N L IY",
    " [ONCE]=W AH N S",
    "*[ON] T=OW N",
    "C[ION]=AX N",
    "[O]NG=AO",
    " ^:[ON]=AH N",
    "#:[ON]=AX N",
    "#*[ON] =AX N",
    "#^[ON]=AX N",
    "[O]ST =OW",
    "[OF]^=AO F",
    "[OTHER]=AH DH ER",
    "[OSS] =AO S",
    "#*:[OM]=AH M",
    "[O]=AA",
    nullptr,
};
static const char* const LTS_P[] = {
    "[PH]=F",
    "[PEOP]=P IY P",
    "[POW]=P AW",
    "[PUT] =P UH T",
    "[P]=P",
    nullptr,
};
static const char* const LTS_Q[] = {
    "[QUAR]=K W AO R",
    "[QU]=K W",
    "[Q]=K",
    nullptr,
};
static const char* const LTS_R[] = {
    " [RE]^#=R IY",
    "[R]=R",
    nullptr,
};
static const char* const LTS_S[] = {
    "[SH]=SH",
    "#[SION]=ZH AX N",
    "[SOME]=S AH M",
    "#[SUR]#=ZH ER",
    "[SUR]#=SH ER",
    "#[SU]#=ZH UW",
    "#[SSU]#=SH UW",
    "#[SED] =Z D",
    "#[S]#=Z",
    "[SAID]=S EH D",
    "^^[SION]=SH AX N",
    "[S]S=",
    ".[S] =Z",
    "#*.E[S] =Z",
    "#*^##[S] =Z",
    "#*^#[S] =S",
    "U[S] =S",
    " :#[S] =Z",
    " [SCH]=S K",
    "[S]C+=",
    "#[SM]=Z M",
    "#[SN] =Z AX N",
    "[S]=S",
    nullptr,
};
static const char* const LTS_T[] = {
    " [THE] =DH AX",
    "[TO] =T UW",
    "[THAT] =DH AE T",
    " [THIS] =DH IH S",
    " [THEY]=DH EY",
    " [THERE]=DH EH R",
    "[THER]=DH ER",
    "[THEIR]=DH EH R",
    " [THAN] =DH AE N",
    " [THEM] =DH EH M",
    "[THESE] =DH IY Z",
    " [THEN]=DH EH N",
    "[THROUGH]=TH R UW",
    "[THOSE]=DH OW Z",
    "[THOUGH] =DH OW",
    " [THUS]=DH AH S",
    "[TH]=TH",
    "#:[TED] =T IH D",
    "S[TI]#N=CH",
    "[TION]=SH AX N",
    "[TIO]=SH",
    "[TIA]=SH",
    "[TIEN]=SH AX N",
    "[TUR]#=CH ER",
    "[TU]A=CH UW",
    " [TWO]=T UW",
    "[T]=T",
    nullptr,
};
static const char* const LTS_U[] = {
    " [UN]I=Y UW N",
    " [UN]=AH N",
    " [UPON]=AX P AO N",
    "@[UR]#=UH R",
    "[UR]#=Y UH R",
    "[UR]=ER",
    "[U]^ =AH",
    "[U]^^=AH",
    "[UY]=AY",
    " G[U]#=",
    "G[U]%=",
    "G[U]#=W",
    "#N[U]=Y UW",
    "@[U]=UW",
    "[U]=Y UW",
    nullptr,
};
static const char* const LTS_V[] = {
    "[VIEW]=V Y UW",
    "[V]=V",
    nullptr,
};
static const char* const LTS_W[] = {
    " [WERE]=W ER",
    "[WA]S=W AA",
    "[WA]T=W AA",
    "[WHERE]=WH EH R",
    "[WHOL]=HH OW L",
    "[WHO]=HH UW",
    "[WH]=WH",
    "[WAR]=W AO R",
    "[WOR]^=W ER",
    "[WR]=R",
    "[W]=W",
    nullptr,
};
static const char* const LTS_X[] = {
    "[X]=K S",
    nullptr,
};
static const char* const LTS_Y[] = {
    "[YOUNG]=Y AH NG",
    " [YOU]=Y UW",
    " [YES]=Y EH S",
    " [Y] =AY",
    "#^:[Y] =IY",
    "#^:[Y]I=IY",
    " :[Y] =AY",
    " :[Y]#=Y",      // initial Y before vowel = glide (year, yellow, yet)
    " :[Y]^+#=IH",
    " :[Y]^#=AY",
    "[Y]=IH",
    nullptr,
};
static const char* const LTS_Z[] = {
    "[Z]=Z",
    nullptr,
};

static const char* const* LetterToSoundRulesSource[26] = {
    LTS_A, LTS_B, LTS_C, LTS_D, LTS_E, LTS_F, LTS_G, LTS_H, LTS_I, LTS_J,
    LTS_K, LTS_L, LTS_M, LTS_N, LTS_O, LTS_P, LTS_Q, LTS_R, LTS_S, LTS_T,
    LTS_U, LTS_V, LTS_W, LTS_X, LTS_Y, LTS_Z,
};

static const std::unordered_map<std::string, uint8_t>& PhonemeMapTable() {
    static const std::unordered_map<std::string, uint8_t> table = {
        {"IY",(uint8_t)_IY_},{"IH",(uint8_t)_IH_},{"EH",(uint8_t)_EH_},{"AE",(uint8_t)_AE_},
        {"AA",(uint8_t)_AA_},{"AH",(uint8_t)_AH_},
        {"AO",(uint8_t)_AO_},{"UH",(uint8_t)_UH_},{"AX",(uint8_t)_AX_},{"ER",(uint8_t)_ER_},
        {"EY",(uint8_t)_EY_},{"AY",(uint8_t)_AY_},
        {"OY",(uint8_t)_OY_},{"AW",(uint8_t)_AW_},{"OW",(uint8_t)_OW_},{"UW",(uint8_t)_UW_},
        {"W",(uint8_t)_W_},{"Y",(uint8_t)_Y_},{"R",(uint8_t)_R_},{"L",(uint8_t)_L_},
        {"HH",(uint8_t)_HH_},{"M",(uint8_t)_M_},{"N",(uint8_t)_N_},
        {"NG",(uint8_t)_NG_},{"F",(uint8_t)_F_},{"V",(uint8_t)_V_},
        {"TH",(uint8_t)_TH_},{"DH",(uint8_t)_DH_},{"S",(uint8_t)_S_},{"Z",(uint8_t)_Z_},
        {"SH",(uint8_t)_SH_},{"ZH",(uint8_t)_ZH_},{"P",(uint8_t)_P_},{"B",(uint8_t)_B_},
        {"T",(uint8_t)_T_},{"D",(uint8_t)_D_},
        {"K",(uint8_t)_K_},{"G",(uint8_t)_G_},{"CH",(uint8_t)_CH_},{"JH",(uint8_t)_JH_},
        {"WH",(uint8_t)_W_},
    };
    return table;
}

std::vector<uint8_t> LetterToSound::ParseOutput(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    // Trim leading/trailing spaces and split on ' '
    std::vector<uint8_t> buf;
    const auto& pmap = PhonemeMapTable();
    size_t start = 0;
    while (start < s.size() && s[start] == ' ') {
        start++;
    }
    while (start < s.size()) {
        size_t end = s.find(' ', start);
        if (end == std::string::npos) {
            end = s.size();
        }
        if (end > start) {
            std::string tok = s.substr(start, end - start);
            auto it = pmap.find(tok);
            buf.push_back(it != pmap.end() ? it->second : (uint8_t)_AX_);
        }
        start = end + 1;
    }
    return buf;
}

LetterToSound::CompiledRule LetterToSound::ParseRule(const char* s) {
    // Splits "LEFT[MATCH]RIGHT=OUTPUT" into its four fields.
    const char* lbr = strchr(s, '[');
    const char* rbr = strchr(s, ']');
    const char* eq  = strchr(rbr + 1, '=');

    std::string left  = (lbr > s) ? std::string(s, lbr - s) : "";
    std::string match = std::string(lbr + 1, rbr - lbr - 1);
    std::string right = std::string(rbr + 1, eq - rbr - 1);
    std::string out   = std::string(eq + 1);

    return CompiledRule(std::move(left), std::move(match), std::move(right), ParseOutput(out));
}

void LetterToSound::Initialize() {
    if (s_initialized) {
        return;
    }
    s_initialized = true;

    // Build character feature table
    for (const char* p = "AEIOUY"; *p; p++) {
        CharacterFeatureTable[(unsigned char)*p] |= CV;
    }
    for (const char* p = "EIY"; *p; p++) {
        CharacterFeatureTable[(unsigned char)*p] |= CF;
    }
    for (const char* p = "BCDFGHJKLMNPQRSTVWXZ"; *p; p++) {
        CharacterFeatureTable[(unsigned char)*p] |= CC;
    }
    for (const char* p = "BDGJLMNRVWZ"; *p; p++) {
        CharacterFeatureTable[(unsigned char)*p] |= CZ;
    }
    for (const char* p = "SCGZXJ"; *p; p++) {
        CharacterFeatureTable[(unsigned char)*p] |= CS;
    }
    for (const char* p = "TSRDLZNJ"; *p; p++) {
        CharacterFeatureTable[(unsigned char)*p] |= CU;
    }

    // Called once at static construction time; converts each rule string to a CompiledRule.
    for (int li = 0; li < 26; li++) {
        const char* const* group = LetterToSoundRulesSource[li];
        for (int ri = 0; group[ri] != nullptr; ri++) {
            CompiledLetterToSoundRules[li].push_back(ParseRule(group[ri]));
        }
    }
}

bool LetterToSound::MatchMid(const std::vector<char>& inp, int pos, const std::string& match, int& end) {
    end = pos;
    for (char m : match) {
        if (end >= (int)inp.size() || inp[end] != m) {
            return false;
        }
        end++;
    }
    return true;
}

bool LetterToSound::IsVowel(const std::vector<char>& inp, int p) {
    return p >= 0 && p < (int)inp.size() && (CharacterFeatureTable[(unsigned char)inp[p]] & CV) != 0;
}

bool LetterToSound::IsConsonant(const std::vector<char>& inp, int p) {
    if (p < 0 || p >= (int)inp.size()) {
        return false;
    }
    char c = inp[p];
    if ((CharacterFeatureTable[(unsigned char)c] & CC) != 0) {
        return true;
    }
    if ((c == 'Q' || c == 'G') && p + 1 < (int)inp.size() && inp[p + 1] == 'U') {
        return true;
    }
    return false;
}

bool LetterToSound::IsVoiced(const std::vector<char>& inp, int p) {
    return p >= 0 && p < (int)inp.size() && (CharacterFeatureTable[(unsigned char)inp[p]] & CZ) != 0;
}

bool LetterToSound::IsUMod(const std::vector<char>& inp, int p) {
    return p >= 0 && p < (int)inp.size() && (CharacterFeatureTable[(unsigned char)inp[p]] & CU) != 0;
}

bool LetterToSound::MatchSibilant(const std::vector<char>& inp, int& pos, int dir) {
    if (pos < 0 || pos >= (int)inp.size()) {
        return false;
    }
    char c = inp[pos];
    if ((CharacterFeatureTable[(unsigned char)c] & CS) != 0) {
        pos += dir;
        return true;
    }
    if (dir == 1 && (c == 'C' || c == 'S') && pos + 1 < (int)inp.size() && inp[pos + 1] == 'H') {
        pos += 2;
        return true;
    }
    if (dir == -1 && c == 'H' && pos > 0 && (inp[pos - 1] == 'C' || inp[pos - 1] == 'S')) {
        pos -= 2;
        return true;
    }
    return false;
}

bool LetterToSound::MatchSuffix(const std::vector<char>& inp, int pos, int& end) {
    end = pos;
    if (pos < 0 || pos >= (int)inp.size()) {
        return false;
    }
    char c = inp[pos];
    if (c == 'E') {
        if (pos + 2 < (int)inp.size() && inp[pos + 1] == 'R') {
            end = pos + 2;
            return true;
        }
        if (pos + 2 < (int)inp.size() && inp[pos + 1] == 'D') {
            end = pos + 2;
            return true;
        }
        if (pos + 2 < (int)inp.size() && inp[pos + 1] == 'S') {
            end = pos + 2;
            return true;
        }
        if (pos + 3 < (int)inp.size() && inp[pos + 1] == 'L' && inp[pos + 2] == 'Y') {
            end = pos + 3;
            return true;
        }
        if (pos + 1 < (int)inp.size() && inp[pos + 1] == ' ') {
            end = pos + 1;
            return true;
        }
        return false;
    }
    if (c == 'I' && pos + 3 < (int)inp.size() && inp[pos + 1] == 'N' && inp[pos + 2] == 'G') {
        end = pos + 3;
        return true;
    }
    return false;
}

// Recursive context matcher with backtracking for #, *, :
// dir=+1: left-to-right (right context), ci advances forward
// dir=-1: right-to-left (left context), ci retreats toward -1
bool LetterToSound::MatchCtx(const std::vector<char>& inp, int pos, const std::string& ctx, int ci, int dir) {
    int cEnd = (dir == 1) ? (int)ctx.size() : -1;
    if (ci == cEnd) {
        return true;
    }

    char sym = ctx[ci];
    int nci = ci + dir;

    switch (sym) {
        case '#': // one or more vowels
            if (!IsVowel(inp, pos)) {
                return false;
            }
            pos += dir;
            while (true) {
                if (MatchCtx(inp, pos, ctx, nci, dir)) {
                    return true;
                }
                if (!IsVowel(inp, pos)) {
                    return false;
                }
                pos += dir;
            }

        case '*': // one or more consonants
            if (!IsConsonant(inp, pos)) {
                return false;
            }
            pos += dir;
            while (true) {
                if (MatchCtx(inp, pos, ctx, nci, dir)) {
                    return true;
                }
                if (!IsConsonant(inp, pos)) {
                    return false;
                }
                pos += dir;
            }

        case ':': // zero or more consonants
            if (MatchCtx(inp, pos, ctx, nci, dir)) {
                return true;
            }
            while (IsConsonant(inp, pos)) {
                pos += dir;
                if (MatchCtx(inp, pos, ctx, nci, dir)) {
                    return true;
                }
            }
            return false;

        case '^': // exactly one consonant
            if (!IsConsonant(inp, pos)) {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);

        case '+': // one front vowel (E I Y)
            if (pos < 0 || pos >= (int)inp.size() || (CharacterFeatureTable[(unsigned char)inp[pos]] & CF) == 0) {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);

        case '.': // one voiced consonant
            if (!IsVoiced(inp, pos)) {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);

        case '&': // one sibilant
        {
            int p2 = pos;
            if (!MatchSibilant(inp, p2, dir)) {
                return false;
            }
            return MatchCtx(inp, p2, ctx, nci, dir);
        }

        case '@': // one long-U consonant
            if (!IsUMod(inp, pos)) {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);

        case '%': // suffix (right context only): ER, E, ES, ED, ING, ELY
            {
                int sfxEnd = 0;
                if (!MatchSuffix(inp, pos, sfxEnd)) {
                    return false;
                }
                return MatchCtx(inp, sfxEnd, ctx, nci, dir);
            }

        case '$': // one consonant followed by E or I
            if (!IsConsonant(inp, pos)) {
                return false;
            }
            pos += dir;
            if (pos < 0 || pos >= (int)inp.size() || (CharacterFeatureTable[(unsigned char)inp[pos]] & CF) == 0) {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);

        case ' ': // word boundary
            if (pos < 0 || pos >= (int)inp.size() || inp[pos] != ' ') {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);

        default: // literal character
            if (pos < 0 || pos >= (int)inp.size() || inp[pos] != sym) {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);
    }
}

std::vector<uint8_t> LetterToSound::Convert(const std::string& word) {
    if (word.empty()) {
        return {};
    }

    Initialize();

    // Pad: leading space (word boundary) + word uppercase + two trailing spaces
    std::vector<char> inp(word.size() + 3);
    inp[0] = ' ';
    for (int i = 0; i < (int)word.size(); i++) {
        inp[i + 1] = (char)toupper((unsigned char)word[i]);
    }
    inp[word.size() + 1] = ' ';
    inp[word.size() + 2] = ' ';

    std::vector<uint8_t> phons;
    phons.reserve(word.size() * 2);
    int pos = 1;

    while (inp[pos] != ' ') {
        char c = inp[pos];
        if (c == '\'' || c == '.') {
            pos++;
            continue;
        }
        int li = c - 'A';
        if (li < 0 || li >= 26) {
            pos++;
            continue;
        }

        bool matched = false;
        for (const auto& rule : CompiledLetterToSoundRules[li]) {
            int endPos = 0;
            if (!MatchMid(inp, pos, rule.Match, endPos)) {
                continue;
            }
            if (!MatchCtx(inp, pos - 1, rule.Left, (int)rule.Left.size() - 1, -1)) {
                continue;
            }
            if (!MatchCtx(inp, endPos, rule.Right, 0, +1)) {
                continue;
            }

            for (uint8_t ph : rule.Out) {
                phons.push_back(ph);
            }
            pos = endPos;
            matched = true;
            break;
        }
        if (!matched) {
            pos++;
        }
    }

    return phons;
}

}  // namespace SharpTalk
