#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define isatty _isatty
#else
#include <unistd.h>
#endif
#include <vector>

#include "../../include/LibraryData.h"
#include "../../include/TtsEngine.h"
#ifdef HAVE_MECAB
#include "../../include/MeCabReader.h"
#endif
#include "WavWriter.h"
#ifdef HAVE_ALSA
#include "AlsaPlayer.h"
#endif

namespace SharpVox {
namespace Cli {

static void PrintHelp() {
    printf("SharpVox TTS\n");
    printf("Usage: sharpvox [options] [\"text\"]\n");
    printf("\n");
    printf("Options:\n");
    printf("  -o, --output <file>    Output WAV file, '-' for stdout (plays via ALSA if omitted on Linux)\n");
    printf("  -i, --input <file>     Input text file (if text not provided as argument)\n");
    printf("  -r, --rate <value>     Speech rate (default: 160)\n");
    printf("  -s, --samplerate <hz>  Output sample rate, 8000-48000 (default: 48000)\n");
    printf("  -v, --voice <name>     Voice preset name \xe2\x80\x94 loads voices/<name>.json, fallback to baseline/whisper builtins\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("With no text argument and stdin piped, lines are read and spoken as they\n");
    printf("arrive. With stdout piped and no -o, a streaming WAV is written to stdout.\n");
    printf("\n");
}

// Reads one line from stdin (without the newline). Returns false at EOF.
static bool ReadStdinLine(std::string& out) {
    out.clear();
    int c;
    while ((c = fgetc(stdin)) != EOF) {
        if (c == '\n') return true;
        out.push_back(static_cast<char>(c));
    }
    return !out.empty();
}

// Returns false if the string is empty or whitespace-only after trimming in-place.
static bool TrimText(std::string& text) {
    const std::string ws = " \t\r\n";
    auto first = text.find_first_not_of(ws);
    if (first == std::string::npos) return false;
    auto last = text.find_last_not_of(ws);
    text = text.substr(first, last - first + 1);
    return true;
}

// Lowercase a string in-place
static std::string ToLower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c + ('a' - 'A'));
        }
    }
    return s;
}

static bool TryParseInt(const std::string& s, int32_t& out) {
    try {
        std::size_t pos = 0;
        int32_t v = std::stoi(s, &pos);
        if (pos != s.size()) {
            return false;
        }
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

static bool FileExists(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static std::string ReadAllText(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return ""; }
    std::string s(sz, '\0');
    fread(&s[0], 1, (size_t)sz, f);
    fclose(f);
    return s;
}

// Returns the directory containing the running executable (best effort).
// Falls back to "." on failure.
static std::string GetExeDir() {
    char buf[4096] = {};
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, buf, sizeof(buf));
    if (len == 0 || len == sizeof(buf)) {
        return ".";
    }
#else
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) {
        return ".";
    }
    buf[len] = '\0';
#endif
    std::string path(buf);
    auto slash = path.rfind(
#ifdef _WIN32
        '\\'
#else
        '/'
#endif
    );
    if (slash == std::string::npos) {
        // Check for forward slash on Windows too just in case
#ifdef _WIN32
        slash = path.rfind('/');
        if (slash == std::string::npos) return ".";
#else
        return ".";
#endif
    }
    return path.substr(0, slash);
}

