#include "../include/KlattschParser.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

// PhonemeToken, phoneme-ID constants (AudioProcessor::_IY_ etc.), and ctrl-flag constants
// (AudioProcessor::kSingingPhon, AudioProcessor::kWord_Start, etc.) all come from tts_engine.h.
#include "../include/TtsEngine.h"

namespace SharpVox {

// ---------------------------------------------------------------------------
// Persistent state for Klattsch mode
// ---------------------------------------------------------------------------
float KlattschParser::_curF0       = 120.0f;
float KlattschParser::_curRate     = 110.0f;
float KlattschParser::_curScale    = 1.0f;
float KlattschParser::_curVibDepth = 0.0f;
float KlattschParser::_curVibRate  = 5.0f;
float KlattschParser::_curTremDepth = 0.0f;
float KlattschParser::_curTremRate  = 5.0f;
float KlattschParser::_curAsp      = 0.0f;
float KlattschParser::_curTilt     = 0.0f;
float KlattschParser::_curEffort   = 0.5f;

// ---------------------------------------------------------------------------
// Static table definitions
// ---------------------------------------------------------------------------

const std::unordered_map<std::string, float> KlattschParser::PauseDurationTable = {
    { ",", 100.0f }, { ";", 200.0f }, { ".", 300.0f }
};

const std::unordered_map<char, int32_t> KlattschParser::NoteSemitonesTable = {
    { 'C', 0 }, { 'D', 2 }, { 'E', 4 }, { 'F', 5 }, { 'G', 7 }, { 'A', 9 }, { 'B', 11 }
};

// Greek and Cyrillic characters that look identical to Latin letters in most fonts.
// Notation files pasted from score editors or other systems may silently contain them.
// Keys are Unicode code points (char32_t), values are their ASCII replacements.
const std::unordered_map<char32_t, char> KlattschParser::HomoglyphMapTable = {
    // Greek uppercase
    { U'Α', 'A' }, { U'Β', 'B' }, { U'Ε', 'E' }, { U'Η', 'H' },
    { U'Ι', 'I' }, { U'Κ', 'K' }, { U'Μ', 'M' }, { U'Ν', 'N' },
    { U'Ο', 'O' }, { U'Ρ', 'P' }, { U'Τ', 'T' }, { U'Υ', 'Y' },
    { U'Ζ', 'Z' },
    // Cyrillic uppercase
    { U'А', 'A' }, { U'В', 'B' }, { U'С', 'C' }, { U'Е', 'E' },
    { U'Н', 'H' }, { U'К', 'K' }, { U'М', 'M' }, { U'О', 'O' },
    { U'Р', 'P' }, { U'Т', 'T' },
    // Cyrillic lowercase
    { U'а', 'a' }, { U'с', 'c' }, { U'е', 'e' }, { U'о', 'o' },
    { U'р', 'p' },
};

const std::unordered_map<char, std::string> KlattschParser::DirectiveKeyMap = {
    { 'b', "base"        }, { 'r', "rate"        }, { 'p', "pause"       },
    { 's', "scale"       }, { 'v', "vibrato"      }, { 'w', "vibratoRate" },
    { 'm', "tremolo"     }, { 'n', "tremoloRate"  },
    { 'h', "aspiration"  }, { 't', "tilt"         }, { 'g', "effort"      },
};

const std::unordered_set<std::string> KlattschParser::StopPhonemeCodesTable = {
    "P", "B", "T", "D", "K", "G", "CH", "JH"
};

// ---------------------------------------------------------------------------
// PhonemeNamesTable and KlattschToSharpVoxPhonemeTable
// (populated from AudioProcessor constants, mirrors C# dictionaries)
// ---------------------------------------------------------------------------

const std::unordered_map<int16_t, std::string>& KlattschParser::GetPhonemeNamesTable() {
    static const std::unordered_map<int16_t, std::string> t = {
        { AudioProcessor::_IY_, "IY" }, { AudioProcessor::_IH_, "IH" }, { AudioProcessor::_EH_, "EH" }, { AudioProcessor::_AE_, "AE" },
        { AudioProcessor::_AA_, "AA" }, { AudioProcessor::_AO_, "AO" }, { AudioProcessor::_AH_, "AH" }, { AudioProcessor::_UH_, "UH" },
        { AudioProcessor::_UW_, "UW" }, { AudioProcessor::_ER_, "ER" }, { AudioProcessor::_AY_, "AY" }, { AudioProcessor::_AW_, "AW" },
        { AudioProcessor::_EY_, "EY" }, { AudioProcessor::_OW_, "OW" }, { AudioProcessor::_OY_, "OY" },
        { AudioProcessor::_W_,  "W"  }, { AudioProcessor::_Y_,  "Y"  }, { AudioProcessor::_R_,  "R"  }, { AudioProcessor::_L_,  "L"  },
        { AudioProcessor::_M_,  "M"  }, { AudioProcessor::_N_,  "N"  }, { AudioProcessor::_NG_, "NG" }, { AudioProcessor::_HH_, "HH" },
        { AudioProcessor::_F_,  "F"  }, { AudioProcessor::_TH_, "TH" }, { AudioProcessor::_S_,  "S"  }, { AudioProcessor::_SH_, "SH" },
        { AudioProcessor::_V_,  "V"  }, { AudioProcessor::_DH_, "DH" }, { AudioProcessor::_Z_,  "Z"  }, { AudioProcessor::_ZH_, "ZH" },
        { AudioProcessor::_P_,  "P"  }, { AudioProcessor::_B_,  "B"  }, { AudioProcessor::_T_,  "T"  }, { AudioProcessor::_D_,  "D"  },
        { AudioProcessor::_K_,  "K"  }, { AudioProcessor::_G_,  "G"  }, { AudioProcessor::_CH_, "CH" }, { AudioProcessor::_JH_, "JH" },
        { AudioProcessor::_AX_, "AX" }, { AudioProcessor::_IX_, "IX" }, { AudioProcessor::_YU_, "YU" },
        { AudioProcessor::_RX_, "RX" }, { AudioProcessor::_LX_, "LX" }, { AudioProcessor::_EL_, "EL" }, { AudioProcessor::_EN_, "EN" },
        { AudioProcessor::_DX_, "DX" }, { AudioProcessor::_TX_, "TX" },
        { AudioProcessor::_JP_A_, "JP_A" }, { AudioProcessor::_JP_I_, "JP_I" }, { AudioProcessor::_JP_U_, "JP_U" },
        { AudioProcessor::_JP_E_, "JP_E" }, { AudioProcessor::_JP_O_, "JP_O" },
    };
    return t;
}

const std::unordered_map<std::string, int16_t>& KlattschParser::GetKlattschToSharpVoxPhonemeTable() {
    static const std::unordered_map<std::string, int16_t> t = {
        { "IY", AudioProcessor::_IY_ }, { "IH", AudioProcessor::_IH_ }, { "EH", AudioProcessor::_EH_ }, { "AE", AudioProcessor::_AE_ },
        { "AA", AudioProcessor::_AA_ }, { "AO", AudioProcessor::_AO_ }, { "AH", AudioProcessor::_AH_ }, { "UH", AudioProcessor::_UH_ },
        { "UW", AudioProcessor::_UW_ }, { "ER", AudioProcessor::_ER_ }, { "AY", AudioProcessor::_AY_ }, { "AW", AudioProcessor::_AW_ },
        { "EY", AudioProcessor::_EY_ }, { "OW", AudioProcessor::_OW_ }, { "OY", AudioProcessor::_OY_ },
        { "W", AudioProcessor::_W_ }, { "Y", AudioProcessor::_Y_ }, { "R", AudioProcessor::_R_ }, { "L", AudioProcessor::_L_ },
        { "M", AudioProcessor::_M_ }, { "N", AudioProcessor::_N_ }, { "NG", AudioProcessor::_NG_ },
        { "F", AudioProcessor::_F_ }, { "TH", AudioProcessor::_TH_ }, { "S", AudioProcessor::_S_ }, { "SH", AudioProcessor::_SH_ },
        { "V", AudioProcessor::_V_ }, { "DH", AudioProcessor::_DH_ }, { "Z", AudioProcessor::_Z_ }, { "ZH", AudioProcessor::_ZH_ },
        { "HH", AudioProcessor::_HH_ },
        { "P", AudioProcessor::_P_ }, { "B", AudioProcessor::_B_ }, { "T", AudioProcessor::_T_ }, { "D", AudioProcessor::_D_ },
        { "K", AudioProcessor::_K_ }, { "G", AudioProcessor::_G_ }, { "CH", AudioProcessor::_CH_ }, { "JH", AudioProcessor::_JH_ },
        { "AX", AudioProcessor::_AX_ }, { "IX", AudioProcessor::_IX_ }, { "YU", AudioProcessor::_YU_ },
        { "RX", AudioProcessor::_RX_ }, { "LX", AudioProcessor::_LX_ }, { "EL", AudioProcessor::_EL_ }, { "EN", AudioProcessor::_EN_ },
        { "DX", AudioProcessor::_DX_ }, { "TX", AudioProcessor::_TX_ },
        { "_", AudioProcessor::_SIL_ },
        { "A", AudioProcessor::_JP_A_ }, { "I", AudioProcessor::_JP_I_ }, { "U", AudioProcessor::_JP_U_ }, { "E", AudioProcessor::_JP_E_ }, { "O", AudioProcessor::_JP_O_ },
    };
    return t;
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

void KlattschParser::Reset() {
    _curF0        = 120.0f;
    _curRate      = 110.0f;
    _curScale     = 1.0f;
    _curVibDepth  = 0.0f;
    _curVibRate   = 5.0f;
    _curTremDepth = 0.0f;
    _curTremRate  = 5.0f;
    _curAsp       = 0.0f;
    _curTilt      = 0.0f;
    _curEffort    = 0.5f;
}

// ---------------------------------------------------------------------------
// Normalize
// ---------------------------------------------------------------------------

// NFKC-normalize, strip zero-width characters (ZWSP, ZWNJ, ZWJ, WJ, BOM),
// then replace any remaining homoglyphs so downstream parsing sees plain ASCII.
//
// Full Unicode NFKC normalization is complex; in practice Klattsch source is
// ASCII-based, so we do a simplified pass: decode UTF-8 to code points, drop
// zero-width joiners/separators, apply the homoglyph map, then re-encode.
std::string KlattschParser::Normalize(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    size_t i = 0;
    while (i < input.size()) {
        unsigned char byte0 = static_cast<unsigned char>(input[i]);
        char32_t cp = 0;
        size_t seqLen = 1;

        // Decode one UTF-8 code point
        if (byte0 < 0x80) {
            cp = byte0;
            seqLen = 1;
        } else if ((byte0 & 0xE0) == 0xC0 && i + 1 < input.size()) {
            cp = (char32_t)(byte0 & 0x1F) << 6;
            cp |= (static_cast<unsigned char>(input[i + 1]) & 0x3F);
            seqLen = 2;
        } else if ((byte0 & 0xF0) == 0xE0 && i + 2 < input.size()) {
            cp  = (char32_t)(byte0 & 0x0F) << 12;
            cp |= (char32_t)(static_cast<unsigned char>(input[i + 1]) & 0x3F) << 6;
            cp |= (static_cast<unsigned char>(input[i + 2]) & 0x3F);
            seqLen = 3;
        } else if ((byte0 & 0xF8) == 0xF0 && i + 3 < input.size()) {
            cp  = (char32_t)(byte0 & 0x07) << 18;
            cp |= (char32_t)(static_cast<unsigned char>(input[i + 1]) & 0x3F) << 12;
            cp |= (char32_t)(static_cast<unsigned char>(input[i + 2]) & 0x3F) << 6;
            cp |= (static_cast<unsigned char>(input[i + 3]) & 0x3F);
            seqLen = 4;
        } else {
            // Malformed byte: pass through unchanged
            result += input[i++];
            continue;
        }
        i += seqLen;

        // Strip zero-width characters (ZWSP, ZWNJ, ZWJ, WJ, BOM)
        if (cp == 0x200B || cp == 0x200C || cp == 0x200D || cp == 0x2060 || cp == 0xFEFF) {
            continue;
        }

        // Replace homoglyphs with their ASCII equivalents
        auto it = HomoglyphMapTable.find(cp);
        if (it != HomoglyphMapTable.end()) {
            result += it->second;
            continue;
        }

        // Re-encode the code point as UTF-8 (or ASCII for low range)
        if (cp < 0x80) {
            result += static_cast<char>(cp);
        } else if (cp < 0x800) {
            result += static_cast<char>(0xC0 | (cp >> 6));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            result += static_cast<char>(0xE0 | (cp >> 12));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            result += static_cast<char>(0xF0 | (cp >> 18));
            result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// NoteToHz (Klattsch variant, supports negative octave and no-remainder check)
// ---------------------------------------------------------------------------

float KlattschParser::NoteToHz(const std::string& name) {
    if (name.size() < 2) {
        return 0.0f;
    }
    char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    auto it = NoteSemitonesTable.find(letter);
    if (it == NoteSemitonesTable.end()) {
        return 0.0f;
    }
    int32_t i = 1;
    int32_t semiadj = 0;
    if (i < static_cast<int32_t>(name.size()) && name[i] == '#') { semiadj = 1; i++; }
    else if (i < static_cast<int32_t>(name.size()) && name[i] == 'b') { semiadj = -1; i++; }
    if (i >= static_cast<int32_t>(name.size())) {
        return 0.0f;
    }
    bool neg = name[i] == '-';
    if (neg) {
        i++;
    }
    if (i >= static_cast<int32_t>(name.size()) || !std::isdigit(static_cast<unsigned char>(name[i]))) {
        return 0.0f;
    }
    int32_t octave = 0;
    while (i < static_cast<int32_t>(name.size()) && std::isdigit(static_cast<unsigned char>(name[i]))) {
        octave = octave * 10 + (name[i] - '0');
        i++;
    }
    if (i != static_cast<int32_t>(name.size())) {
        return 0.0f;
    }
    if (neg) {
        octave = -octave;
    }
    int32_t semi = it->second + semiadj;
    int32_t midi = (octave + 1) * 12 + semi;
    return static_cast<float>(440.0 * std::pow(2.0, (midi - 69) / 12.0));
}

// ---------------------------------------------------------------------------
// Helper: try to parse a float from a string_view-like region of `part`
// ---------------------------------------------------------------------------

static bool TryParseFloat(const std::string& s, size_t offset, size_t length, float& out) {
    if (length == 0) {
        return false;
    }
    // Use stof with exception handling
    try {
        std::size_t pos = 0;
        std::string sub = s.substr(offset, length);
        float v = std::stof(sub, &pos);
        if (pos != sub.size()) {
            return false;
        }
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// ClassifyPart
// ---------------------------------------------------------------------------

// Classify one whitespace-delimited token from the Klattsch source.
// Tries each shape in priority order: punctuation, bracket directives [KEY=N],
// note names (bC4), compact directives (b120 r+10), phoneme codes (AE IY ...).
// Returns nullptr if the token is syntactically valid but produces no output (e.g. bare "p").
KlattschParser::Token* KlattschParser::ClassifyPart(const std::string& part, Token& out) {
    if (part == "(") {
        out = Token{};
        out.Type = "syllable_open";
        return &out;
    }
    if (part == ")") {
        out = Token{};
        out.Type = "syllable_close";
        return &out;
    }
    {
        auto it = PauseDurationTable.find(part);
        if (it != PauseDurationTable.end()) {
            out = Token{};
            out.Type = "pause";
            out.Ms   = it->second;
            return &out;
        }
    }
    if (part == "!" || part == "'") {
        out = Token{};
        out.Type = "stress_mark";
        return &out;
    }

    // [KEY=value] bracket directive
    if (part.size() >= 5 && part[0] == '[' && part[part.size() - 1] == ']') {
        size_t eq = part.find('=', 1);
        if (eq != std::string::npos && eq < part.size() - 1 && eq > 1) {
            bool keyOk = true;
            for (size_t ki = 1; ki < eq && keyOk; ki++) {
                if (!std::isalnum(static_cast<unsigned char>(part[ki])) && part[ki] != '_') {
                    keyOk = false;
                }
            }
            float bval = 0.0f;
            if (keyOk && TryParseFloat(part, eq + 1, part.size() - eq - 2, bval)) {
                out = Token{};
                out.Type     = "directive";
                out.Key      = part.substr(1, eq - 1);
                out.Value    = bval;
                out.Relative = false;
                return &out;
            }
        }
    }

    // note name directive: b[=]NoteOctave (e.g. bC4, b=A#3)
    if (part.size() >= 3 && (part[0] == 'b' || part[0] == 'B')) {
        size_t npos = 1;
        if (part[npos] == '=') {
            npos++;
        }
        float hz = NoteToHz(part.substr(npos));
        if (hz > 0.0f) {
            out = Token{};
            out.Type     = "directive";
            out.Key      = "base";
            out.Value    = hz;
            out.Relative = false;
            return &out;
        }
    }

    // compact directive: single lowercase key letter + optional [=][+-]number
    if (!part.empty() && part[0] >= 'a' && part[0] <= 'z') {
        auto it = DirectiveKeyMap.find(part[0]);
        if (it != DirectiveKeyMap.end()) {
            const std::string& key = it->second;
            if (part.size() == 1) {
                if (key != "pause") {
                    out = Token{};
                    out.Type  = "directive";
                    out.Key   = key;
                    out.Reset = true;
                    return &out;
                }
                return nullptr; // bare "p" produces no output
            }
            size_t pos = 1;
            bool hasEq = (part[pos] == '=');
            if (hasEq) {
                pos++;
            }
            size_t numStart = pos;
            bool hasSign = (pos < part.size() && (part[pos] == '+' || part[pos] == '-'));
            if (hasSign) {
                pos++;
            }
            if (pos < part.size() && std::isdigit(static_cast<unsigned char>(part[pos]))) {
                while (pos < part.size() && std::isdigit(static_cast<unsigned char>(part[pos]))) {
                    pos++;
                }
                if (pos < part.size() && part[pos] == '.' && pos + 1 < part.size()
                        && std::isdigit(static_cast<unsigned char>(part[pos + 1]))) {
                    pos++;
                    while (pos < part.size() && std::isdigit(static_cast<unsigned char>(part[pos]))) {
                        pos++;
                    }
                }
                if (pos == part.size()) {
                    float value = 0.0f;
                    if (TryParseFloat(part, numStart, part.size() - numStart, value)) {
                        out = Token{};
                        out.Type     = "directive";
                        out.Key      = key;
                        out.Value    = value;
                        out.Relative = !hasEq && hasSign;
                        return &out;
                    }
                }
            }
        }
    }

    // phoneme token: optional [-^] slur, uppercase letters (code), optional ['!] stress,
    // optional pitch delta as (+-N) transient or +-N sticky
    {
        size_t pos = 0;
        bool slurred = (pos < part.size() && (part[pos] == '-' || part[pos] == '^'));
        if (slurred) {
            pos++;
        }
        size_t codeStart = pos;
        while (pos < part.size() && std::isupper(static_cast<unsigned char>(part[pos]))) {
            pos++;
        }
        if (pos > codeStart) {
            std::string code = part.substr(codeStart, pos - codeStart);
            const auto& phonTable = GetKlattschToSharpVoxPhonemeTable();
            if (phonTable.count(code)) {
                bool stressed = (pos < part.size() && (part[pos] == '\'' || part[pos] == '!'));
                if (stressed) {
                    pos++;
                }
                float transientDelta = std::numeric_limits<float>::quiet_NaN();
                float stickyDelta    = std::numeric_limits<float>::quiet_NaN();
                if (pos < part.size() && part[pos] == '(') {
                    pos++;
                    size_t numStart = pos;
                    if (pos < part.size() && (part[pos] == '+' || part[pos] == '-')) {
                        pos++;
                    }
                    while (pos < part.size() && (std::isdigit(static_cast<unsigned char>(part[pos])) || part[pos] == '.')) {
                        pos++;
                    }
                    float tv = 0.0f;
                    if (pos < part.size() && part[pos] == ')' &&
                            TryParseFloat(part, numStart, pos - numStart, tv)) {
                        transientDelta = tv;
                        pos++;
                    }
                } else if (pos < part.size() && (part[pos] == '+' || part[pos] == '-')) {
                    size_t numStart = pos++;
                    while (pos < part.size() && (std::isdigit(static_cast<unsigned char>(part[pos])) || part[pos] == '.')) {
                        pos++;
                    }
                    float sv = 0.0f;
                    if (pos == part.size() && TryParseFloat(part, numStart, pos - numStart, sv)) {
                        stickyDelta = sv;
                    }
                }
                if (pos == part.size()) {
                    float pitchDelta = 0.0f;
                    bool transient   = false;
                    if (!std::isnan(transientDelta)) {
                        pitchDelta = transientDelta;
                        transient  = true;
                    } else if (!std::isnan(stickyDelta)) {
                        pitchDelta = stickyDelta;
                    }
                    out = Token{};
                    out.Type       = "phoneme";
                    out.Code       = code;
                    out.Slurred    = slurred;
                    out.Stressed   = stressed;
                    out.PitchDelta = pitchDelta;
                    out.Transient  = transient;
                    return &out;
                }
            }
        }
    }

    out = Token{};
    out.Type = "unknown";
    out.Text = part;
    return &out;
}

// ---------------------------------------------------------------------------
// Tokenize
// ---------------------------------------------------------------------------

// Scan Klattsch source text into a flat Token list.
// Handles: # line comments, /* block comments (including mid-token),
// syllable-group parens ( ), pause punctuation , ; . and stress marks ! '.
// Stress marks retroactively mark the most recent phoneme token as stressed.
std::vector<KlattschParser::Token> KlattschParser::Tokenize(const std::string& rawInput) {
    std::string source = Normalize(rawInput);
    int32_t len = static_cast<int32_t>(source.size());
    std::vector<Token> tokens;
    int32_t i = 0;

    while (i < len) {
        char c = source[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            i++;
            continue;
        }
        if (c == '#' && (i == 0 || std::isspace(static_cast<unsigned char>(source[i - 1])))) {
            while (i < len && source[i] != '\n') {
                i++;
            }
            continue;
        }
        if (c == '/' && i + 1 < len && source[i + 1] == '*') {
            auto endPos = source.find("*/", i + 2);
            if (endPos == std::string::npos) {
                i = len;
            } else {
                i = static_cast<int32_t>(endPos) + 2;
            }
            continue;
        }

        std::string part;
        while (i < len && !std::isspace(static_cast<unsigned char>(source[i]))) {
            char cur = source[i];
            if (cur == '/' && i + 1 < len && source[i + 1] == '*') {
                auto endPos = source.find("*/", i + 2);
                if (endPos == std::string::npos) {
                    i = len;
                } else {
                    i = static_cast<int32_t>(endPos) + 2;
                }
                continue;
            }
            // ) is a syllable-close token unless it ends a pitch expression (AE(+15)),
            // in which case it always follows a digit. Split it off when it follows anything else.
            if (cur == ')' && !part.empty() && !std::isdigit(static_cast<unsigned char>(part.back()))) {
                break;
            }
            // , ; . are pause tokens, split them off when adjacent to phoneme content.
            // Exception: . inside decimal notation always follows a digit (AE+1.5).
            if ((cur == ',' || cur == ';') && !part.empty()) {
                break;
            }
            if (cur == '.' && !part.empty() && !std::isdigit(static_cast<unsigned char>(part.back()))) {
                break;
            }
            part += cur;
            i++;
        }
        if (part.empty()) {
            continue;
        }

        Token outTok;
        Token* tok = ClassifyPart(part, outTok);
        if (tok == nullptr) {
            continue;
        }

        if (tok->Type == "stress_mark") {
            for (int32_t j = static_cast<int32_t>(tokens.size()) - 1; j >= 0; j--) {
                if (tokens[j].Type == "phoneme") {
                    tokens[j].Stressed = true;
                    break;
                }
            }
            continue;
        }
        tokens.push_back(*tok);
    }
    return tokens;
}

// ---------------------------------------------------------------------------
// EmitPhoneme (private helper used by CompileToTokens)
// ---------------------------------------------------------------------------

void KlattschParser::EmitPhoneme(const Token& t, float durationMs,
                                  bool isStartOfBeat, bool isEndOfBeat,
                                  std::vector<PhonemeToken>& result) {
    const auto& phonTable = GetKlattschToSharpVoxPhonemeTable();
    auto it = phonTable.find(t.Code);
    if (it == phonTable.end()) {
        return;
    }
    int16_t phonId = it->second;

    float startF0 = t.Stressed ? _curF0 + 8.0f : _curF0;
    float endF0   = startF0 + t.PitchDelta;

    // Singing flags. We treat each beat as a word for coarticulation purposes.
    int64_t ctrl = AudioProcessor::kSingingPhon | AudioProcessor::kSingingDuration | AudioProcessor::kContent_Word;
    if (isStartOfBeat && !t.Slurred) {
        ctrl |= AudioProcessor::kWord_Start | AudioProcessor::kSyllable_Start;
    }
    if (isEndOfBeat) {
        ctrl |= AudioProcessor::kWord_End;
    }

    // Positive note (IIR settle) for stable-pitch phonemes, snaps to target fast,
    // avoids linear portamento glide from stale TTS pitch at block start.
    // Negative note (portamento) for pitch-delta phonemes, linear glide to endF0.
    int16_t note = (t.PitchDelta != 0.0f)
        ? static_cast<int16_t>(-endF0)
        : static_cast<int16_t>(startF0);

    auto clampByte = [](float v, float lo, float hi) -> uint8_t {
        if (v < lo) { v = lo; }
        if (v > hi) { v = hi; }
        return static_cast<uint8_t>(v);
    };

    PhonemeToken tok{};
    tok.Phon      = phonId;
    tok.Ctrl      = ctrl;
    tok.UserNote  = note;
    tok.UserDur   = static_cast<int16_t>(std::max(5.0f, durationMs));
    tok.Aspiration = clampByte(_curAsp  * 100.0f, 0.0f, 100.0f);
    tok.Tilt       = clampByte(_curTilt * 100.0f, 0.0f, 100.0f);
    tok.Effort     = clampByte(_curEffort * 100.0f, 0.0f, 100.0f);
    tok.VibDepth   = clampByte(_curVibDepth, 0.0f, 255.0f);
    tok.VibRate    = clampByte(_curVibRate * 10.0f, 0.0f, 255.0f);
    tok.TremDepth  = clampByte(_curTremDepth * 100.0f, 0.0f, 100.0f);
    tok.TremRate   = clampByte(_curTremRate * 10.0f, 0.0f, 255.0f);
    result.push_back(tok);

    if (!t.Transient) {
        _curF0 += t.PitchDelta;
    }
}

// ---------------------------------------------------------------------------
// FlushSyllable (private helper)
// ---------------------------------------------------------------------------

void KlattschParser::FlushSyllable(std::vector<Token>& syllableQueue,
                                    bool& inSyllable,
                                    std::vector<PhonemeToken>& result) {
    if (syllableQueue.empty()) {
        inSyllable = false;
        return;
    }

    // Stops/affricates get a short burst slot; the saved time flows to non-stops.
    // Mirrors Klattsch JS, burstMs = min(stopBurstMs, equalSlot * 0.3).
    float equalSlot = _curRate / static_cast<float>(syllableQueue.size());
    float stopBurst = std::min(25.0f, equalSlot * 0.3f);
    int32_t nStops = 0;
    for (const auto& tok : syllableQueue) {
        if (StopPhonemeCodesTable.count(tok.Code)) {
            nStops++;
        }
    }
    int32_t nOther = static_cast<int32_t>(syllableQueue.size()) - nStops;
    float otherDur = (nOther > 0)
        ? (_curRate - nStops * stopBurst) / static_cast<float>(nOther)
        : equalSlot;

    for (int32_t idx = 0; idx < static_cast<int32_t>(syllableQueue.size()); idx++) {
        const Token& t = syllableQueue[idx];
        float dur = StopPhonemeCodesTable.count(t.Code) ? stopBurst : otherDur;
        EmitPhoneme(t, dur, idx == 0, idx == static_cast<int32_t>(syllableQueue.size()) - 1, result);
    }
    syllableQueue.clear();
    inSyllable = false;
}

// ---------------------------------------------------------------------------
// CompileToTokens
// ---------------------------------------------------------------------------

// Convert a Klattsch token list to PhonemeTokens ready for AudioProcessor.
//
// Directives update persistent state (_curF0, _curRate, etc.) as they are encountered.
// Phonemes outside syllable groups get full beat duration (_curRate ms, x1.5 if stressed).
// Phonemes inside ( ) groups share the beat: stops get a short burst, others split the rest.
// Pauses and p-directives emit SIL tokens with exact millisecond durations.
// A trailing SIL with AudioProcessor::kTerm_End is always appended so AudioProcessor sees a clean clause end.
std::vector<PhonemeToken> KlattschParser::CompileToTokens(const std::vector<Token>& tokens) {
    Reset();
    std::vector<PhonemeToken> result;
    bool inSyllable = false;
    std::vector<Token> syllableQueue;

    for (const auto& t : tokens) {
        if (t.Type == "syllable_open") {
            inSyllable = true;
            continue;
        }
        if (t.Type == "syllable_close") {
            FlushSyllable(syllableQueue, inSyllable, result);
            continue;
        }
        if (t.Type == "pause") {
            FlushSyllable(syllableQueue, inSyllable, result);
            PhonemeToken silTok{};
            silTok.Phon    = AudioProcessor::_SIL_;
            silTok.Ctrl    = AudioProcessor::kSingingPhon | AudioProcessor::kSingingDuration | AudioProcessor::kWord_End;
            silTok.UserDur  = static_cast<int16_t>(t.Ms);
            silTok.UserNote = static_cast<int16_t>(-_curF0);
            result.push_back(silTok);
            continue;
        }
        if (t.Type == "directive") {
            // Update persistent state according to each directive key
            if (t.Key == "base") {
                if (t.Reset)         { _curF0 = 120.0f; }
                else if (t.Relative) { _curF0 += t.Value; }
                else                 { _curF0  = t.Value; }
            } else if (t.Key == "rate") {
                if (t.Reset)         { _curRate = 110.0f; }
                else if (t.Relative) { _curRate += t.Value; }
                else                 { _curRate  = t.Value; }
            } else if (t.Key == "scale") {
                if (t.Reset)         { _curScale = 1.0f; }
                else if (t.Relative) { _curScale += t.Value; }
                else                 { _curScale  = t.Value; }
            } else if (t.Key == "vibrato") {
                if (t.Reset)         { _curVibDepth = 0.0f; }
                else if (t.Relative) { _curVibDepth += t.Value; }
                else                 { _curVibDepth  = t.Value; }
            } else if (t.Key == "vibratoRate") {
                if (t.Reset)         { _curVibRate = 5.0f; }
                else if (t.Relative) { _curVibRate += t.Value; }
                else                 { _curVibRate  = t.Value; }
            } else if (t.Key == "tremolo") {
                if (t.Reset)         { _curTremDepth = 0.0f; }
                else if (t.Relative) { _curTremDepth += t.Value; }
                else                 { _curTremDepth  = t.Value; }
            } else if (t.Key == "tremoloRate") {
                if (t.Reset)         { _curTremRate = 5.0f; }
                else if (t.Relative) { _curTremRate += t.Value; }
                else                 { _curTremRate  = t.Value; }
            } else if (t.Key == "aspiration") {
                if (t.Reset)         { _curAsp = 0.0f; }
                else if (t.Relative) { _curAsp += t.Value; }
                else                 { _curAsp  = t.Value; }
            } else if (t.Key == "tilt") {
                if (t.Reset)         { _curTilt = 0.0f; }
                else if (t.Relative) { _curTilt += t.Value; }
                else                 { _curTilt  = t.Value; }
            } else if (t.Key == "effort") {
                if (t.Reset)         { _curEffort = 0.5f; }
                else if (t.Relative) { _curEffort += t.Value; }
                else                 { _curEffort  = t.Value; }
            } else if (t.Key == "pause") {
                PhonemeToken silTok{};
                silTok.Phon    = AudioProcessor::_SIL_;
                silTok.Ctrl    = AudioProcessor::kSingingPhon | AudioProcessor::kSingingDuration | AudioProcessor::kWord_End;
                silTok.UserDur  = static_cast<int16_t>(std::abs(t.Value));
                silTok.UserNote = static_cast<int16_t>(-_curF0);
                result.push_back(silTok);
            }
            continue;
        }
        if (t.Type == "phoneme") {
            if (inSyllable) {
                syllableQueue.push_back(t);
            } else {
                float phoneDur = t.Stressed ? _curRate * 1.5f : _curRate;
                EmitPhoneme(t, phoneDur, true, true, result);
            }
        }
    }
    if (inSyllable) {
        FlushSyllable(syllableQueue, inSyllable, result);
    }

    // Trailing terminal silence so AudioProcessor sees a clean clause end
    PhonemeToken termSil{};
    termSil.Phon    = AudioProcessor::_SIL_;
    termSil.Ctrl    = AudioProcessor::kSingingPhon | AudioProcessor::kSingingDuration | AudioProcessor::kTerm_End | AudioProcessor::kWord_End;
    termSil.UserDur  = 150;
    termSil.UserNote = static_cast<int16_t>(-_curF0);
    result.push_back(termSil);

    return result;
}

}  // namespace SharpVox
