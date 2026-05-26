#ifndef SHARPTALK_TEXT_COMMANDS_H
#define SHARPTALK_TEXT_COMMANDS_H

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace SharpTalk {

// Forward declaration — PhonemeToken is defined in audio_processor.h / tts_engine.h.
struct PhonemeToken;

class EmbeddedCmd {
public:
    struct VoiceCommand {
        enum class Kind { Rate, Pitch, Volume };
        Kind Type;
        int32_t Value;
        VoiceCommand(Kind type, int32_t value) : Type(type), Value(value) {}
    };

    // Discriminated union for a parsed text segment.
    // Exactly one of PlainText / Singing / Cmd / KlattschText is active, indicated by kind.
    struct Segment {
        enum class Kind { PlainText, Singing, Cmd, KlattschText };
        Kind kind;

        std::string                     plainText;    // valid when kind == PlainText
        std::vector<PhonemeToken>       singing;      // valid when kind == Singing
        std::optional<VoiceCommand>     cmd;          // valid when kind == Cmd
        std::string                     klattschText; // valid when kind == KlattschText

        bool IsSinging()   const { return kind == Kind::Singing; }
        bool IsCommand()   const { return kind == Kind::Cmd; }
        bool IsKlattsch()  const { return kind == Kind::KlattschText; }

        // Construct a plain-text segment
        explicit Segment(std::string text)
            : kind(Kind::PlainText), plainText(std::move(text)) {}

        // Construct a singing segment
        explicit Segment(std::vector<PhonemeToken> s)
            : kind(Kind::Singing), singing(std::move(s)) {}

        // Construct a command segment
        explicit Segment(VoiceCommand c)
            : kind(Kind::Cmd), cmd(std::move(c)) {}

        // Construct a Klattsch text segment
        static Segment Klattsch(std::string text) {
            Segment seg;
            seg.kind = Kind::KlattschText;
            seg.klattschText = std::move(text);
            return seg;
        }

    private:
        Segment() = default;
    };

    // Global Klattsch mode flag (mirrors C# static field)
    static bool KlattschMode;

    // Parse text into a list of Segments, handling [:rate N], [:pitch N],
    // [:sing], [:talk], [:klattsch on/off], and [phoneme<dur,note> ...] singing blocks.
    static std::vector<Segment> ParseSegments(const std::string& text);

    // Parse text, returning plain text and optionally singing tokens.
    static std::string Parse(const std::string& text,
                             std::vector<PhonemeToken>* singingTokens);

    // Strip all embedded commands and singing notation, returning plain text only.
    static std::string StripCommands(const std::string& text);

private:
    // ASCII note name (C5, A#4, Bb3) -> Hz using equal temperament (A4 = 440 Hz)
    static int32_t NoteNameToHz(const std::string& name);

    // Map a lowercase phoneme name to a phoneme ID; returns -1 on failure.
    static int16_t MapPhoneme(const std::string& p);
};

}  // namespace SharpTalk

#endif  // SHARPTALK_TEXT_COMMANDS_H
