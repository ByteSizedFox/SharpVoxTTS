cmake_minimum_required(VERSION 3.13)

set(PICO_PLATFORM rp2350-arm-s CACHE STRING "RP2350 ARM build")
set(PICO_BOARD pico2 CACHE STRING "Board type")

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

# Locate pico-extras (provides pico_audio_pwm).
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

project(sharptalk_rp2350 C CXX ASM)
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 11)

pico_sdk_init()

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(sharptalk_demo
    main.cpp
    # API layer; main.cpp must go through this, not TtsEngine directly.
    ${ROOT}/platform/lib/SharpVox.cpp
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

target_include_directories(sharptalk_demo
    PRIVATE ${ROOT}/include
    PRIVATE ${ROOT}/platform/lib
)

# Exceptions on: several SharpVox constructors throw on bad input.
# RTTI off: saves ~10 KB, SharpVox uses no dynamic_cast / typeid.
# Stack at the 4 KB max of the default SCRATCH_Y map; LTS recursion fits.
# Audio gain multiplier applied by the API layer (clipped at int16 bounds).
# SharpVox runs quiet by design; 2.5 puts typical speech peaks near full scale.
set(SHVX_GAIN "2.5" CACHE STRING "Speaker gain multiplier")

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
    hardware_pio
    pico_multicore
    pico_util
    m
)

# PWM speaker sink via pico-extras pico_audio_pwm: DMA streams pre-encoded PWM
# command words into a PIO state machine, so per-sample timing is jitter-free
# and the producer pool take/give paces the pipeline in real time.
# Noise shaping (2-bit dither + error diffusion) stays off: its code path
# still targets SDK1 interpolator APIs removed in SDK2.
if(NOT PICO_EXTRAS_PATH)
    message(FATAL_ERROR
        "pico_audio_pwm needs pico-extras. Set PICO_EXTRAS_PATH or clone to ~/pico-extras:\n"
        "  git clone --depth=1 https://github.com/raspberrypi/pico-extras ~/pico-extras")
endif()
target_link_libraries(sharptalk_demo pico_audio_pwm)
target_compile_definitions(sharptalk_demo PRIVATE SHVX_SPK_PIN=28)

target_compile_definitions(sharptalk_demo PRIVATE SHVX_GAIN=${SHVX_GAIN})

# WS2812 status LED on GP16 (pio1; pico_audio_pwm owns pio0 by default).
pico_generate_pio_header(sharptalk_demo ${CMAKE_CURRENT_SOURCE_DIR}/ws2812.pio)
target_compile_definitions(sharptalk_demo PRIVATE SHVX_LED_PIN=16)

# stdio routes to USB CDC only; the hardware UART is driven manually in raw
# line mode from main().
pico_enable_stdio_usb(sharptalk_demo  1)
pico_enable_stdio_uart(sharptalk_demo 0)

# Emit .uf2 (drag onto the BOOTSEL drive) and .bin alongside the .elf.
pico_add_extra_outputs(sharptalk_demo)
