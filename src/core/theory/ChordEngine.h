#pragma once
// Chord Engine: builds diatonic chords by stacking every other scale degree.
// No chord tables: quality emerges from the scale's interval structure.
#include "theory/Scale.h"
#include <string>
#include <vector>

namespace mc::theory {

enum class Extension { Triad = 0, Seventh = 1, Ninth = 2 };

struct ChordRequest {
    int playedNote = 60;          // MIDI note of the chord root as played
    Extension extension = Extension::Triad;
    int inversion = 0;            // 0 = root position, 1 = 1st, 2 = 2nd (higher values allowed)
};

struct ChordResult {
    bool inScale = false;         // false -> notes == {playedNote}, no chord built
    std::vector<int> notes;       // ascending MIDI notes, ready to play/insert
    std::string symbol;           // "Am7", "Bdim", "Cmaj9"...
    std::string description;      // "Am7 – 1st inversion"
};

class ChordEngine {
public:
    static ChordResult build(const Scale& scale, const ChordRequest& req);
    // Name a chord from its root-position stack (root first). Exposed for tests.
    static std::string symbolFor(int rootMidi, const std::vector<int>& rootPositionNotes);
    static std::string inversionName(int inversion);
};

} // namespace mc::theory
