add_library(mc_core STATIC
    theory/Scale.cpp
    theory/ChordEngine.cpp
    theory/ChordSuggestions.cpp
    model/Track.cpp
    model/Project.cpp
    model/Commands.cpp
    sequencer/Sequence.cpp
    input/ChordInputProcessor.cpp
    io/Json.cpp
    io/ProjectSerializer.cpp
    io/MidiFile.cpp
    io/MusicXml.cpp
)
target_include_directories(mc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(mc_core PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(mc_core PRIVATE /W4)
else()
    target_compile_options(mc_core PRIVATE -Wall -Wextra)
endif()
