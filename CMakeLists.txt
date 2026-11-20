cmake_minimum_required(VERSION 3.22)

project(SoundCanvas VERSION 1.0.1 LANGUAGES C CXX) # x-release-please-version

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE" "${CMAKE_CURRENT_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

set(SOUNDCANVAS_CORE_SOURCES
    src/core/ImageCanvas.cpp
)

set(SOUNDCANVAS_SYNTH_SOURCES
    src/synth/PolySynthEngine.cpp
    src/synth/SynthPluginProcessor.cpp
    src/synth/SynthCanvasComponent.cpp
    src/synth/SynthPluginEditor.cpp
    src/synth/SynthPluginEntry.cpp
)

set(SOUNDCANVAS_CONTROL_SOURCES
    src/control/ImageMidiControlEngine.cpp
    src/control/ControlPluginProcessor.cpp
    src/control/ControlCanvasComponent.cpp
    src/control/ModuleRackComponent.cpp
    src/control/ControlPluginEditor.cpp
    src/control/ControlPluginEntry.cpp
)

if(CMAKE_CROSSCOMPILING)
    set(SOUNDCANVAS_VST3_AUTO_MANIFEST FALSE)
else()
    set(SOUNDCANVAS_VST3_AUTO_MANIFEST TRUE)
endif()

juce_add_plugin(SoundCanvasSynth
    COMPANY_NAME "SoundCanvasAudio"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST ${SOUNDCANVAS_VST3_AUTO_MANIFEST}
    PLUGIN_MANUFACTURER_CODE ScAu
    PLUGIN_CODE ScSy
    FORMATS VST3 Standalone
    PRODUCT_NAME "Sound Canvas Synth"
)

juce_generate_juce_header(SoundCanvasSynth)

target_sources(SoundCanvasSynth PRIVATE
    ${SOUNDCANVAS_CORE_SOURCES}
    ${SOUNDCANVAS_SYNTH_SOURCES}
)

target_include_directories(SoundCanvasSynth PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(SoundCanvasSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(SoundCanvasSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_graphics
        juce::juce_data_structures
        juce::juce_events
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(MINGW)
    target_link_options(SoundCanvasSynth PUBLIC
        -static-libgcc
        -static-libstdc++
        -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic
    )
endif()

juce_add_plugin(SoundCanvasControl
    COMPANY_NAME "SoundCanvasAudio"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST ${SOUNDCANVAS_VST3_AUTO_MANIFEST}
    PLUGIN_MANUFACTURER_CODE ScAu
    PLUGIN_CODE ScCt
    FORMATS VST3 Standalone
    PRODUCT_NAME "Sound Canvas Control"
)

juce_generate_juce_header(SoundCanvasControl)

target_sources(SoundCanvasControl PRIVATE
    ${SOUNDCANVAS_CORE_SOURCES}
    ${SOUNDCANVAS_CONTROL_SOURCES}
)

target_include_directories(SoundCanvasControl PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(SoundCanvasControl PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(SoundCanvasControl
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_graphics
        juce::juce_data_structures
        juce::juce_events
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(MINGW)
    target_link_options(SoundCanvasControl PUBLIC
        -static-libgcc
        -static-libstdc++
        -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic
    )
endif()

option(BUILD_SOUNDCANVAS_TESTS "Build automated tests for Sound Canvas" ON)

if(BUILD_SOUNDCANVAS_TESTS AND NOT CMAKE_CROSSCOMPILING)
    juce_add_console_app(SoundCanvasTests
        PRODUCT_NAME "SoundCanvasTests"
    )

    juce_generate_juce_header(SoundCanvasTests)

    target_sources(SoundCanvasTests PRIVATE
        ${SOUNDCANVAS_CORE_SOURCES}
        ${SOUNDCANVAS_SYNTH_SOURCES}
        ${SOUNDCANVAS_CONTROL_SOURCES}
        src/tests/SoundCanvasTests.cpp
    )

    target_include_directories(SoundCanvasTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )

    target_compile_definitions(SoundCanvasTests PRIVATE
        SOUNDCANVAS_NO_CREATE_PLUGIN_FILTER=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Sound Canvas"
    )

    target_link_libraries(SoundCanvasTests PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_graphics
        juce::juce_data_structures
        juce::juce_events
        juce::juce_core
        juce::juce_recommended_config_flags
    )
endif()
