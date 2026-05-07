cmake_minimum_required(VERSION 3.22)

project(NorcoastAmbience VERSION 0.1.0 LANGUAGES C CXX)

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
    # Auto-install built bundles into the SYSTEM-WIDE Mac plugin folders
    # so Logic / Reaper / Live see them right after a rebuild. These
    # paths usually require admin write access — if the build hits a
    # permissions error, run once: sudo chown -R $(whoami) /Library/Audio/Plug-Ins
    # (or build with sudo) so the post-build copy can succeed.
    COPY_PLUGIN_AFTER_BUILD   TRUE
    AU_COPY_DIR               "/Library/Audio/Plug-Ins/Components"
    VST3_COPY_DIR             "/Library/Audio/Plug-Ins/VST3"
    # App / standalone bundle icon — rendered from public/icons/icon.svg.
    # JUCE expects PNGs and packs both sizes into the AU/VST3/Standalone
    # bundles; macOS Finder / DAW preset menus pick the larger one.
    ICON_BIG                  "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon/icon-256.png"
    ICON_SMALL                "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon/icon-64.png"
)

target_sources(NorcoastAmbience PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PadVoice.cpp
    Source/Waves.cpp
    Source/Texture.cpp
)

# Embed dulcimer.wav so the granular Texture layer doesn't need an
# external file at runtime. The generated NorcoastBinaryData target
# exposes BinaryData::dulcimer_wav and BinaryData::dulcimer_wavSize.
juce_add_binary_data(NorcoastBinaryData
    HEADER_NAME BinaryData.h
    NAMESPACE   BinaryData
    SOURCES     Resources/dulcimer.wav)

target_compile_definitions(NorcoastAmbience PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(NorcoastAmbience PRIVATE
    NorcoastBinaryData
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_formats
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
