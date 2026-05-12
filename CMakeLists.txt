cmake_minimum_required(VERSION 3.25)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(JuceBoyAdvance VERSION 0.1.0)

set(CMAKE_COLOR_DIAGNOSTICS ON)

if (WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE INTERNAL "")
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include (CPM)

CPMAddPackage(
        NAME SDL2
        VERSION 2.28.5
        GITHUB_REPOSITORY libsdl-org/SDL
        GIT_TAG release-2.28.5
        OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_VIDEO OFF"
        "SDL_RENDER OFF"
        "SDL_AUDIO OFF"
        "SDL_FILESYSTEM OFF"
        "SDL_THREADS ON"
        "SDL_TIMERS ON"
        "SDL_FILE OFF"
        "SDL_CPUINFO OFF"
        "SDL_HAPTIC OFF"
        "SDL_JOYSTICK ON"
        "SDL_GAMECONTROLLER ON"
        "SDL_HIDAPI ON"
        "SDL_EVENTS ON"
        "SDL_LOCALE OFF"
        "SDL_POWER OFF"
        "SDL_SENSOR OFF"
)

add_subdirectory(external/JUCE)
add_subdirectory(external/clap-juce-extensions EXCLUDE_FROM_ALL)
add_subdirectory(external/nba)

juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME "Lo-Bit Club"
    BUNDLE_ID "com.lobit.jba"
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE LOBT
    PLUGIN_CODE LJBA
    FORMATS Standalone AU VST3 CLAP
    PRODUCT_NAME "JuceBoyAdvance"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
)

clap_juce_extensions_plugin(TARGET ${PROJECT_NAME}
    CLAP_ID "com.lobit.jba"
    CLAP_FEATURES instrument synthesizer
)

file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")

if (APPLE)
    file(GLOB_RECURSE ObjCppFiles CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/source/*.mm")
    list(APPEND SourceFiles ${ObjCppFiles})
endif ()

target_sources(${PROJECT_NAME} PRIVATE ${SourceFiles})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /Zc:__cplusplus)
endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    PRODUCT_NAME_WITHOUT_VERSION="JuceBoyAdvance"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    nba
    SDL2-static
    juce_audio_utils
    juce_audio_formats
    juce_audio_processors
    juce_gui_basics
    juce_gui_extra
    juce_animation
    clap_juce_extensions
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
