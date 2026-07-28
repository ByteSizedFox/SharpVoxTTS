CXX      := x86_64-w64-mingw32-g++
CXXFLAGS := -std=c++11 -O2 -Wall -I../../include \
            -DSHARPVOX_CAPI_EXPORTS -MMD -MP
LDFLAGS  :=
LDLIBS   := -lm -static

ROOT  := ../..
BDIR  := build

LIB_SRCS := \
    $(ROOT)/src/Tables.cpp \
    $(ROOT)/src/KlattSynthesizerFP.cpp \
    $(ROOT)/src/PitchInterpolator.cpp \
    $(ROOT)/src/AudioProcessor.cpp \
    $(ROOT)/src/SpeechRenderer.cpp \
    $(ROOT)/src/TtsEngine.cpp \
    $(ROOT)/src/Phonemizer.cpp \
    $(ROOT)/src/LetterToSound.cpp \
    $(ROOT)/src/DictionaryReader.cpp \
    $(ROOT)/src/LibraryDataDictionary.cpp \
    $(ROOT)/src/LibraryDataSymbols.cpp \
    $(ROOT)/src/Morphology.cpp \
    $(ROOT)/src/HeteronymResolver.cpp \
    $(ROOT)/src/TextCommands.cpp \
    $(ROOT)/src/KlattschParser.cpp \
    $(ROOT)/src/VoicePresets.cpp \
    $(ROOT)/src/JapaneseParser.cpp

WRAP_SRCS := \
    $(ROOT)/platform/lib/SharpVox.cpp

ALL_SRCS := $(LIB_SRCS) $(WRAP_SRCS) sharpvox_capi.cpp
OBJS     := $(addprefix $(BDIR)/, $(notdir $(ALL_SRCS:.cpp=.o)))

DLL := sharpvox.dll

.PHONY: all clean

all: $(DLL)

$(DLL): $(OBJS)
	$(CXX) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)

# lib sources
$(BDIR)/%.o: $(ROOT)/src/%.cpp
	@mkdir -p $(BDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BDIR)/%.o: $(ROOT)/platform/lib/%.cpp
	@mkdir -p $(BDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BDIR)/%.o: %.cpp
	@mkdir -p $(BDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BDIR) $(DLL)

-include $(OBJS:.o=.d)
