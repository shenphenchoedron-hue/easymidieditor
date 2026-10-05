#include "theory/ChordEngine.h"
#include <algorithm>

namespace mc::theory {

ChordResult ChordEngine::build(const Scale& scale, const ChordRequest& req)
{
    ChordResult r;
    const auto degree = scale.degreeOf(req.playedNote);
    if (!degree) {
        r.notes = {req.playedNote};
        r.symbol = midiNoteName(req.playedNote);
        r.description = r.symbol + " (not in scale)";
        return r;
    }
    r.inScale = true;

    // Stack degree d, d+2, d+4 (+6 for 7th, +8 for 9th).
    const int voices = 3 + (int)req.extension;
    const int base = scale.semitoneOffsetOfDegree(*degree);
    std::vector<int> notes;
    for (int k = 0; k < voices; ++k)
        notes.push_back(req.playedNote + scale.semitoneOffsetOfDegree(*degree + 2 * k) - base);

    r.symbol = symbolFor(req.playedNote, notes);

    // Inversion: move the lowest note up an octave, repeatedly.
    const int inv = std::clamp(req.inversion, 0, voices - 1);
    for (int i = 0; i < inv; ++i) {
        std::sort(notes.begin(), notes.end());
        int low = notes.front();
        const int high = notes.back();
        while (low <= high) low += 12;
        notes.front() = low;
    }
    std::sort(notes.begin(), notes.end());
    notes.erase(std::remove_if(notes.begin(), notes.end(), [](int n) { return n < 0 || n > 127; }), notes.end());

    r.notes = notes;
    r.description = inv == 0 ? r.symbol : r.symbol + " \xE2\x80\x93 " + inversionName(inv);
    return r;
}

std::string ChordEngine::symbolFor(int root, const std::vector<int>& n)
{
    const std::string rootName = pitchClassName(root);
    if (n.size() < 3) return rootName;
    const int third = n[1] - root, fifth = n[2] - root;

    std::string triad; // suffix for triads
    enum { Maj, Min, Dim, Aug, Other } q = Other;
    if (third == 4 && fifth == 7) q = Maj;
    else if (third == 3 && fifth == 7) q = Min;
    else if (third == 3 && fifth == 6) q = Dim;
    else if (third == 4 && fifth == 8) q = Aug;

    if (n.size() == 3) {
        switch (q) {
            case Maj: return rootName;
            case Min: return rootName + "m";
            case Dim: return rootName + "dim";
            case Aug: return rootName + "aug";
            default:  return rootName + "(?)";
        }
    }

    const int seventh = n[3] - root;
    const bool ninth = n.size() >= 5;
    const std::string num = ninth ? "9" : "7";
    std::string s;
    if (q == Maj && seventh == 11)      s = rootName + "maj" + num;
    else if (q == Maj && seventh == 10) s = rootName + num;
    else if (q == Min && seventh == 10) s = rootName + "m" + num;
    else if (q == Min && seventh == 11) s = rootName + "m(maj" + num + ")";
    else if (q == Dim && seventh == 10) s = rootName + "m" + num + "b5";
    else if (q == Dim && seventh == 9)  s = rootName + "dim" + num;
    else if (q == Aug && seventh == 11) s = rootName + "maj" + num + "#5";
    else if (q == Aug && seventh == 10) s = rootName + num + "#5";
    else                                 s = rootName + "(" + num + "?)";

    if (ninth) {
        const int nin = n[4] - root;
        if (nin == 13) s += "(b9)";
        else if (nin == 15) s += "(#9)";
    }
    return s;
}

std::string ChordEngine::inversionName(int inv)
{
    switch (inv) {
        case 0: return "root position";
        case 1: return "1st inversion";
        case 2: return "2nd inversion";
        case 3: return "3rd inversion";
        default: return std::to_string(inv) + "th inversion";
    }
}

} // namespace mc::theory
