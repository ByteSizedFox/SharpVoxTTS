#include "../include/Phonemizer.h"
#include "../include/AudioProcessor.h"
#include "../include/LetterToSound.h"
#include "../include/LexiconReader.h"
#include "../include/Morphology.h"
#include "../include/HeteronymResolver.h"
#include "../include/LibraryData.h"
#include "../include/TextCommands.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <unordered_set>

namespace SharpVox {

    // Local aliases for AudioProcessor opcode constants
    static const uint8_t OP_STRESS1   = AudioProcessor::kOpStress1;
    static const uint8_t OP_STRESS2   = AudioProcessor::kOpStress2;
    static const uint8_t OP_EMPHSTRESS = AudioProcessor::kOpEmphStress;
    static const uint8_t OP_SYLL      = AudioProcessor::kOpSyll;
    static const uint8_t OP_WORD      = AudioProcessor::kOpWord;
    static const uint8_t OP_PREP      = AudioProcessor::kOpPrep;
    static const uint8_t OP_VERB      = AudioProcessor::kOpVerb;
    static const uint8_t OP_COMMA     = AudioProcessor::kOpComma;
    static const uint8_t OP_PERIOD    = AudioProcessor::kOpPeriod;
    static const uint8_t OP_QUEST     = AudioProcessor::kOpQuest;
    static const uint8_t OP_EXCLAM    = AudioProcessor::kOpExclam;

    // Subject pronouns: receive kPronounWord on every phoneme so the backend can
    // apply vocal-confidence emphasis (pitch accent + vowel lengthening).
    static const char* const PronounWordsTableArr[] = {
        "i", "you", "he", "she", "it", "we", "they"
    };

    // Function words do NOT receive kContent_Word, primary dict stress is
    // suppressed so they don't drive pitch peaks in the BackEnd pitch algorithm.
    // Mirrors POS-based content/function distinction.
    static const char* const FunctionWordsTableArr[] = {
        // articles / determiners
        "a", "an", "the",
        // prepositions
        "of", "in", "on", "at", "by", "for", "to", "up", "as", "into",
        "from", "with", "about", "over", "under", "out", "off", "than",
        // coordinating conjunctions
        "and", "or", "but", "nor", "yet", "so",
        // subordinating conjunctions
        "if", "that", "than", "when", "while", "because", "though",
        "although", "unless", "until", "since", "after", "before",
        // auxiliaries & copula
        "be", "am", "is", "are", "was", "were", "been", "being",
        "have", "has", "had", "do", "does", "did",
        "will", "would", "could", "should", "may", "might", "shall",
        "can", "must", "ought",
        // subject / object pronouns
        "i", "he", "she", "we", "they", "you", "it",
        "me", "him", "her", "us", "them",
        // possessive determiners
        "my", "your", "his", "its", "our", "their",
        // other function words
        "not", "no", "there", "here",
    };

    static std::unordered_set<std::string> BuildFunctionWordsTable() {
        std::unordered_set<std::string> s;
        for (const char* w : FunctionWordsTableArr) {
            s.insert(w);
        }
        return s;
    }

    static std::unordered_set<std::string> BuildPronounWordsTable() {
        std::unordered_set<std::string> s;
        for (const char* w : PronounWordsTableArr) {
            s.insert(w);
        }
        return s;
    }

    static const std::unordered_set<std::string> FunctionWordsTable = BuildFunctionWordsTable();
    static const std::unordered_set<std::string> PronounWordsTable  = BuildPronounWordsTable();

    // Hardcoded letter pronunciations - A-Z indexed by (char - 'A').
    // Stress opcode placed immediately before the stressed vowel.
    // Never routed through dict or LTS so missing entries can't break them.
    static const uint8_t LetterPhonemesTable_A[]  = { 0x38, 0x0C };                                                              // A  -> EY
    static const uint8_t LetterPhonemesTable_B[]  = { 0x2D, 0x38, 0x00 };                                                        // B  -> B IY
    static const uint8_t LetterPhonemesTable_C[]  = { 0x27, 0x38, 0x00 };                                                        // C  -> S IY
    static const uint8_t LetterPhonemesTable_D[]  = { 0x2F, 0x38, 0x00 };                                                        // D  -> D IY
    static const uint8_t LetterPhonemesTable_E[]  = { 0x38, 0x00 };                                                              // E  -> IY
    static const uint8_t LetterPhonemesTable_F[]  = { 0x38, 0x02, 0x23 };                                                        // F  -> EH F
    static const uint8_t LetterPhonemesTable_G[]  = { 0x33, 0x38, 0x00 };                                                        // G  -> JH IY
    static const uint8_t LetterPhonemesTable_H[]  = { 0x38, 0x0C, 0x32 };                                                        // H  -> EY CH  (aitch)
    static const uint8_t LetterPhonemesTable_I[]  = { 0x38, 0x0D };                                                              // I  -> AY
    static const uint8_t LetterPhonemesTable_J[]  = { 0x33, 0x38, 0x0C };                                                        // J  -> JH EY
    static const uint8_t LetterPhonemesTable_K[]  = { 0x30, 0x38, 0x0C };                                                        // K  -> K EY
    static const uint8_t LetterPhonemesTable_L[]  = { 0x38, 0x02, 0x1E };                                                        // L  -> EH L
    static const uint8_t LetterPhonemesTable_M[]  = { 0x38, 0x02, 0x18 };                                                        // M  -> EH M
    static const uint8_t LetterPhonemesTable_N[]  = { 0x38, 0x02, 0x19 };                                                        // N  -> EH N
    static const uint8_t LetterPhonemesTable_O[]  = { 0x38, 0x10 };                                                              // O  -> OW
    static const uint8_t LetterPhonemesTable_P[]  = { 0x2C, 0x38, 0x00 };                                                        // P  -> P IY
    static const uint8_t LetterPhonemesTable_Q[]  = { 0x30, 0x1C, 0x38, 0x0B };                                                  // Q  -> K Y UW  (cue)
    static const uint8_t LetterPhonemesTable_R[]  = { 0x38, 0x08, 0x1D };                                                        // R  -> AA R
    static const uint8_t LetterPhonemesTable_S[]  = { 0x38, 0x02, 0x27 };                                                        // S  -> EH S
    static const uint8_t LetterPhonemesTable_T[]  = { 0x2E, 0x38, 0x00 };                                                        // T  -> T IY
    static const uint8_t LetterPhonemesTable_U[]  = { 0x1C, 0x38, 0x0B };                                                        // U  -> Y UW
    static const uint8_t LetterPhonemesTable_V[]  = { 0x24, 0x38, 0x00 };                                                        // V  -> V IY
    static const uint8_t LetterPhonemesTable_W[]  = { 0x2F, 0x38, 0x07, 0x2D, 0x05, 0x1E, 0x1C, 0x38, 0x0B };                   // W  -> D AH B AX L Y UW  (double-you)
    static const uint8_t LetterPhonemesTable_X[]  = { 0x38, 0x02, 0x30, 0x27 };                                                  // X  -> EH K S
    static const uint8_t LetterPhonemesTable_Y[]  = { 0x1B, 0x38, 0x0D };                                                        // Y  -> W AY
    static const uint8_t LetterPhonemesTable_Z[]  = { 0x28, 0x38, 0x00 };                                                        // Z  -> Z IY

    struct LetterPhonEntry { const uint8_t* data; size_t len; };
    static const LetterPhonEntry LetterPhonemesTable[26] = {
        { LetterPhonemesTable_A, sizeof(LetterPhonemesTable_A) },
        { LetterPhonemesTable_B, sizeof(LetterPhonemesTable_B) },
        { LetterPhonemesTable_C, sizeof(LetterPhonemesTable_C) },
        { LetterPhonemesTable_D, sizeof(LetterPhonemesTable_D) },
        { LetterPhonemesTable_E, sizeof(LetterPhonemesTable_E) },
        { LetterPhonemesTable_F, sizeof(LetterPhonemesTable_F) },
        { LetterPhonemesTable_G, sizeof(LetterPhonemesTable_G) },
        { LetterPhonemesTable_H, sizeof(LetterPhonemesTable_H) },
        { LetterPhonemesTable_I, sizeof(LetterPhonemesTable_I) },
        { LetterPhonemesTable_J, sizeof(LetterPhonemesTable_J) },
        { LetterPhonemesTable_K, sizeof(LetterPhonemesTable_K) },
        { LetterPhonemesTable_L, sizeof(LetterPhonemesTable_L) },
        { LetterPhonemesTable_M, sizeof(LetterPhonemesTable_M) },
        { LetterPhonemesTable_N, sizeof(LetterPhonemesTable_N) },
        { LetterPhonemesTable_O, sizeof(LetterPhonemesTable_O) },
        { LetterPhonemesTable_P, sizeof(LetterPhonemesTable_P) },
        { LetterPhonemesTable_Q, sizeof(LetterPhonemesTable_Q) },
        { LetterPhonemesTable_R, sizeof(LetterPhonemesTable_R) },
        { LetterPhonemesTable_S, sizeof(LetterPhonemesTable_S) },
        { LetterPhonemesTable_T, sizeof(LetterPhonemesTable_T) },
        { LetterPhonemesTable_U, sizeof(LetterPhonemesTable_U) },
        { LetterPhonemesTable_V, sizeof(LetterPhonemesTable_V) },
        { LetterPhonemesTable_W, sizeof(LetterPhonemesTable_W) },
        { LetterPhonemesTable_X, sizeof(LetterPhonemesTable_X) },
        { LetterPhonemesTable_Y, sizeof(LetterPhonemesTable_Y) },
        { LetterPhonemesTable_Z, sizeof(LetterPhonemesTable_Z) },
    };

