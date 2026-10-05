#pragma once
// Suggests likely next chords based on root motion around the (diatonic)
// circle of fifths, the strongest harmonic motion in tonal music.
//
// Ranking, relative to the current chord's scale degree d:
//   1. d+3  root down a fifth (up a fourth)      e.g. G -> C, Am -> Dm   strongest
//   2. d+4  root up a fifth                       e.g. C -> G
//   3. d+5  root down a third (2 common tones)    e.g. Am -> F, C -> Am
//   4. tonic, if not already one of the above     "home"
//   5. d+1  step up                               e.g. F -> G
#include "theory/ChordEngine.h"
#include <string>
#include <vector>

namespace mc::theory {

struct ChordSuggestion {
    int rootPitch = 60;      // MIDI root, in the octave nearest the current chord
    int degree = 0;          // 0-based scale degree
    std::string symbol;      // "Dm", "G7", ...
    std::string reason;      // short explanation for tooltips
};

class ChordSuggestions {
public:
    // Empty if `currentRoot` is not in the scale.
    static std::vector<ChordSuggestion> next(const Scale&, int currentRoot, Extension = Extension::Triad, int maxCount = 4);
};

} // namespace mc::theory
