#pragma once
// MusicXML 4.0 (partwise, uncompressed .musicxml) export, dependency-free.
// Opens in MuseScore, Sibelius, Finale, Dorico, ...
//
// Notation strategy:
//  - Notes are quantized to 16th notes.
//  - One part per MIDI track. Tracks spanning both sides of middle C get a
//    grand staff (split at C4), otherwise a single treble or bass staff.
//  - Each staff is one voice: the timeline is sliced at every note start/end
//    and barline; each slice becomes a chord (or rest), and notes that continue
//    across slices are tied. Slice lengths are written as the largest fitting
//    plain/dotted note values, tied together.
//  - Key signature follows the project's harmony (root + scale/mode).
#include "model/Project.h"
#include <string>

namespace mc::io {

struct MusicXmlImportResult {
    bool ok = false;
    std::string error;
    int tracksCreated = 0;
    bool hasKey = false;                // a key signature was found
    model::HarmonySettings harmony;     // root + "major"/"natural_minor" from the first key signature
};

class MusicXml {
public:
    static std::string write(const model::Project&);
    static bool writeFile(const model::Project&, const std::string& path, std::string* error = nullptr);

    // Import of uncompressed MusicXML (partwise). Appends one MIDI track per
    // <part> to `project`; sets tempo/time signature if `adoptTempo`.
    // Notes, chords, rests, ties, <backup>/<forward> (multiple voices/staves)
    // and divisions changes are handled. Repeats are not unrolled; grace and
    // cue notes are skipped. (.mxl is a zip: unpack it before calling this.)
    static MusicXmlImportResult read(const std::string& xml, model::Project& project, bool adoptTempo = true);

    // Exposed for tests: number of sharps (>0) / flats (<0) for the project key.
    static int keyFifths(const model::HarmonySettings&);
};

} // namespace mc::io
