cmake_minimum_required(VERSION 3.22)
project(PatchwerkModular VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local checkout in ./JUCE if present, otherwise download it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(PW_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PW_FORMATS AU)
endif()

juce_add_plugin(PatchwerkModular
    COMPANY_NAME "Patchwerk"
    BUNDLE_ID "com.patchwerk.modular"
    PLUGIN_MANUFACTURER_CODE Ptwk
    PLUGIN_CODE Pwm1
    FORMATS ${PW_FORMATS}
    PRODUCT_NAME "Patchwerk Modular"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(PatchwerkModular PRIVATE
    Source/Engine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(PatchwerkModular PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PatchwerkModular
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(PW_BUILD_TESTS "Build the offline test runner" OFF)
if(PW_BUILD_TESTS)
    juce_add_console_app(PatchwerkTest PRODUCT_NAME "PatchwerkTest")
    target_sources(PatchwerkTest PRIVATE tests/TestMain.cpp Source/Engine.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(PatchwerkTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Patchwerk" JucePlugin_Build_Standalone=0)
    target_link_libraries(PatchwerkTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
