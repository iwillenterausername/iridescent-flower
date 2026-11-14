cmake_minimum_required(VERSION 3.22)

project(IridescentFlower VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(NOT JUCE_PATH AND DEFINED ENV{JUCE_PATH})
    set(JUCE_PATH "$ENV{JUCE_PATH}" CACHE PATH "Path to a local JUCE checkout (optional)" FORCE)
endif()

if(JUCE_PATH)
    add_subdirectory("${JUCE_PATH}" JUCE)
else()
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.13
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(IridescentAssets
    SOURCES
        Assets/portrait_pixel.png
        Assets/texture_pixel.png
        Assets/header_pixel.png
        Assets/avatar_b_pixel.png
        Assets/avatar_c_pixel.png
        Assets/avatar_d_pixel.png
        Assets/Fonts/Orbitron.ttf
        Assets/Fonts/RubikGlitch.ttf
        Assets/Fonts/CormorantGaramond.ttf
        Assets/Fonts/PressStart2P.ttf
        Assets/Presets/pad_easystrings.iflower
        Assets/Presets/pluck_easygrains.iflower
        Assets/Presets/bass_alittlebitwide.iflower
        Assets/Presets/key_spacebits.iflower
        Assets/Presets/drum_stereo.iflower)

juce_add_plugin(IridescentFlower
    COMPANY_NAME "LeGui Studio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Guil
    PLUGIN_CODE Irid
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx Delay Reverb
    PRODUCT_NAME "Iridescent Flower"
    DESCRIPTION "A multi-bus modular VST3 effect rack with independent per-slot parameters, morphing and custom DLL modules."
    BUNDLE_ID "com.leguistudio.iridescentflower"
    VERSION "1.1.0")

target_sources(IridescentFlower
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/RackEngine.cpp
        Source/DSP/MorphModule.cpp
        Source/DSP/CompressorModule.cpp
        Source/DSP/EqModule.cpp
        Source/DSP/FilterModule.cpp
        Source/DSP/ChorusModule.cpp
        Source/DSP/DistortionModule.cpp
        Source/DSP/HyperModule.cpp
        Source/DSP/LfoModule.cpp
        Source/DSP/EchoModule.cpp
        Source/DSP/GrainModule.cpp
        Source/DSP/CloudModule.cpp
        Source/DSP/SlotState.cpp
        Source/DSP/CustomModule.cpp
        Source/CustomModuleRegistry.cpp
        Source/UI/IridescentLookAndFeel.cpp
        Source/UI/FilterGraph.cpp
        Source/UI/EqGraph.cpp)

target_include_directories(IridescentFlower PRIVATE Source)

target_compile_definitions(IridescentFlower
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_PLUGINHOST_VST3=1)

if (MSVC)
    target_compile_options(IridescentFlower PRIVATE /utf-8 /MP)
endif()

target_link_libraries(IridescentFlower
    PRIVATE
        juce::juce_audio_utils
        IridescentAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Custom module example: the Copperline BBD delay built as a user DLL.
add_library(IridescentBbdCustom SHARED
    CustomModules/Bbd/BbdBridge.cpp
    CustomModules/Bbd/BbdEngine.cpp)

target_include_directories(IridescentBbdCustom PRIVATE Source CustomModules/Bbd)
target_compile_features(IridescentBbdCustom PRIVATE cxx_std_17)
target_compile_definitions(IridescentBbdCustom PRIVATE JUCE_STANDALONE_APPLICATION=0)
target_link_libraries(IridescentBbdCustom PRIVATE juce::juce_audio_basics juce::juce_core)

if (MSVC)
    target_compile_options(IridescentBbdCustom PRIVATE /utf-8 /MP)
endif()

# Offline test / VST3 host.
juce_add_console_app(IridescentTests PRODUCT_NAME "IridescentTests")

target_sources(IridescentTests PRIVATE Tests/IridescentTests.cpp)
target_include_directories(IridescentTests PRIVATE Source)

if (MSVC)
    target_compile_options(IridescentTests PRIVATE /utf-8 /MP /wd4819)
endif()

target_compile_definitions(IridescentTests
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_Name="Iridescent Flower")

target_link_libraries(IridescentTests
    PRIVATE
        IridescentFlower
        IridescentAssets
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

