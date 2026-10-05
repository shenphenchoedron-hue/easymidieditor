#include "Test.h"
#include "io/MusicXml.h"

using namespace mc;
using namespace mc::model;

namespace {
int count(const std::string& s, const std::string& sub)
{
    int n = 0;
    for (size_t p = s.find(sub); p != std::string::npos; p = s.find(sub, p + 1)) ++n;
    return n;
}
bool has(const std::string& s, const std::string& sub) { return s.find(sub) != std::string::npos; }
}

TEST(musicxml_key_signatures)
{
    CHECK_EQ(io::MusicXml::keyFifths({0, "major", false}), 0);          // C
    CHECK_EQ(io::MusicXml::keyFifths({9, "natural_minor", false}), 0);  // Am
    CHECK_EQ(io::MusicXml::keyFifths({7, "major", false}), 1);          // G
    CHECK_EQ(io::MusicXml::keyFifths({5, "major", false}), -1);         // F
    CHECK_EQ(io::MusicXml::keyFifths({2, "natural_minor", false}), -1); // Dm
    CHECK_EQ(io::MusicXml::keyFifths({2, "dorian", false}), 0);         // D dorian
}

TEST(musicxml_c_major_chord_in_one_measure)
{
    Project p;
    p.harmony = {0, "major", false};
    auto& t = p.addMidiTrack("Piano & Co");
    for (int pitch : {60, 64, 67}) t.addNote({0, pitch, 100, 0, 4 * kPPQ, -1}); // whole-note C E G
    const auto x = io::MusicXml::write(p);
    CHECK(has(x, "<score-partwise version=\"4.0\">"));
    CHECK(has(x, "<part-name>Piano &amp; Co</part-name>"));
    CHECK_EQ(count(x, "<measure number="), 1);
    CHECK_EQ(count(x, "<type>whole</type>"), 3);
    CHECK_EQ(count(x, "<chord/>"), 2);
    CHECK(!has(x, "<tie"));
    CHECK(has(x, "<step>E</step><octave>4</octave>"));
    CHECK(has(x, "<fifths>0</fifths><mode>major</mode>"));
}

TEST(musicxml_ties_across_barline_and_rests)
{
    Project p;
    auto& t = p.addMidiTrack("Lead");
    t.addNote({0, 72, 100, 3 * kPPQ, 2 * kPPQ, -1}); // beat 4 of bar 1 to beat 1 of bar 2
    const auto x = io::MusicXml::write(p);
    CHECK_EQ(count(x, "<measure number="), 2);
    CHECK_EQ(count(x, "<tie type=\"start\"/>"), 1);
    CHECK_EQ(count(x, "<tie type=\"stop\"/>"), 1);
    CHECK(has(x, "<rest/>"));
}

TEST(musicxml_grand_staff_for_wide_range)
{
    Project p;
    auto& t = p.addMidiTrack("Piano");
    t.addNote({0, 40, 100, 0, kPPQ, -1});
    t.addNote({0, 76, 100, 0, kPPQ, -1});
    const auto x = io::MusicXml::write(p);
    CHECK(has(x, "<staves>2</staves>"));
    CHECK(has(x, "<backup>"));
    CHECK(has(x, "<staff>2</staff>"));
}

TEST(musicxml_flat_key_spells_flats)
{
    Project p;
    p.harmony = {5, "major", false}; // F major
    p.addMidiTrack("x").addNote({0, 70, 100, 0, kPPQ, -1}); // Bb4
    const auto x = io::MusicXml::write(p);
    CHECK(has(x, "<step>B</step><alter>-1</alter><octave>4</octave>"));
}

TEST(musicxml_measure_durations_add_up)
{
    // Every measure of a single-staff part must sum to the measure length.
    Project p;
    p.timeSig = {3, 4};
    auto& t = p.addMidiTrack("x");
    t.addNote({0, 60, 100, 100, 700, -1});   // off-grid, gets quantized
    t.addNote({0, 64, 100, 500, 2000, -1});  // overlaps, crosses barline
    const auto x = io::MusicXml::write(p);
    size_t pos = 0;
    while ((pos = x.find("<measure number=", pos)) != std::string::npos) {
        const auto end = x.find("</measure>", pos);
        const auto m = x.substr(pos, end - pos);
        int sum = 0;
        for (size_t q = m.find("<note>"); q != std::string::npos; q = m.find("<note>", q + 1)) {
            const auto noteEnd = m.find("</note>", q);
            const auto n = m.substr(q, noteEnd - q);
            if (has(n, "<chord/>")) continue;
            const auto d = n.find("<duration>");
            sum += std::stoi(n.substr(d + 10));
        }
        CHECK_EQ(sum, 3 * 8);
        pos = end;
    }
}
