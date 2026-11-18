cmake_minimum_required(VERSION 3.22)
project(VegaTape VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# ---- JUCE: use a local checkout if JUCE_DIR is given, otherwise download it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(VEGA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND VEGA_FORMATS AU)
endif()

juce_add_plugin(VegaTape
    COMPANY_NAME "Vega Productions"
    BUNDLE_ID com.vegaproductions.tape
    PLUGIN_MANUFACTURER_CODE Vgap
    PLUGIN_CODE Vtp1
    FORMATS ${VEGA_FORMATS}
    PRODUCT_NAME "Vega Productions Tape"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion Delay
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(VegaTape PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(VegaTape PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VegaTape
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- optional offline DSP test:  cmake -DVEGA_TESTS=ON
option(VEGA_TESTS "Build offline DSP test" OFF)
if(VEGA_TESTS)
    juce_add_console_app(VegaOfflineTest PRODUCT_NAME "VegaOfflineTest")
    target_sources(VegaOfflineTest PRIVATE tests/OfflineTest.cpp)
    target_include_directories(VegaOfflineTest PRIVATE $<TARGET_PROPERTY:VegaTape,INCLUDE_DIRECTORIES>)
    target_compile_definitions(VegaOfflineTest PRIVATE $<TARGET_PROPERTY:VegaTape,COMPILE_DEFINITIONS>)
    target_link_libraries(VegaOfflineTest PRIVATE VegaTape)
endif()
