#pragma once
// Standard MIDI File (.mid) import/export, dependency-free.
#include "model/Project.h"
#include <cstdint>
#include <string>
#include <vector>

namespace mc::io {

struct MidiImportResult {
    bool ok = false;
    std::string error;
    int tracksCreated = 0;
};

class MidiFile {
public:
    // Export all MIDI tracks as SMF format 1 (tempo/time signature in track 0).
    static std::vector<std::uint8_t> write(const model::Project&);
    static bool writeFile(const model::Project&, const std::string& path, std::string* error = nullptr);

    // Appends tracks to `project` (one per SMF track, split per channel when a
    // track carries several channels). Sets tempo/time signature from the file
    // if `adoptTempo` is true.
    static MidiImportResult read(const std::vector<std::uint8_t>& data, model::Project& project, bool adoptTempo = true);
    static MidiImportResult readFile(const std::string& path, model::Project& project, bool adoptTempo = true);
};

} // namespace mc::io
