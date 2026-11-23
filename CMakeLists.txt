cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)

project(UniverseX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static C++ runtime: users do not need to install Visual C++ redistributables.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Target Windows 7 as the minimum OS.
if (WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601 WINVER=0x0601)
endif()

# JUCE 6.1.6: pinned on purpose. Newer JUCE releases raise the minimum Windows version.
# Preferred: extract the JUCE 6.1.6 Windows zip into this folder so that
#   <this folder>/JUCE/CMakeLists.txt  exists. Otherwise JUCE 6.1.6 is downloaded.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 6.1.6
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(UniverseX
    COMPANY_NAME "YourCompany"            # TODO: change
    PLUGIN_MANUFACTURER_CODE Ycmp         # TODO: change (4 chars, first uppercase)
    PLUGIN_CODE Unvx
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3 Standalone
    PRODUCT_NAME "Universe X")

target_sources(UniverseX PRIVATE UniverseX.cpp)

# The artwork picture (UniverseX_bg.png, next to this file) is embedded in the plugin.
juce_add_binary_data(UniverseXAssets SOURCES UniverseX_bg.png)
set_target_properties(UniverseXAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_compile_definitions(UniverseX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(UniverseX
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        UniverseXAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
