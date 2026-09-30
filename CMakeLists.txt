cmake_minimum_required(VERSION 3.22)
project(SDNASuperAmp VERSION 0.7.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Self-contained Microsoft runtime for Windows builds.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.11
    GIT_SHALLOW TRUE)
FetchContent_Declare(NAMCore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG v0.5.2
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE TRUE)

FetchContent_MakeAvailable(JUCE)
FetchContent_GetProperties(NAMCore)
if(NOT namcore_POPULATED)
    FetchContent_Populate(NAMCore)
endif()

file(GLOB_RECURSE NAM_SOURCES CONFIGURE_DEPENDS "${namcore_SOURCE_DIR}/NAM/*.cpp")
add_library(sdna_nam_core OBJECT ${NAM_SOURCES})
target_include_directories(sdna_nam_core PUBLIC
    "${namcore_SOURCE_DIR}"
    "${namcore_SOURCE_DIR}/Dependencies/eigen"
    "${namcore_SOURCE_DIR}/Dependencies/nlohmann"
    "${namcore_SOURCE_DIR}/Dependencies/AudioDSPTools")
target_compile_definitions(sdna_nam_core PUBLIC NAM_SAMPLE_FLOAT)
set_target_properties(sdna_nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(WIN32)
    function(_juce_generate_icon source_target dest_target)
        set(source_icon "${CMAKE_CURRENT_SOURCE_DIR}/Assets/SonicDNAIconWindows.ico")
        get_target_property(juce_library_code ${source_target} JUCE_GENERATED_SOURCES_DIRECTORY)
        set(icon_file "${juce_library_code}/SonicDNAIconWindows.ico")
        file(MAKE_DIRECTORY "${juce_library_code}")
        configure_file("${source_icon}" "${icon_file}" COPYONLY)
        target_sources(${dest_target} PRIVATE "${icon_file}")
        set_target_properties(${source_target} ${dest_target} PROPERTIES
            JUCE_ICON_FILE "${icon_file}" RESOURCE "${icon_file}")
    endfunction()
    set(SDNA_ICON_ARGS)
else()
    set(SDNA_ICON_ARGS ICON_BIG "Assets/SonicDNAIcon.png" ICON_SMALL "Assets/SonicDNAIcon.png")
endif()

juce_add_plugin(SDNASuperAmp
    COMPANY_NAME "SonicDNA"
    BUNDLE_ID "com.sonicdna.superamp"
    PLUGIN_MANUFACTURER_CODE Sdna
    PLUGIN_CODE Samp
    ${SDNA_ICON_ARGS}
    FORMATS VST3 Standalone
    PRODUCT_NAME "SonicDNA SuperAmp"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "SonicDNA SuperAmp needs audio input to process your guitar."
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(SDNASuperAmp)
juce_add_binary_data(SDNAGraphics SOURCES Assets/SonicDNA.png)
target_sources(SDNASuperAmp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SDNASuperAmp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(WIN32)
    target_compile_definitions(SDNASuperAmp PUBLIC JUCE_ASIO=1)
endif()

target_link_libraries(SDNASuperAmp PRIVATE
    sdna_nam_core
    SDNAGraphics
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)
