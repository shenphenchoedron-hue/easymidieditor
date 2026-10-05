#include "Test.h"
#include "theory/ChordSuggestions.h"

using namespace mc::theory;

namespace {
std::vector<std::string> symbols(const std::vector<ChordSuggestion>& v)
{
    std::vector<std::string> s;
    for (auto& x : v) s.push_back(x.symbol);
    return s;
}
const Scale cMajor(0, *ScaleRegistry::instance().find("major"));
const Scale aMinor(9, *ScaleRegistry::instance().find("natural_minor"));
}

TEST(suggest_after_g_in_c_major_resolves_to_c)
{
    auto s = ChordSuggestions::next(cMajor, 67); // G
    CHECK_EQ(s.front().symbol, std::string("C"));
    CHECK_EQ(symbols(s), (std::vector<std::string>{"C", "Dm", "Em", "Am"}));
}

TEST(suggest_after_c_in_c_major)
{
    // fifth down F, fifth up G, third down Am, (tonic skipped: is current), step up Dm
    CHECK_EQ(symbols(ChordSuggestions::next(cMajor, 60)), (std::vector<std::string>{"F", "G", "Am", "Dm"}));
}

TEST(suggest_after_am_in_a_minor)
{
    CHECK_EQ(symbols(ChordSuggestions::next(aMinor, 57)), (std::vector<std::string>{"Dm", "Em", "F", "Bdim"}));
}

TEST(suggest_roots_stay_near_current_chord)
{
    for (auto& s : ChordSuggestions::next(cMajor, 60)) CHECK(s.rootPitch >= 54 && s.rootPitch <= 65);
}

TEST(suggest_nothing_outside_scale)
{
    CHECK(ChordSuggestions::next(cMajor, 61).empty());
}

TEST(suggest_sevenths)
{
    CHECK_EQ(ChordSuggestions::next(cMajor, 62, Extension::Seventh).front().symbol, std::string("G7")); // Dm7 -> G7
}