    // Phoneme sequences for every word the normalizer can produce.
    // Checked before dict + LTS so dictionary swaps never affect normalizer output.
    struct NormWordEntry { const char* key; const uint8_t* data; size_t len; };

    static const uint8_t nw_ZERO[]          = { 0x28, 0x38, 0x01, 0x1D, 0x10 };
    static const uint8_t nw_ONE[]           = { 0x1B, 0x38, 0x07, 0x19 };
    static const uint8_t nw_TWO[]           = { 0x2E, 0x38, 0x0B };
    static const uint8_t nw_THREE[]         = { 0x25, 0x1D, 0x38, 0x00 };
    static const uint8_t nw_FOUR[]          = { 0x23, 0x38, 0x09, 0x1D };
    static const uint8_t nw_FIVE[]          = { 0x23, 0x38, 0x0D, 0x24 };
    static const uint8_t nw_SIX[]           = { 0x27, 0x38, 0x01, 0x30, 0x27 };
    static const uint8_t nw_SEVEN[]         = { 0x27, 0x38, 0x02, 0x24, 0x05, 0x19 };
    static const uint8_t nw_EIGHT[]         = { 0x38, 0x0C, 0x2E };
    static const uint8_t nw_NINE[]          = { 0x19, 0x38, 0x0D, 0x19 };
    //    Teens
    static const uint8_t nw_TEN[]           = { 0x2E, 0x38, 0x02, 0x19 };
    static const uint8_t nw_ELEVEN[]        = { 0x04, 0x1E, 0x38, 0x02, 0x24, 0x05, 0x19 };
    static const uint8_t nw_TWELVE[]        = { 0x2E, 0x1B, 0x38, 0x02, 0x1E, 0x24 };
    static const uint8_t nw_THIRTEEN[]      = { 0x25, 0x38, 0x06, 0x2E, 0x38, 0x00, 0x19 };
    static const uint8_t nw_FOURTEEN[]      = { 0x23, 0x38, 0x09, 0x1D, 0x2E, 0x38, 0x00, 0x19 };
    static const uint8_t nw_FIFTEEN[]       = { 0x23, 0x04, 0x23, 0x2E, 0x38, 0x00, 0x19 };
    static const uint8_t nw_SIXTEEN[]       = { 0x27, 0x04, 0x30, 0x27, 0x2E, 0x38, 0x00, 0x19 };
    static const uint8_t nw_SEVENTEEN[]     = { 0x27, 0x38, 0x02, 0x24, 0x05, 0x19, 0x2E, 0x38, 0x00, 0x19 };
    static const uint8_t nw_EIGHTEEN[]      = { 0x0C, 0x2E, 0x38, 0x00, 0x19 };
    static const uint8_t nw_NINETEEN[]      = { 0x19, 0x38, 0x0D, 0x19, 0x2E, 0x38, 0x00, 0x19 };
    //    Tens
    static const uint8_t nw_TWENTY[]        = { 0x2E, 0x1B, 0x38, 0x02, 0x19, 0x2E, 0x00 };
    static const uint8_t nw_THIRTY[]        = { 0x25, 0x38, 0x06, 0x2F, 0x39, 0x00 };
    static const uint8_t nw_FORTY[]         = { 0x23, 0x38, 0x09, 0x1D, 0x2E, 0x00 };
    static const uint8_t nw_FIFTY[]         = { 0x23, 0x38, 0x01, 0x23, 0x2E, 0x00 };
    static const uint8_t nw_SIXTY[]         = { 0x27, 0x38, 0x01, 0x30, 0x27, 0x2E, 0x00 };
    static const uint8_t nw_SEVENTY[]       = { 0x27, 0x38, 0x02, 0x24, 0x05, 0x19, 0x2E, 0x00 };
    static const uint8_t nw_EIGHTY[]        = { 0x38, 0x0C, 0x2E, 0x00 };
    static const uint8_t nw_NINETY[]        = { 0x19, 0x38, 0x0D, 0x19, 0x2E, 0x00 };
    //    Large / misc number
    static const uint8_t nw_HUNDRED[]       = { 0x2B, 0x38, 0x07, 0x19, 0x2F, 0x1D, 0x05, 0x2F };
    static const uint8_t nw_THOUSAND[]      = { 0x25, 0x38, 0x0F, 0x28, 0x05, 0x19, 0x2F };
    static const uint8_t nw_MILLION[]       = { 0x18, 0x38, 0x01, 0x1E, 0x1C, 0x05, 0x19 };
    static const uint8_t nw_BILLION[]       = { 0x2D, 0x38, 0x01, 0x1E, 0x1C, 0x05, 0x19 };
    static const uint8_t nw_OH[]            = { 0x38, 0x10 };
    static const uint8_t nw_POINT[]         = { 0x2C, 0x38, 0x0E, 0x19, 0x2E };
    static const uint8_t nw_AND[]           = { 0x05, 0x19, 0x2F };
    //    Currency / percent
    static const uint8_t nw_DOLLAR[]        = { 0x2F, 0x38, 0x08, 0x1E, 0x06 };
    static const uint8_t nw_DOLLARS[]       = { 0x2F, 0x38, 0x08, 0x1E, 0x06, 0x28 };
    static const uint8_t nw_CENT[]          = { 0x27, 0x38, 0x02, 0x19, 0x2E };
    static const uint8_t nw_CENTS[]         = { 0x27, 0x38, 0x02, 0x19, 0x2E, 0x27 };
    static const uint8_t nw_PERCENT[]       = { 0x2C, 0x06, 0x27, 0x38, 0x02, 0x19, 0x2E };
    //    Ordinals
    static const uint8_t nw_ZEROTH[]        = { 0x28, 0x38, 0x00, 0x1D, 0x10, 0x25 };
    static const uint8_t nw_FIRST[]         = { 0x23, 0x38, 0x06, 0x27, 0x2E };
    static const uint8_t nw_SECOND[]        = { 0x27, 0x38, 0x02, 0x30, 0x05, 0x19, 0x2F };
    static const uint8_t nw_THIRD[]         = { 0x25, 0x38, 0x06, 0x2F };
    static const uint8_t nw_FOURTH[]        = { 0x23, 0x38, 0x09, 0x1D, 0x25 };
    static const uint8_t nw_FIFTH[]         = { 0x23, 0x38, 0x01, 0x23, 0x25 };
    static const uint8_t nw_SIXTH[]         = { 0x27, 0x38, 0x01, 0x30, 0x27, 0x25 };
    static const uint8_t nw_SEVENTH[]       = { 0x27, 0x38, 0x02, 0x24, 0x05, 0x19, 0x25 };
    static const uint8_t nw_EIGHTH[]        = { 0x38, 0x0C, 0x2E, 0x25 };
    static const uint8_t nw_NINTH[]         = { 0x19, 0x38, 0x0D, 0x19, 0x25 };
    static const uint8_t nw_TENTH[]         = { 0x2E, 0x38, 0x02, 0x19, 0x25 };
    static const uint8_t nw_ELEVENTH[]      = { 0x04, 0x1E, 0x38, 0x02, 0x24, 0x05, 0x19, 0x25 };
    static const uint8_t nw_TWELFTH[]       = { 0x2E, 0x1B, 0x38, 0x02, 0x1E, 0x23, 0x25 };
    static const uint8_t nw_THIRTEENTH[]    = { 0x25, 0x38, 0x06, 0x2E, 0x38, 0x00, 0x19, 0x25 };
    static const uint8_t nw_FOURTEENTH[]    = { 0x23, 0x38, 0x09, 0x1D, 0x2E, 0x38, 0x00, 0x19, 0x25 };
    static const uint8_t nw_FIFTEENTH[]     = { 0x23, 0x04, 0x23, 0x2E, 0x38, 0x00, 0x19, 0x25 };
    static const uint8_t nw_SIXTEENTH[]     = { 0x27, 0x04, 0x30, 0x27, 0x2E, 0x38, 0x00, 0x19, 0x25 };
    static const uint8_t nw_SEVENTEENTH[]   = { 0x27, 0x38, 0x02, 0x24, 0x05, 0x19, 0x2E, 0x38, 0x00, 0x19, 0x25 };
    static const uint8_t nw_EIGHTEENTH[]    = { 0x0C, 0x2E, 0x38, 0x00, 0x19, 0x25 };
    static const uint8_t nw_NINETEENTH[]    = { 0x19, 0x38, 0x0D, 0x19, 0x2E, 0x38, 0x00, 0x19, 0x25 };
    static const uint8_t nw_TWENTIETH[]     = { 0x2E, 0x1B, 0x38, 0x02, 0x19, 0x2E, 0x00, 0x05, 0x25 };
    static const uint8_t nw_THIRTIETH[]     = { 0x25, 0x38, 0x06, 0x2E, 0x00, 0x05, 0x25 };
    static const uint8_t nw_FORTIETH[]      = { 0x23, 0x38, 0x09, 0x1D, 0x2E, 0x00, 0x04, 0x25 };
    static const uint8_t nw_FIFTIETH[]      = { 0x23, 0x38, 0x01, 0x23, 0x2E, 0x00, 0x04, 0x25 };
    static const uint8_t nw_SIXTIETH[]      = { 0x27, 0x38, 0x01, 0x30, 0x27, 0x2E, 0x00, 0x04, 0x25 };
    static const uint8_t nw_SEVENTIETH[]    = { 0x27, 0x38, 0x02, 0x24, 0x05, 0x19, 0x2E, 0x00, 0x04, 0x25 };
    static const uint8_t nw_EIGHTIETH[]     = { 0x38, 0x0C, 0x2E, 0x00, 0x04, 0x25 };
    static const uint8_t nw_NINETIETH[]     = { 0x19, 0x38, 0x0D, 0x19, 0x2E, 0x00, 0x04, 0x25 };
    //    Letter names (used by dotted abbreviation expansions)
    static const uint8_t nw_AY[]            = { 0x38, 0x0C };
    static const uint8_t nw_BEE[]           = { 0x2D, 0x38, 0x00 };
    static const uint8_t nw_SEE[]           = { 0x27, 0x38, 0x00 };
    static const uint8_t nw_DEE[]           = { 0x2F, 0x38, 0x00 };
    static const uint8_t nw_EF[]            = { 0x38, 0x02, 0x23 };
    static const uint8_t nw_EM[]            = { 0x38, 0x02, 0x18 };
    static const uint8_t nw_PEE[]           = { 0x2C, 0x38, 0x00 };
    //    Dotted abbreviation expansions
    static const uint8_t nw_THAT[]          = { 0x26, 0x38, 0x03, 0x2E };
    static const uint8_t nw_IS[]            = { 0x38, 0x01, 0x28 };
    static const uint8_t nw_FOR[]           = { 0x23, 0x38, 0x09, 0x1D };
    static const uint8_t nw_EXAMPLE[]       = { 0x04, 0x31, 0x28, 0x38, 0x03, 0x18, 0x2C, 0x05, 0x1E };
    static const uint8_t nw_POSTSCRIPT[]    = { 0x2C, 0x38, 0x10, 0x27, 0x30, 0x1D, 0x39, 0x01, 0x2C, 0x2E };
    static const uint8_t nw_WITH[]          = { 0x1B, 0x38, 0x01, 0x26 };
    static const uint8_t nw_REGARD[]        = { 0x1D, 0x04, 0x31, 0x38, 0x08, 0x1D, 0x2F };
    static const uint8_t nw_TO[]            = { 0x2E, 0x38, 0x0B };
    //    Titles
    static const uint8_t nw_DOCTOR[]        = { 0x2F, 0x38, 0x08, 0x30, 0x2E, 0x06 };
    static const uint8_t nw_MISTER[]        = { 0x18, 0x38, 0x01, 0x27, 0x2E, 0x06 };
    static const uint8_t nw_MISSUS[]        = { 0x18, 0x38, 0x01, 0x27, 0x04, 0x28 };
    static const uint8_t nw_MISS[]          = { 0x18, 0x38, 0x01, 0x27 };
    static const uint8_t nw_PROFESSOR[]     = { 0x2C, 0x1D, 0x05, 0x23, 0x38, 0x02, 0x27, 0x06 };
    static const uint8_t nw_JUNIOR[]        = { 0x33, 0x38, 0x0B, 0x19, 0x1C, 0x06 };
    static const uint8_t nw_SENIOR[]        = { 0x27, 0x38, 0x00, 0x19, 0x1C, 0x06 };
    //    Common abbreviation expansions
    static const uint8_t nw_VERSUS[]        = { 0x24, 0x38, 0x06, 0x27, 0x05, 0x27 };
    static const uint8_t nw_ETCETERA[]      = { 0x38, 0x02, 0x2E, 0x27, 0x38, 0x02, 0x2E, 0x06, 0x05 };
    static const uint8_t nw_APPROXIMATELY[] = { 0x05, 0x2C, 0x1D, 0x38, 0x08, 0x30, 0x27, 0x05, 0x18, 0x05, 0x2E, 0x1E, 0x00 };
    static const uint8_t nw_MAXIMUM[]       = { 0x18, 0x38, 0x03, 0x30, 0x27, 0x05, 0x18, 0x05, 0x18 };
    static const uint8_t nw_MINIMUM[]       = { 0x18, 0x38, 0x01, 0x19, 0x05, 0x18, 0x05, 0x18 };
    static const uint8_t nw_AVERAGE[]       = { 0x38, 0x03, 0x24, 0x06, 0x04, 0x33 };
    static const uint8_t nw_VOLUME[]        = { 0x24, 0x38, 0x08, 0x1E, 0x1C, 0x0B, 0x18 };
    static const uint8_t nw_FIGURE[]        = { 0x23, 0x38, 0x01, 0x31, 0x1C, 0x06 };
    static const uint8_t nw_REFERENCE[]     = { 0x1D, 0x38, 0x02, 0x23, 0x06, 0x05, 0x19, 0x27 };
    static const uint8_t nw_ESTABLISHED[]   = { 0x04, 0x27, 0x2E, 0x38, 0x03, 0x2D, 0x1E, 0x04, 0x29, 0x2E };
    static const uint8_t nw_CONTINUED[]     = { 0x30, 0x05, 0x19, 0x2E, 0x38, 0x01, 0x19, 0x1C, 0x0B, 0x2F };
    static const uint8_t nw_ABBREVIATION[]  = { 0x05, 0x2D, 0x1D, 0x39, 0x00, 0x24, 0x00, 0x38, 0x0C, 0x29, 0x05, 0x19 };
    static const uint8_t nw_ATTRIBUTED[]    = { 0x05, 0x2E, 0x1D, 0x38, 0x01, 0x2D, 0x1C, 0x05, 0x2E, 0x04, 0x2F };
    static const uint8_t nw_DISTRICT[]      = { 0x2F, 0x38, 0x01, 0x27, 0x2E, 0x1D, 0x04, 0x30, 0x2E };
    static const uint8_t nw_POPULATION[]    = { 0x2C, 0x39, 0x08, 0x2C, 0x1C, 0x05, 0x1E, 0x38, 0x0C, 0x29, 0x05, 0x19 };
    static const uint8_t nw_TEMPERATURE[]   = { 0x2E, 0x38, 0x02, 0x18, 0x2C, 0x1D, 0x05, 0x32, 0x06 };
    static const uint8_t nw_TECHNICAL[]     = { 0x2E, 0x38, 0x02, 0x30, 0x19, 0x04, 0x30, 0x05, 0x1E };
    static const uint8_t nw_ELECTRIC[]      = { 0x04, 0x1E, 0x38, 0x02, 0x30, 0x2E, 0x1D, 0x04, 0x30 };
    //    Address
    static const uint8_t nw_STREET[]        = { 0x27, 0x2E, 0x1D, 0x38, 0x00, 0x2E };
    static const uint8_t nw_AVENUE[]        = { 0x38, 0x03, 0x24, 0x05, 0x19, 0x39, 0x0B };
    static const uint8_t nw_BOULEVARD[]     = { 0x2D, 0x38, 0x0A, 0x1E, 0x05, 0x24, 0x39, 0x08, 0x1D, 0x2F };
    static const uint8_t nw_ROAD[]          = { 0x1D, 0x38, 0x10, 0x2F };
    static const uint8_t nw_LANE[]          = { 0x1E, 0x38, 0x0C, 0x19 };
    //    Military
    static const uint8_t nw_LIEUTENANT[]    = { 0x1E, 0x0B, 0x2E, 0x38, 0x02, 0x19, 0x05, 0x19, 0x2E };
    static const uint8_t nw_CAPTAIN[]       = { 0x30, 0x38, 0x03, 0x2C, 0x2E, 0x05, 0x19 };
    static const uint8_t nw_GENERAL[]       = { 0x33, 0x38, 0x02, 0x19, 0x06, 0x05, 0x1E };
    static const uint8_t nw_SERGEANT[]      = { 0x27, 0x38, 0x08, 0x1D, 0x33, 0x05, 0x19, 0x2E };
    static const uint8_t nw_PRIVATE[]       = { 0x2C, 0x1D, 0x38, 0x0D, 0x24, 0x05, 0x2E };
    static const uint8_t nw_COLONEL[]       = { 0x30, 0x38, 0x06, 0x19, 0x05, 0x1E };
    static const uint8_t nw_MAJOR[]         = { 0x18, 0x38, 0x0C, 0x33, 0x06 };
    static const uint8_t nw_REVEREND[]      = { 0x1D, 0x38, 0x02, 0x24, 0x06, 0x05, 0x19, 0x2F };
    //    Org
    static const uint8_t nw_DEPARTMENT[]    = { 0x2F, 0x04, 0x2C, 0x38, 0x08, 0x1D, 0x2E, 0x18, 0x05, 0x19, 0x2E };
    static const uint8_t nw_INCORPORATED[]  = { 0x39, 0x01, 0x19, 0x30, 0x38, 0x09, 0x1D, 0x2C, 0x06, 0x39, 0x0C, 0x2E, 0x04, 0x2F };
    static const uint8_t nw_CORPORATION[]   = { 0x30, 0x39, 0x09, 0x1D, 0x2C, 0x06, 0x38, 0x0C, 0x29, 0x05, 0x19 };
    static const uint8_t nw_GOVERNMENT[]    = { 0x31, 0x38, 0x07, 0x24, 0x06, 0x18, 0x05, 0x19, 0x2E };
    static const uint8_t nw_DIVISION[]      = { 0x2F, 0x04, 0x24, 0x38, 0x01, 0x2A, 0x05, 0x19 };
    static const uint8_t nw_INTERNATIONAL[] = { 0x39, 0x01, 0x19, 0x2E, 0x06, 0x19, 0x38, 0x03, 0x29, 0x05, 0x19, 0x05, 0x1E };
    static const uint8_t nw_NATIONAL[]      = { 0x19, 0x38, 0x03, 0x29, 0x05, 0x19, 0x05, 0x1E };
    static const uint8_t nw_ASSOCIATION[]   = { 0x05, 0x27, 0x39, 0x10, 0x27, 0x00, 0x38, 0x0C, 0x29, 0x05, 0x19 };
    static const uint8_t nw_ADMINISTRATION[]= { 0x03, 0x2F, 0x18, 0x39, 0x01, 0x19, 0x04, 0x27, 0x2E, 0x1D, 0x38, 0x0C, 0x29, 0x05, 0x19 };
    static const uint8_t nw_ASSISTANT[]     = { 0x05, 0x27, 0x38, 0x01, 0x27, 0x2E, 0x05, 0x19, 0x2E };
    static const uint8_t nw_MANAGER[]       = { 0x18, 0x38, 0x03, 0x19, 0x05, 0x33, 0x06 };
    static const uint8_t nw_DIRECTOR[]      = { 0x2F, 0x06, 0x38, 0x02, 0x30, 0x2E, 0x06 };
    //    Months
    static const uint8_t nw_JANUARY[]       = { 0x33, 0x38, 0x03, 0x19, 0x1C, 0x0B, 0x39, 0x02, 0x1D, 0x00 };
    static const uint8_t nw_FEBRUARY[]      = { 0x23, 0x38, 0x02, 0x2D, 0x1C, 0x05, 0x1B, 0x39, 0x02, 0x1D, 0x00 };
    static const uint8_t nw_MARCH[]         = { 0x18, 0x38, 0x08, 0x1D, 0x32 };
    static const uint8_t nw_APRIL[]         = { 0x38, 0x0C, 0x2C, 0x1D, 0x05, 0x1E };
    static const uint8_t nw_JUNE[]          = { 0x33, 0x38, 0x0B, 0x19 };
    static const uint8_t nw_JULY[]          = { 0x33, 0x39, 0x0B, 0x1E, 0x38, 0x0D };
    static const uint8_t nw_AUGUST[]        = { 0x38, 0x08, 0x31, 0x05, 0x27, 0x2E };
    static const uint8_t nw_SEPTEMBER[]     = { 0x27, 0x02, 0x2C, 0x2E, 0x38, 0x02, 0x18, 0x2D, 0x06 };
    static const uint8_t nw_OCTOBER[]       = { 0x08, 0x30, 0x2E, 0x38, 0x10, 0x2D, 0x06 };
    static const uint8_t nw_NOVEMBER[]      = { 0x19, 0x10, 0x24, 0x38, 0x02, 0x18, 0x2D, 0x06 };
    static const uint8_t nw_DECEMBER[]      = { 0x2F, 0x04, 0x27, 0x38, 0x02, 0x18, 0x2D, 0x06 };

