#include "theory/Scale.h"
#include <array>
#include <stdexcept>

namespace mc::theory {

namespace {
int mod(int a, int m) { return ((a % m) + m) % m; }
int floorDiv(int a, int m) { return (a - mod(a, m)) / m; }
const std::array<const char*, 12> kNames{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
}

ScaleRegistry::ScaleRegistry()
{
    // Version 1 required: Major, Natural Minor. The rest come for free from intervals.
    scales_ = {
        {"major",          "Major",          {0, 2, 4, 5, 7, 9, 11}},
        {"natural_minor",  "Natural Minor",  {0, 2, 3, 5, 7, 8, 10}},
        {"harmonic_minor", "Harmonic Minor", {0, 2, 3, 5, 7, 8, 11}},
        {"melodic_minor",  "Melodic Minor",  {0, 2, 3, 5, 7, 9, 11}},
        {"dorian",         "Dorian",         {0, 2, 3, 5, 7, 9, 10}},
        {"phrygian",       "Phrygian",       {0, 1, 3, 5, 7, 8, 10}},
        {"lydian",         "Lydian",         {0, 2, 4, 6, 7, 9, 11}},
        {"mixolydian",     "Mixolydian",     {0, 2, 4, 5, 7, 9, 10}},
        {"locrian",        "Locrian",        {0, 1, 3, 5, 6, 8, 10}},
    };
}

const ScaleRegistry& ScaleRegistry::instance()
{
    static const ScaleRegistry r;
    return r;
}

const ScaleDefinition* ScaleRegistry::find(const std::string& id) const
{
    for (auto& s : scales_) if (s.id == id) return &s;
    return nullptr;
}

const ScaleDefinition& ScaleRegistry::byIdOrDefault(const std::string& id) const
{
    if (auto* s = find(id)) return *s;
    return scales_.front();
}

Scale::Scale(PitchClass root, const ScaleDefinition& def) : root_(mod(root, 12)), def_(&def)
{
    if (def.intervals.empty()) throw std::invalid_argument("empty scale");
}

std::vector<PitchClass> Scale::pitchClasses() const
{
    std::vector<PitchClass> out;
    for (int i : def_->intervals) out.push_back(mod(root_ + i, 12));
    return out;
}

std::optional<int> Scale::degreeOf(int midiNote) const
{
    const int rel = mod(midiNote - root_, 12);
    for (int d = 0; d < size(); ++d)
        if (def_->intervals[(size_t)d] == rel) return d;
    return std::nullopt;
}

int Scale::semitoneOffsetOfDegree(int degree) const
{
    const int n = size();
    return floorDiv(degree, n) * 12 + def_->intervals[(size_t)mod(degree, n)];
}

std::string Scale::name() const { return pitchClassName(root_) + " " + def_->displayName; }

std::string pitchClassName(PitchClass pc) { return kNames[(size_t)mod(pc, 12)]; }

std::optional<PitchClass> parsePitchClass(const std::string& s)
{
    for (int i = 0; i < 12; ++i) if (s == kNames[(size_t)i]) return i;
    static const std::array<const char*, 12> flats{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    for (int i = 0; i < 12; ++i) if (s == flats[(size_t)i]) return i;
    return std::nullopt;
}

std::string midiNoteName(int n) { return pitchClassName(n) + std::to_string(floorDiv(n, 12) - 1); }

} // namespace mc::theory
