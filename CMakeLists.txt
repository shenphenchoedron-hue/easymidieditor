cmake_minimum_required(VERSION 3.22)
project(MidiComposer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows: link the C++ runtime statically so users need no VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(MC_BUILD_APP   "Build the JUCE desktop application" ON)
option(MC_BUILD_TESTS "Build unit tests for the core library" ON)

# ---------------------------------------------------------------------------
# Core library: pure C++20, no JUCE. Music theory, data model, sequencing,
# serialization, SMF I/O. Fully testable without GUI/audio/plugins.
# ---------------------------------------------------------------------------
add_subdirectory(src/core)

if(MC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(MC_BUILD_APP)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
        add_subdirectory(external/JUCE EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.4
            GIT_SHALLOW ON)
        FetchContent_MakeAvailable(JUCE)
    endif()
    add_subdirectory(src/app)
endif()