    #define NW(key, arr) { key, arr, sizeof(arr) }
    static const NormWordEntry NormalizationWordsTableArr[] = {
        //    Digits
        NW("ZERO",          nw_ZERO),
        NW("ONE",           nw_ONE),
        NW("TWO",           nw_TWO),
        NW("THREE",         nw_THREE),
        NW("FOUR",          nw_FOUR),
        NW("FIVE",          nw_FIVE),
        NW("SIX",           nw_SIX),
        NW("SEVEN",         nw_SEVEN),
        NW("EIGHT",         nw_EIGHT),
        NW("NINE",          nw_NINE),
        //    Teens
        NW("TEN",           nw_TEN),
        NW("ELEVEN",        nw_ELEVEN),
        NW("TWELVE",        nw_TWELVE),
        NW("THIRTEEN",      nw_THIRTEEN),
        NW("FOURTEEN",      nw_FOURTEEN),
        NW("FIFTEEN",       nw_FIFTEEN),
        NW("SIXTEEN",       nw_SIXTEEN),
        NW("SEVENTEEN",     nw_SEVENTEEN),
        NW("EIGHTEEN",      nw_EIGHTEEN),
        NW("NINETEEN",      nw_NINETEEN),
        //    Tens
        NW("TWENTY",        nw_TWENTY),
        NW("THIRTY",        nw_THIRTY),
        NW("FORTY",         nw_FORTY),
        NW("FIFTY",         nw_FIFTY),
        NW("SIXTY",         nw_SIXTY),
        NW("SEVENTY",       nw_SEVENTY),
        NW("EIGHTY",        nw_EIGHTY),
        NW("NINETY",        nw_NINETY),
        //    Large / misc number
        NW("HUNDRED",       nw_HUNDRED),
        NW("THOUSAND",      nw_THOUSAND),
        NW("MILLION",       nw_MILLION),
        NW("BILLION",       nw_BILLION),
        NW("OH",            nw_OH),
        NW("POINT",         nw_POINT),
        NW("AND",           nw_AND),
        //    Currency / percent
        NW("DOLLAR",        nw_DOLLAR),
        NW("DOLLARS",       nw_DOLLARS),
        NW("CENT",          nw_CENT),
        NW("CENTS",         nw_CENTS),
        NW("PERCENT",       nw_PERCENT),
        //    Ordinals
        NW("ZEROTH",        nw_ZEROTH),
        NW("FIRST",         nw_FIRST),
        NW("SECOND",        nw_SECOND),
        NW("THIRD",         nw_THIRD),
        NW("FOURTH",        nw_FOURTH),
        NW("FIFTH",         nw_FIFTH),
        NW("SIXTH",         nw_SIXTH),
        NW("SEVENTH",       nw_SEVENTH),
        NW("EIGHTH",        nw_EIGHTH),
        NW("NINTH",         nw_NINTH),
        NW("TENTH",         nw_TENTH),
        NW("ELEVENTH",      nw_ELEVENTH),
        NW("TWELFTH",       nw_TWELFTH),
        NW("THIRTEENTH",    nw_THIRTEENTH),
        NW("FOURTEENTH",    nw_FOURTEENTH),
        NW("FIFTEENTH",     nw_FIFTEENTH),
        NW("SIXTEENTH",     nw_SIXTEENTH),
        NW("SEVENTEENTH",   nw_SEVENTEENTH),
        NW("EIGHTEENTH",    nw_EIGHTEENTH),
        NW("NINETEENTH",    nw_NINETEENTH),
        NW("TWENTIETH",     nw_TWENTIETH),
        NW("THIRTIETH",     nw_THIRTIETH),
        NW("FORTIETH",      nw_FORTIETH),
        NW("FIFTIETH",      nw_FIFTIETH),
        NW("SIXTIETH",      nw_SIXTIETH),
        NW("SEVENTIETH",    nw_SEVENTIETH),
        NW("EIGHTIETH",     nw_EIGHTIETH),
        NW("NINETIETH",     nw_NINETIETH),
        //    Letter names (used by dotted abbreviation expansions)
        NW("AY",            nw_AY),
        NW("BEE",           nw_BEE),
        NW("SEE",           nw_SEE),
        NW("DEE",           nw_DEE),
        NW("EF",            nw_EF),
        NW("EM",            nw_EM),
        NW("PEE",           nw_PEE),
        //    Dotted abbreviation expansions
        NW("THAT",          nw_THAT),
        NW("IS",            nw_IS),
        NW("FOR",           nw_FOR),
        NW("EXAMPLE",       nw_EXAMPLE),
        NW("POSTSCRIPT",    nw_POSTSCRIPT),
        NW("WITH",          nw_WITH),
        NW("REGARD",        nw_REGARD),
        NW("TO",            nw_TO),
        //    Titles
        NW("DOCTOR",        nw_DOCTOR),
        NW("MISTER",        nw_MISTER),
        NW("MISSUS",        nw_MISSUS),
        NW("MISS",          nw_MISS),
        NW("PROFESSOR",     nw_PROFESSOR),
        NW("JUNIOR",        nw_JUNIOR),
        NW("SENIOR",        nw_SENIOR),
        //    Common abbreviation expansions
        NW("VERSUS",        nw_VERSUS),
        NW("ETCETERA",      nw_ETCETERA),
        NW("APPROXIMATELY", nw_APPROXIMATELY),
        NW("MAXIMUM",       nw_MAXIMUM),
        NW("MINIMUM",       nw_MINIMUM),
        NW("AVERAGE",       nw_AVERAGE),
        NW("VOLUME",        nw_VOLUME),
        NW("FIGURE",        nw_FIGURE),
        NW("REFERENCE",     nw_REFERENCE),
        NW("ESTABLISHED",   nw_ESTABLISHED),
        NW("CONTINUED",     nw_CONTINUED),
        NW("ABBREVIATION",  nw_ABBREVIATION),
        NW("ATTRIBUTED",    nw_ATTRIBUTED),
        NW("DISTRICT",      nw_DISTRICT),
        NW("POPULATION",    nw_POPULATION),
        NW("TEMPERATURE",   nw_TEMPERATURE),
        NW("TECHNICAL",     nw_TECHNICAL),
        NW("ELECTRIC",      nw_ELECTRIC),
        //    Address
        NW("STREET",        nw_STREET),
        NW("AVENUE",        nw_AVENUE),
        NW("BOULEVARD",     nw_BOULEVARD),
        NW("ROAD",          nw_ROAD),
        NW("LANE",          nw_LANE),
        //    Military
        NW("LIEUTENANT",    nw_LIEUTENANT),
        NW("CAPTAIN",       nw_CAPTAIN),
        NW("GENERAL",       nw_GENERAL),
        NW("SERGEANT",      nw_SERGEANT),
        NW("PRIVATE",       nw_PRIVATE),
        NW("COLONEL",       nw_COLONEL),
        NW("MAJOR",         nw_MAJOR),
        NW("REVEREND",      nw_REVEREND),
        //    Org
        NW("DEPARTMENT",    nw_DEPARTMENT),
        NW("INCORPORATED",  nw_INCORPORATED),
        NW("CORPORATION",   nw_CORPORATION),
        NW("GOVERNMENT",    nw_GOVERNMENT),
        NW("DIVISION",      nw_DIVISION),
        NW("INTERNATIONAL", nw_INTERNATIONAL),
        NW("NATIONAL",      nw_NATIONAL),
        NW("ASSOCIATION",   nw_ASSOCIATION),
        NW("ADMINISTRATION",nw_ADMINISTRATION),
        NW("ASSISTANT",     nw_ASSISTANT),
        NW("MANAGER",       nw_MANAGER),
        NW("DIRECTOR",      nw_DIRECTOR),
        //    Months
        NW("JANUARY",       nw_JANUARY),
        NW("FEBRUARY",      nw_FEBRUARY),
        NW("MARCH",         nw_MARCH),
        NW("APRIL",         nw_APRIL),
        NW("JUNE",          nw_JUNE),
        NW("JULY",          nw_JULY),
        NW("AUGUST",        nw_AUGUST),
        NW("SEPTEMBER",     nw_SEPTEMBER),
        NW("OCTOBER",       nw_OCTOBER),
        NW("NOVEMBER",      nw_NOVEMBER),
        NW("DECEMBER",      nw_DECEMBER),
    };
    #undef NW

