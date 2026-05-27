CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude -fPIC
LDFLAGS  :=
LDLIBS   := -lz -lm

# Library sources (all files shared by both targets)
LIB_SRCS := \
    src/Tables.cpp \
    src/KlattSynthesizer.cpp \
    src/PitchInterpolator.cpp \
    src/AudioProcessor.cpp \
    src/SpeechRenderer.cpp \
    src/TtsEngine.cpp \
    src/Phonemizer.cpp \
    src/LetterToSound.cpp \
    src/LexiconReader.cpp \
    src/LibraryDataDictionary.cpp \
    src/LibraryDataSymbols.cpp \
    src/Morphology.cpp \
    src/HeteronymResolver.cpp \
    src/TextCommands.cpp \
    src/KlattschParser.cpp

LIB_OBJS := $(LIB_SRCS:.cpp=.o)

# CLI sources
CLI_SRCS := \
    platform/cli/Main.cpp \
    platform/cli/WavWriter.cpp

CLI_OBJS := $(CLI_SRCS:.cpp=.o)

# Shared library sources
SHLIB_SRCS := platform/lib/SharpVox.cpp
SHLIB_OBJS := $(SHLIB_SRCS:.cpp=.o)

# Output targets
CLI_BIN  := sharpvox
SHLIB    := libsharpvox.so
ARCHIVE  := libsharpvox.a

TEST_SRCS := tests/DumpStages.cpp
TEST_BIN  := tests/dump_stages

# WASM sources (same engine + speaker + wasm interop)
WASM_SRCS := $(LIB_SRCS) \
    platform/lib/SharpVox.cpp \
    platform/wasm/SharpVoxWasm.cpp

WASM_OUT  := platform/wasm/wwwroot/js/sharpvox.js

EMCC      := emcc
EMCCFLAGS := -std=c++17 -O2 -Iinclude \
    --bind \
    -fwasm-exceptions \
    -sUSE_ZLIB=1 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sINITIAL_MEMORY=134217728 \
    -sSTACK_SIZE=2097152 \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=SharpVoxModule \
    -sEXPORTED_RUNTIME_METHODS=['UTF8ToString'] \
    -sEXPORT_ES6=1 \
    -sENVIRONMENT=web

.PHONY: all cli lib tests wasm clean

all: cli lib

tests: $(TEST_BIN)

$(TEST_BIN): $(LIB_OBJS) $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) $(TEST_SRCS) $(LIB_OBJS) $(LDLIBS) -o $@

cli: $(CLI_BIN)

lib: $(SHLIB) $(ARCHIVE)

$(CLI_BIN): $(LIB_OBJS) $(CLI_OBJS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(SHLIB): $(LIB_OBJS) $(SHLIB_OBJS)
	$(CXX) -shared -fPIC $(LDFLAGS) $^ $(LDLIBS) -o $@

$(ARCHIVE): $(LIB_OBJS)
	ar rcs $@ $^

# Pattern rule: compile .cpp → .o (applies to src/, platform/cli/, platform/lib/)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Shared-library objects need -fPIC
platform/lib/%.o: platform/lib/%.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

wasm:
	$(EMCC) $(EMCCFLAGS) $(WASM_SRCS) -o $(WASM_OUT)

clean:
	rm -f $(LIB_OBJS) $(CLI_OBJS) $(SHLIB_OBJS) $(CLI_BIN) $(SHLIB) $(ARCHIVE)
	rm -f $(WASM_OUT) platform/wasm/wwwroot/js/sharpvox.wasm
