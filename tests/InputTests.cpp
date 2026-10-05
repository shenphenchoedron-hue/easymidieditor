#include "Test.h"
#include "input/ChordInputProcessor.h"

using namespace mc::input;

namespace mc::input { static std::ostream& operator<<(std::ostream& o, const OutputEvent& e) { return o << (e.noteOn ? "on" : "off") << e.pitch; } }

static std::vector<int> ons(const std::vector<OutputEvent>& ev)
{
    std::vector<int> r;
    for (auto& e : ev) if (e.noteOn) r.push_back(e.pitch);
    return r;
}

TEST(normal_mode_passes_single_notes_and_modifier_keys)
{
    ChordInputProcessor p;
    CHECK_EQ(ons(p.noteOn(57, 100)), (std::vector<int>{57}));
    CHECK_EQ(p.noteOff(57).size(), (size_t)1);
    CHECK_EQ(ons(p.noteOn(36, 100)), (std::vector<int>{36})); // modifier key is a normal key when chord mode off
}

TEST(chord_mode_with_modifiers)
{
    ChordInputProcessor p;
    p.setScale(9, "natural_minor");
    p.setChordMode(true);
    auto& m = p.mapping();
    CHECK(p.noteOn(m[Modifier::Inversion1], 100).empty()); // consumed, silent
    CHECK(p.noteOn(m[Modifier::Seventh], 100).empty());
    auto ev = p.noteOn(57, 90);
    CHECK_EQ(ons(ev), (std::vector<int>{55, 57, 60, 64})); // A3 stays, G drops below
    CHECK_EQ(p.displayState().currentChord, std::string("Am7 \xE2\x80\x93 1st inversion"));
    auto off = p.noteOff(57);
    CHECK_EQ(off.size(), (size_t)4);
    CHECK(p.noteOff(m[Modifier::Seventh]).empty());
    CHECK(p.noteOff(m[Modifier::Inversion1]).empty());
    CHECK_EQ(ons(p.noteOn(57, 90)), (std::vector<int>{57, 60, 64}));
}

TEST(overlapping_chords_share_notes_safely)
{
    ChordInputProcessor p;
    p.setScale(9, "natural_minor");
    p.setChordMode(true);
    p.noteOn(57, 100);              // A C E
    auto c = p.noteOn(60, 100);     // C E G : C(60) and E(64) already sounding
    CHECK_EQ(ons(c), (std::vector<int>{67}));
    auto off = p.noteOff(57);       // releases only A; C and E still held by the C chord
    CHECK_EQ(off, (std::vector<OutputEvent>{{false, 57, 0}}));
    CHECK_EQ(p.noteOff(60).size(), (size_t)3);
}

TEST(midi_learn)
{
    ChordInputProcessor p;
    p.beginLearn(Modifier::Ninth);
    CHECK(p.noteOn(48, 100).empty());
    CHECK_EQ(p.mapping()[Modifier::Ninth], 48);
    CHECK(p.takeLearnedModifier() == Modifier::Ninth);
    p.beginLearn(Modifier::Seventh);
    p.noteOn(48, 100); // same key moves to the new role
    CHECK_EQ(p.mapping()[Modifier::Seventh], 48);
    CHECK_EQ(p.mapping()[Modifier::Ninth], -1);
}