    static std::unordered_map<std::string, std::pair<const uint8_t*, size_t>> BuildNormWordMap() {
        std::unordered_map<std::string, std::pair<const uint8_t*, size_t>> m;
        for (const auto& e : NormalizationWordsTableArr) {
            m[e.key] = { e.data, e.len };
        }
        return m;
    }
    static const std::unordered_map<std::string, std::pair<const uint8_t*, size_t>> NormalizationWordsTable
        = BuildNormWordMap();

    static const char* const DigitNames[]  = { "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9" };
    static const char* const TeenNames[]   = { "10", "11", "12", "13", "14", "15", "16", "17", "18", "19" };
    static const char* const TensNames[]   = { "",   "",   "20", "30", "40", "50", "60", "70", "80", "90" };

    // -------------------------------------------------------------------------
    // Normalizer implementation
    // -------------------------------------------------------------------------

    // Longest dotted abbreviations first so "w.r.t." doesn't shadow a shorter prefix.
    static const char* const DottedAbbrevKeys[] = {
        "w.r.t.", "i.e.", "e.g.", "a.m.", "p.m.", "p.s.", "b.c.", "a.d."
    };

    struct StrPair { const char* key; const char* val; };
    static const StrPair DottedAbbreviationMapTable[] = {
        { "i.e.",   "that is"        },
        { "e.g.",   "for example"    },
        { "a.m.",   "ay em"          },
        { "p.m.",   "pee em"         },
        { "p.s.",   "postscript"     },
        { "w.r.t.", "with regard to" },
        { "b.c.",   "bee see"        },
        { "a.d.",   "ay dee"         },
    };

