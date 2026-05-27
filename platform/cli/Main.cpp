#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include "../../include/LibraryData.h"
#include "../../include/TtsEngine.h"
#include "WavWriter.h"

namespace SharpVox {
namespace Cli {

static void PrintHelp() {
    // in case it wasn't obvious enough
    std::cout << "SharpVox TTS\n";
    std::cout << "Usage: sharpvox [options] [\"text\"]\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  -o, --output <file>    Output WAV file (default: out.wav)\n";
    std::cout << "  -i, --input <file>     Input text file (if text not provided as argument)\n";
    std::cout << "  -r, --rate <value>     Speech rate (default: 160)\n";
    std::cout << "  -p, --pitch <value>    Base pitch in Hz (default: 104)\n";
    std::cout << "  -s, --samplerate <hz>  Output sample rate (default: 48000)\n";
    std::cout << "  -v, --voice <name>     Voice preset name \xe2\x80\x94 loads voices/<name>.json, fallback to baseline/whisper builtins\n";
    std::cout << "  -h, --help             Show this help message\n";
    std::cout << "\n";
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
    std::ifstream f(path);
    return f.good();
}

static std::string ReadAllText(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Returns the directory containing the running executable (best effort).
// Falls back to "." on failure.
static std::string GetExeDir() {
    char buf[4096] = {};
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) {
        return ".";
    }
    buf[len] = '\0';
    std::string path(buf);
    auto slash = path.rfind('/');
    if (slash == std::string::npos) {
        return ".";
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
    v.F4pFreq = static_cast<int16_t>(getInt("F4pFreq", v.F4pFreq));
    v.F4pBW = static_cast<int16_t>(getInt("F4pBW", v.F4pBW));
    v.F5pFreq = static_cast<int16_t>(getInt("F5pFreq", v.F5pFreq));
    v.F5pBW = static_cast<int16_t>(getInt("F5pBW", v.F5pBW));
    v.F6pFreq = static_cast<int16_t>(getInt("F6pFreq", v.F6pFreq));
    v.F6pBW = static_cast<int16_t>(getInt("F6pBW", v.F6pBW));
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

    if (argc == 1) {
        PrintHelp();
        return 0;
    }

    std::string text;
    std::string outputPath = "out.wav";
    std::string inputPath;
    int32_t rate = 160;
    int32_t pitch = 104;
    int32_t sampleRate = 48000;
    std::string voicePreset = "baseline";

    // handle arguments so it's a proper CLI citizen
    std::vector<std::string> positionalArgs;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o" || arg == "--output") {
            if (++i < argc) {
                outputPath = argv[i];
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
        } else if (arg == "-p" || arg == "--pitch") {
            if (++i < argc) {
                int32_t p = 0;
                if (TryParseInt(argv[i], p)) {
                    pitch = p;
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
    } else {
        // Read from stdin if it is redirected (i.e. not a terminal)
        if (!::isatty(STDIN_FILENO)) {
            std::ostringstream ss;
            ss << std::cin.rdbuf();
            text = ss.str();
        }
    }

    // Trim leading/trailing whitespace to mirror string.IsNullOrWhiteSpace check
    {
        const std::string ws = " \t\r\n";
        auto first = text.find_first_not_of(ws);
        if (first == std::string::npos) {
            PrintHelp();
            return 0;
        }
        auto last = text.find_last_not_of(ws);
        text = text.substr(first, last - first + 1);
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
    voice.PitchHz = static_cast<int16_t>(pitch);

    TtsEngine* engine = nullptr;
    try {
        engine = new TtsEngine(voice,
            LibraryData::dictionary,
            static_cast<size_t>(LibraryData::dictionarySize),
            [](const std::string& key, size_t& outSize) -> const uint8_t* {
                auto it = LibraryData::SymbolsTable.find(key);
                if (it == LibraryData::SymbolsTable.end()) {
                    outSize = 0;
                    return nullptr;
                }
                outSize = it->second.size();
                return it->second.data();
            },
            sampleRate);
    } catch (const std::invalid_argument& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 0;
    }

    try {
        WavStreamWriter writer(outputPath, sampleRate);
        int32_t totalSamples = 0;
        engine->Speak(text, [&](const int16_t* chunk, int32_t chunkLen) {
            writer.Write(chunk, chunkLen);
            totalSamples += chunkLen;
        });
        std::cout << "Generated " << outputPath
                  << " (" << std::fixed;
        std::cout.precision(2);
        std::cout << (totalSamples / static_cast<float>(sampleRate))
                  << "s @ " << sampleRate << " Hz)\n";
    } catch (const std::exception& ex) {
        std::cout << "Error saving WAV: " << ex.what() << "\n";
    }

    delete engine;
    return 0;
}
