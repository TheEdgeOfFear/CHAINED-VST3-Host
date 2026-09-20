cmake_minimum_required(VERSION 3.22)

# If built standalone without parent project:
if(NOT TARGET juce::juce_audio_basics)
    project(Chained VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# Create Standalone GUI Application
juce_add_gui_app(CHAINED
    COMPANY_NAME "THE EDGE OF FEAR"
    BUNDLE_ID com.theedgeoffear.chained
    PRODUCT_NAME "CHAINED"
    VERSION 1.0.0
)

# Source files
target_sources(CHAINED
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/MainComponent.h
        Source/ChainedAudioEngine.cpp
        Source/ChainedAudioEngine.h
        Source/PluginSlot.h
        Source/ChainedPlayHead.h
        Source/TimeStretchEngine.h
        Source/BackingTrackPlayer.h
        Source/ChainedMidiManager.h
        Source/ChainedLookAndFeel.h
        Source/PatchCableOverlay.h
        Source/WaveformLooperComponent.h
        Source/PluginSlotComponent.h
)

# Binary asset for background image
juce_add_binary_data(CHAINED_Assets
    SOURCES
        "background image/CHAINED.jpg"
)

# Link essential JUCE modules (including audio hosting and formats)
target_link_libraries(CHAINED
    PRIVATE
        CHAINED_Assets
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Compile definitions for VST3 hosting, MP3 support and ASIO
target_compile_definitions(CHAINED
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_VST=0
        JUCE_ASIO=1
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=1
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_USE_OGGVORBIS=1
        JUCE_USE_FLAC=1
)

target_include_directories(CHAINED
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../asiosdk/common"
        "${CMAKE_CURRENT_SOURCE_DIR}/../asiosdk/asiosdk_2.3.3_2019-06-14/common"
)

if(MSVC)
    target_compile_options(CHAINED PUBLIC /utf-8 /MP)
endif()