    static const StrPair AbbreviationMapTable[] = {
        // Titles
        { "Dr",    "Doctor"         },
        { "Mr",    "Mister"         },
        { "Mrs",   "Missus"         },
        { "Ms",    "Miss"           },
        { "Prof",  "Professor"      },
        { "Jr",    "Junior"         },
        { "Sr",    "Senior"         },
        // Common
        { "Vs",    "versus"         },
        { "Etc",   "etcetera"       },
        { "Approx","approximately"  },
        { "Max",   "maximum"        },
        { "Min",   "minimum"        },
        { "Avg",   "average"        },
        { "Vol",   "volume"         },
        { "Fig",   "figure"         },
        { "Ref",   "reference"      },
        { "Est",   "established"    },
        { "Cont",  "continued"      },
        { "Abbr",  "abbreviation"   },
        { "Attr",  "attributed"     },
        { "Dist",  "district"       },
        { "Pop",   "population"     },
        { "Temp",  "temperature"    },
        { "Tech",  "technical"      },
        { "Elec",  "electric"       },
        // Addresses
        { "St",    "Street"         },
        { "Ave",   "Avenue"         },
        { "Blvd",  "Boulevard"      },
        { "Rd",    "Road"           },
        { "Ln",    "Lane"           },
        // Military / ranks
        { "Lt",    "Lieutenant"     },
        { "Cpt",   "Captain"        },
        { "Capt",  "Captain"        },
        { "Gen",   "General"        },
        { "Sgt",   "Sergeant"       },
        { "Pvt",   "Private"        },
        { "Col",   "Colonel"        },
        { "Maj",   "Major"          },
        { "Rev",   "Reverend"       },
        // Org
        { "Dept",  "Department"     },
        { "Inc",   "Incorporated"   },
        { "Corp",  "Corporation"    },
        { "Govt",  "government"     },
        { "Div",   "division"       },
        { "Intl",  "international"  },
        { "Natl",  "national"       },
        { "Assoc", "association"    },
        { "Admin", "administration" },
        { "Asst",  "assistant"      },
        { "Mgr",   "manager"        },
        { "Dir",   "director"       },
        // Months
        { "Jan",   "January"        },
        { "Feb",   "February"       },
        { "Mar",   "March"          },
        { "Apr",   "April"          },
        { "Jun",   "June"           },
        { "Jul",   "July"           },
        { "Aug",   "August"         },
        { "Sep",   "September"      },
        { "Sept",  "September"      },
        { "Oct",   "October"        },
        { "Nov",   "November"       },
        { "Dec",   "December"       },
    };

    static const char* const DigitWordsTable[] = {
        "zero","one","two","three","four","five","six","seven","eight","nine"
    };
    static const char* const TeenWordsTable[] = {
        "ten","eleven","twelve","thirteen","fourteen","fifteen",
        "sixteen","seventeen","eighteen","nineteen"
    };
    static const char* const OnesOrdinalTable[] = {
        "zeroth","first","second","third","fourth","fifth","sixth","seventh",
        "eighth","ninth","tenth","eleventh","twelfth","thirteenth","fourteenth",
        "fifteenth","sixteenth","seventeenth","eighteenth","nineteenth",
    };
    static const char* const TensOrdinalTable[] = {
        "","","twentieth","thirtieth","fortieth","fiftieth",
        "sixtieth","seventieth","eightieth","ninetieth"
    };
    static const char* const TensWordsTable[] = {
        "","","twenty","thirty","forty","fifty","sixty","seventy","eighty","ninety"
    };

    std::string Phonemizer::Normalizer::SmallCardinal(int32_t n) {
        if (n == 0) {
            return "zero";
        }
        if (n < 10) {
            return DigitWordsTable[n];
        }
        if (n < 20) {
            return TeenWordsTable[n - 10];
        }
        int32_t t = n / 10, o = n % 10;
        std::string result = TensWordsTable[t];
        if (o > 0) {
            result += " ";
            result += DigitWordsTable[o];
        }
        return result;
    }

    std::string Phonemizer::Normalizer::YearToWords(int32_t y) {
        int32_t hi = y / 100;
        int32_t lo = y % 100;
        if (y == 2000) {
            return "two thousand";
        }
        if (y > 2000 && y < 2010) {
            return "two thousand " + SmallCardinal(lo);
        }
        std::string hiPart = SmallCardinal(hi);
        if (lo == 0) {
            return hiPart + " hundred";
        }
        if (lo < 10) {
            return hiPart + " oh " + SmallCardinal(lo);
        }
        return hiPart + " " + SmallCardinal(lo);
    }

    std::string Phonemizer::Normalizer::OrdinalToWord(int64_t n) {
        if (n < 0) {
            return std::to_string(n);
        }
        if (n < 20) {
            return OnesOrdinalTable[n];
        }
        if (n < 100) {
            int32_t t = (int32_t)(n / 10), o = (int32_t)(n % 10);
            if (o == 0) {
                return TensOrdinalTable[t];
            }
            return std::string(TensWordsTable[t]) + " " + OnesOrdinalTable[o];
        }
        return std::to_string(n); // cardinal fallback for 100+ (rare as ordinal)
    }

    std::string Phonemizer::Normalizer::SplitCamelCase(const std::string& text) {
        if (text.size() < 2) {
            return text;
        }
        std::string result;
        result.reserve(text.size() + 4);
        for (size_t i = 0; i < text.size(); i++) {
            char c = text[i];
            if (i > 0 && std::isupper((unsigned char)c)) {
                char prev = text[i - 1];
                bool nextLower = (i + 1 < text.size()) && std::islower((unsigned char)text[i + 1]);
                if (std::islower((unsigned char)prev) || (std::isupper((unsigned char)prev) && nextLower)) {
                    result += ' ';
                }
            }
            result += c;
        }
        return result;
    }

    std::string Phonemizer::Normalizer::ReplaceCurrency(const std::string& text) {
        std::string result;
        result.reserve(text.size() + 16);
        size_t i = 0;
        while (i < text.size()) {
            if (text[i] != '$') { result += text[i++]; continue; }
            size_t start = i++;
            while (i < text.size() && std::isspace((unsigned char)text[i])) { i++; }
            if (i >= text.size() || !std::isdigit((unsigned char)text[i])) {
                result += '$';
                i = start + 1;
                continue;
            }
            size_t dolStart = i;
            while (i < text.size() && std::isdigit((unsigned char)text[i])) { i++; }
            int64_t dollars = std::stoll(text.substr(dolStart, i - dolStart));
            result += std::to_string(dollars);
            result += (dollars == 1) ? " dollar" : " dollars";
            if (i + 1 < text.size() && text[i] == '.' && std::isdigit((unsigned char)text[i + 1])) {
                i++;
                size_t centsStart = i;
                size_t centsLen = 0;
                while (i < text.size() && std::isdigit((unsigned char)text[i]) && centsLen < 2) {
                    i++; centsLen++;
                }
                std::string centsStr = text.substr(centsStart, centsLen);
                while (centsStr.size() < 2) { centsStr += '0'; }
                centsStr = centsStr.substr(0, 2);
                int64_t cents = std::stoll(centsStr);
                if (cents > 0) {
                    result += " and ";
                    result += std::to_string(cents);
                    result += (cents == 1) ? " cent" : " cents";
                }
            }
        }
        return result;
    }

