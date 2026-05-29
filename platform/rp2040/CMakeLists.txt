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
    ${ROOT}/src/KlattSynthesizer.cpp
    ${ROOT}/src/PitchInterpolator.cpp
    ${ROOT}/src/AudioProcessor.cpp
    ${ROOT}/src/SpeechRenderer.cpp
    ${ROOT}/src/TtsEngine.cpp
    ${ROOT}/src/Phonemizer.cpp
    ${ROOT}/src/LetterToSound.cpp
    ${ROOT}/src/DictionaryReader.cpp
    ${ROOT}/src/LibraryDataDictionary.cpp
    ${ROOT}/src/LibraryDataSymbols.cpp
    ${ROOT}/src/Morphology.cpp
    ${ROOT}/src/HeteronymResolver.cpp
    ${ROOT}/src/TextCommands.cpp
    ${ROOT}/src/KlattschParser.cpp
)

target_include_directories(sharptalk_demo PRIVATE ${ROOT}/include)

# Exceptions on: several SharpVox constructors throw on bad input.
# RTTI off: saves ~10 KB, SharpVox uses no dynamic_cast / typeid.
# Increase the default 2 KB stack to 8 KB for LTS recursive context matching.
target_compile_options(sharptalk_demo PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    -DPICO_STACK_SIZE=0x1000
)

target_link_libraries(sharptalk_demo
    pico_stdlib
    hardware_uart
    m
)

# No USB or UART stdio — the demo owns UART0 directly.
pico_enable_stdio_usb(sharptalk_demo  0)
pico_enable_stdio_uart(sharptalk_demo 0)

# Emit .uf2 (for rp2040js) and .bin alongside the .elf.
pico_add_extra_outputs(sharptalk_demo)
