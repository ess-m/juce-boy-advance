cmake_minimum_required(VERSION 3.25)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(FMS VERSION 0.9.2)

set(CMAKE_COLOR_DIAGNOSTICS ON)

if (WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE INTERNAL "")
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include (CPM)

CPMAddPackage(
        NAME SDL2
        VERSION 2.32.10
        GITHUB_REPOSITORY libsdl-org/SDL
        GIT_TAG release-2.32.10
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

juce_add_binary_data(${PROJECT_NAME}_BinaryData
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/bios/gba_bios.bin
        ${CMAKE_CURRENT_SOURCE_DIR}/rom/fms-plugin.gba
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/ui-font.otf
)

if (DEFINED ENV{CI})
    set(COPY_AFTER_BUILD FALSE)
else ()
    set(COPY_AFTER_BUILD TRUE)
endif ()

option(BUILD_DEMO OFF)
if (BUILD_DEMO)
    set(ProductName "FMS Demo")
    set(BundleId "com.lobit.fms.demo")
    set(PluginCode LJBD)
else ()
    set(ProductName "FMS")
    set(BundleId "com.lobit.fms")
    set(PluginCode LJBA)
endif ()

if (APPLE)
    set(IconFile "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon-macos.png")
else ()
    set(IconFile "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon-windows.png")
endif ()
if (EXISTS "${IconFile}")
    set(PluginIcon ICON_BIG "${IconFile}")
endif ()

juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME "Lo-Bit Club"
    BUNDLE_ID "${BundleId}"
    COPY_PLUGIN_AFTER_BUILD ${COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE LOBT
    PLUGIN_CODE ${PluginCode}
    FORMATS Standalone AU VST3 CLAP
    PRODUCT_NAME "${ProductName}"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    ${PluginIcon}
)

clap_juce_extensions_plugin(TARGET ${PROJECT_NAME}
    CLAP_ID "${BundleId}"
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

    ${PROJECT_NAME}_BinaryData
)

if (DEFINED ENV{CI})
    set(env_file "${PROJECT_SOURCE_DIR}/.env")
    message("Writing ENV file for CI: ${env_file}")

    file(WRITE  "${env_file}" "PROJECT_NAME=${PROJECT_NAME}\n")
    file(APPEND "${env_file}" "PRODUCT_NAME=${ProductName}\n")
    file(APPEND "${env_file}" "VERSION=${PROJECT_VERSION}\n")
    file(APPEND "${env_file}" "BUNDLE_ID=${BundleId}\n")
    file(APPEND "${env_file}" "COMPANY_NAME=Lo-Bit Club\n")
endif ()
