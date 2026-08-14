set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

idf_component_register(
    SRCS
        "${ROOT}/src/Tables.cpp"
        "${ROOT}/src/KlattSynthesizerFP.cpp"
        "${ROOT}/src/PitchInterpolator.cpp"
        "${ROOT}/src/AudioProcessor.cpp"
        "${ROOT}/src/SpeechRenderer.cpp"
        "${ROOT}/src/TtsEngine.cpp"
        "${ROOT}/src/Phonemizer.cpp"
        "${ROOT}/src/G2PModel.cpp"
        "${ROOT}/src/DictionaryReader.cpp"
        "${ROOT}/src/LibraryDataDictionary.cpp"
        "${ROOT}/src/LibraryDataSymbols.cpp"
        "${ROOT}/src/JapaneseParser.cpp"
        "${ROOT}/src/Morphology.cpp"
        "${ROOT}/src/HeteronymResolver.cpp"
        "${ROOT}/src/TextCommands.cpp"
        "${ROOT}/src/KlattschParser.cpp"
        "${ROOT}/src/VoicePresets.cpp"
        "sharpvox_wrapper.cpp"
    INCLUDE_DIRS
        "${ROOT}/include"
        "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    "-std=c++17" "-fexceptions" "-fno-rtti" "-O3" "-ffast-math")
target_compile_definitions(${COMPONENT_LIB} PRIVATE _HAS_EXCEPTIONS=1)
