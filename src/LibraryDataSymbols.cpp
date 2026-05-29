#include "LibraryData.h"

#include <cstdint>
#include <cstring>

namespace SharpVox {

namespace {
// Phoneme ID aliases (mirror AudioProcessor constants).
enum : uint8_t {
    IY=0,  IH=1,  EH=2,  AE=3,  IX=4,  AX=5,  ER=6,  AH=7,
    AA=8,  AO=9,  UH=10, UW=11, EY=12, AY=13, OY=14, AW=15, OW=16,
    AR=20,
    M=24,  N=25,  NG=26, W=27,  Y=28,  R=29,  L=30,
    HH=43, F=35,  V=36,  TH=37, DH=38,
    S=39,  Z=40,  SH=41, ZH=42,
    P=44,  B=45,  T=46,  D=47,  K=48,  G=49,  CH=50, JH=51,
    S1=56, S2=57,
};

// Per-symbol phoneme sequences — all const, live in flash via XIP.
static const uint8_t kSym_sp[]  = {S,P,EY,S};                                              // " "
static const uint8_t kSym_ex[]  = {EH,K,S,K,L,AX,M,S1,EY,SH,AX,N};                       // "!"
static const uint8_t kSym_qt[]  = {K,W,S1,OW,T};                                           // "\""
static const uint8_t kSym_hs[]  = {N,S1,AH,M,B,ER};                                        // "#"
static const uint8_t kSym_dl[]  = {D,S1,AA,L,ER,Z};                                        // "$"
static const uint8_t kSym_pc[]  = {P,ER,S,S1,EH,N,T};                                      // "%"
static const uint8_t kSym_am[]  = {S2,EH,N,D};                                              // "&"
static const uint8_t kSym_ap[]  = {AX,P,S1,AA,S,T,R,AX,F,IY};                             // "'"
static const uint8_t kSym_op[]  = {S1,OW,P,AX,N,60,P,AX,R,S1,EH,N,TH,AX,S,IH,S};         // "("
static const uint8_t kSym_cp[]  = {K,L,S1,OW,Z,60,P,AX,R,S1,EH,N,TH,AX,S,IH,S};          // ")"
static const uint8_t kSym_as[]  = {S1,AE,S,T,AX,R,IH,S,K};                                // "*"
static const uint8_t kSym_pl[]  = {P,L,S1,AH,S};                                           // "+"
static const uint8_t kSym_cm[]  = {K,S1,AA,M,AX};                                          // ","
static const uint8_t kSym_hy[]  = {HH,S1,AY,F,AX,N};                                       // "-"
static const uint8_t kSym_pd[]  = {P,S1,IH,R,IY,AX,D};                                     // "."
static const uint8_t kSym_sl[]  = {S,L,S1,AE,SH};                                          // "/"
static const uint8_t kSym_d0[]  = {Z,S1,IH,R,S2,OW};                                       // "0"
static const uint8_t kSym_d1[]  = {W,S1,AH,N};                                             // "1"
static const uint8_t kSym_10[]  = {T,S1,EH,N};                                             // "10"
static const uint8_t kSym_100[] = {HH,S1,AH,N,D,R,IX,D};                                   // "100"
static const uint8_t kSym_1E1[] = {TH,S1,AW,Z,IX,N,D};                                     // "1E1" thousand
static const uint8_t kSym_1E10[]= {N,AA,N,S1,IH,L,Y,IX,N};                                 // "1E10" nonillion
static const uint8_t kSym_1E11[]= {D,EH,S,S1,IH,L,Y,IX,N};                                 // "1E11" decillion
static const uint8_t kSym_1E2[] = {M,S1,IH,L,Y,IX,N};                                      // "1E2" million
static const uint8_t kSym_1E3[] = {B,S1,IH,L,Y,IX,N};                                      // "1E3" billion
static const uint8_t kSym_1E4[] = {T,R,S1,IH,L,Y,IX,N};                                    // "1E4" trillion
static const uint8_t kSym_1E5[] = {K,W,AA,D,R,S1,IH,L,Y,IX,N};                             // "1E5" quadrillion
static const uint8_t kSym_1E6[] = {K,W,IH,N,T,S1,IH,L,Y,IX,N};                             // "1E6" quintillion
static const uint8_t kSym_1E7[] = {S,EH,K,S,T,S1,IH,L,Y,IX,N};                             // "1E7" sextillion
static const uint8_t kSym_1E8[] = {S,EH,P,T,S1,IH,L,Y,IX,N};                               // "1E8" septillion
static const uint8_t kSym_1E9[] = {AA,K,T,S1,IH,L,Y,IX,N};                                 // "1E9" octillion
static const uint8_t kSym_11[]  = {IH,L,S1,EH,V,AX,N};                                     // "11"
static const uint8_t kSym_12[]  = {T,W,S1,EH,L,V};                                         // "12"
static const uint8_t kSym_13[]  = {TH,S2,ER,T,S1,IY,N};                                    // "13"
static const uint8_t kSym_14[]  = {F,S2,AO,R,T,S1,IY,N};                                   // "14"
static const uint8_t kSym_15[]  = {F,S2,IH,F,T,S1,IY,N};                                   // "15"
static const uint8_t kSym_16[]  = {S,S2,IH,K,S,T,S1,IY,N};                                 // "16"
static const uint8_t kSym_17[]  = {S,S2,EH,V,IX,N,T,S1,IY,N};                              // "17"
static const uint8_t kSym_18[]  = {S2,EY,T,S1,IY,N};                                        // "18"
static const uint8_t kSym_19[]  = {N,S2,AY,N,T,S1,IY,N};                                    // "19"
static const uint8_t kSym_d2[]  = {T,S1,UW};                                                // "2"
static const uint8_t kSym_20[]  = {T,W,S1,EH,N,T,IY};                                      // "20"
static const uint8_t kSym_d3[]  = {TH,R,S1,IY};                                             // "3"
static const uint8_t kSym_30[]  = {TH,S1,ER,D,IY};                                         // "30"
static const uint8_t kSym_d4[]  = {F,S1,AO,R};                                              // "4"
static const uint8_t kSym_40[]  = {F,S1,AO,R,D,IY};                                        // "40"
static const uint8_t kSym_d5[]  = {F,S1,AY,V};                                              // "5"
static const uint8_t kSym_50[]  = {F,S1,IH,F,T,IY};                                        // "50"
static const uint8_t kSym_d6[]  = {S,S1,IH,K,S};                                           // "6"
static const uint8_t kSym_60[]  = {S,S1,IH,K,S,T,IY};                                      // "60"
static const uint8_t kSym_d7[]  = {S,S1,EH,V,AX,N};                                        // "7"
static const uint8_t kSym_70[]  = {S,S1,EH,V,IX,N,T,IY};                                   // "70"
static const uint8_t kSym_d8[]  = {S1,EY,T};                                                // "8"
static const uint8_t kSym_80[]  = {S1,EY,D,IY};                                             // "80"
static const uint8_t kSym_d9[]  = {N,S1,AY,N};                                              // "9"
static const uint8_t kSym_90[]  = {N,S1,AY,N,T,IY};                                        // "90"
static const uint8_t kSym_cn[]  = {K,S1,OW,L,AX,N};                                        // ":"
static const uint8_t kSym_sc[]  = {S,S1,EH,M,IY,60,K,OW,L,AX,N};                          // ";"
static const uint8_t kSym_lt[]  = {L,S1,EH,S,60,DH,S1,AE,N};                               // "<"
static const uint8_t kSym_eq[]  = {S1,IY,K,W,AX,L,Z};                                      // "="
static const uint8_t kSym_gt[]  = {G,R,S1,EY,T,ER,60,DH,S2,AE,N};                         // ">"
static const uint8_t kSym_qm[]  = {K,W,S1,EH,S,CH,AX,N};                                   // "?"
static const uint8_t kSym_at[]  = {S1,AE,T};                                                // "@"
static const uint8_t kSym_LA[]  = {S1,EY};                                                  // "A"
static const uint8_t kSym_LB[]  = {B,S1,IY};                                                // "B"
static const uint8_t kSym_LC[]  = {S,S1,IY};                                                // "C"
static const uint8_t kSym_LD[]  = {D,S1,IY};                                                // "D"
static const uint8_t kSym_LE[]  = {S1,IY};                                                  // "E"
static const uint8_t kSym_LF[]  = {S1,EH,F};                                                // "F"
static const uint8_t kSym_LG[]  = {JH,S1,IY};                                               // "G"
static const uint8_t kSym_LH[]  = {S1,EY,CH};                                               // "H"
static const uint8_t kSym_LI[]  = {AY};                                                     // "I"
static const uint8_t kSym_LJ[]  = {JH,S1,EY};                                               // "J"
static const uint8_t kSym_LK[]  = {K,S1,EY};                                                // "K"
static const uint8_t kSym_LL[]  = {S1,EH,L};                                                // "L"
static const uint8_t kSym_LM[]  = {S1,EH,M};                                                // "M"
static const uint8_t kSym_LN[]  = {S1,EH,N};                                                // "N"
static const uint8_t kSym_LO[]  = {S1,OW};                                                  // "O"
static const uint8_t kSym_LP[]  = {P,S1,IY};                                                // "P"
static const uint8_t kSym_LQ[]  = {K,Y,S1,UW};                                              // "Q"
static const uint8_t kSym_LR[]  = {S1,AR};                                                  // "R"
static const uint8_t kSym_LS[]  = {S1,EH,S};                                                // "S"
static const uint8_t kSym_LT[]  = {T,S1,IY};                                                // "T"
static const uint8_t kSym_LU[]  = {Y,S1,UW};                                                // "U"
static const uint8_t kSym_LV[]  = {V,S1,IY};                                                // "V"
static const uint8_t kSym_LW[]  = {D,S1,AH,B,AX,L,60,Y,S2,UW};                            // "W"
static const uint8_t kSym_LX[]  = {S1,EH,K,S};                                              // "X"
static const uint8_t kSym_LY[]  = {W,S1,AY};                                                // "Y"
static const uint8_t kSym_LZ[]  = {Z,S1,IY};                                                // "Z"
static const uint8_t kSym_ob[]  = {S1,OW,P,AX,N,60,B,R,S1,AE,K,IH,T};                     // "["

struct SymEntry { const char* key; const uint8_t* data; uint8_t len; };

// Table is ordered by strcmp (ASCII lexicographic) for future binary-search upgrade.
static const SymEntry kSymbols[] = {
    {" ",    kSym_sp,  sizeof(kSym_sp)},
    {"!",    kSym_ex,  sizeof(kSym_ex)},
    {"\"",   kSym_qt,  sizeof(kSym_qt)},
    {"#",    kSym_hs,  sizeof(kSym_hs)},
    {"$",    kSym_dl,  sizeof(kSym_dl)},
    {"%",    kSym_pc,  sizeof(kSym_pc)},
    {"&",    kSym_am,  sizeof(kSym_am)},
    {"'",    kSym_ap,  sizeof(kSym_ap)},
    {"(",    kSym_op,  sizeof(kSym_op)},
    {")",    kSym_cp,  sizeof(kSym_cp)},
    {"*",    kSym_as,  sizeof(kSym_as)},
    {"+",    kSym_pl,  sizeof(kSym_pl)},
    {",",    kSym_cm,  sizeof(kSym_cm)},
    {"-",    kSym_hy,  sizeof(kSym_hy)},
    {".",    kSym_pd,  sizeof(kSym_pd)},
    {"/",    kSym_sl,  sizeof(kSym_sl)},
    {"0",    kSym_d0,  sizeof(kSym_d0)},
    {"1",    kSym_d1,  sizeof(kSym_d1)},
    {"10",   kSym_10,  sizeof(kSym_10)},
    {"100",  kSym_100, sizeof(kSym_100)},
    {"1E1",  kSym_1E1, sizeof(kSym_1E1)},
    {"1E10", kSym_1E10,sizeof(kSym_1E10)},
    {"1E11", kSym_1E11,sizeof(kSym_1E11)},
    {"1E2",  kSym_1E2, sizeof(kSym_1E2)},
    {"1E3",  kSym_1E3, sizeof(kSym_1E3)},
    {"1E4",  kSym_1E4, sizeof(kSym_1E4)},
    {"1E5",  kSym_1E5, sizeof(kSym_1E5)},
    {"1E6",  kSym_1E6, sizeof(kSym_1E6)},
    {"1E7",  kSym_1E7, sizeof(kSym_1E7)},
    {"1E8",  kSym_1E8, sizeof(kSym_1E8)},
    {"1E9",  kSym_1E9, sizeof(kSym_1E9)},
    {"11",   kSym_11,  sizeof(kSym_11)},
    {"12",   kSym_12,  sizeof(kSym_12)},
    {"13",   kSym_13,  sizeof(kSym_13)},
    {"14",   kSym_14,  sizeof(kSym_14)},
    {"15",   kSym_15,  sizeof(kSym_15)},
    {"16",   kSym_16,  sizeof(kSym_16)},
    {"17",   kSym_17,  sizeof(kSym_17)},
    {"18",   kSym_18,  sizeof(kSym_18)},
    {"19",   kSym_19,  sizeof(kSym_19)},
    {"2",    kSym_d2,  sizeof(kSym_d2)},
    {"20",   kSym_20,  sizeof(kSym_20)},
    {"3",    kSym_d3,  sizeof(kSym_d3)},
    {"30",   kSym_30,  sizeof(kSym_30)},
    {"4",    kSym_d4,  sizeof(kSym_d4)},
    {"40",   kSym_40,  sizeof(kSym_40)},
    {"5",    kSym_d5,  sizeof(kSym_d5)},
    {"50",   kSym_50,  sizeof(kSym_50)},
    {"6",    kSym_d6,  sizeof(kSym_d6)},
    {"60",   kSym_60,  sizeof(kSym_60)},
    {"7",    kSym_d7,  sizeof(kSym_d7)},
    {"70",   kSym_70,  sizeof(kSym_70)},
    {"8",    kSym_d8,  sizeof(kSym_d8)},
    {"80",   kSym_80,  sizeof(kSym_80)},
    {"9",    kSym_d9,  sizeof(kSym_d9)},
    {"90",   kSym_90,  sizeof(kSym_90)},
    {":",    kSym_cn,  sizeof(kSym_cn)},
    {";",    kSym_sc,  sizeof(kSym_sc)},
    {"<",    kSym_lt,  sizeof(kSym_lt)},
    {"=",    kSym_eq,  sizeof(kSym_eq)},
    {">",    kSym_gt,  sizeof(kSym_gt)},
    {"?",    kSym_qm,  sizeof(kSym_qm)},
    {"@",    kSym_at,  sizeof(kSym_at)},
    {"A",    kSym_LA,  sizeof(kSym_LA)},
    {"B",    kSym_LB,  sizeof(kSym_LB)},
    {"C",    kSym_LC,  sizeof(kSym_LC)},
    {"D",    kSym_LD,  sizeof(kSym_LD)},
    {"E",    kSym_LE,  sizeof(kSym_LE)},
    {"F",    kSym_LF,  sizeof(kSym_LF)},
    {"G",    kSym_LG,  sizeof(kSym_LG)},
    {"H",    kSym_LH,  sizeof(kSym_LH)},
    {"I",    kSym_LI,  sizeof(kSym_LI)},
    {"J",    kSym_LJ,  sizeof(kSym_LJ)},
    {"K",    kSym_LK,  sizeof(kSym_LK)},
    {"L",    kSym_LL,  sizeof(kSym_LL)},
    {"M",    kSym_LM,  sizeof(kSym_LM)},
    {"N",    kSym_LN,  sizeof(kSym_LN)},
    {"O",    kSym_LO,  sizeof(kSym_LO)},
    {"P",    kSym_LP,  sizeof(kSym_LP)},
    {"Q",    kSym_LQ,  sizeof(kSym_LQ)},
    {"R",    kSym_LR,  sizeof(kSym_LR)},
    {"S",    kSym_LS,  sizeof(kSym_LS)},
    {"T",    kSym_LT,  sizeof(kSym_LT)},
    {"U",    kSym_LU,  sizeof(kSym_LU)},
    {"V",    kSym_LV,  sizeof(kSym_LV)},
    {"W",    kSym_LW,  sizeof(kSym_LW)},
    {"X",    kSym_LX,  sizeof(kSym_LX)},
    {"Y",    kSym_LY,  sizeof(kSym_LY)},
    {"Z",    kSym_LZ,  sizeof(kSym_LZ)},
    {"[",    kSym_ob,  sizeof(kSym_ob)},
};
static constexpr size_t kSymbolCount = sizeof(kSymbols) / sizeof(kSymbols[0]);
}  // namespace

const uint8_t* LibraryData::FindSymbol(const char* key, size_t& sz) {
    for (size_t i = 0; i < kSymbolCount; i++) {
        if (strcmp(key, kSymbols[i].key) == 0) {
            sz = kSymbols[i].len;
            return kSymbols[i].data;
        }
    }
    sz = 0;
    return nullptr;
}

}  // namespace SharpVox
