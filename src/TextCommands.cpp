#include "../include/TextCommands.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// PhonemeToken, phoneme-ID constants, and ctrl-flag constants all come from tts_engine.h.
#include "../include/TtsEngine.h"
#include "../include/Tables.h"
#include "../include/KlattschParser.h"

namespace SharpVox {

// Global Klattsch mode flag
bool EmbeddedCmd::KlattschMode = false;

// ASCII note name (C5, A#4, Bb3) -> Hz using equal temperament (A4 = 440 Hz)
int32_t EmbeddedCmd::NoteNameToHz(const std::string& name) {
    if (name.size() < 2) {
        return 0;
    }
    char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    int32_t semitone;
    switch (letter) {
        case 'C': semitone = 0;  break;
        case 'D': semitone = 2;  break;
        case 'E': semitone = 4;  break;
        case 'F': semitone = 5;  break;
        case 'G': semitone = 7;  break;
        case 'A': semitone = 9;  break;
        case 'B': semitone = 11; break;
        default:  return 0;
    }
    int32_t pos = 1;
    if (pos < static_cast<int32_t>(name.size()) && name[pos] == '#') {
        semitone++;
        pos++;
    } else if (pos < static_cast<int32_t>(name.size()) && name[pos] == 'b') {
        semitone--;
        pos++;
    }
    if (pos >= static_cast<int32_t>(name.size())) {
        return 0;
    }
    // Parse remaining digits as octave number
    for (int32_t k = pos; k < static_cast<int32_t>(name.size()); k++) {
        if (!std::isdigit(static_cast<unsigned char>(name[k]))) {
            return 0;
        }
    }
    int32_t octave = std::stoi(name.substr(pos));
    int32_t midi = 12 * (octave + 1) + semitone;
    return static_cast<int32_t>(std::round(440.0 * std::pow(2.0, (midi - 69) / 12.0)));
}

// Map a lowercase phoneme name to a phoneme ID; returns -1 on failure.
int16_t EmbeddedCmd::MapPhoneme(const std::string& p) {
    // Vowels
    if (p == "iy") { return AudioProcessor::_IY_; }
    if (p == "ih") { return AudioProcessor::_IH_; }
    if (p == "eh") { return AudioProcessor::_EH_; }
    if (p == "ae") { return AudioProcessor::_AE_; }
    if (p == "aa") { return AudioProcessor::_AA_; }
    if (p == "ah") { return AudioProcessor::_AH_; }
    if (p == "ao") { return AudioProcessor::_AO_; }
    if (p == "uh") { return AudioProcessor::_UH_; }
    if (p == "ax") { return AudioProcessor::_AX_; }
    if (p == "er") { return AudioProcessor::_ER_; }
    if (p == "ey") { return AudioProcessor::_EY_; }
    if (p == "ay") { return AudioProcessor::_AY_; }
    if (p == "oy") { return AudioProcessor::_OY_; }
    if (p == "aw") { return AudioProcessor::_AW_; }
    if (p == "ow") { return AudioProcessor::_OW_; }
    if (p == "uw") { return AudioProcessor::_UW_; }
    if (p == "ix") { return AudioProcessor::_IX_; }
    // Single-char vowel shortcuts, "i"->IH, "e"->EH, "a"->AE, "o"->AO, "u"->UW
    // Allows compact notation like "KIT" instead of "KIHT".
    if (p == "i")  { return AudioProcessor::_IH_; }
    if (p == "e")  { return AudioProcessor::_EH_; }
    if (p == "a")  { return AudioProcessor::_AE_; }
    if (p == "o")  { return AudioProcessor::_AO_; }
    if (p == "u")  { return AudioProcessor::_UW_; }
    // Sonorants
    if (p == "w")  { return AudioProcessor::_W_; }
    if (p == "y")  { return AudioProcessor::_Y_; }
    if (p == "r")  { return AudioProcessor::_R_; }
    if (p == "l")  { return AudioProcessor::_L_; }
    // Nasals
    if (p == "m")  { return AudioProcessor::_M_; }
    if (p == "n")  { return AudioProcessor::_N_; }
    if (p == "ng") { return AudioProcessor::_NG_; }
    // Fricatives
    if (p == "hh") { return AudioProcessor::_HH_; }
    if (p == "f")  { return AudioProcessor::_F_; }
    if (p == "v")  { return AudioProcessor::_V_; }
    if (p == "th") { return AudioProcessor::_TH_; }
    if (p == "dh") { return AudioProcessor::_DH_; }
    if (p == "s")  { return AudioProcessor::_S_; }
    if (p == "z")  { return AudioProcessor::_Z_; }
    if (p == "sh") { return AudioProcessor::_SH_; }
    if (p == "zh") { return AudioProcessor::_ZH_; }
    // Stops
    if (p == "p")  { return AudioProcessor::_P_; }
    if (p == "b")  { return AudioProcessor::_B_; }
    if (p == "t")  { return AudioProcessor::_T_; }
    if (p == "d")  { return AudioProcessor::_D_; }
    if (p == "dx") { return AudioProcessor::_DX_; }
    if (p == "k")  { return AudioProcessor::_K_; }
    if (p == "g")  { return AudioProcessor::_G_; }
    // Affricates
    if (p == "ch") { return AudioProcessor::_CH_; }
    if (p == "jh") { return AudioProcessor::_JH_; }
    // Japanese vowels
    if (p == "jp_iy") { return AudioProcessor::_JP_I_; }
    if (p == "jp_eh") { return AudioProcessor::_JP_E_; }
    if (p == "jp_aa") { return AudioProcessor::_JP_A_; }
    if (p == "jp_ow") { return AudioProcessor::_JP_O_; }
    if (p == "jp_uw") { return AudioProcessor::_JP_U_; }
    // Silence / rest
    if (p == "_")  { return AudioProcessor::_SIL_; }
    return -1;
}

// Parse text into a list of Segments, handling [:rate N], [:pitch N],
// [:sing], [:talk], [:klattsch on/off], and [phoneme<dur,note> ...] singing blocks.
std::vector<EmbeddedCmd::Segment> EmbeddedCmd::ParseSegments(const std::string& text) {
    std::vector<Segment> segments;

    if (KlattschMode) {
        // In Klattsch mode, we still look for [:klattsch off]
        static const std::string kKlattschOff = "[:klattsch off]";
        // Case-insensitive search
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        auto pos = lower.find("[:klattsch off]");
        if (pos != std::string::npos) {
            std::string before = text.substr(0, pos);
            if (!before.empty()) {
                segments.push_back(Segment::Klattsch(before));
            }
            KlattschMode = false;
            std::string after = text.substr(pos + kKlattschOff.size());
            if (!after.empty()) {
                auto sub = ParseSegments(after);
                segments.insert(segments.end(), sub.begin(), sub.end());
            }
            return segments;
        }
        segments.push_back(Segment::Klattsch(text));
        return segments;
    }

    if (text.find('[') == std::string::npos) {
        if (!text.empty()) {
            segments.push_back(Segment(text));
        }
        return segments;
    }

    std::string plain;
    bool inSingMode = false;
    int32_t i = 0;
    int32_t len = static_cast<int32_t>(text.size());

    auto FlushPlain = [&]() {
        if (!plain.empty()) {
            segments.push_back(Segment(plain));
            plain.clear();
        }
    };

    while (i < len) {
        if (text[i] != '[') {
            plain += text[i++];
            continue;
        }

        i++; // consume '['
        if (i >= len) {
            break;
        }

        if (text[i] == ':') {
            i++; // Skip ':'
            int32_t cmdStart = i;
            while (i < len && !std::isspace(static_cast<unsigned char>(text[i])) && text[i] != ']') {
                i++;
            }
            std::string cmd = text.substr(cmdStart, i - cmdStart);
            std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            while (i < len && std::isspace(static_cast<unsigned char>(text[i]))) {
                i++;
            }
            int32_t argStart = i;
            while (i < len && text[i] != ']') {
                i++;
            }
            std::string argStr = text.substr(argStart, i - argStart);
            // Trim leading/trailing whitespace
            {
                auto b = argStr.find_first_not_of(" \t\r\n");
                auto e = argStr.find_last_not_of(" \t\r\n");
                argStr = (b == std::string::npos) ? "" : argStr.substr(b, e - b + 1);
            }
            std::transform(argStr.begin(), argStr.end(), argStr.begin(),
                [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            if (i < len) {
                i++; // consume ']'
            }

            if (cmd == "klattsch") {
                if (argStr == "on") {
                    FlushPlain();
                    KlattschMode = true;
                    KlattschParser::Reset();
                    std::string rest = text.substr(i);
                    auto sub = ParseSegments(rest);
                    segments.insert(segments.end(), sub.begin(), sub.end());
                    return segments;
                } else if (argStr == "off") {
                    KlattschMode = false;
                }
            } else if (cmd == "sing") {
                inSingMode = true;
            } else if (cmd == "talk" || cmd == "stop") {
                inSingMode = false;
            } else {
                // Try numeric argument (rate, pitch, volume)
                bool isNum = !argStr.empty();
                for (char c : argStr) {
                    if (!std::isdigit(static_cast<unsigned char>(c)) && c != '-') {
                        isNum = false;
                        break;
                    }
                }
                if (isNum && !argStr.empty()) {
                    int32_t argVal = 0;
                    try { argVal = std::stoi(argStr); } catch (...) { isNum = false; }
                    if (isNum) {
                        VoiceCommand::Kind kind;
                        bool hasKind = true;
                        if (cmd == "rate")        { kind = VoiceCommand::Kind::Rate; }
                        else if (cmd == "pitch")  { kind = VoiceCommand::Kind::Pitch; }
                        else if (cmd == "volume") { kind = VoiceCommand::Kind::Volume; }
                        else                      { hasKind = false; }
                        if (hasKind) {
                            FlushPlain();
                            segments.push_back(Segment(VoiceCommand(kind, argVal)));
                        }
                    }
                }
            }
            continue;
        }

        // Phoneme block [phoneme<dur,note> ...]
        std::vector<PhonemeToken> blockSing;
        bool firstPhon = true;
        bool firstInBlock = true; // Track first note in the [...] block
        int16_t lastPitch = 0;   // inherited by trailing consonants with no <note>

        while (i < len && text[i] != ']') {
            while (i < len && text[i] == ' ') {
                i++;
            }
            if (i >= len || text[i] == ']') {
                break;
            }

            if (text[i] == '_' || std::isalpha(static_cast<unsigned char>(text[i]))) {
                // Collect all phonemes up to '<', ']', or ' '
                // "dey<600,24>" -> [d, ey] with dur=600 note=24
                std::vector<int16_t> group;
                while (i < len && text[i] != '<' && text[i] != ']' && text[i] != ' ') {
                    if ((text[i] == 'J' || text[i] == 'j') && i + 3 < len
                            && (text[i + 1] == 'P' || text[i + 1] == 'p') && text[i + 2] == '_') {
                        bool matchedJp = false;
                        if (i + 4 < len && std::isalpha(static_cast<unsigned char>(text[i + 3]))
                                && std::isalpha(static_cast<unsigned char>(text[i + 4]))) {
                            std::string code5 = "jp_";
                            code5 += static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + 3])));
                            code5 += static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + 4])));
                            int16_t p5 = MapPhoneme(code5);
                            if (p5 >= 0) {
                                group.push_back(p5);
                                i += 5;
                                matchedJp = true;
                            }
                        }
                        if (!matchedJp && std::isalpha(static_cast<unsigned char>(text[i + 3]))) {
                            std::string code4 = "jp_";
                            code4 += static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + 3])));
                            int16_t p4 = MapPhoneme(code4);
                            if (p4 >= 0) {
                                group.push_back(p4);
                                i += 4;
                                matchedJp = true;
                            }
                        }
                        if (matchedJp) {
                            continue;
                        }
                    }
                    if (text[i] == '_') {
                        group.push_back(AudioProcessor::_SIL_);
                        i++;
                        continue;
                    }
                    bool matched2 = false;
                    if (i + 1 < len && std::isalpha(static_cast<unsigned char>(text[i + 1]))) {
                        std::string two;
                        two += static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
                        two += static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + 1])));
                        int16_t op2 = MapPhoneme(two);
                        if (op2 >= 0) {
                            group.push_back(op2);
                            i += 2;
                            matched2 = true;
                        }
                    }
                    if (!matched2) {
                        std::string one;
                        one += static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
                        int16_t op1 = MapPhoneme(one);
                        group.push_back(op1 >= 0 ? op1 : AudioProcessor::_SIL_);
                        i++;
                    }
                }

                int32_t dur = 0, note = 0;
                bool hasNote = false, noteIsNamed = false;
                if (i < len && text[i] == '<') {
                    hasNote = true;
                    i++;
                    while (i < len && std::isdigit(static_cast<unsigned char>(text[i]))) {
                        dur = dur * 10 + (text[i++] - '0');
                    }
                    if (i < len && text[i] == ',') {
                        i++;
                        while (i < len && text[i] == ' ') {
                            i++;
                        }
                        if (i < len && std::isalpha(static_cast<unsigned char>(text[i]))) {
                            int32_t nameStart = i;
                            while (i < len && text[i] != '>' && text[i] != ']') {
                                i++;
                            }
                            // Trim and convert note name
                            std::string noteName = text.substr(nameStart, i - nameStart);
                            {
                                auto b = noteName.find_first_not_of(" \t");
                                auto e = noteName.find_last_not_of(" \t");
                                noteName = (b == std::string::npos) ? "" : noteName.substr(b, e - b + 1);
                            }
                            note = NoteNameToHz(noteName);
                            noteIsNamed = true;
                        } else {
                            while (i < len && std::isdigit(static_cast<unsigned char>(text[i]))) {
                                note = note * 10 + (text[i++] - '0');
                            }
                        }
                    }
                    while (i < len && text[i] != '>' && text[i] != ']') {
                        i++;
                    }
                    if (i < len && text[i] == '>') {
                        i++;
                    }
                }

                if (!hasNote && !inSingMode && blockSing.empty()) {
                    continue;
                }

                int16_t pitch = hasNote
                    ? (noteIsNamed ? static_cast<int16_t>(note) : static_cast<int16_t>(-note))
                    : lastPitch;
                if (hasNote) {
                    lastPitch = pitch;
                }

                int32_t durIdx = static_cast<int32_t>(group.size()) - 1;

                // Subtract every other phoneme's minimum duration from the
                // user-specified duration so the whole cluster fits the beat.
                // We account for the 5ms initial silence and backend frame rounding.
                int32_t overhead = firstInBlock ? 5 : 0;
                firstInBlock = false;
                for (int32_t gi2 = 0; gi2 < static_cast<int32_t>(group.size()); gi2++) {
                    if (gi2 == durIdx) {
                        continue;
                    }
                    int16_t p = group[gi2];
                    int32_t m = (p == AudioProcessor::_SIL_) ? 5 : Tables::GetMinimumDuration(p);
                    overhead += (m / 5) * 5;
                }
                int32_t adjustedDur = std::max(5, dur - overhead);

                for (int32_t gi = 0; gi < static_cast<int32_t>(group.size()); gi++) {
                    int64_t ctrl = AudioProcessor::kWord_Start | AudioProcessor::kContent_Word;
                    if (pitch != 0) {
                        ctrl |= AudioProcessor::kSingingPhon;
                    }
                    if (hasNote && gi == durIdx) {
                        ctrl |= AudioProcessor::kSingingDuration;
                    }
                    if (firstPhon) {
                        firstPhon = false;
                    } else {
                        ctrl &= ~(AudioProcessor::kWord_Start | AudioProcessor::kContent_Word);
                    }
                    PhonemeToken tok{};
                    tok.Phon    = group[gi];
                    tok.Ctrl    = ctrl;
                    tok.UserDur  = (hasNote && gi == durIdx) ? static_cast<int16_t>(adjustedDur) : static_cast<int16_t>(0);
                    tok.UserNote = (hasNote && gi == durIdx) ? pitch : static_cast<int16_t>(0);
                    blockSing.push_back(tok);
                }
            } else {
                i++;
            }
        }
        if (i < len && text[i] == ']') {
            i++;
        }

        if (!blockSing.empty()) {
            FlushPlain();
            segments.push_back(Segment(std::move(blockSing)));
        }
    }

    FlushPlain();
    return segments;
}

// Parse text, returning plain text and optionally singing tokens.
std::string EmbeddedCmd::Parse(const std::string& text,
                               std::vector<PhonemeToken>* singingTokens) {
    if (singingTokens) {
        singingTokens->clear();
    }
    auto segs = ParseSegments(text);

    std::string plain;
    std::vector<PhonemeToken> sing;

    for (auto& seg : segs) {
        if (seg.IsSinging()) {
            sing.insert(sing.end(), seg.singing.begin(), seg.singing.end());
        } else if (!seg.IsCommand()) {
            plain += seg.plainText;
        }
    }

    if (singingTokens && !sing.empty()) {
        *singingTokens = std::move(sing);
    }
    return plain;
}

// Strip all embedded commands and singing notation, returning plain text only.
std::string EmbeddedCmd::StripCommands(const std::string& text) {
    return Parse(text, nullptr);
}

}  // namespace SharpVox