    std::string Phonemizer::Normalizer::ReplacePercent(const std::string& text) {
        std::string result;
        result.reserve(text.size() + 8);
        size_t i = 0;
        while (i < text.size()) {
            if (!std::isdigit((unsigned char)text[i])) { result += text[i++]; continue; }
            size_t numStart = i;
            while (i < text.size() && std::isdigit((unsigned char)text[i])) { i++; }
            size_t numEnd = i;
            size_t ws = i;
            while (ws < text.size() && std::isspace((unsigned char)text[ws])) { ws++; }
            if (ws < text.size() && text[ws] == '%') {
                result += text.substr(numStart, numEnd - numStart);
                result += " percent";
                i = ws + 1;
            } else {
                result += text.substr(numStart, numEnd - numStart);
            }
        }
        return result;
    }

    std::string Phonemizer::Normalizer::ReplaceOrdinals(const std::string& text) {
        std::string result;
        result.reserve(text.size());
        size_t i = 0;
        while (i < text.size()) {
            if (!std::isdigit((unsigned char)text[i]) ||
                    (i > 0 && (std::isalnum((unsigned char)text[i - 1]) || text[i - 1] == '_'))) {
                result += text[i++];
                continue;
            }
            size_t numStart = i;
            while (i < text.size() && std::isdigit((unsigned char)text[i])) { i++; }
            size_t numEnd = i;
            size_t ws = i;
            while (ws < text.size() && std::isspace((unsigned char)text[ws])) { ws++; }
            bool isOrdinal = false;
            size_t afterSuffix = ws + 2;
            if (ws + 2 <= text.size()) {
                char s0 = (char)std::tolower((unsigned char)text[ws]);
                char s1 = (char)std::tolower((unsigned char)text[ws + 1]);
                bool hasSuffix = (s0 == 's' && s1 == 't') || (s0 == 'n' && s1 == 'd') ||
                                 (s0 == 'r' && s1 == 'd') || (s0 == 't' && s1 == 'h');
                if (hasSuffix && (afterSuffix >= text.size() || !std::isalnum((unsigned char)text[afterSuffix]))) {
                    isOrdinal = true;
                }
            }
            if (isOrdinal) {
                int64_t n = std::stoll(text.substr(numStart, numEnd - numStart));
                result += OrdinalToWord(n);
                i = afterSuffix;
            } else {
                result += text.substr(numStart, numEnd - numStart);
            }
        }
        return result;
    }

    bool Phonemizer::Normalizer::IsOrdinalSuffixAt(const std::string& text, int32_t pos) {
        if (pos + 2 > (int32_t)text.size()) {
            return false;
        }
        char s0 = (char)std::tolower((unsigned char)text[pos]);
        char s1 = (char)std::tolower((unsigned char)text[pos + 1]);
        bool isSuffix = (s0 == 's' && s1 == 't') || (s0 == 'n' && s1 == 'd') ||
                        (s0 == 'r' && s1 == 'd') || (s0 == 't' && s1 == 'h');
        return isSuffix && (pos + 2 >= (int32_t)text.size() || !std::isalnum((unsigned char)text[pos + 2]));
    }

    std::string Phonemizer::Normalizer::ReplaceYears(const std::string& text) {
        std::string result;
        result.reserve(text.size() + 16);
        size_t i = 0;
        while (i < text.size()) {
            if (!std::isdigit((unsigned char)text[i])) { result += text[i++]; continue; }
            size_t runStart = i;
            while (i < text.size() && std::isdigit((unsigned char)text[i])) { i++; }
            size_t runLen = i - runStart;
            bool asYear = false;
            if (runLen == 4) {
                char d0 = text[runStart], d1 = text[runStart + 1];
                bool inRange = d0 == '1' || (d0 == '2' && d1 == '0');
                if (inRange) {
                    bool prevOk = (runStart == 0) ||
                        (text[runStart - 1] != '.' && text[runStart - 1] != '$' &&
                         text[runStart - 1] != static_cast<char>(0xE2) && // euro/pound handled differently
                         !std::isalnum((unsigned char)text[runStart - 1]) && text[runStart - 1] != '_');
                    size_t la = i;
                    while (la < text.size() && std::isspace((unsigned char)text[la])) { la++; }
                    bool blocked = (la < text.size()) &&
                        (std::isdigit((unsigned char)text[la]) || text[la] == '%' ||
                         IsOrdinalSuffixAt(text, (int32_t)la));
                    asYear = prevOk && !blocked;
                }
            }
            if (asYear) {
                int32_t yr = std::stoi(text.substr(runStart, 4));
                result += YearToWords(yr);
            } else {
                result += text.substr(runStart, runLen);
            }
        }
        return result;
    }

    std::string Phonemizer::Normalizer::ReplaceDecimals(const std::string& text) {
        std::string result;
        result.reserve(text.size() + 16);
        size_t i = 0;
        while (i < text.size()) {
            if (!std::isdigit((unsigned char)text[i]) ||
                    (i > 0 && (std::isalnum((unsigned char)text[i - 1]) || text[i - 1] == '_'))) {
                result += text[i++];
                continue;
            }
            size_t numStart = i;
            while (i < text.size() && std::isdigit((unsigned char)text[i])) { i++; }
            if (i + 1 < text.size() && text[i] == '.' && std::isdigit((unsigned char)text[i + 1])) {
                size_t fracStart = i + 1;
                size_t j = fracStart;
                while (j < text.size() && std::isdigit((unsigned char)text[j])) { j++; }
                if (j >= text.size() || !std::isalnum((unsigned char)text[j])) {
                    result += text.substr(numStart, i - numStart);
                    result += " point";
                    for (size_t k = fracStart; k < j; k++) {
                        result += ' ';
                        result += DigitWordsTable[text[k] - '0'];
                    }
                    i = j;
                    continue;
                }
            }
            result += text.substr(numStart, i - numStart);
        }
        return result;
    }

    std::string Phonemizer::Normalizer::ReplaceDottedAbbrevs(const std::string& text) {
        std::string result;
        result.reserve(text.size() + 16);
        size_t i = 0;
        while (i < text.size()) {
            bool prevIsWord = (i > 0) && (std::isalnum((unsigned char)text[i - 1]) || text[i - 1] == '_');
            if (!prevIsWord) {
                bool found = false;
                for (const char* abbr : DottedAbbrevKeys) {
                    size_t abbrLen = strlen(abbr);
                    if (i + abbrLen <= text.size() &&
                            strncasecmp(text.c_str() + i, abbr, abbrLen) == 0) {
                        size_t end = i + abbrLen;
                        if (end >= text.size() || !(std::isalnum((unsigned char)text[end]) || text[end] == '_')) {
                            // Look up in DottedAbbreviationMapTable
                            for (const auto& kv : DottedAbbreviationMapTable) {
                                if (strncasecmp(abbr, kv.key, abbrLen) == 0) {
                                    result += kv.val;
                                    break;
                                }
                            }
                            i = end;
                            found = true;
                            break;
                        }
                    }
                }
                if (found) { continue; }
            }
            result += text[i++];
        }
        return result;
    }

    std::string Phonemizer::Normalizer::ReplaceAbbrevs(const std::string& text) {
        std::string result;
        result.reserve(text.size() + 32);
        size_t i = 0;
        while (i < text.size()) {
            bool prevIsWord = (i > 0) && (std::isalnum((unsigned char)text[i - 1]) || text[i - 1] == '_');
            if (!prevIsWord && std::isalpha((unsigned char)text[i])) {
                size_t wordStart = i;
                while (i < text.size() && std::isalpha((unsigned char)text[i])) { i++; }
                if (i < text.size() && text[i] == '.') {
                    std::string word = text.substr(wordStart, i - wordStart);
                    // Case-insensitive lookup in AbbreviationMapTable
                    bool found = false;
                    for (const auto& kv : AbbreviationMapTable) {
                        if (strcasecmp(word.c_str(), kv.key) == 0) {
                            result += kv.val;
                            i++;
                            found = true;
                            break;
                        }
                    }
                    if (found) { continue; }
                }
                result += text.substr(wordStart, i - wordStart);
                continue;
            }
            result += text[i++];
        }
        return result;
    }

