cmake_minimum_required(VERSION 3.22)
project(TwelveStringerHostTest VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(${JUCE_PATH} JUCE)

juce_add_console_app(HostTest PRODUCT_NAME "HostTest")
target_sources(HostTest PRIVATE HostTest.cpp)
target_compile_definitions(HostTest PRIVATE
    JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(HostTest PRIVATE juce::juce_audio_processors juce::juce_gui_basics
    juce::juce_recommended_config_flags)

juce_add_console_app(Snapshot PRODUCT_NAME "Snapshot")
target_sources(Snapshot PRIVATE Snapshot.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Source/PluginProcessor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../Source/PluginEditor.cpp)
target_include_directories(Snapshot PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../Source)
target_compile_definitions(Snapshot PRIVATE JucePlugin_Name="TwelveStringer"
    JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(Snapshot PRIVATE juce::juce_audio_processors juce::juce_gui_basics
    juce::juce_recommended_config_flags)