static bool TryLoadVoiceJson(const std::string& name, VoiceData& out) {
    std::string exeDir = GetExeDir();
    std::vector<std::string> candidates = {
        exeDir + "/voices/" + name + ".json",
        "voices/" + name + ".json",
    };

    std::string path;
    for (const auto& c : candidates) {
        if (FileExists(c)) {
            path = c;
            break;
        }
    }
    if (path.empty()) {
        return false;
    }

    std::string json = ReadAllText(path);
    if (json.empty()) {
        return false;
    }

    // Minimal JSON field extraction helpers (no external dependency).
    // Finds the integer value of key in a flat JSON object; returns defaultVal if absent or unparseable.
    auto getInt = [&](const std::string& key, int32_t defaultVal) -> int32_t {
        std::string needle = "\"" + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos) {
            return defaultVal;
        }
        pos = json.find(':', pos + needle.size());
        if (pos == std::string::npos) {
            return defaultVal;
        }
        pos = json.find_first_not_of(" \t\r\n", pos + 1);
        if (pos == std::string::npos) {
            return defaultVal;
        }
        try {
            std::size_t consumed = 0;
            int32_t v = std::stoi(json.substr(pos), &consumed);
            return consumed > 0 ? v : defaultVal;
        } catch (...) {
            return defaultVal;
        }
    };

    // Finds the float value of key; returns defaultVal if absent or unparseable.
    auto getFloat = [&](const std::string& key, float defaultVal) -> float {
        std::string needle = "\"" + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos) {
            return defaultVal;
        }
        pos = json.find(':', pos + needle.size());
        if (pos == std::string::npos) {
            return defaultVal;
        }
        pos = json.find_first_not_of(" \t\r\n", pos + 1);
        if (pos == std::string::npos) {
            return defaultVal;
        }
        try {
            std::size_t consumed = 0;
            float v = std::stof(json.substr(pos), &consumed);
            return consumed > 0 ? v : defaultVal;
        } catch (...) {
            return defaultVal;
        }
    };

    VoiceData v;
    v.Rate = static_cast<int16_t>(getInt("Rate", v.Rate));
    v.PitchHz = static_cast<int16_t>(getInt("PitchHz", v.PitchHz));
    v.VoiceType = static_cast<int16_t>(getInt("VoiceType", v.VoiceType));
    v.TractScale = getFloat("TractScale", v.TractScale);
    v.VGain = static_cast<int16_t>(getInt("VoicingGain", v.VGain));
    v.AGain = static_cast<int16_t>(getInt("AspirationGain", v.AGain));
    v.ACycle = static_cast<int16_t>(getInt("AspirationCycle", v.ACycle));
    v.NGain = static_cast<int16_t>(getInt("NGain", v.NGain));
    v.F4Freq = static_cast<int16_t>(getInt("F4Freq", v.F4Freq));
    v.F4BW = static_cast<int16_t>(getInt("F4BW", v.F4BW));
    v.F5Freq = static_cast<int16_t>(getInt("F5Freq", v.F5Freq));
    v.F5BW = static_cast<int16_t>(getInt("F5BW", v.F5BW));
    v.BwGain1 = static_cast<int16_t>(getInt("BwGain1", v.BwGain1));
    v.BwGain2 = static_cast<int16_t>(getInt("BwGain2", v.BwGain2));
    v.BwGain3 = static_cast<int16_t>(getInt("BwGain3", v.BwGain3));
    v.NasalBase = static_cast<int16_t>(getInt("NasalBase", v.NasalBase));
    v.NasalTarg = static_cast<int16_t>(getInt("NasalTarg", v.NasalTarg));
    v.NasalBW = static_cast<int16_t>(getInt("NasalBW", v.NasalBW));
    v.PitchRange = static_cast<int16_t>(getInt("PitchRange", v.PitchRange));
    v.StressGain = static_cast<int16_t>(getInt("StressGain", v.StressGain));
    v.Intonation = static_cast<int16_t>(getInt("Intonation", v.Intonation));
    v.RiseAmt = static_cast<int16_t>(getInt("RiseAmt", v.RiseAmt));
    v.FallAmt = static_cast<int16_t>(getInt("FallAmt", v.FallAmt));
    v.BaselineFall = static_cast<int16_t>(getInt("BaselineFall", v.BaselineFall));
    v.Jitter = static_cast<int16_t>(getInt("Jitter", v.Jitter));
    v.Shimmer = static_cast<int16_t>(getInt("Shimmer", v.Shimmer));
    v.Diplophonia = static_cast<int16_t>(getInt("Diplophonia", v.Diplophonia));
    v.FryAmount = static_cast<int16_t>(getInt("FryAmount", v.FryAmount));
    v.SubglottalAmt = static_cast<int16_t>(getInt("SubglottalAmt", v.SubglottalAmt));
    v.BreathAmt = static_cast<int16_t>(getInt("BreathAmt", v.BreathAmt));
    v.OpenQuotient = static_cast<int16_t>(getInt("OpenQuotient", v.OpenQuotient));
    v.OQStressLink = static_cast<int16_t>(getInt("OQStressLink", v.OQStressLink));
    v.OQF0Link = static_cast<int16_t>(getInt("OQF0Link", v.OQF0Link));
    v.LarynxOffset = static_cast<int16_t>(getInt("LarynxOffset", v.LarynxOffset));
    v.PharyngealAmt = static_cast<int16_t>(getInt("PharyngealAmt", v.PharyngealAmt));
    v.PitchOffsetHz = static_cast<int16_t>(getInt("PitchOffsetHz", v.PitchOffsetHz));
    v.LipRounding = static_cast<int16_t>(getInt("LipRounding", v.LipRounding));
    v.OnsetHardness = static_cast<int16_t>(getInt("OnsetHardness", v.OnsetHardness));
    out = v;
    return true;
}

}  // namespace Cli
}  // namespace SharpVox