    std::string Phonemizer::Normalizer::Normalize(const std::string& text) {
        std::string t = text;

        // 0. Split CamelCase/PascalCase so "SharpVox" -> "Sharp Talk"
        t = SplitCamelCase(t);

        // 10. Parentheses -> comma-separated pauses
        {
            static const std::regex reParentheses("\\s*\\(([^)]+)\\)");
            t = std::regex_replace(t, reParentheses, ", $1, ");
        }

        // 1. Currency - before decimal so $3.99 isn't split at the dot
        t = ReplaceCurrency(t);

        // 2. Percentages
        t = ReplacePercent(t);

        // 3. Ordinals - before decimals to avoid "1.5th" oddities
        t = ReplaceOrdinals(t);

        // 4. Years - 4-digit numbers read as pairs ("nineteen eighty-four")
        t = ReplaceYears(t);

        // 5. Decimal numbers - spell each digit after the point individually
        t = ReplaceDecimals(t);

        // 6. Dotted abbreviations (i.e., e.g., a.m. ...) - must run before step 7
        //    so their embedded periods don't trigger sentence splitting.
        t = ReplaceDottedAbbrevs(t);

        // 7. Single-dot abbreviations
        t = ReplaceAbbrevs(t);

        // 8. Em-dash, en-dash, double-hyphen -> sentence break; plain hyphens -> space
        // Replace em-dash (UTF-8: \xe2\x80\x94) and en-dash (UTF-8: \xe2\x80\x93) then '--'
        {
            static const std::regex reEmDash("\xe2\x80\x94");
            static const std::regex reEnDash("\xe2\x80\x93");
            static const std::regex reDblHyph("--");
            t = std::regex_replace(t, reEmDash,  ". ");
            t = std::regex_replace(t, reEnDash,  ". ");
            t = std::regex_replace(t, reDblHyph, ". ");
        }
        for (char& c : t) {
            if (c == '-') { c = ' '; }
        }

        // 9. Expressive reduplication: "hahaha" -> "ha ha ha"
        // Repeated-syllable words: "hahaha" -> "ha ha ha", "lolol" -> "lol ol"
        // Fires for 3+ repetitions of a 1-3 char unit. Rare in real English at that count.
        // Non-greedy {1,3}? so "iiiiiiiii" splits on "i" not "iii".
        {
            static const std::regex reReduplicate("\\b([a-zA-Z]{1,3}?)\\1{2,}\\b");
            std::string out;
            out.reserve(t.size());
            std::sregex_iterator it(t.begin(), t.end(), reReduplicate);
            std::sregex_iterator end;
            size_t lastPos = 0;
            for (; it != end; ++it) {
                const std::smatch& m = *it;
                out += t.substr(lastPos, m.position() - lastPos);
                std::string unit = m[1].str();
                std::string whole = m[0].str();
                int32_t count = (int32_t)(whole.size() / unit.size());
                for (int32_t k = 0; k < count; k++) {
                    if (k > 0) { out += ' '; }
                    out += unit;
                }
                lastPos = m.position() + m.length();
            }
            out += t.substr(lastPos);
            t = out;
        }

        return t;
    }

    // -------------------------------------------------------------------------
    // Phonemizer constructor and methods
    // -------------------------------------------------------------------------

    Phonemizer::Phonemizer(const uint8_t* dictData, size_t dictSize,
                           std::function<const uint8_t*(const std::string&, size_t&)> symbolsTable)
        : StatDict(0), StatMorph(0), StatLts(0),
          _dict(dictData, dictSize),
          _symbols(std::move(symbolsTable)),
          LastEndPunct(AudioProcessor::_Period_)
    {}

    std::vector<std::pair<std::vector<PhonemeToken>, int16_t>>
    Phonemizer::TextToSentenceTokens(const std::string& text) {
        std::vector<std::pair<std::vector<PhonemeToken>, int16_t>> result;
        auto segments = EmbeddedCmd::ParseSegments(text);

        static const std::regex TokenRe(
            "(\\d+)|([a-zA-Z]+(?:'[a-zA-Z]+)*)|([,;:])|(\\.\\.\\.|\\.| !|\\?|~)|(\\s+)");

        for (const auto& seg : segments) {
            if (seg.IsCommand()) {
                continue; // handled by TtsEngine, not FrontEnd
            }

            if (seg.IsSinging()) {
                // Each singing block is its own clause  never mix with speech
                if (!seg.singing.empty()) {
                    result.push_back({ seg.singing, 0 });
                }
                continue;
            }

            // Split at sentence boundaries (.!?) and clause boundaries (,;:).
            // Each clause gets its own BackEnd.Process call so pitch resets cleanly.
            std::string plain = Normalizer::Normalize(seg.plainText);
            size_t start = 0;
            std::sregex_iterator it(plain.begin(), plain.end(), TokenRe);
            std::sregex_iterator end;
            for (; it != end; ++it) {
                const std::smatch& m = *it;
                bool hasPunct = m[4].matched || m[3].matched;
                if (!hasPunct) {
                    continue;
                }
                std::string sentence = plain.substr(start, m.position() + m.length() - start);
                auto tokens = TextSegmentToPhonemes(sentence);
                result.push_back({ tokens, LastEndPunct });
                start = m.position() + m.length();
            }
            if (start < plain.size()) {
                std::string remaining = plain.substr(start);
                // Trim
                size_t trimStart = remaining.find_first_not_of(" \t\r\n");
                if (trimStart != std::string::npos) {
                    auto tokens = TextSegmentToPhonemes(remaining);
                    result.push_back({ tokens, LastEndPunct });
                }
            }
        }

        if (result.empty()) {
            auto tokens = TextToPhonemes(text);
            result.push_back({ tokens, LastEndPunct });
        }

        return result;
    }

    // Process a pure-text span (no embedded commands) into phoneme tokens.
    std::vector<PhonemeToken> Phonemizer::TextSegmentToPhonemes(const std::string& text) {
        std::vector<PhonemeToken> tokens;
        LastEndPunct = AudioProcessor::_Period_;

        static const std::regex TokenRe(
            "(\\d+)|([a-zA-Z]+(?:'[a-zA-Z]+)*)|([,;:])|(\\.\\.\\.|\\.| !|\\?|~)|(\\s+)");

        std::string normalized = Normalizer::Normalize(text);

        std::vector<std::string> ctxWords;
        std::sregex_iterator wit(normalized.begin(), normalized.end(), TokenRe);
        std::sregex_iterator wend;
        for (; wit != wend; ++wit) {
            const std::smatch& wm = *wit;
            if (wm[2].matched) {
                std::string w = wm[2].str();
                for (char& c : w) { c = (char)std::toupper((unsigned char)c); }
                ctxWords.push_back(w);
            }
        }
        int32_t wordIdx = 0;

        std::sregex_iterator it(normalized.begin(), normalized.end(), TokenRe);
        std::sregex_iterator end;
        for (; it != end; ++it) {
            const std::smatch& m = *it;
            if (m[1].matched) {
                int64_t n = std::stoll(m[1].str());
                AppendWordTokens(tokens, NumberToPhonStream(n), true);
            } else if (m[2].matched) {
                std::string word = m[2].str();
                std::string upper = word;
                for (char& c : upper) { c = (char)std::toupper((unsigned char)c); }
                auto stream = HeteronymResolver::Resolve(ctxWords, wordIdx);
                if (stream.empty() && IsAllCaps(word) && _dict.Search(upper).empty()) {
                    stream = SpellOutAcronym(upper);
                }
                if (stream.empty()) {
                    stream = WordToPhonStream(upper);
                }
                std::string wordLower = word;
                for (char& c : wordLower) { c = (char)std::tolower((unsigned char)c); }
                AppendWordTokens(tokens, stream,
                    FunctionWordsTable.find(wordLower) == FunctionWordsTable.end(),
                    PronounWordsTable.find(wordLower) != PronounWordsTable.end());
                wordIdx++;
            } else if (m[3].matched) {
                PhonemeToken tok;
                tok.Phon = AudioProcessor::_SIL_;
                tok.Ctrl = AudioProcessor::kTerm_Bound | ((int64_t)AudioProcessor::kBND_Pause << AudioProcessor::kSilenceTypeShift);
                tokens.push_back(tok);
                LastEndPunct = AudioProcessor::_Comma_;
            } else if (m[4].matched) {
                std::string p4 = m[4].str();
                LastEndPunct = (p4 == "...") ? AudioProcessor::_Ellipsis_
                             : (p4 == "?")   ? AudioProcessor::_Quest_
                             : (p4 == "!")   ? AudioProcessor::_Exclam_
                             : (p4 == "~")   ? AudioProcessor::_Tilde_
                             : AudioProcessor::_Period_;
            }
        }

        return tokens;
    }

