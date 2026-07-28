cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico CACHE STRING "Board type")

# Locate Pico SDK: env var, then ~/pico-sdk, then fail helpfully
if(DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
elseif(EXISTS "$ENV{HOME}/pico-sdk/external/pico_sdk_import.cmake")
    set(PICO_SDK_PATH "$ENV{HOME}/pico-sdk")
else()
    message(FATAL_ERROR
        "Pico SDK not found. Set PICO_SDK_PATH or clone to ~/pico-sdk:\n"
        "  git clone --depth=1 https://github.com/raspberrypi/pico-sdk ~/pico-sdk")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Locate pico-extras (provides pico_audio_i2s, only needed for the I2S DAC build).
if(DEFINED ENV{PICO_EXTRAS_PATH})
    set(PICO_EXTRAS_PATH "$ENV{PICO_EXTRAS_PATH}")
elseif(EXISTS "$ENV{HOME}/pico-extras/external/pico_extras_import.cmake")
    set(PICO_EXTRAS_PATH "$ENV{HOME}/pico-extras")
endif()
if(PICO_EXTRAS_PATH)
    include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
endif()

# Must be set before pico_sdk_init() so pico_cxx_options doesn't add -fno-exceptions.
set(PICO_CXX_ENABLE_EXCEPTIONS 1)
# RTTI is explicitly disabled below; align pico_cxx_options with that.
set(PICO_CXX_ENABLE_RTTI 0)

project(sharptalk_rp2040 C CXX ASM)
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 11)

pico_sdk_init()

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(sharptalk_demo
    main.cpp
    ${ROOT}/src/Tables.cpp
    ${ROOT}/src/KlattSynthesizerFP.cpp
    ${ROOT}/src/PitchInterpolator.cpp
    ${ROOT}/src/AudioProcessor.cpp
    ${ROOT}/src/SpeechRenderer.cpp
    ${ROOT}/src/TtsEngine.cpp
    ${ROOT}/src/Phonemizer.cpp
    ${ROOT}/src/LetterToSound.cpp
    ${ROOT}/src/DictionaryReader.cpp
    ${ROOT}/src/LibraryDataDictionary.cpp
    ${ROOT}/src/LibraryDataSymbols.cpp
    ${ROOT}/src/JapaneseParser.cpp
    ${ROOT}/src/Morphology.cpp
    ${ROOT}/src/HeteronymResolver.cpp
    ${ROOT}/src/TextCommands.cpp
    ${ROOT}/src/KlattschParser.cpp
    ${ROOT}/src/VoicePresets.cpp
)

target_include_directories(sharptalk_demo PRIVATE ${ROOT}/include)

# Exceptions on: several SharpVox constructors throw on bad input.
# RTTI off: saves ~10 KB, SharpVox uses no dynamic_cast / typeid.
# Increase the default 2 KB stack to 8 KB for LTS recursive context matching.
option(BENCH "Silence PCM output for synthesis benchmarking" OFF)
# USB CDC (default) reads the board over its own USB cable; OFF uses hardware
# UART0 on GPIO0/1, which the rp2040js simulator taps directly.
option(SHVX_TRANSPORT_USB "Stream PCM over USB CDC instead of hardware UART0" ON)
# Play synthesized PCM out an I2S DAC (DECtalkMini pinout: BCLK=26, LRCLK=27,
# DIN=28) in addition to the serial SHVX stream. Needs pico-extras.
option(SHVX_OUTPUT_I2S "Play PCM out an I2S DAC alongside the serial stream" OFF)

target_compile_options(sharptalk_demo PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    -DPICO_STACK_SIZE=0x1000
)

if(BENCH)
    target_compile_definitions(sharptalk_demo PRIVATE BENCH)
endif()

target_link_libraries(sharptalk_demo
    pico_stdlib
    hardware_uart
    m
)

# I2S DAC output. Mono int16 stream matched to the DECtalkMini pinout.
if(SHVX_OUTPUT_I2S)
    if(NOT PICO_EXTRAS_PATH)
        message(FATAL_ERROR
            "SHVX_OUTPUT_I2S needs pico-extras. Set PICO_EXTRAS_PATH or clone to ~/pico-extras:\n"
            "  git clone --depth=1 https://github.com/raspberrypi/pico-extras ~/pico-extras")
    endif()
    target_link_libraries(sharptalk_demo pico_audio_i2s)
    target_compile_definitions(sharptalk_demo PRIVATE
        SHVX_OUTPUT_I2S=1
        PICO_AUDIO_I2S_MONO_INPUT=1
        PICO_AUDIO_I2S_MONO_OUTPUT=1
        PICO_AUDIO_I2S_DATA_PIN=28
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=26
    )
else()
    target_compile_definitions(sharptalk_demo PRIVATE SHVX_OUTPUT_I2S=0)
endif()

# Select the I/O transport. The SHVX protocol bytes are identical either way.
if(SHVX_TRANSPORT_USB)
    target_compile_definitions(sharptalk_demo PRIVATE SHVX_TRANSPORT_USB=1)
    pico_enable_stdio_usb(sharptalk_demo  1)
    pico_enable_stdio_uart(sharptalk_demo 0)
else()
    target_compile_definitions(sharptalk_demo PRIVATE SHVX_TRANSPORT_USB=0)
    pico_enable_stdio_usb(sharptalk_demo  0)
    pico_enable_stdio_uart(sharptalk_demo 0)
endif()

# Emit .uf2 (for rp2040js) and .bin alongside the .elf.
pico_add_extra_outputs(sharptalk_demo)
