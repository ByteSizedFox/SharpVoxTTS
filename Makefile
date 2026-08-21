CXX      := g++
# -MMD -MP emits .d header dependency files so edits to include/ trigger rebuilds
CXXFLAGS := -std=c++11 -O2 -Wall -Wextra -Iinclude -fPIC -MMD -MP
LDFLAGS  :=
LDLIBS   := -lm

UNAME := $(shell uname)
CLI_CXXFLAGS := $(CXXFLAGS)
CLI_LDLIBS   := $(LDLIBS)

# Library sources (all files shared by both targets)
LIB_SRCS := \
    src/Tables.cpp \
    src/KlattSynthesizerFP.cpp \
    src/PitchInterpolator.cpp \
    src/AudioProcessor.cpp \
    src/SpeechRenderer.cpp \
    src/TtsEngine.cpp \
    src/Phonemizer.cpp \
    src/LetterToSound.cpp \
    src/DictionaryReader.cpp \
    src/LibraryDataDictionary.cpp \
    src/LibraryDataSymbols.cpp \
    src/Morphology.cpp \
    src/HeteronymResolver.cpp \
    src/TextCommands.cpp \
    src/KlattschParser.cpp \
    src/VoicePresets.cpp \
    src/JapaneseParser.cpp \
    src/MeCabReader.cpp \
    src/IpadicDict.cpp

LIB_OBJS := $(LIB_SRCS:.cpp=.o)

# CLI sources
CLI_SRCS := \
    platform/cli/Main.cpp \
    platform/cli/WavWriter.cpp

ifeq ($(UNAME), Linux)
CLI_SRCS     += platform/cli/AlsaPlayer.cpp
CLI_CXXFLAGS := $(CXXFLAGS) -DHAVE_ALSA
CLI_LDLIBS   := $(LDLIBS) -lasound
endif

CLI_OBJS := $(CLI_SRCS:.cpp=.o)

# Shared library sources
SHLIB_SRCS := platform/lib/SharpVox.cpp
SHLIB_OBJS := $(SHLIB_SRCS:.cpp=.o)

# Output targets
CLI_BIN    := sharpvox
SHLIB      := libsharpvox.so
ARCHIVE    := libsharpvox.a

TEST_SRCS := tests/DumpStages.cpp
TEST_BIN  := tests/dump_stages
TIMING_PROBE_SRCS := tests/MidiTimingProbe.cpp
TIMING_PROBE_BIN  := tests/midi_timing_probe

# WASM sources (same engine + speaker + wasm interop)
WASM_SRCS := $(LIB_SRCS) \
    platform/lib/SharpVox.cpp \
    platform/wasm/SharpVoxWasm.cpp

WASM_OUT  := platform/wasm/wwwroot/js/sharpvox.js

EMCC      := emcc
EMCCFLAGS := -std=c++17 -O2 -Iinclude -DSHARPVOX_SAMPLED_GLOT \
    --bind \
    -fwasm-exceptions \
    -sALLOW_MEMORY_GROWTH=1 \
    -sINITIAL_MEMORY=134217728 \
    -sSTACK_SIZE=2097152 \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=SharpVoxModule \
    -sEXPORTED_RUNTIME_METHODS=['UTF8ToString'] \
    -sEXPORT_ES6=1 \
    -sENVIRONMENT=web

.PHONY: all cli lib tests timing-probe wasm wasm-host clean

all: cli lib

tests: $(TEST_BIN)

timing-probe: $(TIMING_PROBE_BIN)

$(TEST_BIN): $(LIB_OBJS) $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) $(TEST_SRCS) $(LIB_OBJS) $(LDLIBS) -o $@

$(TIMING_PROBE_BIN): $(LIB_OBJS) platform/lib/SharpVox.o $(TIMING_PROBE_SRCS)
	$(CXX) $(CXXFLAGS) $(TIMING_PROBE_SRCS) $(LIB_OBJS) platform/lib/SharpVox.o $(LDLIBS) -o $@

cli: $(CLI_BIN)

lib: $(SHLIB) $(ARCHIVE)

$(CLI_BIN): $(LIB_OBJS) $(CLI_OBJS)
	$(CXX) $(LDFLAGS) $^ $(CLI_LDLIBS) -o $@

$(SHLIB): $(LIB_OBJS) $(SHLIB_OBJS)
	$(CXX) -shared -fPIC $(LDFLAGS) $^ $(LDLIBS) -o $@

$(ARCHIVE): $(LIB_OBJS)
	ar rcs $@ $^

# CLI objects use CLI_CXXFLAGS (adds -DHAVE_ALSA on Linux)
platform/cli/%.o: platform/cli/%.cpp
	$(CXX) $(CLI_CXXFLAGS) -c $< -o $@

# Pattern rule: compile .cpp -> .o (applies to src/, platform/lib/)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Shared-library objects need -fPIC
platform/lib/%.o: platform/lib/%.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

-include $(LIB_OBJS:.o=.d) $(CLI_OBJS:.o=.d) $(SHLIB_OBJS:.o=.d)

wasm:
	$(EMCC) $(EMCCFLAGS) $(WASM_SRCS) \
		--embed-file $(IPADIC_BIN)@ipadic.bin \
		-o $(WASM_OUT)

wasm-host: wasm
	python3 -m http.server 8080 --directory platform/wasm/wwwroot

clean:
	rm -f $(LIB_OBJS) $(CLI_OBJS) $(SHLIB_OBJS) $(CLI_BIN) $(SHLIB) $(ARCHIVE)
	rm -f $(LIB_OBJS:.o=.d) $(CLI_OBJS:.o=.d) $(SHLIB_OBJS:.o=.d)
	rm -rf $(FP_BUILD_DIR)
	rm -f $(WASM_OUT) platform/wasm/wwwroot/js/sharpvox.wasm
	rm -f $(TIMING_PROBE_BIN)
