CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude -fPIC
LDFLAGS  :=
LDLIBS   := -lz -lm

# Library sources (all files shared by both targets)
LIB_SRCS := \
    src/tables.cpp \
    src/klatt_synthesizer.cpp \
    src/pitch_interpolator.cpp \
    src/audio_processor.cpp \
    src/speech_renderer.cpp \
    src/tts_engine.cpp \
    src/phonemizer.cpp \
    src/letter_to_sound.cpp \
    src/lexicon_reader.cpp \
    src/library_data_dictionary.cpp \
    src/library_data_symbols.cpp \
    src/morphology.cpp \
    src/heteronym_resolver.cpp \
    src/text_commands.cpp \
    src/klattsch_parser.cpp

LIB_OBJS := $(LIB_SRCS:.cpp=.o)

# CLI sources
CLI_SRCS := \
    platform/cli/main.cpp \
    platform/cli/wav_writer.cpp

CLI_OBJS := $(CLI_SRCS:.cpp=.o)

# Shared library sources
SHLIB_SRCS := platform/lib/sharptalk_speaker.cpp
SHLIB_OBJS := $(SHLIB_SRCS:.cpp=.o)

# Output targets
CLI_BIN  := sharptalk
SHLIB    := libsharptalk.so
ARCHIVE  := libsharptalk.a

TEST_SRCS := tests/dump_stages.cpp
TEST_BIN  := tests/dump_stages

# WASM sources (same engine + speaker + wasm interop)
WASM_SRCS := $(LIB_SRCS) \
    platform/lib/sharptalk_speaker.cpp \
    platform/wasm/sharptalk_wasm.cpp

WASM_OUT  := platform/wasm/wwwroot/js/sharptalk.js

EMCC      := emcc
EMCCFLAGS := -std=c++17 -O2 -Iinclude \
    --bind \
    -fwasm-exceptions \
    -sUSE_ZLIB=1 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sINITIAL_MEMORY=134217728 \
    -sSTACK_SIZE=2097152 \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=SharpTalkModule \
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
	rm -f $(WASM_OUT) platform/wasm/wwwroot/js/sharptalk.wasm
