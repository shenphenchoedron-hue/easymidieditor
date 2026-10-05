#pragma once
// Which keyboard keys act as chord modifiers. Never hardcoded: user-configurable
// and learnable. -1 = unassigned.
namespace mc::input {

enum class Modifier { Inversion1 = 0, Inversion2, Seventh, Ninth, Count };

struct ControllerMapping {
    // Defaults for a 61-key keyboard starting at C1 (36):
    // lowest white keys C1/D1 -> inversions, lowest black keys C#1/D#1 -> 7/9.
    int notes[(int)Modifier::Count] = {36, 38, 37, 39};

    int& operator[](Modifier m) { return notes[(int)m]; }
    int operator[](Modifier m) const { return notes[(int)m]; }

    int modifierFor(int midiNote) const // returns Modifier index or -1
    {
        for (int i = 0; i < (int)Modifier::Count; ++i)
            if (notes[i] >= 0 && notes[i] == midiNote) return i;
        return -1;
    }
};

inline const char* modifierName(Modifier m)
{
    switch (m) {
        case Modifier::Inversion1: return "1st inversion";
        case Modifier::Inversion2: return "2nd inversion";
        case Modifier::Seventh:    return "7";
        case Modifier::Ninth:      return "9";
        case Modifier::Count:      break;
    }
    return "?";
}

} // namespace mc::input
