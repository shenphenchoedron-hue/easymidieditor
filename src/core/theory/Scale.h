#pragma once
// Scale Engine: scales are defined purely by their interval structure.
// Adding a scale = adding one ScaleDefinition to the registry.
#include <optional>
#include <string>
#include <vector>

namespace mc::theory {

using PitchClass = int; // 0 = C ... 11 = B

struct ScaleDefinition {
    std::string id;              // stable id used in project files, e.g. "natural_minor"
    std::string displayName;     // e.g. "Natural Minor"
    std::vector<int> intervals;  // semitones from root, ascending, first = 0
};

class ScaleRegistry {
public:
    static const ScaleRegistry& instance();
    const std::vector<ScaleDefinition>& all() const { return scales_; }
    const ScaleDefinition* find(const std::string& id) const;
    const ScaleDefinition& byIdOrDefault(const std::string& id) const;
private:
    ScaleRegistry();
    std::vector<ScaleDefinition> scales_;
};

class Scale {
public:
    Scale(PitchClass root, const ScaleDefinition& def);

    PitchClass root() const { return root_; }
    const ScaleDefinition& definition() const { return *def_; }
    int size() const { return (int)def_->intervals.size(); }

    std::vector<PitchClass> pitchClasses() const;          // e.g. A minor -> 9,11,0,2,4,5,7
    std::optional<int> degreeOf(int midiNote) const;       // 0-based scale degree, nullopt if not in scale
    bool contains(int midiNote) const { return degreeOf(midiNote).has_value(); }

    // Semitone offset (relative to the root of the octave containing degree 0)
    // of an arbitrary, possibly >= size(), degree index. Wraps octaves.
    int semitoneOffsetOfDegree(int degree) const;

    std::string name() const; // "A Natural Minor"

private:
    PitchClass root_;
    const ScaleDefinition* def_;
};

std::string pitchClassName(PitchClass pc);           // sharps: C, C#, D ...
std::optional<PitchClass> parsePitchClass(const std::string& s);
std::string midiNoteName(int midiNote);              // "A3" (C4 = 60)

} // namespace mc::theory
