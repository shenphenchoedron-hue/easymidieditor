#include "theory/ChordSuggestions.h"

namespace mc::theory {

std::vector<ChordSuggestion> ChordSuggestions::next(const Scale& scale, int currentRoot, Extension ext, int maxCount)
{
    std::vector<ChordSuggestion> out;
    const auto deg = scale.degreeOf(currentRoot);
    if (!deg) return out;
    const int n = scale.size();

    struct Move { int steps; bool absolute; const char* reason; };
    static const Move moves[] = {
        {3, false, "Root down a fifth (circle of fifths) - strongest resolution"},
        {4, false, "Root up a fifth (circle of fifths)"},
        {5, false, "Root down a third - shares two notes"},
        {0, true,  "Back to the tonic"},
        {1, false, "Step up"},
    };

    for (auto& m : moves) {
        if ((int)out.size() >= maxCount) break;
        const int d = m.absolute ? m.steps : (*deg + m.steps) % n;
        if (d == *deg) continue;
        bool dup = false;
        for (auto& s : out) dup |= s.degree == d;
        if (dup) continue;

        // Root pitch class for degree d, placed nearest the current root (-6..+5 semitones).
        const int pc = (scale.root() + scale.semitoneOffsetOfDegree(d)) % 12;
        int diff = ((pc - currentRoot % 12) % 12 + 12) % 12;
        if (diff > 5) diff -= 12;
        const int root = currentRoot + diff;
        if (root < 0 || root > 127) continue;

        ChordSuggestion s;
        s.rootPitch = root;
        s.degree = d;
        s.symbol = ChordEngine::build(scale, {root, ext, 0}).symbol;
        s.reason = m.reason;
        out.push_back(std::move(s));
    }
    return out;
}

} // namespace mc::theory
