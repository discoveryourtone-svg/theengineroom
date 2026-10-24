cmake_minimum_required(VERSION 3.22)

project(EngineRoom VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary (Apple Silicon + Intel) on macOS
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# Use a local JUCE checkout if present, otherwise download it.
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

# ── Fonts (Jost, SIL Open Font License) ─────────────────────────────────────
juce_add_binary_data(EngineRoomAssets SOURCES
    Resources/Jost-400.ttf
    Resources/Jost-600.ttf)

# ── The plugin ──────────────────────────────────────────────────────────────
set(ER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND ER_FORMATS AU)
endif()

juce_add_plugin(EngineRoom
    COMPANY_NAME "Discover Your Tone"
    COMPANY_WEBSITE "https://discoveryourtone.com"
    BUNDLE_ID com.discoveryourtone.engineroom
    PLUGIN_MANUFACTURER_CODE Dytn
    PLUGIN_CODE Engr
    PRODUCT_NAME "The Engine Room"
    DESCRIPTION "Overdrive, distortion and fuzz you can see."
    FORMATS ${ER_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    ICON_BIG Resources/AppIcon.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(EngineRoom PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DriveEngine.cpp
    Source/DytLookAndFeel.cpp
    Source/Licence.cpp
    Source/ActivationPanel.cpp
    ThirdParty/monocypher/monocypher.c
    ThirdParty/monocypher/monocypher-ed25519.c)

target_compile_definitions(EngineRoom PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(EngineRoom
    PRIVATE
        EngineRoomAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

set_source_files_properties(
    ThirdParty/monocypher/monocypher.c
    ThirdParty/monocypher/monocypher-ed25519.c
    PROPERTIES COMPILE_OPTIONS "$<$<C_COMPILER_ID:MSVC>:/W0>;$<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>")

# ── Offline test tool: renders audio for each mode + a UI snapshot ──────────
option(ER_BUILD_TESTS "Build the offline render/snapshot tool" OFF)
if(ER_BUILD_TESTS)
    juce_add_console_app(EngineRoomRender PRODUCT_NAME "EngineRoomRender")
    target_sources(EngineRoomRender PRIVATE
        Tests/RenderMain.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DriveEngine.cpp
        Source/DytLookAndFeel.cpp
        Source/Licence.cpp
        Source/ActivationPanel.cpp
        ThirdParty/monocypher/monocypher.c
        ThirdParty/monocypher/monocypher-ed25519.c)
    target_compile_definitions(EngineRoomRender PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 ER_STANDALONE_TOOL=1)
    target_link_libraries(EngineRoomRender PRIVATE
        EngineRoomAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
