cmake_minimum_required(VERSION 3.22)

project(NorcoastAmbience VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS deployment target")

include(FetchContent)

# JUCE — pulled at configure time so this repo doesn't carry the framework.
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# CLAP wrapper for JUCE plugins. Optional — comment out the
# clap_juce_extensions_plugin() call below to skip CLAP.
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(clap-juce-extensions)

juce_add_plugin(NorcoastAmbience
    COMPANY_NAME              "NorCoast Audio"
    BUNDLE_ID                 com.norcoast.ambience.plugin
    PLUGIN_MANUFACTURER_CODE  NCAU
    PLUGIN_CODE               NcAm
    FORMATS                   AU VST3 Standalone
    PRODUCT_NAME              "Norcoast Ambience"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   FALSE
)

target_sources(NorcoastAmbience PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PadVoice.cpp
    Source/Waves.cpp
)

target_compile_definitions(NorcoastAmbience PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(NorcoastAmbience PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_plugin_client
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# CLAP wrapper — produces a NorcoastAmbience_CLAP target that builds
# alongside the AU/VST3/Standalone targets.
clap_juce_extensions_plugin(
    TARGET          NorcoastAmbience
    CLAP_ID         "com.norcoast.ambience.plugin"
    CLAP_FEATURES   instrument synthesizer
)