    std::vector<PhonemeToken> Phonemizer::TextToPhonemes(const std::string& text) {
        std::vector<PhonemeToken> tokens;
        LastEndPunct = AudioProcessor::_Period_;

        // Split into ordered segments (plain text spans interleaved with singing blocks)
        auto segments = EmbeddedCmd::ParseSegments(text);

        static const std::regex TokenRe(
            "(\\d+)|([a-zA-Z]+(?:'[a-zA-Z]+)*)|([,;:])|(\\.\\.\\.|\\.| !|\\?|~)|(\\s+)");

        for (const auto& seg : segments) {
            if (seg.IsCommand()) {
                continue; // handled by TtsEngine, not FrontEnd
            }

            if (seg.IsSinging()) {
                for (const auto& tok : seg.singing) {
                    tokens.push_back(tok);
                }
                continue;
            }

            std::string normalized = Normalizer::Normalize(seg.plainText);

            // Pre-extract word list for heteronym context resolution.
            std::vector<std::string> ctxWords;
            std::sregex_iterator wit(normalized.begin(), normalized.end(), TokenRe);
            std::sregex_iterator wend;
            for (; wit != wend; ++wit) {
                const std::smatch& wm = *wit;
                if (wm[2].matched) {
                    std::string w = wm[2].str();
                    for (char& c : w) { c = (char)std::toupper((unsigned char)c); }
                    ctxWords.push_back(w);
                }
            }
            int32_t wordIdx = 0;

            std::sregex_iterator it(normalized.begin(), normalized.end(), TokenRe);
            std::sregex_iterator end;
            for (; it != end; ++it) {
                const std::smatch& m = *it;
                if (m[1].matched) {           // number
                    int64_t n = std::stoll(m[1].str());
                    AppendWordTokens(tokens, NumberToPhonStream(n), true);
                } else if (m[2].matched) {      // word
                    std::string word = m[2].str();
                    std::string wordLower = word;
                    for (char& c : wordLower) { c = (char)std::tolower((unsigned char)c); }
                    bool isContent = (FunctionWordsTable.find(wordLower) == FunctionWordsTable.end());
                    auto stream = HeteronymResolver::Resolve(ctxWords, wordIdx);
                    if (stream.empty()) {
                        std::string upper = word;
                        for (char& c : upper) { c = (char)std::toupper((unsigned char)c); }
                        stream = WordToPhonStream(upper);
                    }
                    AppendWordTokens(tokens, stream, isContent,
                        PronounWordsTable.find(wordLower) != PronounWordsTable.end());
                    wordIdx++;
                } else if (m[3].matched) {      // , ;
                    PhonemeToken tok;
                    tok.Phon = AudioProcessor::_SIL_;
                    tok.Ctrl = AudioProcessor::kTerm_Bound | ((int64_t)AudioProcessor::kBND_Pause << AudioProcessor::kSilenceTypeShift);
                    tokens.push_back(tok);
                    LastEndPunct = AudioProcessor::_Comma_;
                } else if (m[4].matched) {      // ... . ! ? ~
                    std::string p4 = m[4].str();
                    LastEndPunct = (p4 == "...") ? AudioProcessor::_Ellipsis_
                                 : (p4 == "?")   ? AudioProcessor::_Quest_
                                 : (p4 == "!")   ? AudioProcessor::_Exclam_
                                 : (p4 == "~")   ? AudioProcessor::_Tilde_
                                 : AudioProcessor::_Period_;
                }
                // whitespace: skip
            }
        }

        return tokens;
    }

    // For all-caps words absent from the dict, inject letter phonemes directly
    // no dict lookup, no LTS. Each letter becomes its own word-boundary token.
    std::vector<uint8_t> Phonemizer::SpellOutAcronym(const std::string& upper) {
        std::vector<uint8_t> buf;
        buf.reserve(upper.size() * 4);
        for (char c : upper) {
            if (c < 'A' || c > 'Z') {
                continue;
            }
            buf.push_back(OP_WORD);
            const auto& entry = LetterPhonemesTable[c - 'A'];
            for (size_t k = 0; k < entry.len; k++) {
                buf.push_back(entry.data[k]);
            }
        }
        return buf;
    }

    bool Phonemizer::IsAllCaps(const std::string& word) {
        if (word.size() < 2) {
            return false;
        }
        for (char c : word) {
            if (c < 'A' || c > 'Z') {
                return false;
            }
        }
        return true;
    }

    std::vector<uint8_t> Phonemizer::WordToPhonStream(const std::string& upperWord) {
        // Contractions are stored in the dict without apostrophes ("ISN'T" -> "ISNT").
        std::string lookupWord = upperWord;
        if (lookupWord.find('\'') != std::string::npos) {
            std::string stripped;
            stripped.reserve(lookupWord.size());
            for (char c : lookupWord) {
                if (c != '\'') { stripped += c; }
            }
            lookupWord = stripped;
        }

        // 0. Normalizer word table - bypasses dict entirely
        auto normIt = NormalizationWordsTable.find(upperWord);
        if (normIt != NormalizationWordsTable.end()) {
            const uint8_t* data = normIt->second.first;
            size_t len = normIt->second.second;
            std::vector<uint8_t> nb(len + 1);
            nb[0] = OP_WORD;
            memcpy(nb.data() + 1, data, len);
            return nb;
        }

        // 1. Try dictionary directly
        std::vector<uint8_t> phons = _dict.Search(lookupWord);
        if (!phons.empty()) {
            StatDict++;
        }

        // 2. Try morphological decomposition (suffix stripping + root lookup)
        if (phons.empty()) {
            auto morphResult = Morph::TryDecompose(lookupWord, _dict);
            if (!morphResult.empty()) {
                phons = morphResult;
                StatMorph++;
            }
        }

        // 3. Fall back to letter-to-sound rules
        if (phons.empty()) {
            phons = LetterToSound::Convert(upperWord);
            StatLts++;
        }

        // Prepend OP_WORD marker
        std::vector<uint8_t> buf(phons.size() + 1);
        buf[0] = OP_WORD;
        memcpy(buf.data() + 1, phons.data(), phons.size());
        return buf;
    }

    // Number -> raw phoneme stream

    std::vector<uint8_t> Phonemizer::NumberToPhonStream(int64_t n) {
        std::vector<uint8_t> buf;
        BuildNumberPhons(buf, n);
        return buf;
    }

    void Phonemizer::BuildNumberPhons(std::vector<uint8_t>& buf, int64_t n) {
        // "minus" via billion slot TODO: add MINUS to symbols
        if (n < 0) {
            AppendSymbol(buf, "1E3");
            BuildNumberPhons(buf, -n);
            return;
        }
        if (n == 0) {
            AppendSymbol(buf, "0");
            return;
        }

        if (n >= 1000000000LL) {
            BuildNumberPhons(buf, n / 1000000000LL);
            AppendSymbol(buf, "1E3");  // billion
            n %= 1000000000LL;
        }
        if (n >= 1000000LL) {
            BuildNumberPhons(buf, n / 1000000LL);
            AppendSymbol(buf, "1E2");  // million
            n %= 1000000LL;
        }
        if (n >= 1000LL) {
            BuildNumberPhons(buf, n / 1000LL);
            AppendSymbol(buf, "1E1");  // thousand
            n %= 1000LL;
        }
        if (n >= 100) {
            AppendDigit(buf, (int32_t)(n / 100));
            AppendSymbol(buf, "100");  // hundred
            n %= 100;
        }
        if (n >= 20) {
            AppendTens(buf, (int32_t)(n / 10));
            n %= 10;
            if (n > 0) {
                AppendDigit(buf, (int32_t)n);
            }
        } else if (n >= 10) {
            AppendTeen(buf, (int32_t)n);
        } else if (n > 0) {
            AppendDigit(buf, (int32_t)n);
        }
    }

    void Phonemizer::AppendDigit(std::vector<uint8_t>& buf, int32_t d) {
        AppendSymbol(buf, DigitNames[d]);
    }

    void Phonemizer::AppendTeen(std::vector<uint8_t>& buf, int32_t n) {
        AppendSymbol(buf, TeenNames[n - 10]);
    }

    void Phonemizer::AppendTens(std::vector<uint8_t>& buf, int32_t t) {
        AppendSymbol(buf, TensNames[t]);
    }

    void Phonemizer::AppendSymbol(std::vector<uint8_t>& buf, const std::string& sym) {
        if (buf.empty()) {
            buf.push_back(OP_WORD);
        }
        size_t outLen = 0;
        const uint8_t* phons = _symbols(sym, outLen);
        if (phons == nullptr) {
            return;
        }
        buf.insert(buf.end(), phons, phons + outLen);
    }

    // Stream -> PhonemeToken list

    void Phonemizer::AppendWordTokens(std::vector<PhonemeToken>& tokens, const std::vector<uint8_t>& stream,
                                      bool isContent, bool isPronoun) {
        int64_t pending = 0;
        int64_t persistent = isPronoun ? AudioProcessor::kPronounWord : 0LL;
        size_t startIdx = tokens.size();
        bool hadPrimary = false;

        for (uint8_t b : stream) {
            switch (b) {
                case OP_WORD:
                    pending |= AudioProcessor::kWord_Start;
                    if (isContent) {
                        pending |= AudioProcessor::kContent_Word;
                    }
                    break;
                case OP_STRESS1:
                    // Function words: demote dict primary stress to secondary so they
                    // don't trigger pitch peaks in the BackEnd pitch algorithm.
                    if (isContent) {
                        pending |= AudioProcessor::kPrimaryStress;
                        hadPrimary = true;
                    } else {
                        pending |= AudioProcessor::kSecondaryStress;
                    }
                    break;
                case OP_STRESS2: pending |= AudioProcessor::kSecondaryStress; break;
                case OP_EMPHSTRESS: pending |= AudioProcessor::kEmphaticStress; break;
                case OP_SYLL: pending |= AudioProcessor::kSyllable_Start; break;
                case OP_PREP: pending |= AudioProcessor::kPrep_Start; break;
                case OP_VERB: pending |= AudioProcessor::kVerb_Start; break;
                case OP_COMMA:
                case OP_PERIOD:
                case OP_QUEST:
                case OP_EXCLAM: {
                    PhonemeToken tok;
                    tok.Phon = (int16_t)b;
                    tok.Ctrl = AudioProcessor::kTerm_Bound;
                    tokens.push_back(tok);
                    pending = 0;
                    break;
                }
                default:
                    if (b <= 55) {
                        PhonemeToken tok;
                        tok.Phon = (int16_t)b;
                        tok.Ctrl = pending | persistent;
                        tokens.push_back(tok);
                        pending = 0;
                    }
                    break;
            }
        }

        // Content word with only secondary stress: promote to primary so the pitch
        // algorithm has a peak to work with on words like "how".
        if (isContent && !hadPrimary) {
            for (size_t i = startIdx; i < tokens.size(); i++) {
                if ((tokens[i].Ctrl & AudioProcessor::kSecondaryStress) != 0) {
                    tokens[i].Ctrl = (tokens[i].Ctrl & ~AudioProcessor::kSecondaryStress) | AudioProcessor::kPrimaryStress;
                    break;
                }
            }
        }
    }

}  // namespace SharpVox