int main(int argc, char* argv[]) {
    using namespace SharpVox;
    using namespace SharpVox::Cli;

    if (argc == 1 && ::isatty(STDIN_FILENO)) {
        PrintHelp();
        return 0;
    }

    std::string text;
    std::string outputPath;
    bool outputExplicit = false;
    std::string inputPath;
    int32_t rate = 160;
    int32_t sampleRate = 48000;
    std::string voicePreset = "baseline";

    // handle arguments so it's a proper CLI citizen
    std::vector<std::string> positionalArgs;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o" || arg == "--output") {
            if (++i < argc) {
                outputPath = argv[i];
                outputExplicit = true;
            }
        } else if (arg == "-i" || arg == "--input") {
            if (++i < argc) {
                inputPath = argv[i];
            }
        } else if (arg == "-r" || arg == "--rate") {
            if (++i < argc) {
                int32_t r = 0;
                if (TryParseInt(argv[i], r)) {
                    rate = r;
                }
            }
        } else if (arg == "-s" || arg == "--samplerate") {
            if (++i < argc) {
                int32_t sr = 0;
                if (TryParseInt(argv[i], sr)) {
                    sampleRate = sr;
                }
            }
        } else if (arg == "-v" || arg == "--voice") {
            if (++i < argc) {
                voicePreset = ToLower(argv[i]);
            }
        } else if (arg == "-h" || arg == "--help") {
            PrintHelp();
            return 0;
        } else {
            if (!arg.empty() && arg[0] != '-') {
                positionalArgs.push_back(arg);
            }
        }
    }

    // With no text argument or input file, a piped stdin is consumed
    // line by line during synthesis instead of being slurped up front.
    bool streamStdin = false;

    if (!positionalArgs.empty()) {
        std::string joined;
        for (std::size_t i = 0; i < positionalArgs.size(); i++) {
            if (i > 0) {
                joined += ' ';
            }
            joined += positionalArgs[i];
        }
        text = joined;
    } else if (!inputPath.empty() && FileExists(inputPath)) {
        text = ReadAllText(inputPath);
    } else if (!::isatty(STDIN_FILENO)) {
        streamStdin = true;
    }

    if (!streamStdin && !TrimText(text)) {
        PrintHelp();
        return 0;
    }

    VoiceData voice;
    bool voiceLoaded = TryLoadVoiceJson(voicePreset, voice);
    if (!voiceLoaded) {
        if (voicePreset == "whisper") {
            voice = VoiceData::whisper_voice();
        } else {
            voice = VoiceData::baseline_voice();
        }
    }
    voice.Rate = static_cast<int16_t>(rate);

#ifdef HAVE_MECAB
    MeCabReader::Init();
#endif

    TtsEngine* engine = nullptr;
    try {
        engine = new TtsEngine(voice,
            LibraryData::dictionary,
            static_cast<size_t>(LibraryData::dictionarySize),
            [](const std::string& key, size_t& outSize) -> const uint8_t* {
                return LibraryData::FindSymbol(key.c_str(), outSize);
            },
            sampleRate);
    } catch (const std::invalid_argument& ex) {
        fprintf(stderr, "Error: %s\n", ex.what());
        return 0;
    }

    // Feeds the sink either the fixed text or stdin lines as they arrive
    auto speakAll = [&](void (*sink)(const int16_t*, int32_t, void*), void* ctx) {
        if (streamStdin) {
            std::string line;
            while (ReadStdinLine(line)) {
                if (!TrimText(line)) continue;
                engine->Speak(line, sink, ctx);
            }
        } else {
            engine->Speak(text, sink, ctx);
        }
    };

    struct WavCtx { WavStreamWriter* writer; int64_t* total; };
    int64_t totalSamples = 0;

    bool toStdout = (outputPath == "-");
    // No -o with stdout piped means write the WAV to the pipe
    if (!outputExplicit && !::isatty(STDOUT_FILENO)) toStdout = true;

    if (toStdout) {
#ifdef _WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        try {
            WavStreamWriter writer(stdout, sampleRate);
            WavCtx ctx { &writer, &totalSamples };
            speakAll([](const int16_t* chunk, int32_t chunkLen, void* ud) {
                auto* c = static_cast<WavCtx*>(ud);
                c->writer->Write(chunk, chunkLen);
                *c->total += chunkLen;
            }, &ctx);
            writer.Dispose();
            fprintf(stderr, "Streamed %.2fs @ %d Hz to stdout\n",
                    totalSamples / (float)sampleRate,
                    sampleRate);
        } catch (const std::exception& ex) {
            fprintf(stderr, "Error writing WAV to stdout: %s\n", ex.what());
        }
    }
#ifdef HAVE_ALSA
    else if (!outputExplicit) {
        try {
            AlsaPlayer player(sampleRate);
            struct AlsaCtx { AlsaPlayer* player; int64_t* total; };
            AlsaCtx ctx { &player, &totalSamples };
            speakAll([](const int16_t* chunk, int32_t chunkLen, void* ud) {
                auto* c = static_cast<AlsaCtx*>(ud);
                c->player->Write(chunk, chunkLen);
                *c->total += chunkLen;
            }, &ctx);
            printf("Played %.2fs @ %d Hz\n",
                   totalSamples / (float)sampleRate,
                   sampleRate);
        } catch (const std::exception& ex) {
            fprintf(stderr, "ALSA error: %s\n", ex.what());
        }
    }
#endif
    else {
        if (outputPath.empty()) outputPath = "out.wav";
        try {
            WavStreamWriter writer(outputPath, sampleRate);
            WavCtx ctx { &writer, &totalSamples };
            speakAll([](const int16_t* chunk, int32_t chunkLen, void* ud) {
                auto* c = static_cast<WavCtx*>(ud);
                c->writer->Write(chunk, chunkLen);
                *c->total += chunkLen;
            }, &ctx);
            printf("Generated %s (%.2fs @ %d Hz)\n",
                   outputPath.c_str(),
                   totalSamples / (float)sampleRate,
                   sampleRate);
        } catch (const std::exception& ex) {
            fprintf(stderr, "Error saving WAV: %s\n", ex.what());
        }
    }

    delete engine;
    return 0;
}
