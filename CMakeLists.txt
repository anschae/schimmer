cmake_minimum_required(VERSION 3.24)

# ==============================================================================
# Plugin identity -- everything that names the plugin lives in this block.
# NOTE: the manufacturer/plugin codes and the bundle id identify the plugin to
# DAWs. Changing them after release makes existing projects lose the plugin.
# The manufacturer code is shared with IRene and Prog Delay.
# ==============================================================================
set(SCH_PLUGIN_NAME        "Schimmer")
set(SCH_TARGET             "Schimmer")
set(SCH_VERSION            "0.1.0")
set(SCH_COMPANY_NAME       "anschae")
set(SCH_MANUFACTURER_CODE  "Ansc")   # same as IRene / Prog Delay
set(SCH_PLUGIN_CODE        "Schm")
set(SCH_BUNDLE_ID          "ch.anschae.schimmer")
set(SCH_JUCE_TAG           "8.0.15")  # pinned JUCE release (same as Prog Delay)
set(SCH_CATCH2_TAG         "v3.16.1") # pinned Catch2 release

# ==============================================================================
# Platform settings (must be set before project())
# ==============================================================================
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures (universal)")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

project(Schimmer VERSION ${SCH_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)

option(SCH_BUILD_TESTS  "Build the Catch2 unit tests" ON)
option(SCH_BUILD_PLUGIN "Build the plugin targets (VST3/AU/Standalone)" ON)
option(SCH_BUILD_TOOLS  "Build developer tools (render tool, UI snapshot)" OFF)

# ==============================================================================
# Dependencies
# ==============================================================================
include(FetchContent)
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${SCH_JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   FALSE)
FetchContent_MakeAvailable(juce)

if(SCH_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        ${SCH_CATCH2_TAG}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   FALSE)
    FetchContent_MakeAvailable(Catch2)
endif()

# ==============================================================================
# Shared code: compiled into the plugin, the unit tests and the tools.
# (An INTERFACE library, because JUCE modules must be compiled once per final
# target; a static library would duplicate JUCE symbols.)
# ==============================================================================
add_library(SchShared INTERFACE)

file(GLOB_RECURSE SCH_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(FILTER SCH_SOURCES EXCLUDE REGEX ".*/src/PluginEntry\\.cpp$")
target_sources(SchShared INTERFACE ${SCH_SOURCES})
target_include_directories(SchShared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tools)

# The DSP hot loops benefit a lot from relaxed float maths (vectorised
# reductions, FMA). NaN/Inf checks stay valid: no -ffinite-math-only.
file(GLOB SCH_DSP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/*.cpp)
if(MSVC)
    set_source_files_properties(${SCH_DSP_SOURCES} PROPERTIES COMPILE_OPTIONS "/fp:fast")
else()
    set_source_files_properties(${SCH_DSP_SOURCES} PROPERTIES COMPILE_OPTIONS "-fno-math-errno;-funsafe-math-optimizations;-O3")
endif()

target_compile_definitions(SchShared INTERFACE
    SCH_PLUGIN_NAME="${SCH_PLUGIN_NAME}"
    SCH_COMPANY_NAME="${SCH_COMPANY_NAME}"
    SCH_VERSION_STRING="${SCH_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(SchShared INTERFACE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)

if(MSVC)
    target_compile_options(SchShared INTERFACE /W4 /bigobj)
else()
    target_compile_options(SchShared INTERFACE -Wall -Wextra)
endif()

# ==============================================================================
# Plugin
# ==============================================================================
if(SCH_BUILD_PLUGIN)
    set(SCH_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND SCH_FORMATS AU)
    endif()

    juce_add_plugin(${SCH_TARGET}
        PRODUCT_NAME                "${SCH_PLUGIN_NAME}"
        COMPANY_NAME                "${SCH_COMPANY_NAME}"
        BUNDLE_ID                   "${SCH_BUNDLE_ID}"
        PLUGIN_MANUFACTURER_CODE    ${SCH_MANUFACTURER_CODE}
        PLUGIN_CODE                 ${SCH_PLUGIN_CODE}
        FORMATS                     ${SCH_FORMATS}
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        AU_MAIN_TYPE                kAudioUnitType_Effect
        VST3_CATEGORIES             Fx Reverb
        COPY_PLUGIN_AFTER_BUILD     FALSE)

    target_sources(${SCH_TARGET} PRIVATE src/PluginEntry.cpp)
    target_link_libraries(${SCH_TARGET} PRIVATE SchShared)
    target_compile_definitions(${SCH_TARGET} PUBLIC JUCE_MODAL_LOOPS_PERMITTED=0)
endif()

# ==============================================================================
# Unit tests (Catch2)
# ==============================================================================
if(SCH_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(SchimmerTests PRODUCT_NAME "SchimmerTests")
    file(GLOB SCH_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h)
    target_sources(SchimmerTests PRIVATE ${SCH_TEST_SOURCES})
    target_link_libraries(SchimmerTests PRIVATE SchShared Catch2::Catch2)
    target_compile_definitions(SchimmerTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
    add_test(NAME SchimmerTests COMMAND SchimmerTests)
endif()

# ==============================================================================
# Developer tools (not built by default)
# ==============================================================================
if(SCH_BUILD_TOOLS)
    # Offline render tool: test signals through every factory preset -> WAV + spectrogram PNG.
    juce_add_console_app(SchimmerRender PRODUCT_NAME "SchimmerRender")
    target_sources(SchimmerRender PRIVATE tools/RenderTool.cpp)
    target_link_libraries(SchimmerRender PRIVATE SchShared)
    target_compile_definitions(SchimmerRender PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

    # Renders the editor to a PNG without a DAW: handy for checking layouts.
    juce_add_console_app(SchimmerSnapshot PRODUCT_NAME "SchimmerSnapshot")
    target_sources(SchimmerSnapshot PRIVATE tools/Snapshot.cpp)
    target_link_libraries(SchimmerSnapshot PRIVATE SchShared)
    target_compile_definitions(SchimmerSnapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
endif()
