#include "Test.h"
#include "theory/ChordEngine.h"

using namespace mc::theory;

namespace {
const Scale aMinor(9, *ScaleRegistry::instance().find("natural_minor"));
const Scale cMajor(0, *ScaleRegistry::instance().find("major"));
constexpr int A3 = 57, B3 = 59, C4 = 60, D4 = 62, E4 = 64, F4 = 65, G4 = 67, A4 = 69, B4 = 71;
using V = std::vector<int>;
ChordResult chord(const Scale& s, int n, Extension e = Extension::Triad, int inv = 0) { return ChordEngine::build(s, {n, e, inv}); }
}

TEST(a_minor_diatonic_triads)
{
    CHECK_EQ(chord(aMinor, A3).notes, (V{A3, C4, E4}));
    CHECK_EQ(chord(aMinor, C4).notes, (V{C4, E4, G4}));
    CHECK_EQ(chord(aMinor, D4).notes, (V{D4, F4, A4}));
    CHECK_EQ(chord(aMinor, E4).notes, (V{E4, G4, B4}));
    const char* expected[] = {"Am", "Bdim", "C", "Dm", "Em", "F", "G"};
    const int roots[] = {A3, B3, C4, D4, E4, F4, G4};
    for (int i = 0; i < 7; ++i) CHECK_EQ(chord(aMinor, roots[i]).symbol, std::string(expected[i]));
}

TEST(inversions)
{
    // The played note (A3) never moves; top notes drop below it.
    CHECK_EQ(chord(aMinor, A3, Extension::Triad, 1).notes, (V{E4 - 12, A3, C4}));
    CHECK_EQ(chord(aMinor, A3, Extension::Triad, 2).notes, (V{C4 - 12, E4 - 12, A3}));
    // C3 E3 G3 -> G2 C3 E3 -> E2 G2 C3
    CHECK_EQ(chord(cMajor, 48, Extension::Triad, 1).notes, (V{43, 48, 52}));
    CHECK_EQ(chord(cMajor, 48, Extension::Triad, 2).notes, (V{40, 43, 48}));
}

TEST(sevenths_and_ninths)
{
    auto am7 = chord(aMinor, A3, Extension::Seventh);
    CHECK_EQ(am7.notes, (V{A3, C4, E4, G4}));
    CHECK_EQ(am7.symbol, std::string("Am7"));
    auto am9 = chord(aMinor, A3, Extension::Ninth);
    CHECK_EQ(am9.notes, (V{A3, C4, E4, G4, B4}));
    CHECK_EQ(am9.symbol, std::string("Am9"));
    CHECK_EQ(chord(cMajor, G4, Extension::Seventh).symbol, std::string("G7"));
    CHECK_EQ(chord(cMajor, C4, Extension::Seventh).symbol, std::string("Cmaj7"));
    CHECK_EQ(chord(cMajor, B3, Extension::Seventh).symbol, std::string("Bm7b5"));
    CHECK_EQ(chord(aMinor, E4, Extension::Ninth).symbol, std::string("Em9(b9)"));
}

TEST(spec_example_am7_first_inversion)
{
    // Root A, Natural Minor, played A3, extension 7, inversion 1 -> G3 A3 C4 E4
    auto r = chord(aMinor, A3, Extension::Seventh, 1);
    CHECK_EQ(r.notes, (V{G4 - 12, A3, C4, E4}));
    CHECK_EQ(r.description, std::string("Am7 \xE2\x80\x93 1st inversion"));
}

TEST(not_in_scale_passes_through)
{
    auto r = chord(aMinor, 61);
    CHECK(!r.inScale);
    CHECK_EQ(r.notes, (V{61}));
}

TEST(works_for_every_root)
{
    // Major scale triad on degree 1 is always major, degree 2 minor, degree 7 diminished.
    for (int root = 0; root < 12; ++root) {
        Scale s(root, *ScaleRegistry::instance().find("major"));
        const int base = 48 + root;
        auto I = chord(s, base).notes;
        CHECK_EQ(I, (V{base, base + 4, base + 7}));
        auto ii = chord(s, base + 2).notes;
        CHECK_EQ(ii, (V{base + 2, base + 5, base + 9}));
        auto vii = chord(s, base + 11).notes;
        CHECK_EQ(vii, (V{base + 11, base + 14, base + 17}));
    }
}
