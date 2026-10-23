cmake_minimum_required(VERSION 3.22)

project(TwelveStringer VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# --- JUCE --------------------------------------------------------------------
# Uses a local JUCE checkout if you pass -DJUCE_PATH=/path/to/JUCE,
# otherwise downloads JUCE 8.0.9 automatically.
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Plugin ------------------------------------------------------------------
juce_add_plugin(TwelveStringer
    COMPANY_NAME                "Peter Eardley"
    PRODUCT_NAME                "TwelveStringer"
    PLUGIN_MANUFACTURER_CODE    Twsg
    PLUGIN_CODE                 T12s
    FORMATS                     VST3 AU Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Fx" "Modulation"
    AU_MAIN_TYPE                kAudioUnitType_Effect
    BUNDLE_ID                   "com.twelvestringer.plugin")

target_sources(TwelveStringer PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_include_directories(TwelveStringer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(TwelveStringer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(TwelveStringer
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
